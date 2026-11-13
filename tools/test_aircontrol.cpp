#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <Box2D/Box2D.h>
#include "physics/PlayerCar.h"

#define TIMESTEP (1.0f / 60.0f)
#define SIM_STEPS 180

int main() {
    printf("==================================================\n");
    printf("   HCR Regression Test: In-Air Pitch Control     \n");
    printf("==================================================\n");

    FILE* csv = fopen("aircontrol_trajectory.csv", "w");
    if (csv) {
        fprintf(csv, "test_mode,step,time_sec,angle_rad,angular_vel,air_rotation\n");
    }

    // -------------------------------------------------------------
    // TEST 1: Throttle in air -> Backflip (CCW, +torque)
    // -------------------------------------------------------------
    printf("\n[Test 1] Testing Throttle in air (Gas -> Backflip / CCW)...\n");
    {
        b2Vec2 gravity(0.0f, 0.0f); // Zero gravity to isolate purely angular air dynamics
        b2World world(gravity);

        PlayerCar car(&world, b2Vec2(0.0f, 50.0f), 0, 0, 0, 0);
        car.Accelerate();

        int backflipsAwarded = 0;
        float finalAngle = 0.0f;

        for (int step = 0; step < SIM_STEPS; ++step) {
            car.Accelerate();
            car.Update(TIMESTEP);
            world.Step(TIMESTEP, 8, 3);

            int flips = car.PopFlips();
            if (flips > 0) {
                backflipsAwarded += flips;
                printf("  Step %3d (%.2fs): BACKFLIP awarded! (total: %d)\n", step, step * TIMESTEP, backflipsAwarded);
            }

            b2Vec2 pos = car.GetPosition();
            b2Body* chassis = NULL;
            // Get angle directly from world bodies
            for (b2Body* b = world.GetBodyList(); b; b = b->GetNext()) {
                if (b->GetType() == b2_dynamicBody && b->GetFixtureList() && b->GetFixtureList()->GetFilterData().categoryBits == CAT_CHASSIS) {
                    finalAngle = b->GetAngle();
                    if (csv) {
                        fprintf(csv, "throttle,%d,%.4f,%.4f,%.4f,%.4f\n",
                                step, step * TIMESTEP, b->GetAngle(), b->GetAngularVelocity(), car.GetAirTime());
                    }
                    break;
                }
            }
        }

        printf("  Test 1 Result: Final Angle = +%.3f rad (%.1f deg), Backflips = %d\n",
               finalAngle, finalAngle * 180.0f / 3.14159265f, backflipsAwarded);

        if (finalAngle <= 0.0f) {
            fprintf(stderr, "FAIL: Final angle is negative (CW), expected positive (CCW) under throttle!\n");
            if (csv) fclose(csv);
            return 1;
        }
        if (backflipsAwarded < 1) {
            fprintf(stderr, "FAIL: Expected at least 1 backflip, got %d!\n", backflipsAwarded);
            if (csv) fclose(csv);
            return 1;
        }
        printf("  [PASS] Throttle generates positive CCW torque and awards BACKFLIP.\n");
    }

    // -------------------------------------------------------------
    // TEST 2: Brake in air -> Frontflip (CW, -torque)
    // -------------------------------------------------------------
    printf("\n[Test 2] Testing Brake in air (Brake -> Frontflip / CW)...\n");
    {
        b2Vec2 gravity(0.0f, 0.0f);
        b2World world(gravity);

        PlayerCar car(&world, b2Vec2(0.0f, 50.0f), 0, 0, 0, 0);
        car.Decelerate();

        int frontflipsAwarded = 0;
        float finalAngle = 0.0f;

        for (int step = 0; step < SIM_STEPS; ++step) {
            car.Decelerate();
            car.Update(TIMESTEP);
            world.Step(TIMESTEP, 8, 3);

            int flips = car.PopFlips();
            if (flips < 0) {
                frontflipsAwarded += (-flips);
                printf("  Step %3d (%.2fs): FRONTFLIP awarded! (total: %d)\n", step, step * TIMESTEP, frontflipsAwarded);
            }

            for (b2Body* b = world.GetBodyList(); b; b = b->GetNext()) {
                if (b->GetType() == b2_dynamicBody && b->GetFixtureList() && b->GetFixtureList()->GetFilterData().categoryBits == CAT_CHASSIS) {
                    finalAngle = b->GetAngle();
                    if (csv) {
                        fprintf(csv, "brake,%d,%.4f,%.4f,%.4f,%.4f\n",
                                step, step * TIMESTEP, b->GetAngle(), b->GetAngularVelocity(), car.GetAirTime());
                    }
                    break;
                }
            }
        }

        printf("  Test 2 Result: Final Angle = %.3f rad (%.1f deg), Frontflips = %d\n",
               finalAngle, finalAngle * 180.0f / 3.14159265f, frontflipsAwarded);

        if (finalAngle >= 0.0f) {
            fprintf(stderr, "FAIL: Final angle is positive (CCW), expected negative (CW) under brake!\n");
            if (csv) fclose(csv);
            return 1;
        }
        if (frontflipsAwarded < 1) {
            fprintf(stderr, "FAIL: Expected at least 1 frontflip, got %d!\n", frontflipsAwarded);
            if (csv) fclose(csv);
            return 1;
        }
        printf("  [PASS] Brake generates negative CW torque and awards FRONTFLIP.\n");
    }

    if (csv) fclose(csv);
    printf("\nAll air control regression tests PASSED successfully!\n");
    return 0;
}
