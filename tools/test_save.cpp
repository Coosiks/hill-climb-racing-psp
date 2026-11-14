#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "core/PlayerInfo.h"

static void CleanupTestFiles(const char* base) {
    char path[256];
    snprintf(path, sizeof(path), "%s.bin", base); remove(path);
    snprintf(path, sizeof(path), "%s.bak", base); remove(path);
    snprintf(path, sizeof(path), "%s.tmp", base); remove(path);
    snprintf(path, sizeof(path), "%s.dat", base); remove(path);
}

int main() {
    printf("=== RUNNING REGRESSION TEST: test_save (T-030) ===\n\n");

    // -------------------------------------------------------------
    // Sub-test 1: CRC32 IEEE 802.3 Standard Test Vector
    // -------------------------------------------------------------
    printf("[SUBTEST 1] CRC32 standard test vector verification...\n");
    const char* testVector = "123456789";
    uint32_t expectedCrc = 0xCBF43926u;
    uint32_t computedCrc = PlayerInfo::CalculateCRC32(testVector, strlen(testVector));
    printf("  Input: \"%s\" (len=%zu)\n", testVector, strlen(testVector));
    printf("  Expected CRC32: 0x%08X | Computed CRC32: 0x%08X\n", expectedCrc, computedCrc);
    assert(computedCrc == expectedCrc);
    printf("  -> SUBTEST 1 PASSED\n\n");

    // -------------------------------------------------------------
    // Sub-test 2: Strict Binary Alignment & Struct Size Assertion
    // -------------------------------------------------------------
    printf("[SUBTEST 2] MIPS Allegrex 4-byte natural alignment & struct sizes...\n");
    printf("  sizeof(SaveHeader):        %zu bytes (expected 16, mod 4 = %zu)\n",
           sizeof(SaveHeader), sizeof(SaveHeader) % 4);
    printf("  sizeof(SavedVehicleEntry): %zu bytes (expected 12, mod 4 = %zu)\n",
           sizeof(SavedVehicleEntry), sizeof(SavedVehicleEntry) % 4);
    printf("  sizeof(SavedStageEntry):   %zu bytes (expected 12, mod 4 = %zu)\n",
           sizeof(SavedStageEntry), sizeof(SavedStageEntry) % 4);
    printf("  sizeof(SavePayload):       %zu bytes (expected 1556, mod 4 = %zu)\n",
           sizeof(SavePayload), sizeof(SavePayload) % 4);

    assert(sizeof(SaveHeader) == 16);
    assert(sizeof(SaveHeader) % 4 == 0);
    assert(sizeof(SavedVehicleEntry) == 12);
    assert(sizeof(SavedVehicleEntry) % 4 == 0);
    assert(sizeof(SavedStageEntry) == 12);
    assert(sizeof(SavedStageEntry) % 4 == 0);
    assert(sizeof(SavePayload) == 1556);
    assert(sizeof(SavePayload) % 4 == 0);

    // Verify critical field offsets
    assert(offsetof(SavePayload, coins) == 0);
    assert(offsetof(SavePayload, selectedVehicleIdHash) == 8);
    assert(offsetof(SavePayload, selectedStageIdHash) == 12);
    assert(offsetof(SavePayload, vehicleCount) == 16);
    assert(offsetof(SavePayload, stageCount) == 18);
    assert(offsetof(SavePayload, vehicles) == 20);
    assert(offsetof(SavePayload, stages) == 788);

    assert(offsetof(SavedVehicleEntry, idHash) == 0);
    assert(offsetof(SavedVehicleEntry, unlocked) == 4);
    assert(offsetof(SavedVehicleEntry, levelEngine) == 5);
    assert(offsetof(SavedVehicleEntry, levelSuspension) == 6);
    assert(offsetof(SavedVehicleEntry, levelTires) == 7);
    assert(offsetof(SavedVehicleEntry, level4th) == 8);
    assert(offsetof(SavedVehicleEntry, reserved) == 9);

    assert(offsetof(SavedStageEntry, idHash) == 0);
    assert(offsetof(SavedStageEntry, unlocked) == 4);
    assert(offsetof(SavedStageEntry, reserved) == 5);
    assert(offsetof(SavedStageEntry, bestDistance) == 8);
    printf("  -> SUBTEST 2 PASSED\n\n");

    const char* testBase = "build-pc/test_save_sandbox";
    CleanupTestFiles(testBase);

    // -------------------------------------------------------------
    // Sub-test 3: Roundtrip Serialization / Deserialization
    // -------------------------------------------------------------
    printf("[SUBTEST 3] Roundtrip Serialization & Deserialization...\n");
    PlayerInfo::Reset();
    PlayerInfo::SetCoins(88442200);

    uint32_t jeepHash = PlayerInfo::HashId("jeep");
    uint32_t bikeHash = PlayerInfo::HashId("motobike");
    uint32_t monsterHash = PlayerInfo::HashId("monstertruck");

    PlayerInfo::UnlockVehicle(jeepHash);
    PlayerInfo::SetVehicleUpgrade(jeepHash, 0, 15); // Engine
    PlayerInfo::SetVehicleUpgrade(jeepHash, 1, 10); // Suspension
    PlayerInfo::SetVehicleUpgrade(jeepHash, 2, 8);  // Tires
    PlayerInfo::SetVehicleUpgrade(jeepHash, 3, 5);  // AWD/4th

    PlayerInfo::UnlockVehicle(bikeHash);
    PlayerInfo::SetVehicleUpgrade(bikeHash, 0, 20);
    PlayerInfo::SetVehicleUpgrade(bikeHash, 1, 12);

    PlayerInfo::UnlockVehicle(monsterHash);
    PlayerInfo::SetVehicleUpgrade(monsterHash, 2, 18);

    uint32_t desertHash = PlayerInfo::HashId("desert");
    uint32_t moonHash = PlayerInfo::HashId("moon");
    PlayerInfo::UnlockStage(desertHash);
    PlayerInfo::SetStageRecord(desertHash, 1420);
    PlayerInfo::UnlockStage(moonHash);
    PlayerInfo::SetStageRecord(moonHash, 3250);

    PlayerInfo::SetSelectedVehicleIdHash(bikeHash);
    PlayerInfo::SetSelectedStageIdHash(moonHash);

    bool saveOk = PlayerInfo::Save(testBase, true);
    assert(saveOk);
    printf("  Save written successfully to %s.bin\n", testBase);

    // Reset memory state completely
    PlayerInfo::Reset();
    assert(PlayerInfo::GetCoins() == 0);
    assert(PlayerInfo::GetSelectedVehicleIdHash() != bikeHash);

    // Reload from disk
    bool loadOk = PlayerInfo::Load(testBase);
    assert(loadOk);
    printf("  Save loaded successfully from %s.bin\n", testBase);

    assert(PlayerInfo::GetCoins() == 88442200);
    assert(PlayerInfo::GetSelectedVehicleIdHash() == bikeHash);
    assert(PlayerInfo::GetSelectedStageIdHash() == moonHash);

    assert(PlayerInfo::IsVehicleUnlocked(jeepHash));
    assert(PlayerInfo::GetVehicleUpgrade(jeepHash, 0) == 15);
    assert(PlayerInfo::GetVehicleUpgrade(jeepHash, 1) == 10);
    assert(PlayerInfo::GetVehicleUpgrade(jeepHash, 2) == 8);
    assert(PlayerInfo::GetVehicleUpgrade(jeepHash, 3) == 5);

    assert(PlayerInfo::IsVehicleUnlocked(bikeHash));
    assert(PlayerInfo::GetVehicleUpgrade(bikeHash, 0) == 20);
    assert(PlayerInfo::GetVehicleUpgrade(bikeHash, 1) == 12);

    assert(PlayerInfo::IsVehicleUnlocked(monsterHash));
    assert(PlayerInfo::GetVehicleUpgrade(monsterHash, 2) == 18);

    assert(PlayerInfo::IsStageUnlocked(desertHash));
    assert(PlayerInfo::GetStageRecord(desertHash) == 1420);
    assert(PlayerInfo::IsStageUnlocked(moonHash));
    assert(PlayerInfo::GetStageRecord(moonHash) == 3250);
    printf("  -> SUBTEST 3 PASSED\n\n");

    // -------------------------------------------------------------
    // Sub-test 4: 1-Byte Corruption triggers .bak Automatic Recovery
    // -------------------------------------------------------------
    printf("[SUBTEST 4] 1-byte corruption recovery via backup file (.bak)...\n");
    // State 1: 100,000 coins
    PlayerInfo::SetCoins(100000);
    saveOk = PlayerInfo::Save(testBase, true);
    assert(saveOk);

    // State 2: 250,000 coins (this rotation moves state 1 into test_save_sandbox.bak)
    PlayerInfo::SetCoins(250000);
    saveOk = PlayerInfo::Save(testBase, true);
    assert(saveOk);
    assert(PlayerInfo::GetCoins() == 250000);

    // Verify .bak exists and has valid state 1
    char binPath[256], bakPath[256];
    snprintf(binPath, sizeof(binPath), "%s.bin", testBase);
    snprintf(bakPath, sizeof(bakPath), "%s.bak", testBase);

    FILE* bakF = fopen(bakPath, "rb");
    assert(bakF != NULL);
    fclose(bakF);

    // Now corrupt 1 byte in primary .bin file payload
    FILE* binF = fopen(binPath, "r+b");
    assert(binF != NULL);
    fseek(binF, 24, SEEK_SET); // Corrupt byte at offset 24 (inside payload)
    int corruptByte = fgetc(binF) ^ 0xFF;
    fseek(binF, 24, SEEK_SET);
    fputc(corruptByte, binF);
    fclose(binF);
    printf("  Corrupted byte at offset 24 in %s\n", binPath);

    // Reset memory
    PlayerInfo::Reset();
    assert(PlayerInfo::GetCoins() == 0);

    // Load should detect corrupted CRC, fallback to .bak, and recover State 1 (100,000 coins)
    loadOk = PlayerInfo::Load(testBase);
    assert(loadOk);
    printf("  Recovered Coins: %lld (Expected 100000)\n", (long long)PlayerInfo::GetCoins());
    assert(PlayerInfo::GetCoins() == 100000);
    assert(PlayerInfo::IsDirty()); // Must be marked dirty for re-saving
    printf("  -> SUBTEST 4 PASSED\n\n");

    // -------------------------------------------------------------
    // Sub-test 5: Content Preservation across idHash Reordering & Unknown IDs
    // -------------------------------------------------------------
    printf("[SUBTEST 5] Content preservation by FNV-1a idHash with unknown IDs...\n");
    PlayerInfo::Reset();

    // Create unknown ID entries (simulating future content or mods)
    uint32_t futureCarHash1 = PlayerInfo::HashId("future_hovercraft_2030");
    uint32_t futureCarHash2 = PlayerInfo::HashId("future_nuclear_semi");
    uint32_t futureStageHash = PlayerInfo::HashId("stage_mars_colony");

    PlayerInfo::UnlockVehicle(futureCarHash1);
    PlayerInfo::SetVehicleUpgrade(futureCarHash1, 0, 19);
    PlayerInfo::UnlockVehicle(futureCarHash2);
    PlayerInfo::SetVehicleUpgrade(futureCarHash2, 3, 14);
    PlayerInfo::UnlockStage(futureStageHash);
    PlayerInfo::SetStageRecord(futureStageHash, 5500);

    PlayerInfo::Save(testBase, true);

    // Reload
    PlayerInfo::Reset();
    PlayerInfo::Load(testBase);

    assert(PlayerInfo::IsVehicleUnlocked(futureCarHash1));
    assert(PlayerInfo::GetVehicleUpgrade(futureCarHash1, 0) == 19);
    assert(PlayerInfo::IsVehicleUnlocked(futureCarHash2));
    assert(PlayerInfo::GetVehicleUpgrade(futureCarHash2, 3) == 14);
    assert(PlayerInfo::IsStageUnlocked(futureStageHash));
    assert(PlayerInfo::GetStageRecord(futureStageHash) == 5500);
    printf("  Preserved unknown DLC vehicle and stage IDs across reload!\n");
    printf("  -> SUBTEST 5 PASSED\n\n");

    // -------------------------------------------------------------
    // Sub-test 6: Legacy Migration from 'HCR1' save.dat
    // -------------------------------------------------------------
    printf("[SUBTEST 6] Legacy save.dat ('HCR1') automatic migration...\n");
    CleanupTestFiles(testBase);

    // Create a mock legacy save.dat
    char datPath[256];
    snprintf(datPath, sizeof(datPath), "%s.dat", testBase);
    FILE* legacyFile = fopen(datPath, "wb");
    assert(legacyFile != NULL);

    int legacyMagic = 0x48435231; // 'HCR1'
    int legacyCoins = 77123;
    int legacyEng = 12;
    int legacySusp = 8;
    int legacyTires = 14;
    int legacyAwd = 6;
    int legacyScores[4] = { 2450, 1890, 950, 4100 };

    fwrite(&legacyMagic, sizeof(int), 1, legacyFile);
    fwrite(&legacyCoins, sizeof(int), 1, legacyFile);
    fwrite(&legacyEng, sizeof(int), 1, legacyFile);
    fwrite(&legacySusp, sizeof(int), 1, legacyFile);
    fwrite(&legacyTires, sizeof(int), 1, legacyFile);
    fwrite(&legacyAwd, sizeof(int), 1, legacyFile);
    fwrite(legacyScores, sizeof(int), 4, legacyFile);
    fclose(legacyFile);
    printf("  Created mock legacy %s with 77123 coins and custom upgrades\n", datPath);

    // Ensure .bin and .bak do NOT exist
    remove(binPath);
    remove(bakPath);

    // Load should trigger migration!
    PlayerInfo::Reset();
    loadOk = PlayerInfo::Load(testBase);
    assert(loadOk);

    // Verify migrated values in active memory and legacy static variables
    assert(PlayerInfo::GetCoins() == 77123);
    assert(PlayerInfo::Coins == 77123);

    assert(PlayerInfo::EngineLevel == 12);
    assert(PlayerInfo::SuspensionLevel == 8);
    assert(PlayerInfo::TiresLevel == 14);
    assert(PlayerInfo::AWDLevel == 6);

    assert(PlayerInfo::HighScores[0] == 2450);
    assert(PlayerInfo::HighScores[1] == 1890);
    assert(PlayerInfo::HighScores[2] == 950);
    assert(PlayerInfo::HighScores[3] == 4100);

    // Verify that the migration immediately converted and saved as 'HCRS' .bin
    FILE* newBin = fopen(binPath, "rb");
    assert(newBin != NULL);
    fclose(newBin);

    // Remove legacy .dat and re-load from .bin to prove migration persistence
    remove(datPath);
    PlayerInfo::Reset();
    loadOk = PlayerInfo::Load(testBase);
    assert(loadOk);
    assert(PlayerInfo::GetCoins() == 77123);
    assert(PlayerInfo::EngineLevel == 12);
    assert(PlayerInfo::HighScores[3] == 4100);

    printf("  Legacy migration verified and persisted into new 'HCRS' format!\n");
    printf("  -> SUBTEST 6 PASSED\n\n");

    // [SUBTEST 7] Unaligned Buffer Deserialization (MIPS Safety)
    printf("[SUBTEST 7] Unaligned Buffer Deserialization (MIPS Safety)...\n");
    struct SaveFile {
        SaveHeader header;
        SavePayload payload;
    };

    SaveFile sourceSave;
    memset(&sourceSave, 0, sizeof(sourceSave));
    memcpy(sourceSave.header.magic, "HCRS", 4);
    sourceSave.header.version = 1;
    sourceSave.header.payloadSize = sizeof(SavePayload);
    sourceSave.payload.coins = 987654321;
    sourceSave.payload.selectedVehicleIdHash = 0x12345678;
    sourceSave.payload.selectedStageIdHash = 0x87654321;
    sourceSave.payload.vehicleCount = 1;
    sourceSave.payload.vehicles[0].idHash = 0x12345678;
    sourceSave.payload.vehicles[0].unlocked = 1;
    sourceSave.payload.vehicles[0].levelEngine = 15;
    sourceSave.header.crc32 = PlayerInfo::CalculateCRC32(&sourceSave.payload, sizeof(SavePayload));

    alignas(4) uint8_t rawBuf[sizeof(SaveFile) + 4];
    uint8_t* unalignedPtr = rawBuf + 1; // +1 offset guarantees non-4-byte alignment
    assert(((uintptr_t)unalignedPtr % 4) != 0);

    // Copy valid serialized image to unaligned pointer
    memcpy(unalignedPtr, &sourceSave, sizeof(SaveFile));

    // Deserialization via memcpy into aligned stack structures (safe on MIPS Allegrex)
    SaveHeader destHeader;
    SavePayload destPayload;
    memcpy(&destHeader, unalignedPtr, sizeof(SaveHeader));
    memcpy(&destPayload, unalignedPtr + sizeof(SaveHeader), sizeof(SavePayload));

    assert(memcmp(destHeader.magic, "HCRS", 4) == 0);
    assert(destHeader.version == 1);
    assert(destHeader.payloadSize == sizeof(SavePayload));
    assert(destHeader.crc32 == sourceSave.header.crc32);
    assert(PlayerInfo::CalculateCRC32(&destPayload, sizeof(SavePayload)) == destHeader.crc32);
    assert(destPayload.coins == 987654321);
    assert(destPayload.selectedVehicleIdHash == 0x12345678);
    assert(destPayload.vehicles[0].levelEngine == 15);

    printf("  Unaligned buffer read (+1 byte offset) via memcpy verified bit-exact and safe!\n");
    printf("  -> SUBTEST 7 PASSED\n\n");

    // -------------------------------------------------------------
    // Sub-test 8: Migration of legacy 'fingerscrew' (0xDC088A10) -> 'screwfinger' (0xB61B2D9E)
    // -------------------------------------------------------------
    printf("[SUBTEST 8] Legacy vehicle ID hash migration: fingerscrew (0xDC088A10) -> screwfinger (0xB61B2D9E)...\n");
    CleanupTestFiles(testBase);

    const uint32_t OLD_HASH = 0xDC088A10u;
    const uint32_t NEW_HASH = 0xB61B2D9Eu;

    SaveFile legacyVehicleSave;
    memset(&legacyVehicleSave, 0, sizeof(legacyVehicleSave));
    memcpy(legacyVehicleSave.header.magic, "HCRS", 4);
    legacyVehicleSave.header.version = 1;
    legacyVehicleSave.header.payloadSize = sizeof(SavePayload);
    legacyVehicleSave.payload.coins = 550000;
    legacyVehicleSave.payload.selectedVehicleIdHash = OLD_HASH;
    legacyVehicleSave.payload.selectedStageIdHash = PlayerInfo::HashId("countryside");
    legacyVehicleSave.payload.vehicleCount = 1;
    legacyVehicleSave.payload.vehicles[0].idHash = OLD_HASH;
    legacyVehicleSave.payload.vehicles[0].unlocked = 1;
    legacyVehicleSave.payload.vehicles[0].levelEngine = 7;
    legacyVehicleSave.payload.vehicles[0].levelSuspension = 8;
    legacyVehicleSave.payload.vehicles[0].levelTires = 9;
    legacyVehicleSave.payload.vehicles[0].level4th = 10;
    legacyVehicleSave.header.crc32 = PlayerInfo::CalculateCRC32(&legacyVehicleSave.payload, sizeof(SavePayload));

    FILE* sf = fopen(binPath, "wb");
    assert(sf != NULL);
    assert(fwrite(&legacyVehicleSave.header, sizeof(SaveHeader), 1, sf) == 1);
    assert(fwrite(&legacyVehicleSave.payload, sizeof(SavePayload), 1, sf) == 1);
    fclose(sf);

    // Load must migrate OLD_HASH to NEW_HASH
    PlayerInfo::Reset();
    loadOk = PlayerInfo::Load(testBase);
    assert(loadOk);
    assert(PlayerInfo::GetSelectedVehicleIdHash() == NEW_HASH);
    assert(PlayerInfo::IsVehicleUnlocked(NEW_HASH));
    assert(PlayerInfo::GetVehicleUpgrade(NEW_HASH, 0) == 7);
    assert(PlayerInfo::GetVehicleUpgrade(NEW_HASH, 1) == 8);
    assert(PlayerInfo::GetVehicleUpgrade(NEW_HASH, 2) == 9);
    assert(PlayerInfo::GetVehicleUpgrade(NEW_HASH, 3) == 10);
    assert(PlayerInfo::IsDirty());

    // Save and reload to ensure persisted migration
    PlayerInfo::Save(testBase);
    PlayerInfo::Reset();
    loadOk = PlayerInfo::Load(testBase);
    assert(loadOk);
    assert(PlayerInfo::GetSelectedVehicleIdHash() == NEW_HASH);
    assert(PlayerInfo::IsVehicleUnlocked(NEW_HASH));
    assert(PlayerInfo::GetVehicleUpgrade(NEW_HASH, 0) == 7);

    printf("  fingerscrew (0xDC088A10) successfully migrated to screwfinger (0xB61B2D9E) and persisted!\n");
    printf("  -> SUBTEST 8 PASSED\n\n");

    // Cleanup
    CleanupTestFiles(testBase);

    printf("===================================================\n");
    printf("ALL 8 SUB-TESTS OF test_save PASSED SUCCESSFULLY!\n");
    printf("===================================================\n");
    return 0;
}

