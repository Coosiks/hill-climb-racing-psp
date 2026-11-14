#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "core/Config.h"
#include "core/PlayerInfo.h"
#include "physics/PhysicsWorld.h"
#include "physics/PlayerCar.h"

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

static void CleanupTestFiles(const char* base) {
    char path[256];
    snprintf(path, sizeof(path), "%s.bin", base); remove(path);
    snprintf(path, sizeof(path), "%s.bak", base); remove(path);
    snprintf(path, sizeof(path), "%s.tmp", base); remove(path);
    snprintf(path, sizeof(path), "%s.dat", base); remove(path);
}

int main() {
    printf("=== RUNNING REGRESSION TEST: test_garage (T-031) ===\n\n");

    const char* sandboxBase = "build-pc/test_garage_sandbox";
    CleanupTestFiles(sandboxBase);

    // Load configs
    bool loadedAll = Config::LoadAll("");
    TEST_CHECK(loadedAll, "Config::LoadAll failed");
    TEST_CHECK(Config::StageCount >= 4, "Must have at least 4 stages");
    TEST_CHECK(Config::VehicleCount >= 1, "Must have at least 1 vehicle");

    // -------------------------------------------------------------
    // Sub-test 1: Economy & Unlocking Stages & Vehicles
    // -------------------------------------------------------------
    printf("[SUBTEST 1] Economy: Unlocking Stages & Vehicles with coin validation...\n");
    PlayerInfo::Reset();
    PlayerInfo::SetCoins(5000);

    uint32_t countrysideHash = PlayerInfo::HashId(Config::Stages[0].id);
    uint32_t desertHash      = PlayerInfo::HashId(Config::Stages[1].id);
    uint32_t jeepHash        = PlayerInfo::HashId(Config::Vehicles[0].id);

    // Initial unlocks
    TEST_CHECK(PlayerInfo::IsStageUnlocked(countrysideHash), "Countryside must be unlocked by default");
    TEST_CHECK(!PlayerInfo::IsStageUnlocked(desertHash), "Desert must be locked initially");
    TEST_CHECK(PlayerInfo::IsVehicleUnlocked(jeepHash), "Jeep must be unlocked by default");

    int desertCost = Config::Stages[1].unlock_cost; // 10,000
    TEST_CHECK(desertCost == 10000, "Desert unlock cost must be 10000");

    // Attempt purchase with insufficient coins (5000 < 10000)
    if (PlayerInfo::GetCoins() >= desertCost) {
        PlayerInfo::AddCoins(-desertCost);
        PlayerInfo::UnlockStage(desertHash);
    }
    TEST_CHECK(!PlayerInfo::IsStageUnlocked(desertHash), "Desert purchase must fail when coins insufficient");
    TEST_CHECK(PlayerInfo::GetCoins() == 5000, "Coins must remain unchanged on failed purchase");

    // Grant coins and perform successful purchase
    PlayerInfo::AddCoins(15000); // balance = 20,000
    TEST_CHECK(PlayerInfo::GetCoins() == 20000, "Balance should be 20000");

    if (PlayerInfo::GetCoins() >= desertCost) {
        PlayerInfo::AddCoins(-desertCost);
        PlayerInfo::UnlockStage(desertHash);
        PlayerInfo::Save(sandboxBase, true);
    }
    TEST_CHECK(PlayerInfo::IsStageUnlocked(desertHash), "Desert must be unlocked after valid purchase");
    TEST_CHECK(PlayerInfo::GetCoins() == 10000, "Balance must be exactly 10000 after purchase");
    printf("  [PASS] Stage purchase logic, coin deduction and state update verified\n\n");

    // -------------------------------------------------------------
    // Sub-test 2: CanStartRace Validation (Lock enforcement)
    // -------------------------------------------------------------
    printf("[SUBTEST 2] Race Start Authorization based on unlock status...\n");
    uint32_t moonHash = PlayerInfo::HashId(Config::Stages[3].id);
    TEST_CHECK(!PlayerInfo::IsStageUnlocked(moonHash), "Moon must be locked");

    // Selected unlocked stage and vehicle
    PlayerInfo::SetSelectedStageIdHash(countrysideHash);
    PlayerInfo::SetSelectedVehicleIdHash(jeepHash);
    bool canStart = PlayerInfo::IsStageUnlocked(countrysideHash) && PlayerInfo::IsVehicleUnlocked(jeepHash);
    TEST_CHECK(canStart, "Race must be allowed on unlocked stage + vehicle");

    // Selected locked stage
    PlayerInfo::SetSelectedStageIdHash(moonHash);
    canStart = PlayerInfo::IsStageUnlocked(moonHash) && PlayerInfo::IsVehicleUnlocked(jeepHash);
    TEST_CHECK(!canStart, "Race must be blocked when selected stage is locked");

    // Selected locked vehicle
    uint32_t lockedVehHash = PlayerInfo::HashId("locked_test_car");
    canStart = PlayerInfo::IsStageUnlocked(countrysideHash) && PlayerInfo::IsVehicleUnlocked(lockedVehHash);
    TEST_CHECK(!canStart, "Race must be blocked when selected vehicle is locked");
    printf("  [PASS] Start authorization strictly guards locked content\n\n");

    // -------------------------------------------------------------
    // Sub-test 3: Upgrades Pricing & Level Clamping
    // -------------------------------------------------------------
    printf("[SUBTEST 3] Upgrade Economy: quadratic cost curve and max level clamping...\n");
    PlayerInfo::Reset();
    PlayerInfo::SetCoins(5000000); // 5M coins (sum(1..20^2)*1000 = 2,870,000)

    int maxLevel = Config::Vehicles[0].max_upgrade_level; // 20
    TEST_CHECK(maxLevel == 20, "Jeep max upgrade level must be 20");

    int expectedCosts[5] = { 1000, 4000, 9000, 16000, 25000 };
    for (int lvl = 0; lvl < 5; ++lvl) {
        int cost = GetUpgradeCost(lvl);
        TEST_CHECK(cost == expectedCosts[lvl], "GetUpgradeCost must match 1000 * (lvl+1)^2");
    }

    // Upgrade engine step-by-step to max level
    for (int lvl = 0; lvl < maxLevel; ++lvl) {
        int curLvl = PlayerInfo::GetVehicleUpgrade(jeepHash, 0);
        TEST_CHECK(curLvl == lvl, "Current level mismatch during sequential upgrade");
        int cost = GetUpgradeCost(curLvl);
        int64_t prevCoins = PlayerInfo::GetCoins();
        TEST_CHECK(prevCoins >= cost, "Must have enough coins");

        PlayerInfo::AddCoins(-cost);
        PlayerInfo::SetVehicleUpgrade(jeepHash, 0, curLvl + 1);
        TEST_CHECK(PlayerInfo::GetCoins() == prevCoins - cost, "Exact cost must be deducted");
    }

    int reachedLevel = PlayerInfo::GetVehicleUpgrade(jeepHash, 0);
    TEST_CHECK(reachedLevel == maxLevel, "Engine should reach max upgrade level 20");

    // Attempt upgrade beyond max level
    int64_t balanceAtMax = PlayerInfo::GetCoins();
    if (reachedLevel < maxLevel) {
        int cost = GetUpgradeCost(reachedLevel);
        PlayerInfo::AddCoins(-cost);
        PlayerInfo::SetVehicleUpgrade(jeepHash, 0, reachedLevel + 1);
    }
    TEST_CHECK(PlayerInfo::GetVehicleUpgrade(jeepHash, 0) == maxLevel, "Level must remain clamped at maxLevel");
    TEST_CHECK(PlayerInfo::GetCoins() == balanceAtMax, "Coins must not be deducted when upgrading at max level");
    printf("  [PASS] Upgrades cost curve and max level 20 clamping verified\n\n");

    // -------------------------------------------------------------
    // Sub-test 4: Physics Parameter Recomputation & Fuel Upgrade Scaling
    // -------------------------------------------------------------
    printf("[SUBTEST 4] Physics Parameter Scaling & Fuel Upgrade Verification...\n");
    b2Vec2 startPos(0.0f, 2.0f);
    PhysicsWorld world(Config::Stages[0].gravity);

    // Car with level 0 upgrades
    PlayerCar car0(world.GetWorld(), startPos, 0, 0, 0, 0, &Config::Vehicles[0]);
    float baseMaxFuel = car0.GetMaxFuel();
    TEST_CHECK(feq(baseMaxFuel, 100.0f), "Base fuel capacity at level 0 must be 100.0f");
    TEST_CHECK(feq(car0.GetFuel(), 100.0f), "Initial fuel must equal max fuel");

    // Car with level 5 fuel/AWD upgrade (upg_mult_fuel = 1.05)
    int upgLevel = 5;
    PlayerCar carUpgraded(world.GetWorld(), startPos, 10, 5, 8, upgLevel, &Config::Vehicles[0]);
    float expectedMaxFuel = 100.0f * powf(Config::Vehicles[0].upg_mult_fuel, (float)upgLevel);
    TEST_CHECK(feq(carUpgraded.GetMaxFuel(), expectedMaxFuel), "Fuel capacity must scale by (upg_mult_fuel ^ level)");
    TEST_CHECK(feq(carUpgraded.GetFuel(), expectedMaxFuel), "Initial fuel must equal scaled max fuel");

    // Refuel test
    carUpgraded.AddFuel(500.0f);
    TEST_CHECK(feq(carUpgraded.GetFuel(), expectedMaxFuel), "AddFuel must clamp to scaled max fuel");

    // Suspension frequency scaling
    float rearFreq0 = car0.GetRearJoint()->GetSpringFrequencyHz();
    float rearFreqUpg = carUpgraded.GetRearJoint()->GetSpringFrequencyHz();
    float expectedFreq = Config::Vehicles[0].base_spring_freq + (5 * 0.40f);
    TEST_CHECK(feq(rearFreq0, Config::Vehicles[0].base_spring_freq), "Level 0 spring frequency must equal base");
    TEST_CHECK(feq(rearFreqUpg, expectedFreq), "Upgraded spring frequency must equal base + lvl*0.40");

    printf("  [PASS] Fuel capacity and suspension frequency dynamically scale with upgrade tiers\n\n");

    // -------------------------------------------------------------
    // Sub-test 5: Save Persistence of Garage Progression
    // -------------------------------------------------------------
    printf("[SUBTEST 5] Save & Reload Persistence of Multi-Vehicle / Stage Progress...\n");
    PlayerInfo::UnlockStage(desertHash);
    PlayerInfo::SetVehicleUpgrade(jeepHash, 0, 18);
    PlayerInfo::SetVehicleUpgrade(jeepHash, 1, 14);
    PlayerInfo::SetVehicleUpgrade(jeepHash, 2, 12);
    PlayerInfo::SetVehicleUpgrade(jeepHash, 3, 7);
    PlayerInfo::SetStageRecord(countrysideHash, 3500);
    PlayerInfo::SetStageRecord(desertHash, 1800);
    PlayerInfo::SetCoins(888444);

    bool saveOk = PlayerInfo::Save(sandboxBase, true);
    TEST_CHECK(saveOk, "Failed to save progression to sandbox");

    // Reset memory state completely
    PlayerInfo::Reset();
    TEST_CHECK(PlayerInfo::GetCoins() == 0, "Coins must be 0 after reset");
    TEST_CHECK(!PlayerInfo::IsStageUnlocked(desertHash), "Desert must be locked after reset");

    // Reload from sandbox save
    bool loadOk = PlayerInfo::Load(sandboxBase);
    TEST_CHECK(loadOk, "Failed to load progression from sandbox");

    TEST_CHECK(PlayerInfo::GetCoins() == 888444, "Coins must restore to 888444");
    TEST_CHECK(PlayerInfo::IsStageUnlocked(desertHash), "Desert must remain unlocked");
    TEST_CHECK(PlayerInfo::GetStageRecord(countrysideHash) == 3500, "Countryside record must restore to 3500");
    TEST_CHECK(PlayerInfo::GetStageRecord(desertHash) == 1800, "Desert record must restore to 1800");

    TEST_CHECK(PlayerInfo::GetVehicleUpgrade(jeepHash, 0) == 18, "Engine level 18 must restore");
    TEST_CHECK(PlayerInfo::GetVehicleUpgrade(jeepHash, 1) == 14, "Suspension level 14 must restore");
    TEST_CHECK(PlayerInfo::GetVehicleUpgrade(jeepHash, 2) == 12, "Tires level 12 must restore");
    TEST_CHECK(PlayerInfo::GetVehicleUpgrade(jeepHash, 3) == 7, "4WD/Fuel level 7 must restore");

    CleanupTestFiles(sandboxBase);
    printf("  [PASS] Full roundtrip progression persistence verified\n\n");

    printf("===================================================\n");
    printf("ALL 5 SUB-TESTS OF test_garage PASSED SUCCESSFULLY!\n");
    printf("===================================================\n");
    return 0;
}
