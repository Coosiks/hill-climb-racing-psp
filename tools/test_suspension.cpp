#include <stdio.h>
#include <math.h>
#include <Box2D/Box2D.h>
#include "core/Config.h"
#include "physics/PlayerCar.h"

#define TIMESTEP (1.0f / 60.0f)
#define LIMIT_TOLERANCE 0.02f // Box2D numerical constraint solver tolerance

static int RunDrop(const VehicleConfig& config, int level, const char* csvPath) {
    printf("==================================================\n");
    printf("   HCR Regression Test: Suspension Travel Limits  \n");
    printf("==================================================\n");

    b2Vec2 gravity(0.0f, -10.0f);
    b2World world(gravity);

    // Create flat terrain at Y = 0
    b2BodyDef bd;
    bd.type = b2_staticBody;
    b2Body* ground = world.CreateBody(&bd);

    b2EdgeShape groundEdge;
    groundEdge.Set(b2Vec2(-50.0f, 0.0f), b2Vec2(50.0f, 0.0f));
    b2FixtureDef gfd;
    gfd.shape = &groundEdge;
    gfd.friction = 0.6f;
    gfd.filter.categoryBits = CAT_TERRAIN;
    ground->CreateFixture(&gfd);

    // Drop from 5 meters height:
    const float DROP_HEIGHT = 5.0f;
    const float rearRadius = config.rear_wheel_radius > 0.0f ? config.rear_wheel_radius : config.wheel_radius;
    const float frontRadius = config.front_wheel_radius > 0.0f ? config.front_wheel_radius : config.wheel_radius;
    b2Vec2 spawnPos(0.0f, DROP_HEIGHT + fmaxf(rearRadius - config.rear_wheel_offset[1],
                                             frontRadius - config.front_wheel_offset[1]));
    printf("Spawning car at Y = %.2fm (free-fall drop height: %.2fm)...\n", spawnPos.y, DROP_HEIGHT);

    PlayerCar car(&world, spawnPos, 0, level, 0, 0, &config);
    printf("Drop lvl%d: %.6f Hz\n", level, car.GetRearJoint()->GetSpringFrequencyHz());

    FILE* csv = fopen(csvPath, "w");
    if (!csv) return 1;
    if (csv) {
        fprintf(csv, "step,time_sec,chassis_y,rear_trans,front_trans,rear_vel_y,front_vel_y,is_dead\n");
    }

    float maxRearTrans = -999.0f, minRearTrans = 999.0f;
    float maxFrontTrans = -999.0f, minFrontTrans = 999.0f;
    bool impactOccurred = false;
    int impactStep = -1;

    // Simulate 180 steps (3.0 seconds)
    for (int step = 0; step < 180; ++step) {
        car.Idle();
        car.Update(TIMESTEP);
        world.Step(TIMESTEP, 8, 3);

        b2Vec2 pos = car.GetPosition();
        float rTrans = car.GetRearSuspensionTranslation();
        float fTrans = car.GetFrontSuspensionTranslation();

        if (rTrans > maxRearTrans) maxRearTrans = rTrans;
        if (rTrans < minRearTrans) minRearTrans = rTrans;
        if (fTrans > maxFrontTrans) maxFrontTrans = fTrans;
        if (fTrans < minFrontTrans) minFrontTrans = fTrans;

        if (!impactOccurred && !car.IsInAir()) {
            impactOccurred = true;
            impactStep = step;
            printf("  Hard impact detected at step %d (t = %.2fs, chassis Y = %.3fm)\n",
                   step, step * TIMESTEP, pos.y);
        }

        if (csv) {
            fprintf(csv, "%d,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%d\n",
                    step, step * TIMESTEP, pos.y, rTrans, fTrans,
                    car.GetRearJoint()->GetJointSpeed(),
                    car.GetFrontJoint()->GetJointSpeed(),
                    car.IsDead() ? 1 : 0);
        }

        // Check for physics explosion or NaN
        if (isnan(pos.x) || isnan(pos.y) || isnan(rTrans) || isnan(fTrans)) {
            fprintf(stderr, "FAIL: Step %d: NaN detected in physics simulation!\n", step);
            if (csv) fclose(csv);
            return 1;
        }

        // Verify limits at every frame: lower = -0.35m, upper = +0.35m
        const float LOWER_BOUND = -0.35f - LIMIT_TOLERANCE;
        const float UPPER_BOUND =  0.35f + LIMIT_TOLERANCE;

        if (rTrans < LOWER_BOUND || rTrans > UPPER_BOUND) {
            fprintf(stderr, "FAIL: Step %d: Rear suspension exceeded limits! (trans: %.4fm, allowed: [%.2f, %.2f])\n",
                    step, rTrans, LOWER_BOUND, UPPER_BOUND);
            if (csv) fclose(csv);
            return 1;
        }

        if (fTrans < LOWER_BOUND || fTrans > UPPER_BOUND) {
            fprintf(stderr, "FAIL: Step %d: Front suspension exceeded limits! (trans: %.4fm, allowed: [%.2f, %.2f])\n",
                    step, fTrans, LOWER_BOUND, UPPER_BOUND);
            if (csv) fclose(csv);
            return 1;
        }

        if (car.IsDead()) {
            fprintf(stderr, "FAIL: Step %d: Car driver died on landing impact!\n", step);
            if (csv) fclose(csv);
            return 1;
        }
    }

    if (csv) fclose(csv);

    printf("Impact step: %d (%.2fs)\n", impactStep, impactStep * TIMESTEP);
    printf("Rear suspension translation range:  [%.4fm, %.4fm]\n", minRearTrans, maxRearTrans);
    printf("Front suspension translation range: [%.4fm, %.4fm]\n", minFrontTrans, maxFrontTrans);

    if (!impactOccurred) {
        fprintf(stderr, "FAIL: Car did not hit the ground within simulation timeframe!\n");
        return 1;
    }

    printf("Suspension travel limits regression test PASSED successfully!\n");
    return 0;
}

int main() {
    const int base = RunDrop(Config::VehicleJeep, 0, "suspension_droptest.csv");
    Config::LoadAll();
    const VehicleConfig* jeep = Config::FindVehicleById("jeep");
    if (!jeep || Config::VehicleCount != 29) return 1;

    printf("vehicle,lvl0_hz,lvl20_hz\n");
    for (int i = 0; i < Config::VehicleCount; ++i) {
        const VehicleConfig& config = Config::Vehicles[i];
        b2World world(b2Vec2(0.0f, -10.0f));
        PlayerCar base(&world, b2Vec2(0.0f, 3.0f), 0, 0, 0, 0, &config);
        PlayerCar upgraded(&world, b2Vec2(0.0f, 3.0f), 0, 20, 0, 0, &config);
        printf("%s,%.6f,%.6f\n", config.id, base.GetRearJoint()->GetSpringFrequencyHz(),
               upgraded.GetRearJoint()->GetSpringFrequencyHz());
    }
    const int upgraded = RunDrop(*jeep, 20, "suspension_droptest_lvl20.csv");
    return base || upgraded;
}
