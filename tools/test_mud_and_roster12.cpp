#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <Box2D/Box2D.h>

#include "core/Config.h"
#include "core/GameConstants.h"
#include "core/PlayerInfo.h"
#include "core/Game.h"
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

static inline bool feq(float a, float b, float eps = 0.001f) {
    return fabsf(a - b) <= eps;
}

// 64-bit FNV-1a hash
static uint64_t FNV1a_64(const void* data, size_t len) {
    const uint8_t* ptr = (const uint8_t*)data;
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        hash ^= (uint64_t)ptr[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

int main() {
    printf("=================================================================\n");
    printf("   HCR Regression Test 18: Mud, Roster 12 & Race Summary (T-033) \n");
    printf("=================================================================\n\n");

    // Load configs
    bool loaded = Config::LoadAll("");
    if (!loaded) loaded = Config::LoadAll("config/");
    TEST_CHECK(loaded, "Failed to load config files");

    // =============================================================
    // SUB-TEST A: 12 Stages & Mud Pools Mechanics (T-123)
    // =============================================================
    printf("[Sub-test A] Verifying 12 stages, 1.30.0 provenance, height hashes, and mud damping...\n");
    TEST_CHECK(Config::StageCount == 12, "Must load exactly 12 stages from stages.cfg");

    const char* expectedStageIds[12] = {
        "countryside", "desert", "arctic", "moon",
        "highway", "cave", "mars", "forest",
        "mountain", "mudpool", "volcano", "beach"
    };
    int expectedStageCosts[12] = {
        0, 10000, 20000, 50000,
        125000, 150000, 250000, 850000,
        850000, 1000000, 1000000, 1000000
    };

    uint32_t stageHashes[12];
    for (int i = 0; i < 12; ++i) {
        const StageConfig& st = Config::Stages[i];
        TEST_CHECK(strcmp(st.id, expectedStageIds[i]) == 0, "Stage ID matches expected");
        TEST_CHECK(st.unlock_cost == expectedStageCosts[i], "Stage unlock cost matches 1.30.0 provenance");
        stageHashes[i] = PlayerInfo::HashId(st.id);
        printf("  Stage [%2d]: %-12s (hash 0x%08X, cost %7d, mud=%d, damp=%.1f)\n",
               i, st.id, stageHashes[i], st.unlock_cost, st.has_mud_pools, st.mud_damping);
    }

    // Verify all 12 stage hashes are strictly unique
    for (int i = 0; i < 12; ++i) {
        for (int j = i + 1; j < 12; ++j) {
            TEST_CHECK(stageHashes[i] != stageHashes[j], "Stage hashes must be unique");
        }
    }

    // Bit-exact frozen heightfield baseline hashes for stages 0..7 over 4001 points (0..2000m, step 0.5m)
    const uint64_t EXPECTED_BASELINE_HASHES[8] = {
        0x6e83945913e2d687ULL, // Countryside
        0x6dfb669a32592de7ULL, // Desert
        0xf7cc9dd1f3aa54b0ULL, // Arctic
        0x0083f35054ddbad2ULL, // Moon
        0x530e55920af948caULL, // Highway
        0x4b3f21147a6045caULL, // Cave
        0x307e7d5f71ea2410ULL, // Mars
        0xd1b5fb3e9da68f3dULL  // Forest
    };

    const int NUM_POINTS = 4001; // 0 to 2000m with step 0.5m
    float* heightBuffer = (float*)malloc(NUM_POINTS * sizeof(float));
    TEST_CHECK(heightBuffer != NULL, "Failed to allocate height buffer");

    b2World dummyWorld(b2Vec2(0.0f, -10.0f));
    uint64_t computedHeightHashes[12];

    for (int s = 0; s < 12; ++s) {
        Terrain terrain(&dummyWorld, (StageType)s);
        for (int i = 0; i < NUM_POINTS; ++i) {
            float x = (float)i * 0.5f;
            heightBuffer[i] = terrain.GetHeightAtX(x);
        }
        computedHeightHashes[s] = FNV1a_64(heightBuffer, NUM_POINTS * sizeof(float));

        if (s < 8) {
            TEST_CHECK(computedHeightHashes[s] == EXPECTED_BASELINE_HASHES[s],
                       "Stages 0..7 baseline height hash mismatch! Non-regression violated!");
            printf("  Stage %-11s [0..7]: hash 0x%016llx -> 100%% BIT-EXACT MATCH\n",
                   Config::Stages[s].id, (unsigned long long)computedHeightHashes[s]);
        } else {
            printf("  Stage %-11s [8..11]: hash 0x%016llx -> DETERMINISTIC 2000M\n",
                   Config::Stages[s].id, (unsigned long long)computedHeightHashes[s]);
        }
    }
    free(heightBuffer);

    // Mud Zone Flag & Damping Verification
    TEST_CHECK(Config::Stages[STAGE_MUDPOOL].has_mud_pools == 1, "Mudpool must have has_mud_pools == 1");
    TEST_CHECK(Config::Stages[STAGE_MUDPOOL].mud_damping >= 2.0f, "Mudpool damping must be >= 2.0");
    TEST_CHECK(Config::Stages[STAGE_VOLCANO].has_mud_pools == 1, "Volcano must have has_mud_pools == 1");
    TEST_CHECK(Config::Stages[STAGE_VOLCANO].mud_damping >= 1.5f, "Volcano damping must be >= 1.5");
    TEST_CHECK(Config::Stages[STAGE_MOUNTAIN].has_mud_pools == 0, "Mountain must have has_mud_pools == 0");
    TEST_CHECK(Config::Stages[STAGE_BEACH].has_mud_pools == 0, "Beach must have has_mud_pools == 0");

    // Deterministic Mud Zone spatial checks
    {
        Terrain terrainMud(&dummyWorld, STAGE_MUDPOOL);
        TEST_CHECK(!terrainMud.IsInMudZone(5.0f), "x < 15m must not be mud");
        TEST_CHECK(!terrainMud.IsInMudZone(15.0f), "x = 15m must not be mud pool");
        TEST_CHECK(terrainMud.IsInMudZone(25.0f), "x = 25m must be in mud pool [18..32]");
        TEST_CHECK(!terrainMud.IsInMudZone(45.0f), "x = 45m must be outside mud pool");
        TEST_CHECK(terrainMud.IsInMudZone(85.0f), "x = 85m (mod 60 = 25m) must be periodic mud pool");
    }

    // Physical Mud Damping Simulation Test
    // Compare car driving over mud vs dry terrain under full throttle
    {
        b2Vec2 grav(0.0f, -9.8f);
        b2World worldDry(grav);
        b2World worldMud(grav);

        Terrain terrDry(&worldDry, STAGE_COUNTRYSIDE);
        Terrain terrMud(&worldMud, STAGE_MUDPOOL);

        const VehicleConfig* vCfg = &Config::Vehicles[0]; // Jeep
        // Position car at x = 16.0m right in front of mud pool
        PlayerCar carDry(&worldDry, b2Vec2(16.0f, 3.0f), 0, 0, 0, 0, vCfg, &terrDry);
        PlayerCar carMud(&worldMud, b2Vec2(16.0f, 3.0f), 0, 0, 0, 0, vCfg, &terrMud);

        // Warm up contacts for 30 steps
        for (int i = 0; i < 30; ++i) {
            worldDry.Step(1.0f / 60.0f, 8, 3);
            carDry.Update(1.0f / 60.0f);
            terrDry.Update(carDry.GetPosition().x);

            worldMud.Step(1.0f / 60.0f, 8, 3);
            carMud.Update(1.0f / 60.0f);
            terrMud.Update(carMud.GetPosition().x);
        }

        // Apply full throttle for 90 steps into the zone [18..32]
        carDry.Accelerate();
        carMud.Accelerate();
        for (int i = 0; i < 90; ++i) {
            worldDry.Step(1.0f / 60.0f, 8, 3);
            carDry.Update(1.0f / 60.0f);
            terrDry.Update(carDry.GetPosition().x);

            worldMud.Step(1.0f / 60.0f, 8, 3);
            carMud.Update(1.0f / 60.0f);
            terrMud.Update(carMud.GetPosition().x);
        }

        float drySpeed = carDry.GetChassisBody()->GetLinearVelocity().x;
        float mudSpeed = carMud.GetChassisBody()->GetLinearVelocity().x;
        float dryX = carDry.GetPosition().x;
        float mudX = carMud.GetPosition().x;

        printf("  Mud physics simulation (90 steps): Dry speed=%.2f m/s (dist=%.2f m) vs Mud speed=%.2f m/s (dist=%.2f m)\n",
               drySpeed, dryX, mudSpeed, mudX);
        TEST_CHECK(mudSpeed < drySpeed, "Mudpool damping must significantly reduce speed compared to dry terrain");
        TEST_CHECK(mudX < dryX, "Car in mud must travel less distance than on dry ground");
    }
    printf("  [PASS] Sub-test A: 12 stages, bit-exact height baseline, and mud damping verified!\n\n");

    // =============================================================
    // SUB-TEST B: 12 Vehicles & STABILITY / BOOST Slots (T-124)
    // =============================================================
    printf("[Sub-test B] Verifying 12 vehicles, Box2D masses, STABILITY (4) and BOOST (5)...\n");
    TEST_CHECK(Config::VehicleCount >= 12, "Must load at least 12 vehicles from vehicles.cfg");

    const char* expectedVehIds[12] = {
        "jeep", "motocross", "monster_truck", "race_car",
        "tractor", "hippievan", "quadbike", "touristbus",
        "onewheeler", "fingerscrew", "policecar", "ambulance"
    };
    int expectedVehCosts[12] = {
        0, 75000, 100000, 250000,
        100000, 120000, 175000, 200000,
        150000, 300000, 350000, 400000
    };
    int expectedFourthTypes[12] = {
        0, 2, 0, 3, // jeep (4WD), motocross (AIR), monster (4WD), race_car (DOWNFORCE)
        4, 5, 0, 0, // tractor (STABILITY), hippievan (BOOST), quadbike (4WD), touristbus (4WD)
        4, 0, 5, 4  // onewheeler (STABILITY), fingerscrew (4WD), policecar (BOOST), ambulance (STABILITY)
    };

    uint32_t vehHashes[12];
    for (int i = 0; i < 12; ++i) {
        const VehicleConfig& vc = Config::Vehicles[i];
        TEST_CHECK(strcmp(vc.id, expectedVehIds[i]) == 0, "Vehicle ID matches expected");
        TEST_CHECK(vc.unlock_cost == expectedVehCosts[i], "Vehicle unlock cost matches 1.30.0 provenance");
        TEST_CHECK(vc.fourth_upgrade_type == expectedFourthTypes[i], "Vehicle 4th upgrade type matches expected");
        vehHashes[i] = PlayerInfo::HashId(vc.id);
    }

    // Verify all 12 vehicle hashes are strictly unique
    for (int i = 0; i < 12; ++i) {
        for (int j = i + 1; j < 12; ++j) {
            TEST_CHECK(vehHashes[i] != vehHashes[j], "Vehicle hashes must be unique");
        }
    }

    // Authoritative Box2D Mass Table for all 12 vehicles
    printf("\n  Authoritative 12-Vehicle Box2D Physical Properties:\n");
    printf("  | #  | Vehicle ID     | idHash     | Chassis (kg) | Wheels (kg) | Head (kg) | Total (kg) | 4th Slot Name  |\n");
    printf("  | :- | :------------- | :--------- | :----------- | :---------- | :-------- | :--------- | :------------- |\n");
    for (int i = 0; i < 12; ++i) {
        const VehicleConfig& vc = Config::Vehicles[i];
        PlayerCar pc(&dummyWorld, b2Vec2(0.0f, 2.0f), 0, 0, 0, 0, &vc);
        float cm = pc.GetChassisBody()->GetMass();
        float wm = pc.GetFrontWheelBody()->GetMass() + pc.GetRearWheelBody()->GetMass();
        float hm = pc.GetDriverHeadBody() ? pc.GetDriverHeadBody()->GetMass() : 0.0f;
        float tm = cm + wm + hm;
        const char* slotName = "UNKNOWN";
        switch (vc.fourth_upgrade_type) {
            case 0: slotName = "4WD"; break;
            case 1: slotName = "FUEL"; break;
            case 2: slotName = "AIR CONTROL"; break;
            case 3: slotName = "DOWNFORCE"; break;
            case 4: slotName = "STABILITY"; break;
            case 5: slotName = "BOOST"; break;
        }
        printf("  | %2d | %-14s | 0x%08X | %12.4f | %11.4f | %9.4f | %10.4f | %-14s |\n",
               i, vc.id, vehHashes[i], cm, wm, hm, tm, slotName);
    }
    printf("\n");

    // Verification of STABILITY (4th upgrade slot 4) on Tractor or Ambulance
    {
        b2Vec2 grav(0.0f, -9.8f);
        b2World worldStab0(grav);
        b2World worldStab10(grav);

        const VehicleConfig* ambCfg = Config::GetVehicle(11); // Ambulance (STABILITY)
        TEST_CHECK(ambCfg && ambCfg->fourth_upgrade_type == 4, "Ambulance must have STABILITY");

        PlayerCar carStock(&worldStab0, b2Vec2(0.0f, 2.0f), 0, 0, 0, 0, ambCfg);
        PlayerCar carUpgraded(&worldStab10, b2Vec2(0.0f, 2.0f), 0, 0, 0, 10, ambCfg);

        // Verify base angular damping difference in Box2D body
        TEST_CHECK(carUpgraded.GetChassisBody()->GetAngularDamping() > carStock.GetChassisBody()->GetAngularDamping(),
                   "STABILITY upgraded chassis must have strictly higher angular damping");

        // Apply identical high pitch rotational impulse
        carStock.GetChassisBody()->ApplyAngularImpulse(25.0f, true);
        carUpgraded.GetChassisBody()->ApplyAngularImpulse(25.0f, true);

        // Step for 20 frames
        for (int i = 0; i < 20; ++i) {
            worldStab0.Step(1.0f / 60.0f, 8, 3);
            carStock.Update(1.0f / 60.0f);

            worldStab10.Step(1.0f / 60.0f, 8, 3);
            carUpgraded.Update(1.0f / 60.0f);
        }

        float wStock = fabsf(carStock.GetChassisBody()->GetAngularVelocity());
        float wUpgraded = fabsf(carUpgraded.GetChassisBody()->GetAngularVelocity());

        printf("  STABILITY test: Stock angVel=%.3f rad/s vs Upgraded (lvl 10) angVel=%.3f rad/s\n",
               wStock, wUpgraded);
        TEST_CHECK(wUpgraded < wStock, "STABILITY must significantly suppress pitch rotational velocity");
    }

    // Verification of BOOST (4th upgrade slot 5) on Police Car or Hippie Van
    {
        b2Vec2 grav(0.0f, -9.8f);
        b2World worldBoost0(grav);
        b2World worldBoost10(grav);

        // Flat ground bodies for traction
        b2BodyDef bd;
        b2EdgeShape shape;
        shape.Set(b2Vec2(-50.0f, 0.0f), b2Vec2(500.0f, 0.0f));
        b2FixtureDef fd;
        fd.shape = &shape;
        fd.friction = 0.8f;
        fd.filter.categoryBits = CAT_TERRAIN;
        fd.filter.maskBits = CAT_CHASSIS | CAT_WHEEL | CAT_DRIVER;

        b2Body* g0 = worldBoost0.CreateBody(&bd);
        g0->CreateFixture(&fd);
        b2Body* g10 = worldBoost10.CreateBody(&bd);
        g10->CreateFixture(&fd);

        const VehicleConfig* policeCfg = Config::GetVehicle(10); // Police Car (BOOST)
        TEST_CHECK(policeCfg && policeCfg->fourth_upgrade_type == 5, "Police car must have BOOST");

        PlayerCar carStock(&worldBoost0, b2Vec2(0.0f, 1.0f), 5, 0, 0, 0, policeCfg);
        PlayerCar carBoost(&worldBoost10, b2Vec2(0.0f, 1.0f), 5, 0, 0, 10, policeCfg);

        // Settle onto ground for 15 steps
        for (int i = 0; i < 15; ++i) {
            worldBoost0.Step(1.0f / 60.0f, 8, 3);
            carStock.Update(1.0f / 60.0f);
            worldBoost10.Step(1.0f / 60.0f, 8, 3);
            carBoost.Update(1.0f / 60.0f);
        }

        carStock.Accelerate();
        carBoost.Accelerate();

        for (int i = 0; i < 120; ++i) {
            worldBoost0.Step(1.0f / 60.0f, 8, 3);
            carStock.Update(1.0f / 60.0f);

            worldBoost10.Step(1.0f / 60.0f, 8, 3);
            carBoost.Update(1.0f / 60.0f);
        }

        float vStock = carStock.GetChassisBody()->GetLinearVelocity().x;
        float vBoost = carBoost.GetChassisBody()->GetLinearVelocity().x;
        float xStock = carStock.GetPosition().x;
        float xBoost = carBoost.GetPosition().x;

        printf("  BOOST test (120 steps): Stock speed=%.2f m/s (dist=%.2f m) vs Boost speed=%.2f m/s (dist=%.2f m)\n",
               vStock, xStock, vBoost, xBoost);
        TEST_CHECK(vBoost > vStock, "BOOST slot must increase forward velocity under full throttle");
        TEST_CHECK(xBoost > xStock, "BOOST slot must achieve strictly greater distance over 120 steps");
    }
    printf("  [PASS] Sub-test B: 12 vehicles, Box2D masses, STABILITY and BOOST verified!\n\n");

    // =============================================================
    // SUB-TEST C: Detailed Race Summary & New Record Logic (T-033)
    // =============================================================
    printf("[Sub-test C] Verifying Race Summary breakdown and New Record tracking...\n");
    {
        PlayerInfo::Reset();
        Game game;
        game.SetCurrentStage(0); // Countryside
        game.SetCurrentVehicle(0); // Jeep
        game.StartRace();

        float mult = game.GetTerrain() ? game.GetTerrain()->GetCoinMultiplier() : 1.0f;
        int c1 = (int)(50 * mult);
        int c2 = (int)(100 * mult);
        int expectedRace1 = c1 + c2;

        // 1. Initial State Checks
        TEST_CHECK(game.GetRaceCollectedCoins() == 0, "Initial race collected coins == 0");
        TEST_CHECK(game.GetRaceTrickBonusCoins() == 0, "Initial trick coins == 0");
        TEST_CHECK(game.GetSessionCoins() == 0, "Initial session coins == 0");
        TEST_CHECK(!game.IsNewRecord(), "Initial record flag == false");

        // 2. Track coin accumulation breakdown
        game.OnCoinCollected(50);
        game.OnCoinCollected(100);
        TEST_CHECK(game.GetRaceCollectedCoins() == expectedRace1, "Collected coins must match earned coins");
        TEST_CHECK(game.GetSessionCoins() == expectedRace1, "Session coins must reflect collected coins");

        // 3. New Record detection and commitment
        uint32_t stageHash = PlayerInfo::HashId(Config::Stages[0].id);
        PlayerInfo::SetStageRecord(stageHash, 200); // Prior record: 200m

        // Race 1: Dist 350m (exceeds 200m -> NEW RECORD!)
        game.SetMaxDistance(350.0f);
        game.CommitRaceResults();

        TEST_CHECK(game.IsNewRecord() == true, "Must flag mIsNewRecord when distance exceeds previous best");
        TEST_CHECK(PlayerInfo::GetStageRecord(stageHash) == 350, "Record must be updated to 350m");
        TEST_CHECK(PlayerInfo::GetCoins() == expectedRace1, "PlayerInfo wallet must be credited with race 1 coins");

        // Race 2: Dist 280m (does not exceed 350m -> NO new record)
        game.StartRace();
        TEST_CHECK(game.GetRaceCollectedCoins() == 0, "Race stats must reset on StartRace()");
        TEST_CHECK(game.GetSessionCoins() == 0, "Session coins must reset on StartRace()");
        TEST_CHECK(!game.IsNewRecord(), "mIsNewRecord must reset on StartRace()");

        int c3 = (int)(500 * mult);
        game.OnCoinCollected(500);
        TEST_CHECK(game.GetRaceCollectedCoins() == c3, "Race 2 collected coins must match");
        game.SetMaxDistance(280.0f);
        game.CommitRaceResults();

        TEST_CHECK(game.IsNewRecord() == false, "mIsNewRecord must be false when distance is less than record");
        TEST_CHECK(PlayerInfo::GetStageRecord(stageHash) == 350, "Record must remain at 350m");
        int64_t expectedTotal = (int64_t)expectedRace1 + c3;
        TEST_CHECK(PlayerInfo::GetCoins() == expectedTotal, "PlayerInfo wallet must match sum of both races");

        printf("  Race 1: Distance=350m (previous 200m) -> NEW RECORD = YES (Record set to %dm)\n",
               PlayerInfo::GetStageRecord(stageHash));
        printf("  Race 2: Distance=280m (previous 350m) -> NEW RECORD = NO (Record kept at %dm)\n",
               PlayerInfo::GetStageRecord(stageHash));
        printf("  Wallet balance verified: %lld coins accumulated.\n", (long long)PlayerInfo::GetCoins());
    }
    printf("  [PASS] Sub-test C: Race Summary and New Record logic verified!\n\n");

    printf("=================================================================\n");
    printf("   ALL 3 SUB-TESTS PASSED (TEST 18 / T-033 FULLY VERIFIED)       \n");
    printf("=================================================================\n");
    return 0;
}
