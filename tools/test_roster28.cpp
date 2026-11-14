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
        printf("   HCR Regression Test 21: 28 Stages, 25 Vehicles & H13 Regress   \n");
        printf("=================================================================\n\n");
    }

    // -------------------------------------------------------------
    // Sub-test A: 28 Stages Verification & Heightfield Determinism
    // -------------------------------------------------------------
    if (!dumpHashesOnly) {
        printf("[Sub-test A] Verifying 28 stages configuration and heightfield determinism...\n");
    }
    bool loadedStages = Config::LoadStages("config/stages.cfg");
    if (!loadedStages) loadedStages = Config::LoadStages("data/stages.cfg");
    TEST_CHECK(loadedStages, "Failed to load stages.cfg");
    TEST_CHECK(Config::StageCount == 28, "Must load exactly 28 stages from stages.cfg");

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
        TEST_CHECK(stageHashes[i] == expectedStageHashes[i], "Stage ID hash matches authoritative FNV-1a 32-bit reference");
        if (!dumpHashesOnly) {
            printf("  Stage [%2d]: %-14s (hash 0x%08X, cost %7d, night=%d, bridges=%d, mud=%d, ceiling=%d)\n",
                   i, st.id, stageHashes[i], st.unlock_cost, st.is_night, st.has_bridges, st.has_mud_pools, st.has_ceiling);
        }
    }

    // Verify all 28 stage hashes are strictly unique
    for (int i = 0; i < 28; ++i) {
        for (int j = i + 1; j < 28; ++j) {
            TEST_CHECK(stageHashes[i] != stageHashes[j], "Stage hashes must be strictly unique");
        }
    }

    // 64-bit heightfield hashes for all 28 stages over 4001 points (0..2000m)
    const uint64_t CANONICAL_STAGE_HASHES[28] = {
        0x6e83945913e2d687ULL, // 0: Countryside (Frozen baseline)
        0x6dfb669a32592de7ULL, // 1: Desert (Frozen baseline)
        0xf7cc9dd1f3aa54b0ULL, // 2: Arctic (Frozen baseline)
        0x0083f35054ddbad2ULL, // 3: Moon (Frozen baseline)
        0x79cb14fc941aed46ULL, // 4: Highway (Deterministic double)
        0x4b3f21147a6045caULL, // 5: Cave (Frozen baseline)
        0x307e7d5f71ea2410ULL, // 6: Mars (Frozen baseline)
        0xabd7ad1150f9fbf7ULL, // 7: Forest (Deterministic double)
        0x3f496da68e62c7ecULL, // 8: Mountain (Deterministic double)
        0x12fd17d4ac8a936dULL, // 9: Mudpool (Deterministic double)
        0x50aa43daaf13ada2ULL, // 10: Volcano (Deterministic double)
        0xd9e985e660616346ULL, // 11: Beach (Frozen baseline)
        0xa8fd4bb25c7067d8ULL, // 12: Rollercoaster (Deterministic double)
        0x8177c160f2e579b9ULL, // 13: Night (Deterministic double)
        0x721cab00477aa490ULL, // 14: Rooftops (Deterministic double)
        0x168b4778e4ccfc72ULL, // 15: Junkyard (Deterministic double)
        0x53ed7a75b127d093ULL, // 16: Construction (Deterministic double)
        0xcd88490d45999148ULL, // 17: Rainbow (Deterministic double)
        0x11b9b29b0a89ed3dULL, // 18: Seasons (Deterministic double)
        0x7b7829e20dedb615ULL, // 19: Bootcamp (Deterministic double)
        0x4dee6c89fff49f7cULL, // 20: Xmas (Deterministic double)
        0xb6d53b9c59560286ULL, // 21: Alienplanet (Deterministic double)
        0xbf1efe0cf6e33ddeULL, // 22: Arcticcave (Deterministic double)
        0x7701828bb20753abULL, // 23: Ragnarok (Deterministic double)
        0x5f7b940443dc8475ULL, // 24: Factory
        0x34068b9f4f6d3417ULL, // 25: Haunted
        0x56a8242ec2790a3eULL, // 26: Northpole
        0x733d9464a31061d6ULL  // 27: Nuclear
    };

    const int NUM_POINTS = 4001; // 0 to 2000m with step 0.5m
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
            if (CANONICAL_STAGE_HASHES[s] != 0ULL) {
                TEST_CHECK(h == CANONICAL_STAGE_HASHES[s], "Stage height hash mismatch! Non-regression violated!");
            }
            bool isBaseline = (s == 0 || s == 1 || s == 2 || s == 3 || s == 5 || s == 6 || s == 11);
            printf("  Stage %-14s [%2d]: hash 0x%016llx -> %s\n",
                   Config::Stages[s].id, s, (unsigned long long)h,
                   isBaseline ? "100% BIT-EXACT BASELINE MATCH" : "DETERMINISTIC CANONICAL MATCH");
        }
    }
    free(heightBuffer);

    if (dumpHashesOnly) {
        return 0;
    }

    // Stage feature flags assertions
    TEST_CHECK(Config::Stages[STAGE_NIGHT].is_night == 1, "Night stage must have is_night == 1");
    TEST_CHECK(Config::Stages[STAGE_HAUNTED].is_night == 1, "Haunted stage must have is_night == 1");
    for (int s = 0; s < 28; ++s) {
        if (s != STAGE_NIGHT && s != STAGE_HAUNTED) {
            TEST_CHECK(Config::Stages[s].is_night == 0, "Non-night stages must have is_night == 0");
        }
    }
    TEST_CHECK(Config::Stages[STAGE_FOREST].has_bridges == 1, "Forest must have bridges");
    TEST_CHECK(Config::Stages[STAGE_MOUNTAIN].has_bridges == 1, "Mountain must have bridges");
    TEST_CHECK(Config::Stages[STAGE_ROOFTOPS].has_bridges == 1, "Rooftops must have bridges");
    TEST_CHECK(Config::Stages[STAGE_CONSTRUCTION].has_bridges == 1, "Construction must have bridges");
    TEST_CHECK(Config::Stages[STAGE_FACTORY].has_bridges == 1, "Factory must have bridges");

    TEST_CHECK(Config::Stages[STAGE_MUDPOOL].has_mud_pools == 1, "Mudpool must have mud");
    TEST_CHECK(Config::Stages[STAGE_VOLCANO].has_mud_pools == 1, "Volcano must have mud");
    TEST_CHECK(Config::Stages[STAGE_JUNKYARD].has_mud_pools == 1, "Junkyard must have mud");
    TEST_CHECK(Config::Stages[STAGE_RAGNAROK].has_mud_pools == 1, "Ragnarok must have mud");
    TEST_CHECK(Config::Stages[STAGE_HAUNTED].has_mud_pools == 1, "Haunted must have mud");
    TEST_CHECK(Config::Stages[STAGE_NUCLEAR].has_mud_pools == 1, "Nuclear must have mud");

    TEST_CHECK(Config::Stages[STAGE_CAVE].has_ceiling == 1, "Cave must have ceiling");
    TEST_CHECK(Config::Stages[STAGE_ARCTICCAVE].has_ceiling == 1, "Arctic Cave must have ceiling");

    // Bridge gating verification: MaybeSpawnBridge responds only to has_bridges=1
    {
        Terrain terrainFactory(&dummyWorld, STAGE_FACTORY);
        Terrain terrainBeach(&dummyWorld, STAGE_BEACH);
        TEST_CHECK(terrainFactory.GetStageConfig()->has_bridges == 1, "Factory has bridges");
        TEST_CHECK(terrainBeach.GetStageConfig()->has_bridges == 0, "Beach has no bridges");
    }

    // Mud zone gating verification
    {
        Terrain terrainHaunted(&dummyWorld, STAGE_HAUNTED);
        Terrain terrainNuclear(&dummyWorld, STAGE_NUCLEAR);
        TEST_CHECK(terrainHaunted.IsInMudZone(25.0f), "Haunted is in mud zone at 25m");
        TEST_CHECK(!terrainHaunted.IsInMudZone(5.0f), "Haunted is not in mud zone before 15m");
        TEST_CHECK(terrainNuclear.IsInMudZone(25.0f), "Nuclear is in mud zone at 25m");
    }

    printf("  [PASS] Sub-test A: 28 stages, heightfield determinism & feature mechanics verified!\n\n");

    // -------------------------------------------------------------
    // Sub-test B: 25 Vehicles, Physics Properties & 4th Slot Types
    // -------------------------------------------------------------
    printf("[Sub-test B] Verifying 25 vehicles configuration, Box2D physics & 4th slot types...\n");
    bool loadedVehicles = Config::LoadVehicles("config/vehicles.cfg");
    if (!loadedVehicles) loadedVehicles = Config::LoadVehicles("data/vehicles.cfg");
    TEST_CHECK(loadedVehicles, "Failed to load vehicles.cfg");
    TEST_CHECK(Config::VehicleCount >= 25, "Must load at least 25 vehicles from vehicles.cfg");

    const char* expectedVehIds[25] = {
        "jeep", "motocross", "monster_truck", "race_car",
        "tractor", "hippievan", "quadbike", "touristbus",
        "onewheeler", "screwfinger", "policecar", "ambulance",
        "electric", "dieseltruck", "rallycar", "dragster",
        "firetruck", "tank",
        "dunebuggy", "superoffroad", "truck", "minibike",
        "bigfinger", "hotrod",
        "train"
    };
    int expectedVehCosts[25] = {
        0, 75000, 100000, 250000,
        100000, 120000, 175000, 200000,
        150000, 300000, 350000, 400000,
        450000, 500000, 750000, 800000,
        800000, 1000000,
        800000, 1000000, 1000000, 1000000,
        1200000, 1500000,
        1500000
    };
    int expectedFourthTypes[25] = {
        0, 2, 0, 3, // jeep (4WD), motocross (AIR), monster (4WD), race_car (DOWNFORCE)
        4, 5, 0, 0, // tractor (STABILITY), hippievan (BOOST), quadbike (4WD), touristbus (4WD)
        4, 0, 5, 4, // onewheeler (STABILITY), screwfinger (4WD), policecar (BOOST), ambulance (STABILITY)
        1, 0, 3, 3, // electric (FUEL), dieseltruck (4WD), rallycar (DOWNFORCE), dragster (DOWNFORCE)
        0, 0,       // firetruck (4WD), tank (4WD)
        0, 3, 0, 2, // dunebuggy (4WD), superoffroad (DOWNFORCE), truck (4WD), minibike (AIR_CONTROL)
        0, 5,       // bigfinger (4WD), hotrod (BOOST)
        5           // train (BOOST mapping from TORQUE)
    };
    const uint32_t expectedVehHashes[25] = {
        0x64809865u, 0xC7D0636Cu, 0x73E941F5u, 0x135E4961u,
        0x592F7EF8u, 0x62C00991u, 0xFDEB8743u, 0x0A87F33Du,
        0xA0F5B831u, 0xB61B2D9Eu, 0x15BBA1FBu, 0x8FF9071Du,
        0xDDC6B3BEu, 0x90B2B8BAu, 0xB16FA6CFu, 0x1B89769Bu,
        0x01CC7C24u, 0x0C285A89u,
        0x3386A573u, 0xAB2471FFu, 0xF542BDC8u, 0x292EB177u,
        0x9E846550u, 0x827BB5D9u,
        0x78556C69u
    };

    uint32_t vehHashes[25];
    b2World vehWorld(b2Vec2(0.0f, -9.8f));

    for (int i = 0; i < 25; ++i) {
        const VehicleConfig& vc = Config::Vehicles[i];
        TEST_CHECK(strcmp(vc.id, expectedVehIds[i]) == 0, "Vehicle ID matches expected");
        TEST_CHECK(vc.unlock_cost == expectedVehCosts[i], "Vehicle unlock cost matches 1.30.0 provenance");
        TEST_CHECK(vc.fourth_upgrade_type == expectedFourthTypes[i], "Vehicle 4th upgrade slot matches specification");
        vehHashes[i] = PlayerInfo::HashId(vc.id);
        TEST_CHECK(vehHashes[i] == expectedVehHashes[i], "Vehicle ID hash matches authoritative FNV-1a 32-bit reference");

        // Verify positive Box2D mass properties
        PlayerCar pc(&vehWorld, b2Vec2(0.0f, 2.0f), 0, 0, 0, 0, &vc);
        float cm = pc.GetChassisBody()->GetMass();
        float fm = pc.GetFrontWheelBody()->GetMass();
        float rm = pc.GetRearWheelBody()->GetMass();
        float hm = pc.GetDriverHeadBody() ? pc.GetDriverHeadBody()->GetMass() : 0.0f;
        TEST_CHECK(cm > 0.0f, "Chassis mass must be strictly positive");
        TEST_CHECK(fm > 0.0f, "Front wheel mass must be strictly positive");
        TEST_CHECK(rm > 0.0f, "Rear wheel mass must be strictly positive");
        TEST_CHECK(hm > 0.0f, "Driver head mass must be strictly positive");

        printf("  Vehicle [%2d]: %-14s (hash 0x%08X, cost %7d, 4th slot=%d, mass=%6.2f kg)\n",
               i, vc.id, vehHashes[i], vc.unlock_cost, vc.fourth_upgrade_type, cm + fm + rm + hm);
    }

    // Verify all 25 vehicle hashes are strictly unique
    for (int i = 0; i < 25; ++i) {
        for (int j = i + 1; j < 25; ++j) {
            TEST_CHECK(vehHashes[i] != vehHashes[j], "Vehicle hashes must be strictly unique");
        }
    }

    // Verify Train / Kiddie Express (T-131a)
    const VehicleConfig* train = Config::FindVehicleById("train");
    const VehicleConfig* touristbus = Config::FindVehicleById("touristbus");
    TEST_CHECK(train != NULL, "Train must exist");
    TEST_CHECK(touristbus != NULL, "Tourist Bus must exist");
    {
        PlayerCar pcTrain(&vehWorld, b2Vec2(0.0f, 2.0f), 0, 0, 0, 0, train);
        float trainChassisMass = pcTrain.GetChassisBody()->GetMass();
        TEST_CHECK(trainChassisMass > 3.4206f, "Train chassis mass must strictly exceed 24-vehicle median (> 3.4206 kg)");
        TEST_CHECK(trainChassisMass >= 8.0f && trainChassisMass <= 10.5f, "Train chassis mass in target range ~8-10 kg");
        float trainWheelbase = fabsf(train->front_wheel_offset[0] - train->rear_wheel_offset[0]);
        float touristbusWheelbase = fabsf(touristbus->front_wheel_offset[0] - touristbus->rear_wheel_offset[0]);
        TEST_CHECK(trainWheelbase > touristbusWheelbase, "Train wheelbase must be strictly longer than touristbus");
        TEST_CHECK(train->fourth_upgrade_type == 5, "Train 4th slot == 5 (BOOST)");
        TEST_CHECK(train->drive_type == 2, "Train drive type == 2 (4WD)");
    }
    printf("  [PASS] Sub-test B: 25 vehicles, positive masses, 4th slot types & unique hashes verified!\n\n");

    // -------------------------------------------------------------
    // Sub-test C: H13 Regression & Full Save Profile Roundtrip
    // -------------------------------------------------------------
    printf("[Sub-test C] Verifying H13 regressions & full save profile roundtrip...\n");
    // 1. StageTagAt bounds and validity checks
    for (int i = 0; i < 28; ++i) {
        const char* tag = PlayerInfo::StageTagAt(i);
        TEST_CHECK(tag != NULL, "StageTagAt(i) must not be NULL for i < 28");
        TEST_CHECK(strlen(tag) > 0, "StageTagAt(i) must not be empty");
        TEST_CHECK(PlayerInfo::HashId(tag) != 0, "StageTagAt(i) hash must be non-zero");
        TEST_CHECK(PlayerInfo::HashId(tag) == expectedStageHashes[i], "StageTagAt(i) hash matches expected");
    }
    TEST_CHECK(PlayerInfo::StageTagAt(-1) == NULL, "StageTagAt(-1) must return NULL");
    TEST_CHECK(PlayerInfo::StageTagAt(28) == NULL, "StageTagAt(28) must return NULL");
    TEST_CHECK(PlayerInfo::StageTagAt(35) == NULL, "StageTagAt(35) must return NULL");

    // 2. Full save profile serialization and deserialization
    const char* sandboxBase = "build-pc/test_roster28_sandbox";
    CleanupTestFiles(sandboxBase);

    PlayerInfo::Reset();
    PlayerInfo::SetCoins(25000000); // 25 million coins
    uint32_t trainHash = PlayerInfo::HashId("train");
    uint32_t nuclearHash = PlayerInfo::HashId("nuclear");
    PlayerInfo::SetSelectedVehicleIdHash(trainHash);
    PlayerInfo::SetSelectedStageIdHash(nuclearHash);

    // Populate all 25 vehicles and 28 stages
    for (int i = 0; i < 25; ++i) {
        uint32_t vh = PlayerInfo::HashId(expectedVehIds[i]);
        PlayerInfo::UnlockVehicle(vh);
        PlayerInfo::SetVehicleUpgrade(vh, 0, (i % 20) + 1);
        PlayerInfo::SetVehicleUpgrade(vh, 1, ((i + 3) % 20) + 1);
        PlayerInfo::SetVehicleUpgrade(vh, 2, ((i + 7) % 20) + 1);
        PlayerInfo::SetVehicleUpgrade(vh, 3, ((i + 11) % 20) + 1);
    }
    for (int i = 0; i < 28; ++i) {
        uint32_t sh = PlayerInfo::HashId(expectedStageIds[i]);
        PlayerInfo::UnlockStage(sh);
        PlayerInfo::SetStageRecord(sh, 500 + i * 250);
        PlayerInfo::HighScores[i] = 500 + i * 250;
    }

    bool saved = PlayerInfo::Save(sandboxBase, true);
    TEST_CHECK(saved, "Failed to save full profile to sandboxBase");

    // Clear state
    PlayerInfo::Reset();
    TEST_CHECK(PlayerInfo::GetCoins() == 0, "Reset coins == 0");
    TEST_CHECK(PlayerInfo::GetSelectedVehicleIdHash() == PlayerInfo::HashId("jeep"), "Reset selected vehicle");

    // Reload and verify
    bool loadedSave = PlayerInfo::Load(sandboxBase);
    TEST_CHECK(loadedSave, "Failed to load full profile from sandboxBase");

    TEST_CHECK(PlayerInfo::GetCoins() == 25000000, "Reloaded coins == 25000000");
    TEST_CHECK(PlayerInfo::GetSelectedVehicleIdHash() == trainHash, "Reloaded selected vehicle == train");
    TEST_CHECK(PlayerInfo::GetSelectedStageIdHash() == nuclearHash, "Reloaded selected stage == nuclear");

    for (int i = 0; i < 25; ++i) {
        uint32_t vh = PlayerInfo::HashId(expectedVehIds[i]);
        TEST_CHECK(PlayerInfo::IsVehicleUnlocked(vh), "Vehicle must remain unlocked");
        TEST_CHECK(PlayerInfo::GetVehicleUpgrade(vh, 0) == ((i % 20) + 1), "Vehicle engine upgrade roundtripped");
        TEST_CHECK(PlayerInfo::GetVehicleUpgrade(vh, 1) == (((i + 3) % 20) + 1), "Vehicle suspension upgrade roundtripped");
        TEST_CHECK(PlayerInfo::GetVehicleUpgrade(vh, 2) == (((i + 7) % 20) + 1), "Vehicle tires upgrade roundtripped");
        TEST_CHECK(PlayerInfo::GetVehicleUpgrade(vh, 3) == (((i + 11) % 20) + 1), "Vehicle 4th upgrade roundtripped");
    }

    for (int i = 0; i < 28; ++i) {
        uint32_t sh = PlayerInfo::HashId(expectedStageIds[i]);
        TEST_CHECK(PlayerInfo::IsStageUnlocked(sh), "Stage must remain unlocked");
        TEST_CHECK(PlayerInfo::GetStageRecord(sh) == (500 + i * 250), "Stage record roundtripped");
        TEST_CHECK(PlayerInfo::HighScores[i] == (500 + i * 250), "HighScores runtime cache roundtripped");
    }

    // Invariant assertions
    TEST_CHECK(sizeof(SavePayload) == 1556, "sizeof(SavePayload) must remain strictly 1556 bytes");
    TEST_CHECK(sizeof(SaveHeader) == 16, "sizeof(SaveHeader) must remain strictly 16 bytes");

    CleanupTestFiles(sandboxBase);
    printf("  [PASS] Sub-test C: H13 regressions, StageTagAt, and full save roundtrip verified!\n\n");

    printf("=================================================================\n");
    printf("   ALL 3 SUB-TESTS PASSED (TEST 21 FULLY VERIFIED)\n");
    printf("=================================================================\n");
    return 0;
}
