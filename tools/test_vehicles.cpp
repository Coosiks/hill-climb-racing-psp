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
    // Sub-test A: Config Parsing & 24 Vehicles Registry
    // ----------------------------------------------------
    printf("[Sub-test A] Checking 29 vehicles configuration and hashes...\n");
    bool loaded = Config::LoadVehicles("config/vehicles.cfg");
    if (!loaded) loaded = Config::LoadVehicles("data/vehicles.cfg");
    TEST_CHECK(loaded, "Failed to load vehicles.cfg");
    TEST_CHECK(Config::VehicleCount >= 29, "Config::VehicleCount must be at least 29");

    const char* expectedIds[29] = {
        "jeep", "motocross", "monster_truck", "race_car",
        "tractor", "hippievan", "quadbike", "touristbus",
        "onewheeler", "screwfinger", "policecar", "ambulance",
        "electric", "dieseltruck", "rallycar", "dragster",
        "firetruck", "tank",
        "dunebuggy", "superoffroad", "truck", "minibike",
        "bigfinger", "hotrod",
        "train",
        "snowmobile", "hovercraft", "moonlander", "sleigh"
    };
    int expectedCosts[29] = {
        0, 75000, 100000, 250000,
        100000, 120000, 175000, 200000,
        150000, 300000, 350000, 400000,
        450000, 500000, 750000, 800000,
        800000, 1000000,
        800000, 1000000, 1000000, 1000000,
        1200000, 1500000,
        1500000,
        1000000, 1000000, 1500000, 1000000
    };
    uint32_t hashes[29];

    for (int i = 0; i < 29; ++i) {
        const VehicleConfig& v = Config::Vehicles[i];
        TEST_CHECK(strcmp(v.id, expectedIds[i]) == 0, "Vehicle ID matches expected");
        TEST_CHECK(v.unlock_cost == expectedCosts[i], "Vehicle unlock cost matches expected");
        hashes[i] = PlayerInfo::HashId(v.id);
        printf("  Vehicle [%2d]: %-14s (hash 0x%08X, cost %7d)\n", i, v.id, hashes[i], v.unlock_cost);
    }

    // Verify all 29 hashes are strictly unique
    for (int i = 0; i < 29; ++i) {
        for (int j = i + 1; j < 29; ++j) {
            TEST_CHECK(hashes[i] != hashes[j], "Vehicle hashes must be unique");
        }
    }

    // Textures verification
    TEST_CHECK(strcmp(Config::Vehicles[0].body_texture, "assets/Car.png") == 0, "Jeep body texture");
    TEST_CHECK(strcmp(Config::Vehicles[1].body_texture, "assets/bike-motocross.png") == 0, "Motocross body texture");
    TEST_CHECK(strcmp(Config::Vehicles[2].body_texture, "assets/car-monster.png") == 0, "Monster truck body texture");
    TEST_CHECK(strcmp(Config::Vehicles[3].body_texture, "assets/car-racecar.png") == 0, "Race car body texture");
    TEST_CHECK(strcmp(Config::Vehicles[4].body_texture, "assets/car-tractor.png") == 0, "Tractor body texture");
    TEST_CHECK(strcmp(Config::Vehicles[4].wheel_texture, "assets/car-tractor-tire.png") == 0, "Tractor wheel texture");
    TEST_CHECK(strcmp(Config::Vehicles[5].body_texture, "assets/car-hippievan-chassis.png") == 0, "Hippievan body texture");
    TEST_CHECK(strcmp(Config::Vehicles[5].wheel_texture, "assets/car-hippievan-tire.png") == 0, "Hippievan wheel texture");
    TEST_CHECK(strcmp(Config::Vehicles[6].body_texture, "assets/car-quadbike.png") == 0, "Quadbike body texture");
    TEST_CHECK(strcmp(Config::Vehicles[6].wheel_texture, "assets/Tire.png") == 0, "Quadbike wheel texture");
    TEST_CHECK(strcmp(Config::Vehicles[7].body_texture, "assets/car-touristbus.png") == 0, "Touristbus body texture");
    TEST_CHECK(strcmp(Config::Vehicles[7].wheel_texture, "assets/Tire.png") == 0, "Touristbus wheel texture");
    printf("  [PASS] Sub-test A: %d vehicles parsed, unique hashes and textures verified!\n\n", Config::VehicleCount);

    // ----------------------------------------------------
    // Sub-test B: Box2D Physical Properties Per Car
    // ----------------------------------------------------
    printf("[Sub-test B] Verifying Box2D physical properties per vehicle...\n");
    b2Vec2 gravity(0.0f, -9.8f);
    b2World world(gravity);

    const VehicleConfig& jeep = Config::Vehicles[0];
    const VehicleConfig& motocross = Config::Vehicles[1];
    const VehicleConfig& monster = Config::Vehicles[2];
    const VehicleConfig& racecar = Config::Vehicles[3];
    const VehicleConfig& tractor = Config::Vehicles[4];
    const VehicleConfig& hippievan = Config::Vehicles[5];
    const VehicleConfig& quadbike = Config::Vehicles[6];
    const VehicleConfig& touristbus = Config::Vehicles[7];

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

    // 5. Tractor
    {
        PlayerCar car(&world, b2Vec2(0.0f, 2.0f), 0, 0, 0, 0, &tractor);
        b2CircleShape* rwShape = (b2CircleShape*)car.GetRearWheelBody()->GetFixtureList()->GetShape();
        b2CircleShape* fwShape = (b2CircleShape*)car.GetFrontWheelBody()->GetFixtureList()->GetShape();
        TEST_CHECK(feq(rwShape->m_radius, 0.60f), "Tractor rear wheel radius == 0.60 (oversized)");
        TEST_CHECK(feq(fwShape->m_radius, 0.35f), "Tractor front wheel radius == 0.35 (small front)");
        TEST_CHECK(tractor.fourth_upgrade_type == 4, "Tractor 4th upgrade type == 4 (STABILITY)");
        TEST_CHECK(tractor.drive_type == 0, "Tractor drive type == 0 (RWD)");
    }

    // 6. Hippie Van
    {
        PlayerCar car(&world, b2Vec2(0.0f, 2.0f), 0, 0, 0, 0, &hippievan);
        b2CircleShape* rwShape = (b2CircleShape*)car.GetRearWheelBody()->GetFixtureList()->GetShape();
        b2CircleShape* fwShape = (b2CircleShape*)car.GetFrontWheelBody()->GetFixtureList()->GetShape();
        TEST_CHECK(feq(rwShape->m_radius, 0.40f), "Hippie van wheel radius == 0.40");
        TEST_CHECK(feq(fwShape->m_radius, 0.40f), "Hippie van wheel radius == 0.40");
        TEST_CHECK(hippievan.fourth_upgrade_type == 5, "Hippie van 4th upgrade type == 5 (BOOST)");
        TEST_CHECK(hippievan.drive_type == 0, "Hippie van drive type == 0 (RWD)");
    }

    // 7. Quad Bike
    {
        PlayerCar car(&world, b2Vec2(0.0f, 2.0f), 0, 0, 0, 0, &quadbike);
        b2CircleShape* rwShape = (b2CircleShape*)car.GetRearWheelBody()->GetFixtureList()->GetShape();
        b2CircleShape* fwShape = (b2CircleShape*)car.GetFrontWheelBody()->GetFixtureList()->GetShape();
        TEST_CHECK(feq(rwShape->m_radius, 0.42f), "Quad bike wheel radius == 0.42");
        TEST_CHECK(feq(fwShape->m_radius, 0.42f), "Quad bike wheel radius == 0.42");
        TEST_CHECK(quadbike.fourth_upgrade_type == 0, "Quad bike 4th upgrade type == 0 (4WD)");
        TEST_CHECK(quadbike.drive_type == 2, "Quad bike drive type == 2 (4WD)");
    }

    // 8. Tourist Bus
    {
        PlayerCar car(&world, b2Vec2(0.0f, 2.0f), 0, 0, 0, 0, &touristbus);
        b2CircleShape* rwShape = (b2CircleShape*)car.GetRearWheelBody()->GetFixtureList()->GetShape();
        b2CircleShape* fwShape = (b2CircleShape*)car.GetFrontWheelBody()->GetFixtureList()->GetShape();
        TEST_CHECK(feq(rwShape->m_radius, 0.44f), "Tourist bus wheel radius == 0.44");
        TEST_CHECK(feq(fwShape->m_radius, 0.44f), "Tourist bus wheel radius == 0.44");
        TEST_CHECK(touristbus.fourth_upgrade_type == 0, "Tourist bus 4th upgrade type == 0 (4WD)");
        TEST_CHECK(touristbus.drive_type == 2, "Tourist bus drive type == 2 (4WD)");
        TEST_CHECK(car.GetChassisBody()->GetMass() > 4.0f, "Tourist bus chassis mass must be heavy (> 4.0 kg)");
    }

    // 9. Dragster (T-126: Asymmetrical wheels and high downforce)
    const VehicleConfig* dragster = Config::FindVehicleById("dragster");
    TEST_CHECK(dragster != NULL, "Dragster must exist");
    TEST_CHECK(dragster->rear_wheel_radius > dragster->front_wheel_radius * 1.4f, "Dragster wheel radius asymmetry (R_rear > 1.4 * R_front)");
    TEST_CHECK(dragster->fourth_upgrade_type == 3, "Dragster 4th slot == 3 (DOWNFORCE)");
    TEST_CHECK(dragster->downforce_coeff > 0.5f, "Dragster downforce coeff > 0.5");

    // 10. Tank (T-126: Heavy armored chassis)
    const VehicleConfig* tank = Config::FindVehicleById("tank");
    TEST_CHECK(tank != NULL, "Tank must exist");
    {
        PlayerCar carTank(&world, b2Vec2(0.0f, 2.0f), 0, 0, 0, 0, tank);
        TEST_CHECK(carTank.GetChassisBody()->GetMass() > 5.0f, "Tank chassis mass must be heavy (> 5.0 kg)");
        TEST_CHECK(tank->drive_type == 2, "Tank drive type == 2 (4WD)");
    }

    // 11. Train / Kiddie Express (T-131a: Heavy locomotive chassis)
    const VehicleConfig* train = Config::FindVehicleById("train");
    TEST_CHECK(train != NULL, "Train must exist");
    {
        PlayerCar carTrain(&world, b2Vec2(0.0f, 2.0f), 0, 0, 0, 0, train);
        float trainMass = carTrain.GetChassisBody()->GetMass();
        printf("  Train chassis mass: %.4f kg\n", trainMass);
        TEST_CHECK(trainMass > 3.4206f, "Train chassis mass must be strictly above median of 24 vehicles (> 3.4206 kg)");
        TEST_CHECK(trainMass >= 8.0f && trainMass <= 10.5f, "Train chassis mass in target range ~8-10 kg");
        float trainWheelbase = fabsf(train->front_wheel_offset[0] - train->rear_wheel_offset[0]);
        float touristbusWheelbase = fabsf(touristbus.front_wheel_offset[0] - touristbus.rear_wheel_offset[0]);
        TEST_CHECK(trainWheelbase > touristbusWheelbase, "Train wheelbase must be longer than touristbus");
        TEST_CHECK(train->fourth_upgrade_type == 5, "Train 4th slot == 5 (BOOST)");
        TEST_CHECK(train->drive_type == 2, "Train drive type == 2 (4WD)");
    }

    // 12. Snowmobile (T-131b: Mode 1 SKI_GLIDE, 4WD)
    const VehicleConfig* snowmobile = Config::FindVehicleById("snowmobile");
    TEST_CHECK(snowmobile != NULL, "Snowmobile must exist");
    TEST_CHECK(snowmobile->special_physics_mode == 1, "Snowmobile special_physics_mode == 1 (SKI_GLIDE)");
    TEST_CHECK(snowmobile->fourth_upgrade_type == 0, "Snowmobile 4th slot == 0 (4WD)");
    TEST_CHECK(snowmobile->drive_type == 2, "Snowmobile drive type == 2 (4WD)");
    TEST_CHECK(feq(snowmobile->ski_friction_mult, 0.35f), "Snowmobile ski friction mult == 0.35");
    TEST_CHECK(feq(snowmobile->ski_mud_mult, 0.25f), "Snowmobile ski mud mult == 0.25");

    // 13. Hovercraft (T-131b: Mode 2 HOVER_CUSHION, BOOST, hide_wheels)
    const VehicleConfig* hovercraft = Config::FindVehicleById("hovercraft");
    TEST_CHECK(hovercraft != NULL, "Hovercraft must exist");
    TEST_CHECK(hovercraft->special_physics_mode == 2, "Hovercraft special_physics_mode == 2 (HOVER_CUSHION)");
    TEST_CHECK(hovercraft->hide_wheels == 1, "Hovercraft hide_wheels == 1");
    TEST_CHECK(hovercraft->fourth_upgrade_type == 5, "Hovercraft 4th slot == 5 (BOOST)");
    TEST_CHECK(feq(hovercraft->hover_height, 1.20f), "Hovercraft hover height == 1.20m");

    // 14. Moonlander (T-131b: Mode 3 THRUSTER_JET, BOOST)
    const VehicleConfig* moonlander = Config::FindVehicleById("moonlander");
    TEST_CHECK(moonlander != NULL, "Moonlander must exist");
    TEST_CHECK(moonlander->special_physics_mode == 3, "Moonlander special_physics_mode == 3 (THRUSTER_JET)");
    TEST_CHECK(moonlander->fourth_upgrade_type == 5, "Moonlander 4th slot == 5 (BOOST)");
    TEST_CHECK(feq(moonlander->thruster_g_mult, 1.80f), "Moonlander thruster_g_mult == 1.80");

    // 15. Sleigh (T-131b: Mode 1 SKI_GLIDE, BOOST, hide_wheels)
    const VehicleConfig* sleigh = Config::FindVehicleById("sleigh");
    TEST_CHECK(sleigh != NULL, "Sleigh must exist");
    TEST_CHECK(sleigh->special_physics_mode == 1, "Sleigh special_physics_mode == 1 (SKI_GLIDE)");
    TEST_CHECK(sleigh->hide_wheels == 1, "Sleigh hide_wheels == 1");
    TEST_CHECK(sleigh->fourth_upgrade_type == 5, "Sleigh 4th slot == 5 (BOOST)");

    printf("\n  Authoritative Vehicle Physics Table (Box2D Simulation):\n");
    printf("  | Vehicle ID     | idHash     | R_front | R_rear | F-Wheel (kg) | R-Wheel (kg) | Chassis (kg) | Head (kg) | Total Mass (kg) | 4th Upgrade Type |\n");
    printf("  | :------------- | :--------- | :------ | :----- | :----------- | :----------- | :----------- | :-------- | :-------------- | :--------------- |\n");
    for (int i = 0; i < Config::VehicleCount; ++i) {
        VehicleConfig* vc = &Config::Vehicles[i];
        PlayerCar pc(&world, b2Vec2(0.0f, 2.0f), 0, 0, 0, 0, vc);
        float cm = pc.GetChassisBody()->GetMass();
        float fm = pc.GetFrontWheelBody()->GetMass();
        float rm = pc.GetRearWheelBody()->GetMass();
        float hm = pc.GetDriverHeadBody() ? pc.GetDriverHeadBody()->GetMass() : 0.0f;
        float total = cm + fm + rm + hm;
        uint32_t vh = PlayerInfo::HashId(vc->id);
        printf("  | %-14s | 0x%08X | %7.2f | %6.2f | %12.4f | %12.4f | %12.4f | %9.4f | %15.4f | %16d |\n",
               vc->id, vh, vc->front_wheel_radius, vc->rear_wheel_radius, fm, rm, cm, hm, total, vc->fourth_upgrade_type);
    }
    printf("\n");
    printf("  [PASS] Sub-test B: Box2D physical properties (radii, mass, drive type, downforce) verified!\n\n");

    // ----------------------------------------------------
    // Sub-test C: PlayerInfo Independent Upgrades
    // ----------------------------------------------------
    printf("[Sub-test C] Verifying independent upgrades per vehicle...\n");
    PlayerInfo::Reset();
    uint32_t h0 = hashes[0];
    uint32_t h1 = hashes[1];
    uint32_t h3 = hashes[3];
    uint32_t h4 = hashes[4];

    PlayerInfo::UnlockVehicle(h1); // Motocross

    PlayerInfo::SetVehicleUpgrade(h1, 0, 7); // Motocross Engine lvl 7
    PlayerInfo::SetVehicleUpgrade(h1, 3, 4); // Motocross Air Control lvl 4

    // Check that Jeep (h0) was not modified
    TEST_CHECK(PlayerInfo::GetVehicleUpgrade(h0, 0) == 0, "Jeep engine must remain at lvl 0");
    TEST_CHECK(PlayerInfo::GetVehicleUpgrade(h0, 3) == 0, "Jeep AWD must remain at lvl 0");

    // Check Motocross upgrades
    TEST_CHECK(PlayerInfo::GetVehicleUpgrade(h1, 0) == 7, "Motocross engine must be at lvl 7");
    TEST_CHECK(PlayerInfo::GetVehicleUpgrade(h1, 3) == 4, "Motocross Air Control must be at lvl 4");

    // Check Tractor independent upgrade
    PlayerInfo::UnlockVehicle(h4); // Tractor
    PlayerInfo::SetVehicleUpgrade(h4, 0, 10);
    TEST_CHECK(PlayerInfo::GetVehicleUpgrade(h4, 0) == 10, "Tractor engine must be at lvl 10");
    TEST_CHECK(PlayerInfo::GetVehicleUpgrade(h0, 0) == 0, "Jeep engine must remain at lvl 0");

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
