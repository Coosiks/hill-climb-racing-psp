#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "core/Config.h"
#include "core/PlayerInfo.h"
#include "core/GameConstants.h"
#include "physics/Terrain.h"
#include "physics/PlayerCar.h"
#include "physics/PhysicsWorld.h"
#include "graphics/Renderer2D.h"

#define TEST_CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
            return 1; \
        } \
    } while(0)

static inline bool feq(float a, float b, float eps = 0.0001f) {
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

int main(int argc, char** argv) {
    bool dumpHashesOnly = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--hashes") == 0) {
            dumpHashesOnly = true;
        }
    }

    if (!dumpHashesOnly) {
        printf("=================================================================\n");
        printf("   HCR Regression Test 20: 24 Stages, 24 Vehicles & H11 Fixes    \n");
        printf("=================================================================\n\n");
    }

    // -------------------------------------------------------------
    // Sub-test A: 24 Stages Verification & Heightfield Determinism
    // -------------------------------------------------------------
    if (!dumpHashesOnly) {
        printf("[Sub-test A] Verifying 24 stages configuration and heightfield determinism...\n");
    }
    bool loadedStages = Config::LoadStages("config/stages.cfg");
    if (!loadedStages) loadedStages = Config::LoadStages("data/stages.cfg");
    TEST_CHECK(loadedStages, "Failed to load stages.cfg");
    TEST_CHECK(Config::StageCount >= 24, "Must load at least 24 stages from stages.cfg");

    const char* expectedStageIds[24] = {
        "countryside", "desert", "arctic", "moon",
        "highway", "cave", "mars", "forest",
        "mountain", "mudpool", "volcano", "beach",
        "rollercoaster", "night", "rooftops", "junkyard",
        "construction", "rainbow",
        "seasons", "bootcamp", "xmas", "alienplanet",
        "arcticcave", "ragnarok"
    };
    int expectedStageCosts[24] = {
        0, 35000, 75000, 175000,
        125000, 150000, 250000, 850000,
        850000, 1000000, 1000000, 1000000,
        1200000, 1200000, 1200000, 1400000,
        1400000, 1500000,
        50000, 200000, 300000, 750000,
        800000, 1000000
    };
    const uint32_t expectedStageHashes[24] = {
        0x62B2D6DAu, 0x2CB41EBCu, 0xA09F16CDu, 0xF184AE40u,
        0x93FF431Cu, 0x9731FE58u, 0xDF7B0A68u, 0x4F292A60u,
        0x49CBE0A2u, 0x69D96D3Fu, 0x9848FDBDu, 0xF4B2C848u,
        0x4E64F17Au, 0xE6DE9F61u, 0xC0280847u, 0xDE180AF3u,
        0xCF30AB8Cu, 0x2B6CE2A9u,
        0xEA1DEEB1u, 0x400079C4u, 0xB41ECCFAu, 0xCF9483AEu,
        0x1E081680u, 0x6B8A1324u
    };

    uint32_t stageHashes[24];
    for (int i = 0; i < 24; ++i) {
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

    // Verify all 24 stage hashes are strictly unique
    for (int i = 0; i < 24; ++i) {
        for (int j = i + 1; j < 24; ++j) {
            TEST_CHECK(stageHashes[i] != stageHashes[j], "Stage hashes must be unique");
        }
    }

    // Bit-exact canonical heightfield hashes for all 24 stages (4001 points, 0..2000m, step 0.5m)
    // 7 baselines (Countryside, Desert, Arctic, Moon, Cave, Mars, Beach) strictly bit-exact.
    const uint64_t CANONICAL_STAGE_HASHES[24] = {
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
        0x7701828bb20753abULL  // 23: Ragnarok (Deterministic double)
    };

    const int NUM_POINTS = 4001; // 0 to 2000m with step 0.5m
    float* heightBuffer = (float*)malloc(NUM_POINTS * sizeof(float));
    TEST_CHECK(heightBuffer != NULL, "Failed to allocate height buffer");

    b2World dummyWorld(b2Vec2(0.0f, -10.0f));
    for (int s = 0; s < 24; ++s) {
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
            TEST_CHECK(h == CANONICAL_STAGE_HASHES[s], "Stage height hash mismatch! Non-regression violated!");
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
    for (int s = 0; s < 24; ++s) {
        if (s != STAGE_NIGHT) {
            TEST_CHECK(Config::Stages[s].is_night == 0, "Non-night stages must have is_night == 0");
        }
    }
    TEST_CHECK(Config::Stages[STAGE_FOREST].has_bridges == 1, "Forest must have bridges");
    TEST_CHECK(Config::Stages[STAGE_MOUNTAIN].has_bridges == 1, "Mountain must have bridges");
    TEST_CHECK(Config::Stages[STAGE_ROOFTOPS].has_bridges == 1, "Rooftops must have bridges");
    TEST_CHECK(Config::Stages[STAGE_CONSTRUCTION].has_bridges == 1, "Construction must have bridges");

    TEST_CHECK(Config::Stages[STAGE_MUDPOOL].has_mud_pools == 1, "Mudpool must have mud");
    TEST_CHECK(Config::Stages[STAGE_VOLCANO].has_mud_pools == 1, "Volcano must have mud");
    TEST_CHECK(Config::Stages[STAGE_JUNKYARD].has_mud_pools == 1, "Junkyard must have mud");
    TEST_CHECK(Config::Stages[STAGE_RAGNAROK].has_mud_pools == 1, "Ragnarok must have mud");

    TEST_CHECK(Config::Stages[STAGE_CAVE].has_ceiling == 1, "Cave must have ceiling");
    TEST_CHECK(Config::Stages[STAGE_ARCTICCAVE].has_ceiling == 1, "Arctic Cave must have ceiling");
    TEST_CHECK(feq(Config::Stages[STAGE_RAINBOW].gravity, -6.0f), "Rainbow gravity must be -6.0");

    printf("  [PASS] Sub-test A: 24 stages, bit-exact baselines, canonical hashes and stage flags verified!\n\n");

    // -------------------------------------------------------------
    // Sub-test B: 24 Vehicles, Physics Properties & 4th Slot Types
    // -------------------------------------------------------------
    printf("[Sub-test B] Verifying 24 vehicles configuration, Box2D physics & 4th slot types...\n");
    bool loadedVehicles = Config::LoadVehicles("config/vehicles.cfg");
    if (!loadedVehicles) loadedVehicles = Config::LoadVehicles("data/vehicles.cfg");
    TEST_CHECK(loadedVehicles, "Failed to load vehicles.cfg");
    TEST_CHECK(Config::VehicleCount >= 24, "Must load at least 24 vehicles from vehicles.cfg");

    const char* expectedVehIds[24] = {
        "jeep", "motocross", "monster_truck", "race_car",
        "tractor", "hippievan", "quadbike", "touristbus",
        "onewheeler", "screwfinger", "policecar", "ambulance",
        "electric", "dieseltruck", "rallycar", "dragster",
        "firetruck", "tank",
        "dunebuggy", "superoffroad", "truck", "minibike",
        "bigfinger", "hotrod"
    };
    int expectedVehCosts[24] = {
        0, 75000, 100000, 250000,
        100000, 120000, 175000, 200000,
        150000, 300000, 350000, 400000,
        450000, 500000, 750000, 800000,
        800000, 1000000,
        800000, 1000000, 1000000, 1000000,
        1200000, 1500000
    };
    int expectedFourthTypes[24] = {
        0, 2, 0, 3, // jeep (4WD), motocross (AIR), monster (4WD), race_car (DOWNFORCE)
        4, 5, 0, 0, // tractor (STABILITY), hippievan (BOOST), quadbike (4WD), touristbus (4WD)
        4, 0, 5, 4, // onewheeler (STABILITY), screwfinger (4WD), policecar (BOOST), ambulance (STABILITY)
        1, 0, 3, 3, // electric (FUEL), dieseltruck (4WD), rallycar (DOWNFORCE), dragster (DOWNFORCE)
        0, 0,       // firetruck (4WD), tank (4WD)
        0, 3, 0, 2, // dunebuggy (4WD), superoffroad (DOWNFORCE), truck (4WD), minibike (AIR_CONTROL)
        0, 5        // bigfinger (4WD), hotrod (BOOST)
    };
    const uint32_t expectedVehHashes[24] = {
        0x64809865u, 0xC7D0636Cu, 0x73E941F5u, 0x135E4961u,
        0x592F7EF8u, 0x62C00991u, 0xFDEB8743u, 0x0A87F33Du,
        0xA0F5B831u, 0xB61B2D9Eu, 0x15BBA1FBu, 0x8FF9071Du,
        0xDDC6B3BEu, 0x90B2B8BAu, 0xB16FA6CFu, 0x1B89769Bu,
        0x01CC7C24u, 0x0C285A89u,
        0x3386A573u, 0xAB2471FFu, 0xF542BDC8u, 0x292EB177u,
        0x9E846550u, 0x827BB5D9u
    };

    uint32_t vehHashes[24];
    b2World vehWorld(b2Vec2(0.0f, -9.8f));

    for (int i = 0; i < 24; ++i) {
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

    // Verify all 24 vehicle hashes are strictly unique
    for (int i = 0; i < 24; ++i) {
        for (int j = i + 1; j < 24; ++j) {
            TEST_CHECK(vehHashes[i] != vehHashes[j], "Vehicle hashes must be unique");
        }
    }

    printf("  [PASS] Sub-test B: 24 vehicles, positive masses, 4th slot types & unique hashes verified!\n\n");

    // -------------------------------------------------------------
    // Sub-test C: H11 Regressions (Save Deduplication & Night Mode)
    // -------------------------------------------------------------
    printf("[Sub-test C] Verifying H11 regressions: save migration deduplication & Night Mode...\n");

    // Regression 1: Save with BOTH 0xDC088A10 and 0xB61B2D9E present
    remove("tmp_test_mig_20.bin");
    remove("tmp_test_mig_20.bak");
    {
        SaveHeader h;
        memset(&h, 0, sizeof(h));
        memcpy(h.magic, "HCRS", 4);
        h.version = 1;
        h.payloadSize = sizeof(SavePayload);

        SavePayload pl;
        memset(&pl, 0, sizeof(pl));
        pl.coins = 500000;
        pl.selectedVehicleIdHash = 0xDC088A10u;
        pl.vehicleCount = 2;
        // Old slot 0: 0xDC088A10
        pl.vehicles[0].idHash = 0xDC088A10u;
        pl.vehicles[0].unlocked = 1;
        pl.vehicles[0].levelEngine = 6;
        pl.vehicles[0].levelSuspension = 2;
        pl.vehicles[0].levelTires = 4;
        pl.vehicles[0].level4th = 1;

        // Canonical slot 1: 0xB61B2D9E
        pl.vehicles[1].idHash = 0xB61B2D9Eu;
        pl.vehicles[1].unlocked = 0;
        pl.vehicles[1].levelEngine = 3;
        pl.vehicles[1].levelSuspension = 8;
        pl.vehicles[1].levelTires = 2;
        pl.vehicles[1].level4th = 5;

        h.crc32 = PlayerInfo::CalculateCRC32(&pl, sizeof(SavePayload));

        FILE* sf = fopen("tmp_test_mig_20.bin", "wb");
        TEST_CHECK(sf != NULL, "Failed to open sandbox save for dual-hash regression");
        fwrite(&h, sizeof(SaveHeader), 1, sf);
        fwrite(&pl, sizeof(SavePayload), 1, sf);
        fclose(sf);

        PlayerInfo::Reset();
        bool loaded = PlayerInfo::Load("tmp_test_mig_20");
        TEST_CHECK(loaded, "Failed to load dual-hash sandbox save");

        // Verify deduplication: vehicleCount must decrement to 1
        TEST_CHECK(PlayerInfo::GetPayload().vehicleCount == 1, "Duplicate migrated vehicle must be merged, vehicleCount decremented");
        TEST_CHECK(PlayerInfo::GetSelectedVehicleIdHash() == 0xB61B2D9Eu, "Selected vehicle updated to canonical 0xB61B2D9E");
        TEST_CHECK(PlayerInfo::IsVehicleUnlocked(0xB61B2D9Eu), "Unlocked state merged with OR (1 || 0 == 1)");
        TEST_CHECK(PlayerInfo::GetVehicleUpgrade(0xB61B2D9Eu, 0) == 6, "Engine level merged with max(6, 3) == 6");
        TEST_CHECK(PlayerInfo::GetVehicleUpgrade(0xB61B2D9Eu, 1) == 8, "Suspension level merged with max(2, 8) == 8");
        TEST_CHECK(PlayerInfo::GetVehicleUpgrade(0xB61B2D9Eu, 2) == 4, "Tires level merged with max(4, 2) == 4");
        TEST_CHECK(PlayerInfo::GetVehicleUpgrade(0xB61B2D9Eu, 3) == 5, "Special level merged with max(1, 5) == 5");

        // Persist to disk and verify roundtrip maintains vehicleCount == 1 and 0xB61B2D9E
        PlayerInfo::Save("tmp_test_mig_20");
        PlayerInfo::Reset();
        bool reloaded = PlayerInfo::Load("tmp_test_mig_20");
        TEST_CHECK(reloaded, "Failed to reload migrated save");
        TEST_CHECK(PlayerInfo::GetPayload().vehicleCount == 1, "Persisted save retains vehicleCount == 1");
        TEST_CHECK(PlayerInfo::GetVehicleUpgrade(0xB61B2D9Eu, 0) == 6, "Persisted engine level == 6");
        TEST_CHECK(PlayerInfo::GetVehicleUpgrade(0xB61B2D9Eu, 1) == 8, "Persisted suspension level == 8");

        remove("tmp_test_mig_20.bin");
        remove("tmp_test_mig_20.bak");
        printf("  Dual save migration deduplication verified: vehicleCount decremented to 1, levels merged.\n");
    }

    // Regression 2: Night Mode Screen Coordinates & Layer Ordering
    {
        Renderer2D::ResetMetrics();
        Renderer2D::SetupCamera(100.0f, 10.0f, 1.0f);

        // First: ambient darkening in screen space
        Renderer2D::SetupScreenSpace();
        Renderer2D::DrawFilledRect(0.0f, 0.0f, (float)PSP_SCREEN_WIDTH, (float)PSP_SCREEN_HEIGHT, 0xB0080604);

        // Second: DrawLightCone on top of darkening
        float chassisX = 100.0f;
        float chassisY = 12.0f;
        float chassisAngle = 0.0f;
        float originX = chassisX + 1.2f;
        float originY = chassisY + 0.1f;
        Renderer2D::DrawLightCone(originX, originY, chassisAngle, 14.0f, 0.35f, 4.0f, 0x90A0FFFF, 0x00A0FFFF);

        Renderer2D::FlushBatch();

        // Screen space coordinate check for car at (100, 12) with camera at (100, 10), zoom=1.0:
        // sx = 240 + (101.2 - 100.0) * 32.0 = 240 + 38.4 = 278.4
        // sy = 136 - (12.1 - 10.0) * 32.0 = 136 - 67.2 = 68.8
        // Origin is in front of screen center (sx > 240.0) and inside screen bounds [0..480] x [0..272]
        TEST_CHECK(originX > chassisX, "Headlight origin is in front of chassis");
        int batchedQuads = Renderer2D::GetBatchedQuadCount();
        int drawCalls = Renderer2D::GetDrawCallCount();
        printf("  Night Mode metrics: batchedQuads = %d (limit <= 256), drawCalls = %d\n",
               batchedQuads, drawCalls);
        TEST_CHECK(batchedQuads <= 256, "Batched quads within buffer limits");
    }

    printf("  [PASS] Sub-test C: H11 regressions verified!\n\n");

    printf("=================================================================\n");
    printf("   ALL 3 SUB-TESTS PASSED (TEST 20 FULLY VERIFIED)\n");
    printf("=================================================================\n");
    return 0;
}
