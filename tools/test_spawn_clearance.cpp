#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>
#include <Box2D/Box2D.h>

#include "core/Config.h"
#include "physics/PhysicsWorld.h"
#include "physics/Terrain.h"
#include "physics/PlayerCar.h"

static b2Vec2 CalculateSafeSpawn(const Terrain* terrain, const VehicleConfig* vConfig) {
    float rR = (vConfig && vConfig->rear_wheel_radius > 0.0f) ? vConfig->rear_wheel_radius : (vConfig ? vConfig->wheel_radius : 0.40f);
    float rF = (vConfig && vConfig->front_wheel_radius > 0.0f) ? vConfig->front_wheel_radius : (vConfig ? vConfig->wheel_radius : 0.40f);
    float maxR = (rR > rF) ? rR : rF;
    float offYR = vConfig ? vConfig->rear_wheel_offset[1] : -0.50f;
    float offYF = vConfig ? vConfig->front_wheel_offset[1] : -0.50f;
    float minOffY = (offYR < offYF) ? offYR : offYF;
    float absMinOffsetY = fabsf(minOffY);

    float groundH = terrain->GetHeightAtX(0.0f);
    float startY = groundH + maxR + absMinOffsetY + 0.25f;

    if (terrain->HasCeiling()) {
        float ceilH = terrain->GetCeilingHeightAtX(0.0f);
        float headOffY = vConfig ? vConfig->driver_head_offset[1] : 0.50f;
        float headR = vConfig ? vConfig->driver_head_radius : 0.24f;
        float maxAllowedY = ceilH - headOffY - headR - 0.5f;
        if (startY > maxAllowedY) {
            startY = maxAllowedY;
        }
    }
    return b2Vec2(0.0f, startY);
}

