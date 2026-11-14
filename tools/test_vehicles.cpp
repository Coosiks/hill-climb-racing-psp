#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "core/Config.h"
#include "core/PlayerInfo.h"
#include "physics/PlayerCar.h"
#include "Box2D/Box2D.h"

#define TEST_CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
            return 1; \
        } \
    } while(0)

static bool feq(float a, float b, float eps = 0.001f) {
    return fabsf(a - b) <= eps;
}

int main() {
    printf("========================================================\n");
    printf("   HCR Regression Test: Multi-Vehicle Architecture (T-032)\n");
    printf("========================================================\n\n");

    // ----------------------------------------------------
    // Sub-test A: Config Parsing & 4 Vehicles Registry
    // ----------------------------------------------------
    printf("[Sub-test A] Checking 4 vehicles configuration and hashes...\n");
    bool loaded = Config::LoadVehicles("config/vehicles.cfg");
    if (!loaded) loaded = Config::LoadVehicles("data/vehicles.cfg");
    TEST_CHECK(loaded, "Failed to load vehicles.cfg");
    TEST_CHECK(Config::VehicleCount >= 4, "Config::VehicleCount must be at least 4");

    const VehicleConfig& jeep = Config::Vehicles[0];
    const VehicleConfig& motocross = Config::Vehicles[1];
    const VehicleConfig& monster = Config::Vehicles[2];
    const VehicleConfig& racecar = Config::Vehicles[3];

    TEST_CHECK(strcmp(jeep.id, "jeep") == 0, "Vehicle 0 ID must be jeep");
    TEST_CHECK(strcmp(motocross.id, "motocross") == 0, "Vehicle 1 ID must be motocross");
    TEST_CHECK(strcmp(monster.id, "monster_truck") == 0, "Vehicle 2 ID must be monster_truck");
    TEST_CHECK(strcmp(racecar.id, "race_car") == 0, "Vehicle 3 ID must be race_car");

    uint32_t h0 = PlayerInfo::HashId(jeep.id);
    uint32_t h1 = PlayerInfo::HashId(motocross.id);
    uint32_t h2 = PlayerInfo::HashId(monster.id);
    uint32_t h3 = PlayerInfo::HashId(racecar.id);

    printf("  Vehicles: [0] %s (hash 0x%08X), [1] %s (hash 0x%08X), [2] %s (hash 0x%08X), [3] %s (hash 0x%08X)\n",
           jeep.id, h0, motocross.id, h1, monster.id, h2, racecar.id, h3);

    // Verify unique hashes
    TEST_CHECK(h0 != h1 && h0 != h2 && h0 != h3, "Jeep hash must be unique");
    TEST_CHECK(h1 != h2 && h1 != h3, "Motocross hash must be unique");
    TEST_CHECK(h2 != h3, "Monster truck hash must be unique");

    // Unlock costs
    TEST_CHECK(jeep.unlock_cost == 0, "Jeep unlock cost == 0");
    TEST_CHECK(motocross.unlock_cost == 75000, "Motocross unlock cost == 75000");
    TEST_CHECK(monster.unlock_cost == 100000, "Monster truck unlock cost == 100000");
    TEST_CHECK(racecar.unlock_cost == 250000, "Race car unlock cost == 250000");

    // Textures
    TEST_CHECK(strcmp(jeep.body_texture, "assets/Car.png") == 0, "Jeep body texture");
    TEST_CHECK(strcmp(motocross.body_texture, "assets/bike-motocross.png") == 0, "Motocross body texture");
    TEST_CHECK(strcmp(monster.body_texture, "assets/car-monster.png") == 0, "Monster truck body texture");
    TEST_CHECK(strcmp(racecar.body_texture, "assets/car-racecar.png") == 0, "Race car body texture");
    printf("  [PASS] Sub-test A: 4 vehicles parsed, unique hashes and textures verified!\n\n");

    // ----------------------------------------------------
    // Sub-test B: Box2D Physical Properties Per Car
    // ----------------------------------------------------
    printf("[Sub-test B] Verifying Box2D physical properties per vehicle...\n");
    b2Vec2 gravity(0.0f, -9.8f);
    b2World world(gravity);

    // 1. Jeep
    {
        PlayerCar car(&world, b2Vec2(0.0f, 2.0f), 0, 0, 0, 0, &jeep);
        b2CircleShape* rwShape = (b2CircleShape*)car.GetRearWheelBody()->GetFixtureList()->GetShape();
        b2CircleShape* fwShape = (b2CircleShape*)car.GetFrontWheelBody()->GetFixtureList()->GetShape();
        TEST_CHECK(feq(rwShape->m_radius, 0.40f), "Jeep rear wheel radius == 0.40");
        TEST_CHECK(feq(fwShape->m_radius, 0.40f), "Jeep front wheel radius == 0.40");
        TEST_CHECK(feq(car.GetAirPitchTorque(), 10.0f), "Jeep air pitch torque == 10.0");
    }

    // 2. Motocross
    float motoMass = 0.0f;
    {
        PlayerCar car(&world, b2Vec2(0.0f, 2.0f), 0, 0, 0, 0, &motocross);
        b2CircleShape* rwShape = (b2CircleShape*)car.GetRearWheelBody()->GetFixtureList()->GetShape();
        b2CircleShape* fwShape = (b2CircleShape*)car.GetFrontWheelBody()->GetFixtureList()->GetShape();
        TEST_CHECK(feq(rwShape->m_radius, 0.45f), "Motocross rear wheel radius == 0.45");
        TEST_CHECK(feq(fwShape->m_radius, 0.45f), "Motocross front wheel radius == 0.45");
        TEST_CHECK(feq(car.GetAirPitchTorque(), 16.0f), "Motocross air pitch torque == 16.0");
        motoMass = car.GetChassisBody()->GetMass();

        // RWD drive type check
        car.Accelerate();
        car.Update(1.0f / 60.0f);
        // Joint motor checks
        b2JointEdge* je = car.GetRearWheelBody()->GetJointList();
        TEST_CHECK(je != NULL, "Rear wheel must have joint");
        b2WheelJoint* rJoint = (b2WheelJoint*)je->joint;
        TEST_CHECK(rJoint->IsMotorEnabled(), "Motocross rear motor must be enabled under throttle");

        b2JointEdge* fe = car.GetFrontWheelBody()->GetJointList();
        TEST_CHECK(fe != NULL, "Front wheel must have joint");
        b2WheelJoint* fJoint = (b2WheelJoint*)fe->joint;
        TEST_CHECK(!fJoint->IsMotorEnabled(), "Motocross front motor must NOT be enabled (RWD)");
    }

    // 3. Monster Truck
    float monsterMass = 0.0f;
    {
        PlayerCar car(&world, b2Vec2(0.0f, 2.0f), 0, 0, 0, 0, &monster);
        b2CircleShape* rwShape = (b2CircleShape*)car.GetRearWheelBody()->GetFixtureList()->GetShape();
        b2CircleShape* fwShape = (b2CircleShape*)car.GetFrontWheelBody()->GetFixtureList()->GetShape();
        TEST_CHECK(feq(rwShape->m_radius, 0.65f), "Monster truck rear wheel radius == 0.65");
        TEST_CHECK(feq(fwShape->m_radius, 0.65f), "Monster truck front wheel radius == 0.65");
        TEST_CHECK(feq(car.GetAirPitchTorque(), 8.0f), "Monster truck air pitch torque == 8.0");
        monsterMass = car.GetChassisBody()->GetMass();
        TEST_CHECK(monsterMass > motoMass, "Monster truck mass must be larger than motocross");
    }

    // 4. Race Car
    {
        PlayerCar car(&world, b2Vec2(0.0f, 2.0f), 0, 0, 0, 0, &racecar);
        b2CircleShape* rwShape = (b2CircleShape*)car.GetRearWheelBody()->GetFixtureList()->GetShape();
        b2CircleShape* fwShape = (b2CircleShape*)car.GetFrontWheelBody()->GetFixtureList()->GetShape();
        TEST_CHECK(feq(rwShape->m_radius, 0.42f), "Race car rear wheel radius == 0.42");
        TEST_CHECK(feq(fwShape->m_radius, 0.36f), "Race car front wheel radius == 0.36 (asymmetrical)");

        // Aerodynamic downforce tests
        TEST_CHECK(feq(car.GetDownforce(), 0.0f), "Downforce at rest == 0");

        car.GetChassisBody()->SetLinearVelocity(b2Vec2(20.0f, 0.0f));
        float df20 = car.GetDownforce();
        // coeff = 0.15, vx = 20, vx^2 = 400 => df = 60.0 N
        TEST_CHECK(feq(df20, 60.0f, 0.5f), "Downforce at 20 m/s must be ~60 N");

        car.GetChassisBody()->SetLinearVelocity(b2Vec2(50.0f, 0.0f));
        float df50 = car.GetDownforce();
        // coeff = 0.15 * 2500 = 375 N => clamped to 150.0 N
        TEST_CHECK(feq(df50, 150.0f, 0.1f), "Downforce must be clamped at downforce_max (150 N)");
    }
    printf("  [PASS] Sub-test B: Box2D physical properties (radii, mass, drive type, downforce) verified!\n\n");

    // ----------------------------------------------------
    // Sub-test C: PlayerInfo Independent Upgrades
    // ----------------------------------------------------
    printf("[Sub-test C] Verifying independent upgrades per vehicle...\n");
    PlayerInfo::Reset();
    PlayerInfo::UnlockVehicle(h1); // Motocross

    PlayerInfo::SetVehicleUpgrade(h1, 0, 7); // Motocross Engine lvl 7
    PlayerInfo::SetVehicleUpgrade(h1, 3, 4); // Motocross Air Control lvl 4

    // Check that Jeep (h0) was not modified
    TEST_CHECK(PlayerInfo::GetVehicleUpgrade(h0, 0) == 0, "Jeep engine must remain at lvl 0");
    TEST_CHECK(PlayerInfo::GetVehicleUpgrade(h0, 3) == 0, "Jeep AWD must remain at lvl 0");

    // Check Motocross upgrades
    TEST_CHECK(PlayerInfo::GetVehicleUpgrade(h1, 0) == 7, "Motocross engine must be at lvl 7");
    TEST_CHECK(PlayerInfo::GetVehicleUpgrade(h1, 3) == 4, "Motocross Air Control must be at lvl 4");

    // Check that PlayerCar applies scaled air control torque
    PlayerCar upgradedMoto(&world, b2Vec2(0.0f, 2.0f), 7, 0, 0, 4, &motocross);
    float baseAir = motocross.air_pitch_torque;
    float expectedAir = baseAir * (1.0f + 0.06f * 4.0f);
    TEST_CHECK(feq(upgradedMoto.GetAirPitchTorque(), expectedAir, 0.01f), "Air control torque must scale with slot 4 level");

    // Upgrade Race Car downforce slot
    PlayerInfo::UnlockVehicle(h3);
    PlayerInfo::SetVehicleUpgrade(h3, 3, 5); // Downforce lvl 5
    PlayerCar upgradedRaceCar(&world, b2Vec2(0.0f, 2.0f), 0, 0, 0, 5, &racecar);
    upgradedRaceCar.GetChassisBody()->SetLinearVelocity(b2Vec2(20.0f, 0.0f));
    // coeff = 0.15 * (1 + 0.10 * 5) = 0.15 * 1.50 = 0.225; df = 0.225 * 400 = 90.0 N
    float dfRaceLvl5 = upgradedRaceCar.GetDownforce();
    TEST_CHECK(feq(dfRaceLvl5, 90.0f, 0.5f), "Downforce must scale with slot 4 level");

    printf("  [PASS] Sub-test C: Independent upgrades and PlayerCar scaling verified!\n\n");

    printf("========================================================\n");
    printf("   ALL 3 SUB-TESTS PASSED (T-032 VERIFIED)\n");
    printf("========================================================\n");
    return 0;
}
