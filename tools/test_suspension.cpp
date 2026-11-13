#include <stdio.h>
#include <math.h>
#include <Box2D/Box2D.h>
#include "physics/PlayerCar.h"

#define TIMESTEP (1.0f / 60.0f)
#define LIMIT_TOLERANCE 0.02f // Box2D numerical constraint solver tolerance

int main() {
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
    // Wheel rest offset is Y = -0.50m, wheel radius is 0.40m -> wheel bottom is -0.90m from chassis.
    // To drop wheels from exactly 5.0m above ground (Y = 0), chassis starts at Y = 5.90m.
    const float DROP_HEIGHT = 5.0f;
    b2Vec2 spawnPos(0.0f, DROP_HEIGHT + 0.90f);
    printf("Spawning car at Y = %.2fm (free-fall drop height: %.2fm)...\n", spawnPos.y, DROP_HEIGHT);

    PlayerCar car(&world, spawnPos, 0, 0, 0, 0);

    FILE* csv = fopen("suspension_droptest.csv", "w");
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
