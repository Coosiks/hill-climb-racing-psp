#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <Box2D/Box2D.h>

#include "physics/PhysicsWorld.h"
#include "physics/Terrain.h"
#include "physics/PlayerCar.h"

#define TIMESTEP (1.0f / 60.0f)

class TestGameListener : public GameListener {
public:
    int coinCalls;
    int coinTotal;
    int fuelCalls;
    bool driverDown;

    TestGameListener() : coinCalls(0), coinTotal(0), fuelCalls(0), driverDown(false) {}

    virtual void OnDriverDown() {
        driverDown = true;
    }

    virtual void OnCoinCollected(int value) {
        coinCalls++;
        coinTotal += value;
    }

    virtual void OnFuelCollected() {
        fuelCalls++;
    }
};

int main() {
    printf("==================================================\n");
    printf("   HCR Regression Test: Item Lifecycle (T-106)    \n");
    printf("==================================================\n");

    // -------------------------------------------------------------------------
    // Sub-test A: Single collection & duplicate contact immunity
    // -------------------------------------------------------------------------
    printf("\n[Sub-test A] Testing single collection & duplicate contact immunity...\n");
    {
        TestGameListener listener;
        PhysicsWorld physics(GRAVITY_EARTH, &listener);
        Terrain terrain(physics.GetWorld(), STAGE_COUNTRYSIDE);
        physics.SetTerrain(&terrain);

        // Spawn test coin at (5.0, 2.0) with value 500
        b2Body* coinBody = terrain.SpawnItem(5.0f, 2.0f, 500);
        assert(coinBody != NULL);

        // Spawn test fuel canister at (15.0, 2.0)
        b2Body* fuelBody = terrain.SpawnItem(15.0f, 2.0f, 1000);
        assert(fuelBody != NULL);

        assert(listener.coinCalls == 0);
        assert(listener.coinTotal == 0);
        assert(listener.fuelCalls == 0);

        // Spawn a collector dynamic body overlapping the coin
        b2BodyDef bd;
        bd.type = b2_dynamicBody;
        bd.position.Set(5.0f, 2.0f);
        b2Body* collector = physics.GetWorld()->CreateBody(&bd);

        b2CircleShape collectorShape;
        collectorShape.m_radius = 0.5f;

        b2FixtureDef cfd;
        cfd.shape = &collectorShape;
        cfd.filter.categoryBits = CAT_CHASSIS;
        cfd.filter.maskBits = CAT_ITEM | CAT_TERRAIN;
        collector->CreateFixture(&cfd);

        // Also add a second fixture (simulating wheel touching at the exact same timestep)
        b2FixtureDef wfd;
        wfd.shape = &collectorShape;
        wfd.filter.categoryBits = CAT_WHEEL;
        wfd.filter.maskBits = CAT_ITEM | CAT_TERRAIN;
        collector->CreateFixture(&wfd);

        // Step 1: Contact occurs
        physics.Step(TIMESTEP);

        // Verification A.1: Awarded exactly once even with multi-fixture overlap
        printf("  Step 1 contact: coinCalls = %d (expected 1), coinTotal = %d (expected 500)\n",
               listener.coinCalls, listener.coinTotal);
        if (listener.coinCalls != 1 || listener.coinTotal != 500) {
            fprintf(stderr, "FAIL: Coin awarded %d times instead of 1!\n", listener.coinCalls);
            return 1;
        }

        // Process deferred destruction queue
        terrain.ProcessDeferredDestroy(5.0f);

        // Step 2..10: Simulate 10 further steps while overlapping the same position
        for (int step = 0; step < 10; ++step) {
            physics.Step(TIMESTEP);
            terrain.ProcessDeferredDestroy(5.0f);
        }

        printf("  After 10 subsequent steps: coinCalls = %d, coinTotal = %d\n",
               listener.coinCalls, listener.coinTotal);
        if (listener.coinCalls != 1 || listener.coinTotal != 500) {
            fprintf(stderr, "FAIL: Duplicate coin awarded upon subsequent contact! coinCalls = %d\n", listener.coinCalls);
            return 1;
        }

        // Spawn fuel collector to overlap fuel canister at (15.0, 2.0)
        b2BodyDef fbd;
        fbd.type = b2_dynamicBody;
        fbd.position.Set(15.0f, 2.0f);
        b2Body* fuelCollector = physics.GetWorld()->CreateBody(&fbd);
        fuelCollector->CreateFixture(&cfd);

        physics.Step(TIMESTEP);

        printf("  Fuel contact: fuelCalls = %d (expected 1)\n", listener.fuelCalls);
        if (listener.fuelCalls != 1) {
            fprintf(stderr, "FAIL: Fuel collected %d times instead of 1!\n", listener.fuelCalls);
            return 1;
        }

        terrain.ProcessDeferredDestroy(15.0f);

        // Further steps overlapping fuel
        for (int step = 0; step < 10; ++step) {
            physics.Step(TIMESTEP);
            terrain.ProcessDeferredDestroy(15.0f);
        }

        if (listener.fuelCalls != 1) {
            fprintf(stderr, "FAIL: Duplicate fuel award upon subsequent contact! fuelCalls = %d\n", listener.fuelCalls);
            return 1;
        }

        printf("  [PASS] Single collection and duplicate contact immunity verified!\n");
    }

    // -------------------------------------------------------------------------
    // Sub-test B: 10-Minute Simulated Drive & Body Count Stability
    // -------------------------------------------------------------------------
    printf("\n[Sub-test B] Simulating 10 minutes of driving (36,000 steps @ 60 Hz)...\n");
    {
        TestGameListener listener;
        PhysicsWorld physics(GRAVITY_EARTH, &listener);
        Terrain terrain(physics.GetWorld(), STAGE_COUNTRYSIDE);
        physics.SetTerrain(&terrain);

        b2Vec2 startPos(0.0f, 2.5f);
        PlayerCar car(physics.GetWorld(), startPos, 2, 2, 2, 2);

        FILE* csv = fopen("items_10min_telemetry.csv", "w");
        if (csv) {
            fprintf(csv, "step,time_sec,car_x,body_count,item_count\n");
        }

        int initialBodies = physics.GetWorld()->GetBodyCount();
        printf("  Initial world body count: %d\n", initialBodies);

        const int TOTAL_STEPS = 36000; // 10 minutes * 60 seconds * 60 steps/sec
        int maxBodyCount = 0;
        int minBodyCount = 9999;

        for (int step = 0; step < TOTAL_STEPS; ++step) {
            float speed = car.GetSpeedKmH() / 3.6f;
            float angle = car.GetRearJoint() ? car.GetRearJoint()->GetBodyA()->GetAngle() : 0.0f;
            if (angle > 0.35f) {
                car.Decelerate(); // tilt nose down in air
            } else if (angle < -0.35f) {
                car.Accelerate(); // tilt nose up in air
            } else if (speed < 6.0f) {
                car.Accelerate();
            } else {
                car.Idle();
            }
            if (car.GetFuel() < 25.0f) {
                car.AddFuel(MAX_FUEL); // continuous drive
            }

            physics.Step(TIMESTEP);

            b2Vec2 pos = car.GetPosition();
            terrain.ProcessDeferredDestroy(pos.x);

            car.Update(TIMESTEP);
            terrain.Update(pos.x);

            int currentBodies = physics.GetWorld()->GetBodyCount();
            if (currentBodies > maxBodyCount) maxBodyCount = currentBodies;
            if (currentBodies < minBodyCount) minBodyCount = currentBodies;

            // Log telemetry every 5.0 seconds (every 300 steps)
            if (step % 300 == 0 || step == TOTAL_STEPS - 1) {
                float timeSec = (float)step * TIMESTEP;
                if (csv) {
                    fprintf(csv, "%d,%.2f,%.2f,%d,%d\n",
                            step, timeSec, pos.x, currentBodies, terrain.GetItemCount());
                }

                // Print console milestone every 60 seconds
                if (step % 3600 == 0 || step == TOTAL_STEPS - 1) {
                    printf("    Time: %5.1fs | Step: %5d | CarX: %7.1fm | Bodies: %2d | Items: %2d | CoinsCollected: %d\n",
                           timeSec, step, pos.x, currentBodies, terrain.GetItemCount(), listener.coinCalls);
                }
            }
        }

        if (csv) {
            fclose(csv);
            printf("  Telemetry saved to items_10min_telemetry.csv\n");
        }

        printf("  10-min run complete. Body count range: [%d, %d]\n", minBodyCount, maxBodyCount);

        // Body count must stay tightly bounded:
        // Chassis (1) + 2 Wheels (2) + Driver head (1) + Terrain body (1) + rear wall (1) + item window items (~6-10) <= 20
        if (maxBodyCount > 25) {
            fprintf(stderr, "FAIL: Body count leaked! Max observed: %d (threshold: 25)\n", maxBodyCount);
            return 1;
        }

        printf("  [PASS] 10-minute simulation confirmed zero body memory leak!\n");
    }

    // -------------------------------------------------------------------------
    // Sub-test C: Despawn of items behind carX - 40m
    // -------------------------------------------------------------------------
    printf("\n[Sub-test C] Testing despawn of items behind carX - 40m...\n");
    {
        TestGameListener listener;
        PhysicsWorld physics(GRAVITY_EARTH, &listener);
        Terrain terrain(physics.GetWorld(), STAGE_COUNTRYSIDE);
        physics.SetTerrain(&terrain);

        // Spawn item at X = 10.0m
        b2Body* item10 = terrain.SpawnItem(10.0f, 2.0f, 100);
        assert(item10 != NULL);

        // Spawn item at X = 70.0m
        b2Body* item70 = terrain.SpawnItem(70.0f, 2.0f, 500);
        assert(item70 != NULL);

        int countBefore = terrain.GetItemCount();
        printf("  Spawned items: count = %d\n", countBefore);

        // Position car at X = 45.0m (carX - 40 = 5.0m < 10.0m -> item at 10.0m should NOT be despawned yet)
        terrain.ProcessDeferredDestroy(45.0f);
        if (terrain.GetItemCount() != countBefore) {
            fprintf(stderr, "FAIL: Item at 10m prematurely despawned when car was at 45m!\n");
            return 1;
        }
        printf("  Car at 45m: Item at 10m preserved (despawn threshold: 5.0m)\n");

        // Now move car to X = 55.0m (carX - 40 = 15.0m > 10.0m -> item at 10.0m MUST be despawned)
        terrain.ProcessDeferredDestroy(55.0f);
        printf("  Car at 55m: items remaining = %d\n", terrain.GetItemCount());

        bool foundItem10 = false;
        bool foundItem70 = false;
        for (int i = 0; i < terrain.GetItemCount(); ++i) {
            if (terrain.GetItems()[i].x == 10.0f) foundItem10 = true;
            if (terrain.GetItems()[i].x == 70.0f) foundItem70 = true;
        }

        if (foundItem10) {
            fprintf(stderr, "FAIL: Item at 10m was NOT despawned when car reached 55m!\n");
            return 1;
        }
        if (!foundItem70) {
            fprintf(stderr, "FAIL: Item at 70m was erroneously despawned!\n");
            return 1;
        }

        printf("  [PASS] Item behind carX - 40m cleanly despawned!\n");
    }

    printf("\nAll item lifecycle regression tests PASSED successfully!\n");
    return 0;
}
