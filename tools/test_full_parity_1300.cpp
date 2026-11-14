#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "core/Config.h"
#include "core/PlayerInfo.h"
#include "core/GameConstants.h"
#include "physics/PhysicsWorld.h"
#include "physics/PlayerCar.h"
#include "physics/Terrain.h"
#include "graphics/Renderer2D.h"
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

static uint64_t FNV1a_64(const void* data, size_t size) {
    const uint8_t* ptr = (const uint8_t*)data;
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < size; ++i) {
        hash ^= (uint64_t)ptr[i];
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

static void CleanupTestFiles(const char* base) {
    char path[256];
    snprintf(path, sizeof(path), "%s.bin", base); remove(path);
    snprintf(path, sizeof(path), "%s.bak", base); remove(path);
    snprintf(path, sizeof(path), "%s.tmp", base); remove(path);
    snprintf(path, sizeof(path), "%s.dat", base); remove(path);
}

int main(int argc, char** argv) {
    bool dumpHashesOnly = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--hashes") == 0) {
            dumpHashesOnly = true;
        }
    }

    if (!dumpHashesOnly) {
        printf("=================================================================\n");
        printf("   HCR Regression Test 22: Full 1.30.0 Parity (28 Stages, 29 Veh) \n");
        printf("=================================================================\n\n");
    }

    // -------------------------------------------------------------
    // Sub-test A: Complete Content & Price Parity (28 Stages, 29 Vehicles)
    // -------------------------------------------------------------
    if (!dumpHashesOnly) {
        printf("[Sub-test A] Verifying 28 stages & 29 vehicles against 1.30.0 inventory...\n");
    }

    // 1. Stages verification
    bool loadedStages = Config::LoadStages("config/stages.cfg");
    if (!loadedStages) loadedStages = Config::LoadStages("data/stages.cfg");
    TEST_CHECK(loadedStages, "Failed to load stages.cfg");
    TEST_CHECK(Config::StageCount == 28, "Config::StageCount must be strictly 28");

    const char* expectedStageIds[28] = {
        "countryside", "desert", "arctic", "moon",
        "highway", "cave", "mars", "forest",
        "mountain", "mudpool", "volcano", "beach",
        "rollercoaster", "night", "rooftops", "junkyard",
        "construction", "rainbow",
        "seasons", "bootcamp", "xmas", "alienplanet",
        "arcticcave", "ragnarok",
        "factory", "haunted", "northpole", "nuclear"
    };

    int expectedStageCosts[28] = {
        0, 35000, 75000, 175000,
        125000, 150000, 250000, 850000,
        850000, 1000000, 1000000, 1000000,
        1200000, 1200000, 1200000, 1400000,
        1400000, 1500000,
        50000, 200000, 300000, 750000,
        800000, 1000000,
        1000000, 1500000, 1500000, 1500000
    };

    const uint32_t expectedStageHashes[28] = {
        0x62B2D6DAu, 0x2CB41EBCu, 0xA09F16CDu, 0xF184AE40u,
        0x93FF431Cu, 0x9731FE58u, 0xDF7B0A68u, 0x4F292A60u,
        0x49CBE0A2u, 0x69D96D3Fu, 0x9848FDBDu, 0xF4B2C848u,
        0x4E64F17Au, 0xE6DE9F61u, 0xC0280847u, 0xDE180AF3u,
        0xCF30AB8Cu, 0x2B6CE2A9u,
        0xEA1DEEB1u, 0x400079C4u, 0xB41ECCFAu, 0xCF9483AEu,
        0x1E081680u, 0x6B8A1324u,
        0x959CE80Du, 0xAF6E424Eu, 0xBB6CA3EEu, 0x722E8B21u
    };

    uint32_t stageHashes[28];
    for (int i = 0; i < 28; ++i) {
        const StageConfig& st = Config::Stages[i];
        TEST_CHECK(strcmp(st.id, expectedStageIds[i]) == 0, "Stage ID matches expected");
        TEST_CHECK(st.unlock_cost == expectedStageCosts[i], "Stage unlock cost matches 1.30.0 provenance");
        stageHashes[i] = PlayerInfo::HashId(st.id);
        TEST_CHECK(stageHashes[i] == expectedStageHashes[i], "Stage hash matches reference FNV-1a 32-bit");
        if (!dumpHashesOnly) {
            printf("  Stage [%2d]: %-14s (hash 0x%08X, cost %7d)\n", i, st.id, stageHashes[i], st.unlock_cost);
        }
    }

    for (int i = 0; i < 28; ++i) {
        for (int j = i + 1; j < 28; ++j) {
            TEST_CHECK(stageHashes[i] != stageHashes[j], "Stage hashes must be strictly unique");
        }
    }

    // 28 heightfield hashes verification
    const uint64_t CANONICAL_STAGE_HASHES[28] = {
        0x6e83945913e2d687ULL, // 0: Countryside
        0x6dfb669a32592de7ULL, // 1: Desert
        0xf7cc9dd1f3aa54b0ULL, // 2: Arctic
        0x0083f35054ddbad2ULL, // 3: Moon
        0x79cb14fc941aed46ULL, // 4: Highway
        0x4b3f21147a6045caULL, // 5: Cave
        0x307e7d5f71ea2410ULL, // 6: Mars
        0xabd7ad1150f9fbf7ULL, // 7: Forest
        0x3f496da68e62c7ecULL, // 8: Mountain
        0x12fd17d4ac8a936dULL, // 9: Mudpool
        0x50aa43daaf13ada2ULL, // 10: Volcano
        0xd9e985e660616346ULL, // 11: Beach
        0xa8fd4bb25c7067d8ULL, // 12: Rollercoaster
        0x8177c160f2e579b9ULL, // 13: Night
        0x721cab00477aa490ULL, // 14: Rooftops
        0x168b4778e4ccfc72ULL, // 15: Junkyard
        0x53ed7a75b127d093ULL, // 16: Construction
        0xcd88490d45999148ULL, // 17: Rainbow
        0x11b9b29b0a89ed3dULL, // 18: Seasons
        0x7b7829e20dedb615ULL, // 19: Bootcamp
        0x4dee6c89fff49f7cULL, // 20: Xmas
        0xb6d53b9c59560286ULL, // 21: Alienplanet
        0xbf1efe0cf6e33ddeULL, // 22: Arcticcave
        0x7701828bb20753abULL, // 23: Ragnarok
        0x5f7b940443dc8475ULL, // 24: Factory
        0x34068b9f4f6d3417ULL, // 25: Haunted
        0x56a8242ec2790a3eULL, // 26: Northpole
        0x733d9464a31061d6ULL  // 27: Nuclear
    };

    const int NUM_POINTS = 4001;
    float* heightBuffer = (float*)malloc(NUM_POINTS * sizeof(float));
    TEST_CHECK(heightBuffer != NULL, "Failed to allocate height buffer");
    b2World dummyWorld(b2Vec2(0.0f, -10.0f));
    for (int s = 0; s < 28; ++s) {
        Terrain terrain(&dummyWorld, (StageType)s);
        for (int i = 0; i < NUM_POINTS; ++i) {
            float x = (float)i * 0.5f;
            heightBuffer[i] = terrain.GetHeightAtX(x);
        }
        uint64_t h = FNV1a_64(heightBuffer, NUM_POINTS * sizeof(float));
        if (dumpHashesOnly) {
            printf("[HASH] Stage %-14s (4001 pts) -> FNV-1a: 0x%016llx\n",
                   Config::Stages[s].id, (unsigned long long)h);
        } else {
            TEST_CHECK(h == CANONICAL_STAGE_HASHES[s], "Stage height hash mismatch! Determinism violated!");
        }
    }
    free(heightBuffer);

    if (dumpHashesOnly) {
        return 0;
    }

    // 2. Vehicles verification (29 vehicles)
    bool loadedVehicles = Config::LoadVehicles("config/vehicles.cfg");
    if (!loadedVehicles) loadedVehicles = Config::LoadVehicles("data/vehicles.cfg");
    TEST_CHECK(loadedVehicles, "Failed to load vehicles.cfg");
    TEST_CHECK(Config::VehicleCount == 29, "Config::VehicleCount must be strictly 29");

    const char* expectedVehIds[29] = {
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

    int expectedVehCosts[29] = {
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

    const uint32_t expectedVehHashes[29] = {
        0x64809865u, 0xC7D0636Cu, 0x73E941F5u, 0x135E4961u,
        0x592F7EF8u, 0x62C00991u, 0xFDEB8743u, 0x0A87F33Du,
        0xA0F5B831u, 0xB61B2D9Eu, 0x15BBA1FBu, 0x8FF9071Du,
        0xDDC6B3BEu, 0x90B2B8BAu, 0xB16FA6CFu, 0x1B89769Bu,
        0x01CC7C24u, 0x0C285A89u,
        0x3386A573u, 0xAB2471FFu, 0xF542BDC8u, 0x292EB177u,
        0x9E846550u, 0x827BB5D9u,
        0x78556C69u,
        0x58FE2244u, 0x628822B9u, 0xADF177B6u, 0xCA205AABu
    };

    int expectedFourthSlots[29] = {
        0, 2, 0, 3, // jeep (4WD), motocross (AIR), monster (4WD), race_car (DOWNFORCE)
        4, 5, 0, 0, // tractor (STABILITY), hippievan (BOOST), quadbike (4WD), touristbus (4WD)
        4, 0, 5, 4, // onewheeler (STABILITY), screwfinger (4WD), policecar (BOOST), ambulance (STABILITY)
        1, 0, 3, 3, // electric (FUEL), dieseltruck (4WD), rallycar (DOWNFORCE), dragster (DOWNFORCE)
        0, 0,       // firetruck (4WD), tank (4WD)
        0, 3, 0, 2, // dunebuggy (4WD), superoffroad (DOWNFORCE), truck (4WD), minibike (AIR_CONTROL)
        0, 5,       // bigfinger (4WD), hotrod (BOOST)
        5,          // train (BOOST mapping from TORQUE)
        0, 5, 5, 5  // snowmobile (4WD), hovercraft (BOOST), moonlander (BOOST), sleigh (BOOST)
    };

    uint32_t vehHashes[29];
    b2World massWorld(b2Vec2(0.0f, -9.8f));

    for (int i = 0; i < 29; ++i) {
        const VehicleConfig& vc = Config::Vehicles[i];
        TEST_CHECK(strcmp(vc.id, expectedVehIds[i]) == 0, "Vehicle ID matches expected");
        TEST_CHECK(vc.unlock_cost == expectedVehCosts[i], "Vehicle unlock cost matches 1.30.0 provenance");
        TEST_CHECK(vc.fourth_upgrade_type == expectedFourthSlots[i], "Vehicle 4th slot matches expected");
        vehHashes[i] = PlayerInfo::HashId(vc.id);
        TEST_CHECK(vehHashes[i] == expectedVehHashes[i], "Vehicle hash matches authoritative FNV-1a 32-bit reference");

        PlayerCar pc(&massWorld, b2Vec2(0.0f, 2.0f), 0, 0, 0, 0, &vc);
        float totalMass = pc.GetChassisBody()->GetMass() +
                          pc.GetFrontWheelBody()->GetMass() +
                          pc.GetRearWheelBody()->GetMass() +
                          (pc.GetDriverHeadBody() ? pc.GetDriverHeadBody()->GetMass() : 0.0f);
        TEST_CHECK(totalMass > 0.5f && totalMass < 15.0f, "Total mass must be within physically sane bounds");

        if (!dumpHashesOnly) {
            printf("  Vehicle [%2d]: %-14s (hash 0x%08X, cost %7d, 4th slot=%d, mass=%6.2f kg)\n",
                   i, vc.id, vehHashes[i], vc.unlock_cost, vc.fourth_upgrade_type, totalMass);
        }
    }

    for (int i = 0; i < 29; ++i) {
        for (int j = i + 1; j < 29; ++j) {
            TEST_CHECK(vehHashes[i] != vehHashes[j], "Vehicle hashes must be strictly unique");
        }
    }

    printf("  [PASS] Sub-test A: 28 stages, 29 vehicles, 100%% price parity & heightfields bit-exact!\n\n");

    // -------------------------------------------------------------
    // Sub-test B: Special Physics Modes Simulation (Modes 0, 1, 2, 3)
    // -------------------------------------------------------------
    printf("[Sub-test B] Verifying special physics modes (Modes 0, 1, 2, 3)...\n");

    const VehicleConfig* vJeep       = Config::FindVehicleById("jeep");
    const VehicleConfig* vSnowmobile = Config::FindVehicleById("snowmobile");
    const VehicleConfig* vHovercraft = Config::FindVehicleById("hovercraft");
    const VehicleConfig* vMoonlander = Config::FindVehicleById("moonlander");
    const VehicleConfig* vSleigh      = Config::FindVehicleById("sleigh");

    TEST_CHECK(vJeep != NULL, "Jeep must exist");
    TEST_CHECK(vSnowmobile != NULL, "Snowmobile must exist");
    TEST_CHECK(vHovercraft != NULL, "Hovercraft must exist");
    TEST_CHECK(vMoonlander != NULL, "Moonlander must exist");
    TEST_CHECK(vSleigh != NULL, "Sleigh must exist");

    // 1. Mode 0: Zero Regression (Jeep)
    TEST_CHECK(vJeep->special_physics_mode == 0, "Jeep must have mode 0 (STANDARD)");
    TEST_CHECK(vJeep->hide_wheels == 0, "Jeep must not hide wheels");
    TEST_CHECK(feq(vJeep->ski_friction_mult, 1.0f), "Jeep ski friction mult == 1.0");
    TEST_CHECK(feq(vJeep->ski_mud_mult, 1.0f), "Jeep ski mud mult == 1.0");

    // 2. Mode 1: Ski Glide (Snowmobile & Sleigh)
    TEST_CHECK(vSnowmobile->special_physics_mode == 1, "Snowmobile mode == 1 (SKI_GLIDE)");
    TEST_CHECK(vSleigh->special_physics_mode == 1, "Sleigh mode == 1 (SKI_GLIDE)");
    TEST_CHECK(vSleigh->hide_wheels == 1, "Sleigh hides wheels");
    TEST_CHECK(feq(vSnowmobile->ski_friction_mult, 0.35f), "Snowmobile ski friction mult == 0.35");
    TEST_CHECK(feq(vSnowmobile->ski_mud_mult, 0.25f), "Snowmobile ski mud mult == 0.25");

    // Mud comparison: Snowmobile vs standard car in mud
    {
        b2World simWorld(b2Vec2(0.0f, -10.0f));
        Terrain mudTerrain(&simWorld, STAGE_MUDPOOL);

        PlayerCar carJeep(&simWorld, b2Vec2(25.0f, 5.0f), 0, 0, 0, 0, vJeep, &mudTerrain);
        PlayerCar carSnow(&simWorld, b2Vec2(25.0f, 5.0f), 0, 0, 0, 0, vSnowmobile, &mudTerrain);

        // Let both settle on ground in mud zone
        for (int step = 0; step < 60; ++step) {
            simWorld.Step(1.0f / 60.0f, 8, 3);
            carJeep.Update(1.0f / 60.0f);
            carSnow.Update(1.0f / 60.0f);
        }

        // Apply throttle in mud zone for 60 steps
        for (int step = 0; step < 60; ++step) {
            carJeep.Accelerate();
            carSnow.Accelerate();
            simWorld.Step(1.0f / 60.0f, 8, 3);
            carJeep.Update(1.0f / 60.0f);
            carSnow.Update(1.0f / 60.0f);
        }

        float vxJeep = carJeep.GetChassisBody()->GetLinearVelocity().x;
        float vxSnow = carSnow.GetChassisBody()->GetLinearVelocity().x;
        printf("  Mud performance: Jeep vx = %.2f m/s vs Snowmobile vx = %.2f m/s\n", vxJeep, vxSnow);
        TEST_CHECK(vxSnow > vxJeep, "Snowmobile must achieve higher forward velocity in mud due to ski_mud_mult (0.25)");
    }

    // 3. Mode 2: Hover Cushion (Hovercraft)
    TEST_CHECK(vHovercraft->special_physics_mode == 2, "Hovercraft mode == 2 (HOVER_CUSHION)");
    TEST_CHECK(vHovercraft->hide_wheels == 1, "Hovercraft hides wheels");
    TEST_CHECK(feq(vHovercraft->hover_height, 1.20f), "Hovercraft hover height == 1.20m");
    TEST_CHECK(feq(vHovercraft->hover_thrust, 35.0f), "Hovercraft hover thrust == 35.0 N");

    {
        b2World hoverWorld(b2Vec2(0.0f, -10.0f));
        Terrain flatTerrain(&hoverWorld, STAGE_COUNTRYSIDE);

        float groundY = flatTerrain.GetHeightAtX(5.0f);
        // Create hovercraft compressed close to ground (clearance 0.3m < hover_height 1.20m)
        PlayerCar pcHover(&hoverWorld, b2Vec2(5.0f, groundY + 0.3f), 0, 0, 0, 0, vHovercraft, &flatTerrain);

        float initialY = pcHover.GetChassisBody()->GetPosition().y;
        float clearance = initialY - groundY;
        TEST_CHECK(clearance < vHovercraft->hover_height, "Hovercraft initialized below hover height");

        // Run 1 update step to apply cushion force
        pcHover.Update(1.0f / 60.0f);
        hoverWorld.Step(1.0f / 60.0f, 8, 3);

        float vyAfterCushion = pcHover.GetChassisBody()->GetLinearVelocity().y;
        printf("  Hovercraft cushion push: initial vy = 0 -> vy after cushion = %.2f m/s\n", vyAfterCushion);
        TEST_CHECK(vyAfterCushion > 0.0f, "Air cushion must push chassis upwards when below target height");

        // Test forward propeller thrust
        pcHover.Accelerate();
        pcHover.Update(1.0f / 60.0f);
        hoverWorld.Step(1.0f / 60.0f, 8, 3);
        float vxAfterThrust = pcHover.GetChassisBody()->GetLinearVelocity().x;
        TEST_CHECK(vxAfterThrust > 0.0f, "Hovercraft forward thrust must impart positive horizontal velocity");
    }

    // 4. Mode 3: Moonlander Thruster Jet
    TEST_CHECK(vMoonlander->special_physics_mode == 3, "Moonlander mode == 3 (THRUSTER_JET)");
    TEST_CHECK(feq(vMoonlander->thruster_g_mult, 1.80f), "Moonlander thruster_g_mult == 1.80");

    {
        b2World moonWorld(b2Vec2(0.0f, -9.8f));
        PlayerCar pcFreefall(&moonWorld, b2Vec2(0.0f, 50.0f), 0, 0, 0, 0, vMoonlander);
        PlayerCar pcThrust(&moonWorld, b2Vec2(10.0f, 50.0f), 0, 0, 0, 0, vMoonlander);

        pcThrust.SetThruster(true);
        TEST_CHECK(pcThrust.IsThrusterActive(), "pcThrust thruster must be active");
        TEST_CHECK(!pcFreefall.IsThrusterActive(), "pcFreefall thruster must be inactive");

        float initialFuel = pcThrust.GetFuel();

        // Step physics for 30 steps in air
        for (int i = 0; i < 30; ++i) {
            pcFreefall.Update(1.0f / 60.0f);
            pcThrust.Update(1.0f / 60.0f);
            moonWorld.Step(1.0f / 60.0f, 8, 3);
        }

        float yFreefall = pcFreefall.GetChassisBody()->GetPosition().y;
        float yThrust   = pcThrust.GetChassisBody()->GetPosition().y;
        float vyFreefall = pcFreefall.GetChassisBody()->GetLinearVelocity().y;
        float vyThrust   = pcThrust.GetChassisBody()->GetLinearVelocity().y;

        printf("  Moonlander in-air: freefall vy = %.2f m/s (y = %.2f) vs thruster vy = %.2f m/s (y = %.2f)\n",
               vyFreefall, yFreefall, vyThrust, yThrust);

        TEST_CHECK(vyThrust > vyFreefall, "Thruster jet must produce positive upward acceleration relative to freefall");
        TEST_CHECK(yThrust > yFreefall, "Thruster craft must be strictly higher than freefall craft");
        TEST_CHECK(pcThrust.GetFuel() < initialFuel, "Thruster jet must consume fuel during burn");

        // Airborne pitch torque test: airborne rotation must operate normally
        pcThrust.Accelerate(); // holding throttle in air tilts backward
        pcThrust.Update(1.0f / 60.0f);
        moonWorld.Step(1.0f / 60.0f, 8, 3);
        float wThrust = pcThrust.GetChassisBody()->GetAngularVelocity();
        TEST_CHECK(wThrust != 0.0f, "Air pitch control must remain functional while thruster is active");
    }

    printf("  [PASS] Sub-test B: All 4 special physics modes (0, 1, 2, 3) fully verified!\n\n");

    // -------------------------------------------------------------
    // Sub-test C: Full Save Profile Parity Roundtrip (28 Stages, 29 Vehicles)
    // -------------------------------------------------------------
    printf("[Sub-test C] Verifying full save profile roundtrip (28 stages, 29 vehicles)...\n");

    const char* sandboxBase = "build-pc/test_full_parity_sandbox";
    CleanupTestFiles(sandboxBase);

    PlayerInfo::Reset();
    PlayerInfo::SetCoins(50000000); // 50M coins
    uint32_t sMoonlander = PlayerInfo::HashId("moonlander");
    uint32_t sNuclear    = PlayerInfo::HashId("nuclear");
    PlayerInfo::SetSelectedVehicleIdHash(sMoonlander);
    PlayerInfo::SetSelectedStageIdHash(sNuclear);

    // Populate all 29 vehicles and 28 stages
    for (int i = 0; i < 29; ++i) {
        uint32_t vh = PlayerInfo::HashId(expectedVehIds[i]);
        PlayerInfo::UnlockVehicle(vh);
        PlayerInfo::SetVehicleUpgrade(vh, 0, (i % 20) + 1);
        PlayerInfo::SetVehicleUpgrade(vh, 1, ((i + 4) % 20) + 1);
        PlayerInfo::SetVehicleUpgrade(vh, 2, ((i + 8) % 20) + 1);
        PlayerInfo::SetVehicleUpgrade(vh, 3, ((i + 12) % 20) + 1);
    }

    for (int i = 0; i < 28; ++i) {
        uint32_t sh = PlayerInfo::HashId(expectedStageIds[i]);
        PlayerInfo::UnlockStage(sh);
        PlayerInfo::SetStageRecord(sh, 1000 + i * 350);
        PlayerInfo::HighScores[i] = 1000 + i * 350;
    }

    bool saved = PlayerInfo::Save(sandboxBase, true);
    TEST_CHECK(saved, "Failed to save full 28 stages + 29 vehicles profile");

    // Reset state
    PlayerInfo::Reset();
    TEST_CHECK(PlayerInfo::GetCoins() == 0, "Coins reset to 0");
    TEST_CHECK(PlayerInfo::GetSelectedVehicleIdHash() == PlayerInfo::HashId("jeep"), "Vehicle reset to jeep");

    // Reload state
    bool loadedSave = PlayerInfo::Load(sandboxBase);
    TEST_CHECK(loadedSave, "Failed to load full 28 stages + 29 vehicles profile");

    TEST_CHECK(PlayerInfo::GetCoins() == 50000000, "Reloaded coins match 50,000,000");
    TEST_CHECK(PlayerInfo::GetSelectedVehicleIdHash() == sMoonlander, "Reloaded selected vehicle == moonlander");
    TEST_CHECK(PlayerInfo::GetSelectedStageIdHash() == sNuclear, "Reloaded selected stage == nuclear");

    for (int i = 0; i < 29; ++i) {
        uint32_t vh = PlayerInfo::HashId(expectedVehIds[i]);
        TEST_CHECK(PlayerInfo::IsVehicleUnlocked(vh), "All 29 vehicles must remain unlocked");
        TEST_CHECK(PlayerInfo::GetVehicleUpgrade(vh, 0) == ((i % 20) + 1), "Slot 0 level roundtripped");
        TEST_CHECK(PlayerInfo::GetVehicleUpgrade(vh, 1) == (((i + 4) % 20) + 1), "Slot 1 level roundtripped");
        TEST_CHECK(PlayerInfo::GetVehicleUpgrade(vh, 2) == (((i + 8) % 20) + 1), "Slot 2 level roundtripped");
        TEST_CHECK(PlayerInfo::GetVehicleUpgrade(vh, 3) == (((i + 12) % 20) + 1), "Slot 3 level roundtripped");
    }

    for (int i = 0; i < 28; ++i) {
        uint32_t sh = PlayerInfo::HashId(expectedStageIds[i]);
        TEST_CHECK(PlayerInfo::IsStageUnlocked(sh), "All 28 stages must remain unlocked");
        TEST_CHECK(PlayerInfo::GetStageRecord(sh) == (1000 + i * 350), "Stage record roundtripped");
        TEST_CHECK(PlayerInfo::HighScores[i] == (1000 + i * 350), "HighScores cache roundtripped");
    }

    // Binary format layout invariants
    TEST_CHECK(sizeof(SavePayload) == 1556, "sizeof(SavePayload) must remain strictly 1556 bytes");
    TEST_CHECK(sizeof(SaveHeader) == 16, "sizeof(SaveHeader) must remain strictly 16 bytes");

    CleanupTestFiles(sandboxBase);
    printf("  [PASS] Sub-test C: Full 28 stages + 29 vehicles save profile roundtrip verified!\n\n");

    printf("=================================================================\n");
    printf("   ALL 3 SUB-TESTS PASSED (TEST 22 FULLY VERIFIED)\n");
    printf("=================================================================\n");
    return 0;
}

