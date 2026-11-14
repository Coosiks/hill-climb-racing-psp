#ifndef HCR_PLAYER_INFO_H
#define HCR_PLAYER_INFO_H

#include <stdint.h>
#include <stddef.h>
#include "GameConstants.h"

#pragma pack(push, 1)

struct SaveHeader {
    char     magic[4];        // 'H','C','R','S' (0x53524348 in Little-Endian)
    uint16_t version;         // Format version (currently 1)
    uint16_t flags;           // Reserved / settings flags (0)
    uint32_t payloadSize;     // Sizeof SavePayload (1556 bytes)
    uint32_t crc32;           // IEEE 802.3 CRC32 checksum of SavePayload
};
static_assert(sizeof(SaveHeader) == 16, "SaveHeader size must be exactly 16 bytes");
static_assert(sizeof(SaveHeader) % 4 == 0, "SaveHeader must have 4-byte natural alignment");

struct SavedVehicleEntry {
    uint32_t idHash;          // offset 0: FNV-1a 32-bit hash of vehicle string ID
    uint8_t  unlocked;        // offset 4: 1 if purchased/unlocked, 0 if locked
    uint8_t  levelEngine;     // offset 5: upgrade tier (0..20)
    uint8_t  levelSuspension; // offset 6: upgrade tier (0..20)
    uint8_t  levelTires;      // offset 7: upgrade tier (0..20)
    uint8_t  level4th;        // offset 8: upgrade tier for 4WD or Fuel (0..20)
    uint8_t  reserved[3];     // offset 9..11: padding for strict 4-byte alignment
};
static_assert(sizeof(SavedVehicleEntry) == 12, "SavedVehicleEntry size must be exactly 12 bytes");
static_assert(sizeof(SavedVehicleEntry) % 4 == 0, "SavedVehicleEntry must have 4-byte natural alignment");

struct SavedStageEntry {
    uint32_t idHash;          // offset 0: FNV-1a 32-bit hash of stage string ID
    uint8_t  unlocked;        // offset 4: 1 if unlocked, 0 if locked
    uint8_t  reserved[3];     // offset 5..7: padding for strict 4-byte alignment
    int32_t  bestDistance;    // offset 8: record in meters
};
static_assert(sizeof(SavedStageEntry) == 12, "SavedStageEntry size must be exactly 12 bytes");
static_assert(sizeof(SavedStageEntry) % 4 == 0, "SavedStageEntry must have 4-byte natural alignment");

#define SAVE_MAX_VEHICLES 64
#define SAVE_MAX_STAGES   64

struct SavePayload {
    int64_t           coins;                 // offset 0: player coin balance (8 bytes)
    uint32_t          selectedVehicleIdHash; // offset 8: currently equipped vehicle hash (4 bytes)
    uint32_t          selectedStageIdHash;   // offset 12: currently selected stage hash (4 bytes)
    uint16_t          vehicleCount;          // offset 16: number of saved vehicles (2 bytes)
    uint16_t          stageCount;            // offset 18: number of saved stages (2 bytes)
    SavedVehicleEntry vehicles[SAVE_MAX_VEHICLES]; // offset 20: (64 * 12 = 768 bytes)
    SavedStageEntry   stages[SAVE_MAX_STAGES];     // offset 788: (64 * 12 = 768 bytes)
};
static_assert(sizeof(SavePayload) == 1556, "SavePayload size must be exactly 1556 bytes");
static_assert(sizeof(SavePayload) % 4 == 0, "SavePayload must have 4-byte natural alignment");

// MIPS Allegrex 4-byte natural alignment assertions.
// On PSP (MIPS R4000 derivative), unaligned 32-bit reads/writes trigger hardware Address Error Exceptions.
// Explicit padding ensures all 32-bit fields reside at offsets divisible by 4.
static_assert(offsetof(SavedVehicleEntry, idHash) % 4 == 0, "MIPS alignment");
static_assert(offsetof(SavedStageEntry, idHash) % 4 == 0, "MIPS alignment");
static_assert(offsetof(SavedStageEntry, bestDistance) % 4 == 0, "MIPS alignment");
static_assert(offsetof(SavePayload, vehicles) % 4 == 0, "MIPS alignment");
static_assert(offsetof(SavePayload, stages) % 4 == 0, "MIPS alignment");

#pragma pack(pop)

class PlayerInfo {
public:
    // Legacy / Quick Access properties (kept synchronized with active vehicle/stage)
    static int Coins;
    static int EngineLevel;
    static int SuspensionLevel;
    static int TiresLevel;
    static int AWDLevel;
    static int HighScores[STAGE_COUNT];

    // Currency API
    static int64_t GetCoins();
    static void    SetCoins(int64_t coins);
    static void    AddCoins(int64_t delta);

    // Active Selection API
    static uint32_t GetSelectedVehicleIdHash();
    static void     SetSelectedVehicleIdHash(uint32_t idHash);
    static uint32_t GetSelectedStageIdHash();
    static void     SetSelectedStageIdHash(uint32_t idHash);

    // Vehicles API
    static bool               IsVehicleUnlocked(uint32_t idHash);
    static void               UnlockVehicle(uint32_t idHash);
    static SavedVehicleEntry* GetVehicleEntry(uint32_t idHash);
    static SavedVehicleEntry* GetOrCreateVehicleEntry(uint32_t idHash);
    static void               SetVehicleUpgrade(uint32_t idHash, int slot, int level);
    static int                GetVehicleUpgrade(uint32_t idHash, int slot);

    // Stages API
    static bool             IsStageUnlocked(uint32_t idHash);
    static void             UnlockStage(uint32_t idHash);
    static int32_t          GetStageRecord(uint32_t idHash);
    static void             SetStageRecord(uint32_t idHash, int32_t distance);
    static SavedStageEntry* GetStageEntry(uint32_t idHash);
    static SavedStageEntry* GetOrCreateStageEntry(uint32_t idHash);
    static const char*      StageTagAt(int i);

    // Dirty state management
    static bool IsDirty();
    static void SetDirty(bool dirty);

    // Persistence Lifecycle
    static void Reset();
    static bool Load(const char* basePath = NULL);
    static bool Save(const char* basePath = NULL, bool force = false);

    // Synchronization between legacy static fields and new structured payload
    static void SyncToLegacy();
    static void SyncFromLegacy();

    // Raw payload access for diagnostics / verification tests
    static const SavePayload& GetPayload();
    static void               SetPayload(const SavePayload& payload);

    // Pure hashing & checksum utilities
    static uint32_t CalculateCRC32(const void* data, size_t length);
    static uint32_t HashId(const char* str);

private:
    static SavePayload sPayload;
    static bool        sDirty;

    static void BuildFilePaths(const char* basePath, char* binPath, char* bakPath, char* tmpPath, char* datPath, size_t maxLen);
};

#endif // HCR_PLAYER_INFO_H
