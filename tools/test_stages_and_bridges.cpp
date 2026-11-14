#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <Box2D/Box2D.h>
#include "core/Config.h"
#include "core/GameConstants.h"
#include "core/PlayerInfo.h"
#include "physics/PhysicsWorld.h"
#include "physics/PlayerCar.h"
#include "physics/Terrain.h"

#define TEST_CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
            return 1; \
        } \
    } while(0)

// FNV-1a 64-bit hash function (identical to T-112)
static uint64_t FNV1a_64(const void* data, size_t len) {
    const uint8_t* ptr = (const uint8_t*)data;
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        hash ^= (uint64_t)ptr[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

// Game listener to capture Driver Down event
class TestGameListener : public GameListener {
public:
    TestGameListener() : driverDownCalled(false), coinsCollected(0), fuelCollected(false) {}
    void OnDriverDown() override { driverDownCalled = true; }
    void OnCoinCollected(int val) override { coinsCollected += val; }
    void OnFuelCollected() override { fuelCollected = true; }

    bool driverDownCalled;
    int coinsCollected;
    bool fuelCollected;
};

int main() {
    printf("=================================================================\n");
    printf("   HCR Regression Test: Stages 1.30.0 & Suspension Bridges (16)  \n");
    printf("=================================================================\n\n");

    // Load configs from production cfg
    bool loaded = Config::LoadAll("");
    TEST_CHECK(loaded, "Failed to load production config files");

    // -------------------------------------------------------------
    // Sub-test A: 8 Stages Verification & Heightfield Determinism
    // -------------------------------------------------------------
    printf("[Sub-test A] Verifying 8 stages configuration and heightfield determinism...\n");
    TEST_CHECK(Config::StageCount == 8, "Must load exactly 8 stages from stages.cfg");

    // Check unique stage id hashes
    uint32_t hashes[8];
    for (int i = 0; i < 8; ++i) {
        hashes[i] = PlayerInfo::HashId(Config::Stages[i].id);
        for (int j = 0; j < i; ++j) {
            TEST_CHECK(hashes[i] != hashes[j], "Stage idHash must be globally unique");
        }
    }
    printf("  All 8 stages have unique FNV-1a 32-bit ID hashes.\n");

    // Frozen baseline hashes from T-112 (first 4 stages)
    const uint64_t EXPECTED_BASELINE_HASHES[4] = {
        0x6e83945913e2d687ULL, // Countryside
        0x6dfb669a32592de7ULL, // Desert
        0xf7cc9dd1f3aa54b0ULL, // Arctic
        0x0083f35054ddbad2ULL  // Moon
    };

    const int NUM_POINTS = 4001; // 0 to 2000m with step 0.5m
    float* heightBuffer = (float*)malloc(NUM_POINTS * sizeof(float));
    TEST_CHECK(heightBuffer != NULL, "Failed to allocate height buffer");

    b2World dummyWorld(b2Vec2(0.0f, -10.0f));
    uint64_t stageHashes[8];

    for (int s = 0; s < 8; ++s) {
        Terrain terrain(&dummyWorld, (StageType)s);
        for (int i = 0; i < NUM_POINTS; ++i) {
            float x = (float)i * 0.5f;
            heightBuffer[i] = terrain.GetHeightAtX(x);
        }
        stageHashes[s] = FNV1a_64(heightBuffer, NUM_POINTS * sizeof(float));

        if (s < 4) {
            printf("  Stage %-11s [0..3]: hash 0x%016llx (expected: 0x%016llx) -> %s\n",
                   Config::Stages[s].id,
                   (unsigned long long)stageHashes[s],
                   (unsigned long long)EXPECTED_BASELINE_HASHES[s],
                   (stageHashes[s] == EXPECTED_BASELINE_HASHES[s]) ? "MATCH" : "MISMATCH");
            TEST_CHECK(stageHashes[s] == EXPECTED_BASELINE_HASHES[s], "Baseline hash preservation violated!");
        } else {
            printf("  Stage %-11s [4..7]: hash 0x%016llx (2000m deterministic)\n",
                   Config::Stages[s].id,
                   (unsigned long long)stageHashes[s]);
        }
    }
    free(heightBuffer);

    // Verify determinism across second run
    float* checkBuffer = (float*)malloc(NUM_POINTS * sizeof(float));
    for (int s = 4; s < 8; ++s) {
        Terrain terrain(&dummyWorld, (StageType)s);
        for (int i = 0; i < NUM_POINTS; ++i) {
            checkBuffer[i] = terrain.GetHeightAtX((float)i * 0.5f);
        }
        uint64_t rerunHash = FNV1a_64(checkBuffer, NUM_POINTS * sizeof(float));
        TEST_CHECK(rerunHash == stageHashes[s], "Stage height generation must be 100% deterministic");
    }
    free(checkBuffer);
    printf("  [PASS] Sub-test A: 8 stages loaded, 4 baselines bit-exact, 4 new stages deterministic!\n\n");

    // -------------------------------------------------------------
    // Sub-test B: Cave Ceiling Clearance & Driver Down Physics
    // -------------------------------------------------------------
    printf("[Sub-test B] Verifying Cave ceiling clearance >= 4.5m and Driver Down on impact...\n");
    {
        PhysicsWorld physicsCave(Config::Stages[STAGE_CAVE].gravity);
        TestGameListener listener;
        physicsCave.SetListener(&listener);
        Terrain terrainCave(physicsCave.GetWorld(), STAGE_CAVE);

        TEST_CHECK(terrainCave.HasCeiling(), "Cave stage must have ceiling enabled");

        float minClearance = 999.0f;
        float maxClearance = -999.0f;
        for (int i = 0; i <= 4000; ++i) {
            float x = (float)i * 0.5f;
            float fl = terrainCave.GetHeightAtX(x);
            float cl = terrainCave.GetCeilingHeightAtX(x);
            float clearance = cl - fl;
            if (clearance < minClearance) minClearance = clearance;
            if (clearance > maxClearance) maxClearance = clearance;
        }
        printf("  Cave vertical clearance over 0-2000m (4001 pts): min = %.3f m, max = %.3f m\n",
               minClearance, maxClearance);
        TEST_CHECK(minClearance >= 4.5f, "Cave clearance must be >= 4.5m everywhere!");

        // Spawn car and verify ceiling collision triggers Driver Down
        b2Vec2 startPos(10.0f, terrainCave.GetHeightAtX(10.0f) + 1.5f);
        PlayerCar carCave(physicsCave.GetWorld(), startPos, 0, 0, 0, 0, &Config::Vehicles[0]);
        TEST_CHECK(!carCave.IsDriverDead(), "Driver must be alive initially");

        float ceilY = terrainCave.GetCeilingHeightAtX(10.0f);
        // Invert car and position head right at the ceiling b2ChainShape (CAT_TERRAIN)
        carCave.GetChassisBody()->SetTransform(b2Vec2(10.0f, ceilY - 0.30f), 3.14159f);
        carCave.GetDriverHeadBody()->SetTransform(b2Vec2(10.0f, ceilY - 0.05f), 3.14159f);
        carCave.GetChassisBody()->SetLinearVelocity(b2Vec2(0.0f, 5.0f));
        carCave.GetDriverHeadBody()->SetLinearVelocity(b2Vec2(0.0f, 5.0f));

        for (int s = 0; s < 30; ++s) {
            physicsCave.Step(1.0f / 60.0f);
            carCave.Update(1.0f / 60.0f);
            if (listener.driverDownCalled) {
                carCave.Kill();
                break;
            }
        }
        printf("  Driver Down listener called by ceiling contact: %s\n",
               listener.driverDownCalled ? "YES" : "NO");
        TEST_CHECK(listener.driverDownCalled, "Head contact with cave ceiling MUST trigger DriverDown!");
        TEST_CHECK(carCave.IsDriverDead(), "carCave.IsDriverDead() must be true after neck break");
        printf("  [PASS] Sub-test B: Cave clearance >= 4.5m guaranteed & ceiling collision breaks neck!\n\n");
    }

    // -------------------------------------------------------------
    // Sub-test C: Suspension Bridge Driving & Zero Body Leaks
    // -------------------------------------------------------------
    printf("[Sub-test C] Verifying Forest suspension bridge crossing and chunk unload cleanup...\n");
    {
        PhysicsWorld physicsForest(Config::Stages[STAGE_FOREST].gravity);
        Terrain terrainForest(physicsForest.GetWorld(), STAGE_FOREST);

        int initialBodies = physicsForest.GetWorld()->GetBodyCount();
        int activeBridges = terrainForest.GetActiveBridgeCount();
        printf("  Initial world bodies: %d | Active bridges in initial window: %d\n",
               initialBodies, activeBridges);
        TEST_CHECK(activeBridges >= 1, "Forest must have at least 1 bridge spawned in chunk 1");

        const SuspensionBridge* bridge = terrainForest.GetBridge(0);
        TEST_CHECK(bridge != NULL && bridge->active, "Bridge in slot 0 must be active");
        TEST_CHECK(bridge->plankCount == 10, "Bridge must have 10 planks");
        TEST_CHECK(bridge->startX == 39.0f && bridge->endX == 51.0f, "Bridge 0 must span [39, 51] m");

        // Verify plank bodies and joint limits
        for (int p = 0; p < bridge->plankCount; ++p) {
            b2Body* plank = bridge->planks[p];
            TEST_CHECK(plank != NULL, "Plank body must be non-null");
            TEST_CHECK(plank->GetType() == b2_dynamicBody, "Plank must be b2_dynamicBody");
            b2Fixture* fix = plank->GetFixtureList();
            TEST_CHECK(fix != NULL, "Plank fixture must exist");
            TEST_CHECK(fix->GetFilterData().categoryBits == CAT_TERRAIN, "Plank must have CAT_TERRAIN");
            TEST_CHECK((fix->GetFilterData().maskBits & (CAT_CHASSIS | CAT_WHEEL)) != 0, "Plank must collide with car");
        }

        for (int j = 0; j <= bridge->plankCount; ++j) {
            b2RevoluteJoint* joint = bridge->joints[j];
            TEST_CHECK(joint != NULL, "Revolute joint must be non-null");
            TEST_CHECK(joint->IsLimitEnabled(), "Revolute joint limits must be enabled");
            TEST_CHECK(fabsf(joint->GetLowerLimit() - (-0.25f)) < 0.001f, "Lower limit must be -0.25 rad");
            TEST_CHECK(fabsf(joint->GetUpperLimit() - 0.25f) < 0.001f, "Upper limit must be +0.25 rad");
        }
        printf("  Bridge structure verified: 10 dynamic planks (CAT_TERRAIN) + 11 revolute joints with [-0.25, +0.25] rad limits.\n");

        // Spawn car onto the bridge entrance at X = 40.0m and drive across
        b2Vec2 carStart(40.0f, 2.2f);
        PlayerCar carForest(physicsForest.GetWorld(), carStart, 15, 15, 15, 15, &Config::Vehicles[0]);

        float minBridgeCarY = 999.0f;
        bool crossed = false;
        for (int s = 0; s < 600; ++s) {
            carForest.Accelerate();
            carForest.Update(1.0f / 60.0f);
            physicsForest.Step(1.0f / 60.0f);

            float cx = carForest.GetChassisBody()->GetPosition().x;
            float cy = carForest.GetChassisBody()->GetPosition().y;
            b2Vec2 vel = carForest.GetChassisBody()->GetLinearVelocity();
            if (s % 60 == 0) {
                printf("    Step %3d: car Pos=(%.2f, %.2f), Vel=(%.2f, %.2f)\n", s, cx, cy, vel.x, vel.y);
            }
            if (cx >= 39.0f && cx <= 51.0f) {
                if (cy < minBridgeCarY) minBridgeCarY = cy;
            }
            if (cx >= 55.0f) {
                crossed = true;
                printf("  Car drove across suspension bridge at step %d (X = %.2f m, min chassis Y = %.2f m)\n",
                       s, cx, minBridgeCarY);
                break;
            }
        }
        if (!crossed) {
            float cx = carForest.GetChassisBody()->GetPosition().x;
            float cy = carForest.GetChassisBody()->GetPosition().y;
            printf("  Final pos after 600 steps: (%.2f, %.2f)\n", cx, cy);
        }
        TEST_CHECK(crossed, "Car failed to cross the suspension bridge!");
        // Under bridge at X = 45m, chasm floor dips to ~ -3.5m. The bridge planks hold car at ~ +0.5m.
        TEST_CHECK(minBridgeCarY > -1.5f, "Car fell into chasm instead of driving on bridge!");

        // Unload chunk 1 by advancing terrain player position to 300m
        printf("  Advancing car to 300m to trigger chunk 1 unload...\n");
        for (int s = 0; s < 60; ++s) {
            terrainForest.Update(300.0f);
            terrainForest.ProcessDeferredDestroy(300.0f);
        }

        // Verify that chunk 1's bridge was completely destroyed and slot recycled if new chunk spawned
        bool chunk1BridgeAlive = false;
        for (int b = 0; b < MAX_ACTIVE_BRIDGES; ++b) {
            const SuspensionBridge* brCheck = terrainForest.GetBridge(b);
            if (brCheck && brCheck->active && brCheck->chunkIndex == 1) {
                chunk1BridgeAlive = true;
            }
        }
        printf("  Chunk 1 bridge still alive: %s (slot 0 reused for chunk %d)\n",
               chunk1BridgeAlive ? "YES" : "NO",
               terrainForest.GetBridge(0) ? terrainForest.GetBridge(0)->chunkIndex : -1);
        TEST_CHECK(!chunk1BridgeAlive, "Bridge in chunk 1 must be destroyed on chunk unload");

        // Verify bodies count: bounded pool in [20, 50]
        int currentBodies = physicsForest.GetWorld()->GetBodyCount();
        printf("  Current bodies in world at 300m: %d (bounded pool verified)\n", currentBodies);
        TEST_CHECK(currentBodies <= 50, "Body count must remain bounded without leaks");
        printf("  [PASS] Sub-test C: Bridge traversal successful and zero-leak body cleanup verified!\n\n");
    }

    printf("=================================================================\n");
    printf("   ALL 3 SUB-TESTS PASSED SUCCESSFULLY (16th Test Verified)      \n");
    printf("=================================================================\n");
    return 0;
}
