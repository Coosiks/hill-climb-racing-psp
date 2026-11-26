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

int main() {
    printf("=================================================================\n");
    printf("   HCR Regression Test 19: Roster 18 & Night Mode (T-125/T-126)  \n");
    printf("=================================================================\n\n");

    // -------------------------------------------------------------
    // Sub-test A: 18 Stages Verification & Heightfield Determinism
    // -------------------------------------------------------------
    printf("[Sub-test A] Verifying 18 stages configuration and heightfield determinism...\n");
    bool loadedStages = Config::LoadStages("config/stages.cfg");
    if (!loadedStages) loadedStages = Config::LoadStages("data/stages.cfg");
    TEST_CHECK(loadedStages, "Failed to load stages.cfg");
    TEST_CHECK(Config::StageCount >= 18, "Must load at least 18 stages from stages.cfg");

    const char* expectedStageIds[18] = {
        "countryside", "desert", "arctic", "moon",
        "highway", "cave", "mars", "forest",
        "mountain", "mudpool", "volcano", "beach",
        "rollercoaster", "night", "rooftops", "junkyard",
        "construction", "rainbow"
    };
    int expectedStageCosts[18] = {
        0, 35000, 75000, 175000,
        125000, 150000, 250000, 850000,
        850000, 1000000, 1000000, 1000000,
        1200000, 1200000, 1200000, 1400000,
        1400000, 1500000
    };

    uint32_t stageHashes[18];
    for (int i = 0; i < 18; ++i) {
        const StageConfig& st = Config::Stages[i];
        TEST_CHECK(strcmp(st.id, expectedStageIds[i]) == 0, "Stage ID matches expected");
        TEST_CHECK(st.unlock_cost == expectedStageCosts[i], "Stage unlock cost matches 1.30.0 provenance");
        stageHashes[i] = PlayerInfo::HashId(st.id);
        printf("  Stage [%2d]: %-14s (hash 0x%08X, cost %7d, night=%d, bridges=%d, mud=%d)\n",
               i, st.id, stageHashes[i], st.unlock_cost, st.is_night, st.has_bridges, st.has_mud_pools);
    }

    // Verify all 18 stage hashes are strictly unique
    for (int i = 0; i < 18; ++i) {
        for (int j = i + 1; j < 18; ++j) {
            TEST_CHECK(stageHashes[i] != stageHashes[j], "Stage hashes must be unique");
        }
    }

    // Bit-exact canonical heightfield hashes for all 18 stages (4001 points, 0..2000m, step 0.5m)
    // 7 baselines (Countryside, Desert, Arctic, Moon, Cave, Mars, Beach) strictly bit-exact.
    const uint64_t CANONICAL_STAGE_HASHES[18] = {
        0x6e83945913e2d687ULL, // 0: Countryside (Frozen baseline)
        0x6dfb669a32592de7ULL, // 1: Desert (Frozen baseline)
        0xf7cc9dd1f3aa54b0ULL, // 2: Arctic (Frozen baseline)
        0x0083f35054ddbad2ULL, // 3: Moon (Frozen baseline)
        0x38177290f869560bULL, // 4: Highway
        0x17c907bfa1af843eULL, // 5: Cave
        0x83f9140086a76014ULL, // 6: Mars
        0x18f4e37021de766bULL, // 7: Forest
        0x09474c26a03555f7ULL, // 8: Mountain
        0xb157f101e6988ddeULL, // 9: Mudpool
        0x399eed0c879f0b1bULL, // 10: Volcano
        0xd920a446c0299459ULL, // 11: Beach
        0x08282fbda9ed6b43ULL, // 12: Rollercoaster
        0x0b90766649f62ed9ULL, // 13: Night
        0x827f20352b8b6f8bULL, // 14: Rooftops
        0x4cf29d89406e73c4ULL, // 15: Junkyard
        0x53ed7a75b127d093ULL, // 16: Construction
        0x7fb3da7c76ddd61fULL  // 17: Rainbow
    };

    const int NUM_POINTS = 4001; // 0 to 2000m with step 0.5m
    float* heightBuffer = (float*)malloc(NUM_POINTS * sizeof(float));
    TEST_CHECK(heightBuffer != NULL, "Failed to allocate height buffer");

    b2World dummyWorld(b2Vec2(0.0f, -10.0f));
    for (int s = 0; s < 18; ++s) {
        Terrain terrain(&dummyWorld, (StageType)s);
        for (int i = 0; i < NUM_POINTS; ++i) {
            float x = (float)i * 0.5f;
            heightBuffer[i] = terrain.GetHeightAtX(x);
        }
        uint64_t h = FNV1a_64(heightBuffer, NUM_POINTS * sizeof(float));
        TEST_CHECK(h == CANONICAL_STAGE_HASHES[s], "Stage height hash mismatch! Non-regression violated!");
        bool isBaseline = (s == 0 || s == 1 || s == 2 || s == 3 || s == 5 || s == 6 || s == 11);
        printf("  Stage %-14s [%2d]: hash 0x%016llx -> %s\n",
               Config::Stages[s].id, s, (unsigned long long)h,
               isBaseline ? "100% BIT-EXACT BASELINE MATCH" : "DETERMINISTIC CANONICAL MATCH");
    }
    free(heightBuffer);

    // Stage feature flags assertions
    TEST_CHECK(Config::Stages[STAGE_NIGHT].is_night == 1, "Night stage must have is_night == 1");
    for (int s = 0; s < 18; ++s) {
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

    TEST_CHECK(Config::Stages[STAGE_CAVE].has_ceiling == 1, "Cave must have ceiling");
    TEST_CHECK(feq(Config::Stages[STAGE_RAINBOW].gravity, -6.0f), "Rainbow gravity must be -6.0");

    printf("  [PASS] Sub-test A: 18 stages, bit-exact baselines, canonical hashes and stage flags verified!\n\n");

    // -------------------------------------------------------------
    // Sub-test B: 18 Vehicles, Physics Properties & Save Migration
    // -------------------------------------------------------------
    printf("[Sub-test B] Verifying 18 vehicles configuration, Box2D physics & migration...\n");
    bool loadedVehicles = Config::LoadVehicles("config/vehicles.cfg");
    if (!loadedVehicles) loadedVehicles = Config::LoadVehicles("data/vehicles.cfg");
    TEST_CHECK(loadedVehicles, "Failed to load vehicles.cfg");
    TEST_CHECK(Config::VehicleCount >= 18, "Must load at least 18 vehicles from vehicles.cfg");

    const char* expectedVehIds[18] = {
        "jeep", "motocross", "monster_truck", "race_car",
        "tractor", "hippievan", "quadbike", "touristbus",
        "onewheeler", "screwfinger", "policecar", "ambulance",
        "electric", "dieseltruck", "rallycar", "dragster",
        "firetruck", "tank"
    };
    int expectedVehCosts[18] = {
        0, 75000, 100000, 250000,
        100000, 120000, 175000, 200000,
        150000, 300000, 350000, 400000,
        450000, 500000, 750000, 800000,
        800000, 1000000
    };
    int expectedFourthTypes[18] = {
        0, 2, 0, 3, // jeep (4WD), motocross (AIR), monster (4WD), race_car (DOWNFORCE)
        4, 5, 0, 0, // tractor (STABILITY), hippievan (BOOST), quadbike (4WD), touristbus (4WD)
        4, 0, 5, 4, // onewheeler (STABILITY), screwfinger (4WD), policecar (BOOST), ambulance (STABILITY)
        1, 0, 3, 3, // electric (FUEL), dieseltruck (4WD), rallycar (DOWNFORCE), dragster (DOWNFORCE)
        0, 0        // firetruck (4WD), tank (4WD)
    };

    uint32_t vehHashes[18];
    for (int i = 0; i < 18; ++i) {
        const VehicleConfig& vc = Config::Vehicles[i];
        TEST_CHECK(strcmp(vc.id, expectedVehIds[i]) == 0, "Vehicle ID matches expected");
        TEST_CHECK(vc.unlock_cost == expectedVehCosts[i], "Vehicle unlock cost matches 1.30.0 provenance");
        TEST_CHECK(vc.fourth_upgrade_type == expectedFourthTypes[i], "Vehicle 4th upgrade slot matches specification");
        vehHashes[i] = PlayerInfo::HashId(vc.id);
        printf("  Vehicle [%2d]: %-14s (hash 0x%08X, cost %7d, 4th slot=%d)\n",
               i, vc.id, vehHashes[i], vc.unlock_cost, vc.fourth_upgrade_type);
    }

    // Verify all 18 vehicle hashes are strictly unique
    for (int i = 0; i < 18; ++i) {
        for (int j = i + 1; j < 18; ++j) {
            TEST_CHECK(vehHashes[i] != vehHashes[j], "Vehicle hashes must be unique");
        }
    }

    // Dragster wheel radius asymmetry assertion
    const VehicleConfig* dragster = Config::FindVehicleById("dragster");
    TEST_CHECK(dragster != NULL, "Dragster vehicle config exists");
    TEST_CHECK(dragster->rear_wheel_radius > dragster->front_wheel_radius * 1.4f,
               "Dragster must have asymmetrical wheels (R_rear > 1.4 * R_front)");
    printf("  Dragster wheel asymmetry: R_front=%.2fm, R_rear=%.2fm (ratio=%.2fx > 1.40x)\n",
           dragster->front_wheel_radius, dragster->rear_wheel_radius,
           dragster->rear_wheel_radius / dragster->front_wheel_radius);

    // Tank heavy chassis assertion
    const VehicleConfig* tank = Config::FindVehicleById("tank");
    TEST_CHECK(tank != NULL, "Tank vehicle config exists");
    b2World vehWorld(b2Vec2(0.0f, -9.8f));
    {
        PlayerCar carTank(&vehWorld, b2Vec2(0.0f, 2.0f), 0, 0, 0, 0, tank);
        TEST_CHECK(carTank.GetChassisBody()->GetMass() > 5.0f, "Tank chassis mass must be heavy (> 5.0 kg)");
        TEST_CHECK(tank->drive_type == 2, "Tank drive type == 2 (4WD)");
    }

    // Verify fingerscrew -> screwfinger save migration in PlayerInfo
    const uint32_t OLD_FINGERSCREW_HASH = 0xDC088A10u;
    const uint32_t NEW_SCREWFINGER_HASH = 0xB61B2D9Eu;
    TEST_CHECK(vehHashes[9] == NEW_SCREWFINGER_HASH, "screwfinger hash must be 0xB61B2D9E");

    // Clean up test save sandbox
    remove("tmp_test_mig_19.bin");
    remove("tmp_test_mig_19.bak");
    SaveHeader h;
    memset(&h, 0, sizeof(h));
    memcpy(h.magic, "HCRS", 4);
    h.version = 1;
    h.payloadSize = sizeof(SavePayload);

    SavePayload pl;
    memset(&pl, 0, sizeof(pl));
    pl.coins = 250000;
    pl.selectedVehicleIdHash = OLD_FINGERSCREW_HASH;
    pl.vehicleCount = 1;
    pl.vehicles[0].idHash = OLD_FINGERSCREW_HASH;
    pl.vehicles[0].unlocked = 1;
    pl.vehicles[0].levelEngine = 5;
    h.crc32 = PlayerInfo::CalculateCRC32(&pl, sizeof(SavePayload));

    FILE* sf = fopen("tmp_test_mig_19.bin", "wb");
    TEST_CHECK(sf != NULL, "Failed to open sandbox save");
    fwrite(&h, sizeof(SaveHeader), 1, sf);
    fwrite(&pl, sizeof(SavePayload), 1, sf);
    fclose(sf);

    PlayerInfo::Reset();
    bool loadedMig = PlayerInfo::Load("tmp_test_mig_19");
    TEST_CHECK(loadedMig, "Failed to load sandbox save for migration");
    TEST_CHECK(PlayerInfo::GetSelectedVehicleIdHash() == NEW_SCREWFINGER_HASH, "selected vehicle migrated to 0xB61B2D9E");
    TEST_CHECK(PlayerInfo::IsVehicleUnlocked(NEW_SCREWFINGER_HASH), "screwfinger must be unlocked");
    TEST_CHECK(PlayerInfo::GetVehicleUpgrade(NEW_SCREWFINGER_HASH, 0) == 5, "screwfinger engine level preserved");
    remove("tmp_test_mig_19.bin");
    remove("tmp_test_mig_19.bak");

    printf("  [PASS] Sub-test B: 18 vehicles, Dragster asymmetry, Tank mass & save migration verified!\n\n");

    // -------------------------------------------------------------
    // Sub-test C: Night Mode Rendering & Batch Limit (256 Quads)
    // -------------------------------------------------------------
    printf("[Sub-test C] Verifying Night Mode mechanics and Renderer2D batch limits...\n");

    // Reset metrics before rendering simulation
    Renderer2D::ResetMetrics();

    // Setup camera in world space
    Renderer2D::SetupCamera(100.0f, 10.0f, 1.0f);

    // Simulate Night Mode render sequence:
    // 1. Headlight cone angled with chassis (world space)
    float chassisX = 100.0f;
    float chassisY = 12.0f;
    float chassisAngle = 0.18f; // ~10 degrees pitch
    float cosA = cosf(chassisAngle);
    float sinA = sinf(chassisAngle);
    float headlightOriginX = chassisX + cosA * 1.2f - sinA * 0.1f;
    float headlightOriginY = chassisY + sinA * 1.2f + cosA * 0.1f;
    Renderer2D::DrawLightCone(headlightOriginX, headlightOriginY, chassisAngle, 14.0f, 0.35f, 4.0f, 0x90A0FFFF, 0x00A0FFFF);

    // 2. Ambient darkening in screen space
    Renderer2D::SetupScreenSpace();
    Renderer2D::DrawFilledRect(0.0f, 0.0f, (float)PSP_SCREEN_WIDTH, (float)PSP_SCREEN_HEIGHT, 0xB0080604);

    // 3. Flush batch
    Renderer2D::FlushBatch();

    int batchedQuads = Renderer2D::GetBatchedQuadCount();
    printf("  Night Mode simulation: Batched quad count = %d (limit = 256), Draw calls = %d\n",
           batchedQuads, Renderer2D::GetDrawCallCount());

    TEST_CHECK(batchedQuads <= 256, "Batched quad count must stay within static batcher capacity (<= 256)");
    printf("  [PASS] Sub-test C: Night Mode headlight cone & ambient darkening verified within batch limits!\n\n");

    printf("=================================================================\n");
    printf("   ALL 3 SUB-TESTS PASSED (TEST 19 / T-125 / T-126 FULLY VERIFIED)\n");
    printf("=================================================================\n");
    return 0;
}

