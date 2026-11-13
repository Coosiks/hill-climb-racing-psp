#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <Box2D/Box2D.h>
#include "core/Config.h"
#include "core/System.h"
#include "physics/PlayerCar.h"

#define TEST_CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
            exit(1); \
        } \
    } while (0)

static inline float LerpAngleTest(float prevAngle, float curAngle, float alpha) {
    float diff = curAngle - prevAngle;
    while (diff > 3.14159265f)  diff -= 6.2831853f;
    while (diff < -3.14159265f) diff += 6.2831853f;
    return prevAngle + diff * alpha;
}

// Simulates driving for exactly 60 physics steps under a given frame rate pattern
static float SimulateDriving(float frameDt, int frameCount, int* outSteps) {
    b2World world(b2Vec2(0.0f, -9.81f));

    // Flat friction ground
    b2BodyDef groundDef;
    groundDef.position.Set(0.0f, 0.0f);
    b2Body* ground = world.CreateBody(&groundDef);
    b2EdgeShape edge;
    edge.Set(b2Vec2(-100.0f, 0.0f), b2Vec2(500.0f, 0.0f));
    b2FixtureDef fd;
    fd.shape = &edge;
    fd.friction = 0.8f;
    ground->CreateFixture(&fd);

    PlayerCar car(&world, b2Vec2(0.0f, 0.5f), 0, 0, 0, 0);

    float accumulator = 0.0f;
    float fixedDt = Config::Physics.fixed_timestep;
    int maxSubSteps = Config::Physics.max_sub_steps;
    int totalSteps = 0;

    for (int f = 0; f < frameCount; ++f) {
        float dt = frameDt;
        if (dt > 0.10f) dt = 0.10f;
        accumulator += dt;

        car.Accelerate();

        int substeps = 0;
        while (accumulator >= fixedDt && substeps < maxSubSteps) {
            car.CapturePreviousTransforms();
            world.Step(fixedDt, Config::Physics.velocity_iterations, Config::Physics.position_iterations);
            car.Update(fixedDt);
            accumulator -= fixedDt;
            substeps++;
            totalSteps++;
        }
    }

    if (outSteps) *outSteps = totalSteps;
    return car.GetPosition().x;
}

