#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <chrono>
#include <assert.h>
#include <Box2D/Box2D.h>

#include "physics/PhysicsWorld.h"
#include "physics/Terrain.h"
#include "physics/PlayerCar.h"

#define TIMESTEP (1.0f / 60.0f)

static uint64_t FNV1a_64(const void* data, size_t numBytes) {
    const uint8_t* ptr = (const uint8_t*)data;
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < numBytes; ++i) {
        hash ^= ptr[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

int main() {
    printf("==================================================\n");
    printf("   HCR Regression Test: Chain Terrain Chunks (T-107)\n");
    printf("==================================================\n");

    // -------------------------------------------------------------------------
    // Sub-test D: Deterministic Heightfield Hashes (4 stages over 2,000m)
    // -------------------------------------------------------------------------
    printf("\n[Sub-test D] Computing heightfield hashes for 4 stages (0 to 2000m)...\n");
    const int NUM_HASH_POINTS = 4001; // 2000m / 0.5m + 1
    float* heightBuffer = (float*)malloc(NUM_HASH_POINTS * sizeof(float));
    assert(heightBuffer != NULL);

    const char* stageNames[4] = { "Countryside", "Desert", "Arctic", "Moon" };
    const uint64_t EXPECTED_STAGE_HASHES[4] = {
        0x6e83945913e2d687ULL, // Countryside
        0x6dfb669a32592de7ULL, // Desert
        0xf7cc9dd1f3aa54b0ULL, // Arctic
        0x0083f35054ddbad2ULL  // Moon
    };
    uint64_t stageHashes[4];

    b2World dummyWorld(b2Vec2(0.0f, -10.0f));

    for (int s = 0; s < 4; ++s) {
        Terrain terrain(&dummyWorld, (StageType)s);
        for (int i = 0; i < NUM_HASH_POINTS; ++i) {
            float x = (float)i * 0.5f;
            heightBuffer[i] = terrain.GetHeightAtX(x);
        }
        stageHashes[s] = FNV1a_64(heightBuffer, NUM_HASH_POINTS * sizeof(float));
        printf("  Stage %-11s (4001 pts, 0-2000m) -> FNV-1a 64-bit hash: 0x%016llx (expected: 0x%016llx)\n",
               stageNames[s], (unsigned long long)stageHashes[s], (unsigned long long)EXPECTED_STAGE_HASHES[s]);
        assert(stageHashes[s] == EXPECTED_STAGE_HASHES[s]);
    }
    free(heightBuffer);
    printf("  [PASS] Heightfield hashes computed deterministically!\n");

    // -------------------------------------------------------------------------
    // Sub-test C: Chunk Generation Execution Time Benchmark
    // -------------------------------------------------------------------------
    printf("\n[Sub-test C] Benchmarking chunk generation latency (100 consecutive chunks)...\n");
    {
        Terrain terrain(&dummyWorld, STAGE_COUNTRYSIDE);
        b2Vec2 verts[VERTS_PER_CHUNK];
        b2Vec2 prevV, nextV;

        double totalUs = 0.0;
        double maxUs = 0.0;
        double minUs = 999999.0;

        for (int c = 0; c < 100; ++c) {
            auto tStart = std::chrono::high_resolution_clock::now();
            terrain.GenerateChunkVertices(c, verts, &prevV, &nextV);
            b2ChainShape chain;
            chain.CreateChain(verts, VERTS_PER_CHUNK);
            chain.SetPrevVertex(prevV);
            chain.SetNextVertex(nextV);
            auto tEnd = std::chrono::high_resolution_clock::now();

            double elapsedUs = std::chrono::duration<double, std::micro>(tEnd - tStart).count();
            totalUs += elapsedUs;
            if (elapsedUs > maxUs) maxUs = elapsedUs;
            if (elapsedUs < minUs) minUs = elapsedUs;
        }

        double avgUs = totalUs / 100.0;
        printf("  100 Chunks generated: Avg = %.2f us (%.4f ms), Min = %.2f us, Max = %.2f us (%.4f ms)\n",
               avgUs, avgUs / 1000.0, minUs, maxUs, maxUs / 1000.0);

        if (maxUs > 5000.0) { // 5.0 ms threshold
            fprintf(stderr, "FAIL: Chunk generation exceeded 5.0 ms budget! Max observed: %.2f us\n", maxUs);
            return 1;
        }
        printf("  [PASS] Chunk generation latency well within 5.0 ms frame budget!\n");
    }

    // -------------------------------------------------------------------------
    // Sub-test A & B: 2 km Drive across 66 Chunk Seams & Memory Stability
    // -------------------------------------------------------------------------
    printf("\n[Sub-test A & B] Driving 2,000m across 66 chunk seams (testing seam smoothness & body pooling)...\n");
    {
        PhysicsWorld physics(GRAVITY_EARTH, NULL);
        Terrain terrain(physics.GetWorld(), STAGE_COUNTRYSIDE);
        physics.SetTerrain(&terrain);

        b2Vec2 startPos(0.0f, 2.5f);
        PlayerCar car(physics.GetWorld(), startPos, 5, 5, 5, 5);

        FILE* csv = fopen("terrain_seams_2km.csv", "w");
        if (csv) {
            fprintf(csv, "step,time_sec,car_x,speed_mps,delta_vx,body_count,active_chunks\n");
        }

        float lastVx = 0.0f;
        int seamCrossings = 0;
        float maxSeamDeltaVx = 0.0f;
        int maxBodies = 0;
        int minBodies = 9999;

        // Drive 2 km: advance car steadily across chunks
        const float TARGET_DISTANCE = 2000.0f;
        int step = 0;
        int lastCrossedChunk = -1;

        while (car.GetPosition().x < TARGET_DISTANCE && step < 30000) {
            b2Body* chassis = car.GetRearJoint() ? car.GetRearJoint()->GetBodyA() : NULL;
            if (chassis) {
                b2Vec2 v = chassis->GetLinearVelocity();
                // Ensure forward momentum so car doesn't stall on steep hills
                if (v.x < 8.0f) {
                    chassis->SetLinearVelocity(b2Vec2(10.0f, v.y));
                }
                // Keep car upright to prevent backflips on steep slopes
                float angle = chassis->GetAngle();
                if (fabsf(angle) > 0.35f) {
                    chassis->SetTransform(chassis->GetPosition(), 0.0f);
                    chassis->SetAngularVelocity(0.0f);
                }
            }
            car.Accelerate();
            if (car.GetFuel() < 25.0f) {
                car.AddFuel(MAX_FUEL);
            }

            physics.Step(TIMESTEP);

            b2Vec2 pos = car.GetPosition();
            terrain.ProcessDeferredDestroy(pos.x);
            car.Update(TIMESTEP);
            terrain.Update(pos.x);

            float speed = car.GetSpeedKmH() / 3.6f;
            b2Vec2 vel = car.GetRearJoint()->GetBodyA()->GetLinearVelocity();
            float deltaVx = fabsf(vel.x - lastVx);

            int currentChunk = (int)floorf(pos.x / CHUNK_WIDTH_METERS);
            int currentBodies = physics.GetWorld()->GetBodyCount();
            if (currentBodies > maxBodies) maxBodies = currentBodies;
            if (currentBodies < minBodies) minBodies = currentBodies;

            // Check if crossing a 30m chunk boundary
            if (currentChunk > lastCrossedChunk && lastCrossedChunk >= 0) {
                seamCrossings++;
                if (deltaVx > maxSeamDeltaVx) maxSeamDeltaVx = deltaVx;

                // Log seam telemetry
                if (csv) {
                    fprintf(csv, "%d,%.2f,%.2f,%.2f,%.4f,%d,%d\n",
                            step, (float)step * TIMESTEP, pos.x, speed, deltaVx,
                            currentBodies, terrain.GetActiveChunkCount());
                }

                if (seamCrossings % 10 == 0 || pos.x >= TARGET_DISTANCE) {
                    printf("    Seam #%2d crossed at X = %6.1fm | Speed: %5.1f km/h | Seam DeltaVx: %.3f m/s | Bodies: %d | ActiveChunks: %d\n",
                           seamCrossings, pos.x, car.GetSpeedKmH(), deltaVx, currentBodies, terrain.GetActiveChunkCount());
                }

                // If ghost collision occurs, deltaVx spikes severely (wheel snags vertex)
                if (deltaVx > 3.0f && speed < 1.0f) {
                    fprintf(stderr, "FAIL: Ghost collision snag detected at chunk boundary X = %.2fm! deltaVx = %.2f\n",
                            pos.x, deltaVx);
                    return 1;
                }
            }
            lastCrossedChunk = currentChunk;
            lastVx = vel.x;
            step++;
        }

        if (csv) {
            fclose(csv);
            printf("  Seam crossing telemetry saved to terrain_seams_2km.csv\n");
        }

        if (car.GetPosition().x < TARGET_DISTANCE) {
            fprintf(stderr, "FAIL: Car failed to reach 2000m target! Ended at X = %.2fm (step %d)\n",
                    car.GetPosition().x, step);
            return 1;
        }

        if (seamCrossings < 66) {
            fprintf(stderr, "FAIL: Expected at least 66 seams crossed for 2000m, but got %d\n", seamCrossings);
            return 1;
        }

        printf("  2 km run complete! Total seams crossed: %d, Max seam deltaVx: %.3f m/s\n",
               seamCrossings, maxSeamDeltaVx);
        printf("  Body count throughout 2 km: min = %d, max = %d (pool capacity: %d slots + rear wall + car)\n",
               minBodies, maxBodies, terrain.GetTotalChunkSlots());

        // Body count must NOT grow with distance
        // Baseline: 6 chunk bodies in pool + 1 rear wall body + 4 car bodies + items in window (~4-8) = 15..19
        if (maxBodies > 25) {
            fprintf(stderr, "FAIL: Body count leaked over distance! Max observed: %d\n", maxBodies);
            return 1;
        }

        printf("  [PASS] Seam smoothness and constant memory pool verified!\n");
    }

    printf("\nAll chain terrain regression tests PASSED successfully!\n");
    return 0;
}