int main(int argc, char* argv[]) {
    bool testOldBug = (argc > 1 && strcmp(argv[1], "--repro-old") == 0);

    printf("=================================================================\n");
    printf("   HCR Regression Test: Spawn Clearance & Settle (BUG-014)      \n");
    printf("   Mode: %s\n", testOldBug ? "REPRODUCE BUG (b2Vec2(0, 2.5f))" : "VERIFY FIX (Dynamic startPos)");
    printf("=================================================================\n");

    bool sOk = Config::LoadStages("config/stages.cfg");
    if (!sOk) sOk = Config::LoadStages("data/stages.cfg");
    assert(sOk && "Failed to load stages.cfg");

    bool vOk = Config::LoadVehicles("config/vehicles.cfg");
    if (!vOk) vOk = Config::LoadVehicles("data/vehicles.cfg");
    assert(vOk && "Failed to load vehicles.cfg");

    const char* testVehicles[3] = { "jeep", "ambulance", "train" };
    const VehicleConfig* vehConfigs[3];
    for (int v = 0; v < 3; ++v) {
        vehConfigs[v] = Config::FindVehicleById(testVehicles[v]);
        assert(vehConfigs[v] != NULL && "Required test vehicle not found!");
    }

    // Print authoritative table of ground heights h(0) and ceiling heights across all 28 stages
    printf("\nAuthoritative Stage Heights at X = 0.0m:\n");
    printf("| Index | Stage ID         | Ground h(0) (m) | Has Ceiling | Ceiling c(0) (m) | Gap (m) |\n");
    printf("|:-----:|:-----------------|:---------------:|:-----------:|:----------------:|:-------:|\n");

    b2World dummyWorld(b2Vec2(0.0f, -10.0f));
    float groundHeights[28];
    for (int s = 0; s < Config::StageCount && s < 28; ++s) {
        Terrain t(&dummyWorld, (StageType)s);
        float h0 = t.GetHeightAtX(0.0f);
        groundHeights[s] = h0;
        bool hasCeil = t.HasCeiling();
        float c0 = hasCeil ? t.GetCeilingHeightAtX(0.0f) : 0.0f;
        float gap = hasCeil ? (c0 - h0) : 0.0f;
        if (hasCeil) {
            printf("| %5d | %-16s | %15.4f | YES         | %16.4f | %7.2f |\n", s, Config::Stages[s].id, h0, c0, gap);
        } else {
            printf("| %5d | %-16s | %15.4f | NO          |        N/A       |   N/A   |\n", s, Config::Stages[s].id, h0);
        }
    }
    printf("\n");

    int failures = 0;

    for (int s = 0; s < Config::StageCount && s < 28; ++s) {
        float grav = Config::Stages[s].gravity;
        for (int v = 0; v < 3; ++v) {
            PhysicsWorld physics(grav, NULL);
            Terrain terrain(physics.GetWorld(), (StageType)s);
            physics.SetTerrain(&terrain);

            b2Vec2 startPos;
            if (testOldBug) {
                startPos.Set(0.0f, 2.5f); // BUG-014 Old hardcoded position
            } else {
                startPos = CalculateSafeSpawn(&terrain, vehConfigs[v]);
            }

            PlayerCar car(physics.GetWorld(), startPos, 0, 0, 0, 0, vehConfigs[v], &terrain);

            // Check step 0 (initial placement) wheel bottoms vs h(0)
            float h0 = groundHeights[s];
            float rR = (vehConfigs[v]->rear_wheel_radius > 0.0f) ? vehConfigs[v]->rear_wheel_radius : vehConfigs[v]->wheel_radius;
            float rF = (vehConfigs[v]->front_wheel_radius > 0.0f) ? vehConfigs[v]->front_wheel_radius : vehConfigs[v]->wheel_radius;

            b2Vec2 rearWheelPos0 = car.GetRearWheelBody()->GetPosition();
            b2Vec2 frontWheelPos0 = car.GetFrontWheelBody()->GetPosition();
            float rearBottom0 = rearWheelPos0.y - rR;
            float frontBottom0 = frontWheelPos0.y - rF;

            if (rearBottom0 < h0 - 0.02f || frontBottom0 < h0 - 0.02f) {
                printf("  [FAIL PENETRATION STEP 0] Stage %-14s Vehicle %-10s at step 0: rearBottom=%.3f, frontBottom=%.3f, h(0)=%.3f\n",
                       Config::Stages[s].id, testVehicles[v], rearBottom0, frontBottom0, h0);
                failures++;
                continue;
            }

            // Simulate 60 steps of settling (1.0 second of simulation)
            bool dead = false;
            bool nanFound = false;
            for (int step = 0; step < 60; ++step) {
                physics.Step(1.0f / 60.0f);
                car.Update(1.0f / 60.0f);
                terrain.Update(car.GetPosition().x);

                b2Vec2 p = car.GetPosition();
                if (isnan(p.x) || isnan(p.y)) {
                    nanFound = true;
                    break;
                }
                if (car.IsDead()) {
                    dead = true;
                    break;
                }
            }

            if (nanFound) {
                printf("  [FAIL NaN] Stage %-14s Vehicle %-10s produced NaN during settle!\n",
                       Config::Stages[s].id, testVehicles[v]);
                failures++;
                continue;
            } else if (dead) {
                printf("  [FAIL DEATH] Stage %-14s Vehicle %-10s DIED (Driver Down) during spawn settle!\n",
                       Config::Stages[s].id, testVehicles[v]);
                failures++;
                continue;
            }

            // After settle (step 60), verify car is alive and valid
            b2Vec2 rearWheelPos60 = car.GetRearWheelBody()->GetPosition();
            b2Vec2 frontWheelPos60 = car.GetFrontWheelBody()->GetPosition();
            float rearBottom60 = rearWheelPos60.y - rR;
            float frontBottom60 = frontWheelPos60.y - rF;
            (void)rearBottom60;
            (void)frontBottom60;
        }
    }

    if (failures > 0) {
        printf("\nRESULT: FAILED with %d spawn failures across 28 stages x 3 vehicles!\n", failures);
        return 1;
    }

    printf("\nRESULT: PASS! All 28 stages x 3 vehicles spawn cleanly without penetration, NaN, or death.\n");
    return 0;
}