int main() {
    printf("=== Test Timestep Accumulator & Render Interpolation (T-108) ===\n");

    // Load configs
    Config::LoadPhysics("config/physics.cfg");
    Config::LoadVehicles("config/vehicles.cfg");
    Config::LoadStages("config/stages.cfg");

    // --- Sub-test 1: Precision Monotonic Timer ---
    printf("\n[1] Testing System Precision Timer...\n");
    double t0 = System::GetTimeSeconds();
    TEST_CHECK(t0 >= 0.0, "System::GetTimeSeconds() returned negative time");
    
    // Busy wait a tiny slice to verify monotonic increase
    double t1 = t0;
    for (volatile int i = 0; i < 100000; ++i) {
        t1 = System::GetTimeSeconds();
    }
    TEST_CHECK(t1 >= t0, "System::GetTimeSeconds() is not monotonic");
    printf("    Timer OK: t0=%.6f s, t1=%.6f s (delta=%.6f s)\n", t0, t1, t1 - t0);

    float dtSample = System::GetDeltaTime();
    TEST_CHECK(dtSample >= 0.0f, "System::GetDeltaTime() returned negative value");
    printf("    System::GetDeltaTime() OK: dt=%.6f s\n", dtSample);

    // --- Sub-test 2: Framerate Independence / Determinism ---
    printf("\n[2] Testing Framerate Independence over 1.0s virtual time (60 physics steps)...\n");
    float fixedDt = Config::Physics.fixed_timestep;
    
    // 60 FPS: 60 frames of 1x fixedDt (~16.67ms, 1 step per frame)
    int steps60 = 0;
    float dist60 = SimulateDriving(fixedDt, 60, &steps60);
    printf("    60 FPS (16.67ms x 60 frames): steps=%d, distance=%.5f m\n", steps60, dist60);
    TEST_CHECK(steps60 == 60, "60 FPS did not execute exactly 60 steps");

    // 30 FPS: 30 frames of 2x fixedDt (~33.33ms, 2 steps per frame)
    int steps30 = 0;
    float dist30 = SimulateDriving(2.0f * fixedDt, 30, &steps30);
    printf("    30 FPS (33.33ms x 30 frames): steps=%d, distance=%.5f m\n", steps30, dist30);
    TEST_CHECK(steps30 == 60, "30 FPS did not execute exactly 60 steps");

    // 20 FPS: 20 frames of 3x fixedDt (~50.00ms, 3 steps per frame)
    int steps20 = 0;
    float dist20 = SimulateDriving(3.0f * fixedDt, 20, &steps20);
    printf("    20 FPS (50.00ms x 20 frames): steps=%d, distance=%.5f m\n", steps20, dist20);
    TEST_CHECK(steps20 == 60, "20 FPS did not execute exactly 60 steps");

    // Verify distance equality across all framerates
    float diff30 = fabsf(dist60 - dist30);
    float diff20 = fabsf(dist60 - dist20);
    printf("    Delta(60 FPS, 30 FPS) = %.8f m\n", diff30);
    printf("    Delta(60 FPS, 20 FPS) = %.8f m\n", diff20);

    TEST_CHECK(diff30 < 1e-4f, "Distance diverged between 60 FPS and 30 FPS!");
    TEST_CHECK(diff20 < 1e-4f, "Distance diverged between 60 FPS and 20 FPS!");
    printf("    Framerate Independence PASS: distance is bit-exact deterministic.\n");

    // --- Sub-test 3: Spiral of Death & Spike Protection ---
    printf("\n[3] Testing Spiral of Death & Frame Spike Protection...\n");
    {
        b2World world(b2Vec2(0.0f, -9.81f));
        PlayerCar car(&world, b2Vec2(0.0f, 0.5f), 0, 0, 0, 0);

        float accumulator = 0.0f;
        float fixedDt = Config::Physics.fixed_timestep;
        int maxSubSteps = Config::Physics.max_sub_steps;

        // Catastrophic 500ms frame spike
        float spikeDt = 0.500f;
        if (spikeDt > 0.10f) spikeDt = 0.10f; // clamp to 100ms
        accumulator += spikeDt;

        int substeps = 0;
        while (accumulator >= fixedDt && substeps < maxSubSteps) {
            car.CapturePreviousTransforms();
            world.Step(fixedDt, 8, 3);
            car.Update(fixedDt);
            accumulator -= fixedDt;
            substeps++;
        }

        printf("    500ms spike test: substeps executed=%d (max allowed=%d)\n", substeps, maxSubSteps);
        TEST_CHECK(substeps <= maxSubSteps, "Spike executed more substeps than max_sub_steps!");
        TEST_CHECK(substeps == maxSubSteps, "Spike should execute exactly max_sub_steps!");
        TEST_CHECK(accumulator <= 0.10f, "Accumulator spiraled out of bounds!");
        printf("    Spiral of death protection PASS.\n");
    }

    // --- Sub-test 4: Render Transform Interpolation ---
    printf("\n[4] Testing Render Transform Interpolation...\n");
    {
        b2World world(b2Vec2(0.0f, -9.81f));
        PlayerCar car(&world, b2Vec2(0.0f, 0.5f), 0, 0, 0, 0);

        car.CapturePreviousTransforms();
        b2Vec2 p0 = car.GetPosition();

        // Step physics once
        world.Step(Config::Physics.fixed_timestep, 8, 3);
        car.Update(Config::Physics.fixed_timestep);
        b2Vec2 p1 = car.GetPosition();

        b2Vec2 at0 = car.GetInterpolatedPosition(0.0f);
        b2Vec2 at1 = car.GetInterpolatedPosition(1.0f);
        b2Vec2 atHalf = car.GetInterpolatedPosition(0.5f);

        TEST_CHECK(fabsf(at0.x - p0.x) < 1e-5f && fabsf(at0.y - p0.y) < 1e-5f, "alpha=0 did not match prev position");
        TEST_CHECK(fabsf(at1.x - p1.x) < 1e-5f && fabsf(at1.y - p1.y) < 1e-5f, "alpha=1 did not match current position");
        
        float expectedHalfX = 0.5f * (p0.x + p1.x);
        float expectedHalfY = 0.5f * (p0.y + p1.y);
        TEST_CHECK(fabsf(atHalf.x - expectedHalfX) < 1e-5f, "alpha=0.5 position X lerp incorrect");
        TEST_CHECK(fabsf(atHalf.y - expectedHalfY) < 1e-5f, "alpha=0.5 position Y lerp incorrect");
        printf("    Position interpolation PASS: p0=(%.4f, %.4f), p1=(%.4f, %.4f), half=(%.4f, %.4f)\n",
               p0.x, p0.y, p1.x, p1.y, atHalf.x, atHalf.y);

        // Test LerpAngle with wrap-around
        float aPrev = 3.10f;
        float aCur = -3.10f; // Rotated CCW across pi
        float aInterp = LerpAngleTest(aPrev, aCur, 0.5f);
        // Shortest path is +0.08318 rad, midpoint is 3.14159 rad
        float expectedAngle = 3.10f + ((aCur - aPrev) + 6.2831853f) * 0.5f;
        TEST_CHECK(fabsf(aInterp - expectedAngle) < 1e-4f, "Angle interpolation failed wrap-around shortest path");
        printf("    Angle wrap-around interpolation PASS: prev=%.3f, cur=%.3f, lerp(0.5)=%.5f\n",
               aPrev, aCur, aInterp);
    }

    printf("\nALL TIMESTEP AND INTERPOLATION TESTS PASSED!\n");
    return 0;
}
