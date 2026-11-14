#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "core/Config.h"
#include "core/GameConstants.h"
#include "physics/PlayerCar.h"
#include "physics/PhysicsWorld.h"
#include <Box2D/Box2D.h>

#define TEST_CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
            exit(1); \
        } \
    } while(0)

static bool feq(float a, float b, float eps = 0.005f) {
    return fabsf(a - b) <= eps;
}

// Mock listener for PhysicsWorld
class MockGameListener : public GameListener {
public:
    MockGameListener() : driverDownCalled(false), coinsCollected(0), fuelCollected(false) {}
    void OnDriverDown() override { driverDownCalled = true; }
    void OnCoinCollected(int value) override { coinsCollected += value; }
    void OnFuelCollected() override { fuelCollected = true; }

    bool driverDownCalled;
    int coinsCollected;
    bool fuelCollected;
};

int main() {
    printf("========================================================\n");
    printf("   HCR Regression Test: Tricks & Driver Ragdoll (T-113) \n");
    printf("========================================================\n\n");

    Config::LoadPhysics("config/physics.cfg");
    Config::LoadVehicles("config/vehicles.cfg");

    // -------------------------------------------------------------
    // Sub-test A: Neck Spring & Angular Limits
    // -------------------------------------------------------------
    printf("[Sub-test A] Driver head revolute neck joint limits & PD spring...\n");
    {
        b2Vec2 gravity(0.0f, -10.0f);
        b2World world(gravity);

        PlayerCar car(&world, b2Vec2(0.0f, 10.0f), 0, 0, 0, 0);
        b2RevoluteJoint* neck = car.GetNeckJoint();
        TEST_CHECK(neck != NULL, "Neck joint must not be NULL");
        TEST_CHECK(neck->IsLimitEnabled(), "Neck angular limits must be enabled");
        TEST_CHECK(feq(neck->GetLowerLimit(), -0.60f), "Neck lower limit must be -0.60 rad");
        TEST_CHECK(feq(neck->GetUpperLimit(),  0.60f), "Neck upper limit must be +0.60 rad");

        b2Body* head = car.GetDriverHeadBody();
        TEST_CHECK(head != NULL, "Driver head body must not be NULL");

        // Apply angular impulses and torque to driver head to simulate violent shaking
        for (int i = 0; i < 60; ++i) {
            head->ApplyTorque((i % 2 == 0) ? 3.0f : -3.0f, true);
            car.Update(1.0f / 60.0f);
            world.Step(1.0f / 60.0f, 8, 3);

            float jointAngle = neck->GetJointAngle();
            TEST_CHECK(jointAngle >= -0.65f && jointAngle <= 0.65f,
                       "Neck joint angle must stay within revolute limits under shake");
        }

        // Allow system to settle in calm conditions; PD spring must restore angle to near 0
        for (int i = 0; i < 90; ++i) {
            car.Update(1.0f / 60.0f);
            world.Step(1.0f / 60.0f, 8, 3);
        }

        float settledAngle = neck->GetJointAngle();
        printf("  Settled neck angle after spring restoration: %.4f rad (target ~0.0)\n", settledAngle);
        TEST_CHECK(fabsf(settledAngle) < 0.08f, "PD spring motor must restore head to vertical equilibrium");
        printf("  -> SUBTEST A PASSED\n\n");
    }

    // -------------------------------------------------------------
    // Sub-test B: Driver Down on Terrain Contact vs Coin Sensor Immunity
    // -------------------------------------------------------------
    printf("[Sub-test B] Solid terrain collision triggers DriverDown; coin sensors immune...\n");
    {
        PhysicsWorld physics(-10.0f);
        MockGameListener listener;
        physics.SetListener(&listener);
        b2World* world = physics.GetWorld();

        // 1. Place a coin sensor right at the driver head location
        b2BodyDef coinBd;
        coinBd.type = b2_staticBody;
        coinBd.position.Set(0.0f, 5.5f);
        b2Body* coinBody = world->CreateBody(&coinBd);

        b2CircleShape coinShape;
        coinShape.m_radius = 0.50f;
        b2FixtureDef coinFd;
        coinFd.shape = &coinShape;
        coinFd.isSensor = true;
        coinFd.filter.categoryBits = CAT_ITEM;
        coinBody->CreateFixture(&coinFd);

        // Spawn car with driver head directly inside coin sensor
        PlayerCar car(world, b2Vec2(0.0f, 5.0f), 0, 0, 0, 0);
        TEST_CHECK(!car.IsDriverDead(), "Driver must be alive initially");

        // Step physics with coin sensor contact
        for (int i = 0; i < 5; ++i) {
            physics.Step(1.0f / 60.0f);
            car.Update(1.0f / 60.0f);
        }
        TEST_CHECK(!listener.driverDownCalled, "Coin sensor contact must NOT trigger DriverDown");
        TEST_CHECK(!car.IsDriverDead(), "Driver must remain alive after coin sensor contact");

        // 2. Spawn a solid terrain ceiling and press inverted driver head into it
        b2BodyDef ceilingBd;
        ceilingBd.type = b2_staticBody;
        ceilingBd.position.Set(0.0f, 6.0f);
        b2Body* ceilingBody = world->CreateBody(&ceilingBd);

        b2EdgeShape ceilingShape;
        ceilingShape.Set(b2Vec2(-10.0f, 0.0f), b2Vec2(10.0f, 0.0f));
        b2FixtureDef ceilingFd;
        ceilingFd.shape = &ceilingShape;
        ceilingFd.filter.categoryBits = CAT_TERRAIN;
        ceilingFd.isSensor = false;
        ceilingBody->CreateFixture(&ceilingFd);

        // Turn car upside down so head strikes ceiling terrain
        car.GetChassisBody()->SetTransform(b2Vec2(0.0f, 5.70f), 3.14159f);
        car.GetDriverHeadBody()->SetTransform(b2Vec2(0.0f, 5.95f), 3.14159f);

        physics.Step(1.0f / 60.0f);
        TEST_CHECK(listener.driverDownCalled, "Solid terrain contact on driver head MUST trigger DriverDown");

        // Simulate game handling driver down
        car.Kill();
        TEST_CHECK(car.IsDriverDead(), "car.IsDriverDead() must be true after Kill()");

        // Joint must remain intact with weakened ragdoll torque
        b2RevoluteJoint* neck = car.GetNeckJoint();
        TEST_CHECK(neck != NULL, "Neck joint must not be deleted on Kill() for ragdoll effect");
        TEST_CHECK(feq(neck->GetMaxMotorTorque(), 0.5f), "Ragdoll neck motor torque must weaken to 0.5 N*m");
        printf("  -> SUBTEST B PASSED\n\n");
    }

    // -------------------------------------------------------------
    // Sub-test C: Air Time & Wheelie Bonuses
    // -------------------------------------------------------------
    printf("[Sub-test C] Air Time & Wheelie trick bonus awards...\n");
    {
        b2Vec2 gravity(0.0f, -10.0f);
        b2World world(gravity);

        // Create ground at Y = 0
        b2BodyDef gbd;
        gbd.type = b2_staticBody;
        b2Body* ground = world.CreateBody(&gbd);

        b2EdgeShape gShape;
        gShape.Set(b2Vec2(-50.0f, 0.0f), b2Vec2(50.0f, 0.0f));
        b2FixtureDef gfd;
        gfd.shape = &gShape;
        gfd.filter.categoryBits = CAT_TERRAIN;
        ground->CreateFixture(&gfd);

        // Part 1: Air Time Bonus
        // Spawn car high in air: Y = 20.0m
        PlayerCar airCar(&world, b2Vec2(0.0f, 20.0f), 0, 0, 0, 0);
        TEST_CHECK(airCar.IsInAir(), "Car must start in air");

        // Float in air for 75 frames (1.25s > 1.0s threshold)
        for (int i = 0; i < 75; ++i) {
            airCar.Update(1.0f / 60.0f);
            // Counteract gravity to stay airborne
            airCar.GetChassisBody()->SetLinearVelocity(b2Vec2(0.0f, 0.0f));
            world.Step(1.0f / 60.0f, 8, 3);
        }
        TEST_CHECK(airCar.GetAirTime() >= 1.0f, "Air time must accumulate past threshold");

        // Teleport to ground to land
        airCar.GetChassisBody()->SetTransform(b2Vec2(0.0f, 0.70f), 0.0f);
        airCar.GetRearWheelBody()->SetTransform(b2Vec2(-1.15f, 0.40f), 0.0f);
        airCar.GetFrontWheelBody()->SetTransform(b2Vec2(1.15f, 0.40f), 0.0f);

        // Step to trigger landing transition
        world.Step(1.0f / 60.0f, 8, 3);
        airCar.Update(1.0f / 60.0f);
        TEST_CHECK(!airCar.IsInAir(), "Car must be on ground after landing");

        float airSec = 0.0f;
        int airBonus = airCar.PopAirTimeBonus(&airSec);
        printf("  Landed after %.2fs air time -> Air Time Bonus: +%d coins\n", airSec, airBonus);
        TEST_CHECK(airBonus >= 250, "Air time >= 1.0s must award at least 250 coins");
        TEST_CHECK(airCar.PopAirTimeBonus() == 0, "Subsequent PopAirTimeBonus() must return 0");

        // Part 2: Wheelie Bonus
        // Spawn car with rear wheel on ground, front wheel elevated, pitch angle ~0.35 rad
        PlayerCar wheelieCar(&world, b2Vec2(10.0f, 0.75f), 0, 0, 0, 0);
        wheelieCar.GetChassisBody()->SetTransform(b2Vec2(10.0f, 0.75f), 0.35f);
        wheelieCar.GetRearWheelBody()->SetTransform(b2Vec2(8.85f, 0.40f), 0.0f);
        wheelieCar.GetFrontWheelBody()->SetTransform(b2Vec2(11.15f, 1.10f), 0.0f);

        // Step 1 frame to establish contact
        world.Step(1.0f / 60.0f, 8, 3);
        wheelieCar.Update(1.0f / 60.0f);

        TEST_CHECK(wheelieCar.IsRearWheelOnGround(), "Rear wheel must be on ground for wheelie");
        TEST_CHECK(!wheelieCar.IsFrontWheelOnGround(), "Front wheel must be airborne for wheelie");

        // Hold wheelie for 100 frames (1.667s > 1.5s threshold)
        for (int i = 0; i < 100; ++i) {
            // Keep rear wheel grounded and front elevated
            wheelieCar.GetChassisBody()->SetTransform(b2Vec2(10.0f, 0.75f), 0.35f);
            wheelieCar.GetChassisBody()->SetLinearVelocity(b2Vec2(5.0f, 0.0f));
            wheelieCar.GetRearWheelBody()->SetTransform(b2Vec2(8.85f, 0.40f), 0.0f);
            wheelieCar.GetFrontWheelBody()->SetTransform(b2Vec2(11.15f, 1.10f), 0.0f);
            world.Step(1.0f / 60.0f, 8, 3);
            wheelieCar.Update(1.0f / 60.0f);
        }
        TEST_CHECK(wheelieCar.GetWheelieTime() >= 1.5f, "Wheelie time must exceed 1.5s threshold");

        // End wheelie by putting front wheel on ground
        wheelieCar.GetChassisBody()->SetTransform(b2Vec2(10.0f, 0.70f), 0.0f);
        wheelieCar.GetFrontWheelBody()->SetTransform(b2Vec2(11.15f, 0.40f), 0.0f);
        world.Step(1.0f / 60.0f, 8, 3);
        wheelieCar.Update(1.0f / 60.0f);

        float wheelieSec = 0.0f;
        int wheelieBonus = wheelieCar.PopWheelieBonus(&wheelieSec);
        printf("  Ended wheelie after %.2fs -> Wheelie Bonus: +%d coins\n", wheelieSec, wheelieBonus);
        TEST_CHECK(wheelieBonus >= 500, "Wheelie >= 1.5s must award at least 500 coins");
        TEST_CHECK(wheelieCar.PopWheelieBonus() == 0, "Subsequent PopWheelieBonus() must return 0");
        printf("  -> SUBTEST C PASSED\n\n");
    }

    printf("========================================================\n");
    printf("   ALL 3 SUB-TESTS PASSED (T-113 VERIFIED)\n");
    printf("========================================================\n");
    return 0;
}
