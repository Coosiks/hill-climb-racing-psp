#include "PlayerInfo.h"
#include <stdio.h>
#include <string.h>

SavePayload PlayerInfo::sPayload;
bool PlayerInfo::sDirty = false;

int PlayerInfo::Coins = 0;
int PlayerInfo::EngineLevel = 0;
int PlayerInfo::SuspensionLevel = 0;
int PlayerInfo::TiresLevel = 0;
int PlayerInfo::AWDLevel = 0;
int PlayerInfo::HighScores[STAGE_COUNT] = { 0 };

uint32_t PlayerInfo::CalculateCRC32(const void* data, size_t length) {
    static uint32_t table[256];
    static bool haveTable = false;
    if (!haveTable) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int j = 0; j < 8; ++j) {
                if (c & 1) c = 0xEDB88320u ^ (c >> 1);
                else       c = c >> 1;
            }
            table[i] = c;
        }
        haveTable = true;
    }

    const uint8_t* p = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < length; ++i) {
        crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

uint32_t PlayerInfo::HashId(const char* str) {
    return HashFNV1a_32(str);
}

void PlayerInfo::BuildFilePaths(const char* basePath, char* binPath, char* bakPath, char* tmpPath, char* datPath, size_t maxLen) {
    if (!basePath || basePath[0] == '\0') {
        snprintf(binPath, maxLen, "save.bin");
        snprintf(bakPath, maxLen, "save.bak");
        snprintf(tmpPath, maxLen, "save.tmp");
        snprintf(datPath, maxLen, "save.dat");
    } else {
        snprintf(binPath, maxLen, "%s.bin", basePath);
        snprintf(bakPath, maxLen, "%s.bak", basePath);
        snprintf(tmpPath, maxLen, "%s.tmp", basePath);
        snprintf(datPath, maxLen, "%s.dat", basePath);
    }
}

int64_t PlayerInfo::GetCoins() {
    return sPayload.coins;
}

void PlayerInfo::SetCoins(int64_t coins) {
    if (sPayload.coins != coins) {
        sPayload.coins = coins;
        Coins = (int)coins;
        sDirty = true;
    }
}

void PlayerInfo::AddCoins(int64_t delta) {
    if (delta != 0) {
        sPayload.coins += delta;
        Coins = (int)sPayload.coins;
        sDirty = true;
    }
}

uint32_t PlayerInfo::GetSelectedVehicleIdHash() {
    return sPayload.selectedVehicleIdHash;
}

void PlayerInfo::SetSelectedVehicleIdHash(uint32_t idHash) {
    if (sPayload.selectedVehicleIdHash != idHash) {
        sPayload.selectedVehicleIdHash = idHash;
        sDirty = true;
        SyncToLegacy();
    }
}

uint32_t PlayerInfo::GetSelectedStageIdHash() {
    return sPayload.selectedStageIdHash;
}

void PlayerInfo::SetSelectedStageIdHash(uint32_t idHash) {
    if (sPayload.selectedStageIdHash != idHash) {
        sPayload.selectedStageIdHash = idHash;
        sDirty = true;
    }
}

SavedVehicleEntry* PlayerInfo::GetVehicleEntry(uint32_t idHash) {
    for (uint16_t i = 0; i < sPayload.vehicleCount; ++i) {
        if (sPayload.vehicles[i].idHash == idHash) {
            return &sPayload.vehicles[i];
        }
    }
    return NULL;
}

SavedVehicleEntry* PlayerInfo::GetOrCreateVehicleEntry(uint32_t idHash) {
    SavedVehicleEntry* entry = GetVehicleEntry(idHash);
    if (entry) return entry;

    if (sPayload.vehicleCount < SAVE_MAX_VEHICLES) {
        uint16_t idx = sPayload.vehicleCount++;
        memset(&sPayload.vehicles[idx], 0, sizeof(SavedVehicleEntry));
        sPayload.vehicles[idx].idHash = idHash;
        sDirty = true;
        return &sPayload.vehicles[idx];
    }
    return NULL;
}

bool PlayerInfo::IsVehicleUnlocked(uint32_t idHash) {
    SavedVehicleEntry* v = GetVehicleEntry(idHash);
    return v ? (v->unlocked != 0) : false;
}

void PlayerInfo::UnlockVehicle(uint32_t idHash) {
    SavedVehicleEntry* v = GetOrCreateVehicleEntry(idHash);
    if (v && !v->unlocked) {
        v->unlocked = 1;
        sDirty = true;
    }
}

void PlayerInfo::SetVehicleUpgrade(uint32_t idHash, int slot, int level) {
    SavedVehicleEntry* v = GetOrCreateVehicleEntry(idHash);
    if (!v) return;

    uint8_t uLevel = (uint8_t)(level < 0 ? 0 : (level > 255 ? 255 : level));
    bool changed = false;
    switch (slot) {
        case 0: if (v->levelEngine != uLevel) { v->levelEngine = uLevel; changed = true; } break;
        case 1: if (v->levelSuspension != uLevel) { v->levelSuspension = uLevel; changed = true; } break;
        case 2: if (v->levelTires != uLevel) { v->levelTires = uLevel; changed = true; } break;
        case 3: if (v->level4th != uLevel) { v->level4th = uLevel; changed = true; } break;
        default: break;
    }

    if (changed) {
        sDirty = true;
        if (idHash == sPayload.selectedVehicleIdHash) {
            SyncToLegacy();
        }
    }
}

int PlayerInfo::GetVehicleUpgrade(uint32_t idHash, int slot) {
    SavedVehicleEntry* v = GetVehicleEntry(idHash);
    if (!v) return 0;
    switch (slot) {
        case 0: return v->levelEngine;
        case 1: return v->levelSuspension;
        case 2: return v->levelTires;
        case 3: return v->level4th;
        default: return 0;
    }
}

SavedStageEntry* PlayerInfo::GetStageEntry(uint32_t idHash) {
    for (uint16_t i = 0; i < sPayload.stageCount; ++i) {
        if (sPayload.stages[i].idHash == idHash) {
            return &sPayload.stages[i];
        }
    }
    return NULL;
}

SavedStageEntry* PlayerInfo::GetOrCreateStageEntry(uint32_t idHash) {
    SavedStageEntry* entry = GetStageEntry(idHash);
    if (entry) return entry;

    if (sPayload.stageCount < SAVE_MAX_STAGES) {
        uint16_t idx = sPayload.stageCount++;
        memset(&sPayload.stages[idx], 0, sizeof(SavedStageEntry));
        sPayload.stages[idx].idHash = idHash;
        sDirty = true;
        return &sPayload.stages[idx];
    }
    return NULL;
}

bool PlayerInfo::IsStageUnlocked(uint32_t idHash) {
    SavedStageEntry* s = GetStageEntry(idHash);
    return s ? (s->unlocked != 0) : false;
}

void PlayerInfo::UnlockStage(uint32_t idHash) {
    SavedStageEntry* s = GetOrCreateStageEntry(idHash);
    if (s && !s->unlocked) {
        s->unlocked = 1;
        sDirty = true;
    }
}

int32_t PlayerInfo::GetStageRecord(uint32_t idHash) {
    SavedStageEntry* s = GetStageEntry(idHash);
    return s ? s->bestDistance : 0;
}

void PlayerInfo::SetStageRecord(uint32_t idHash, int32_t distance) {
    SavedStageEntry* s = GetOrCreateStageEntry(idHash);
    if (s && s->bestDistance < distance) {
        s->bestDistance = distance;
        sDirty = true;
        SyncToLegacy();
    }
}

bool PlayerInfo::IsDirty() {
    return sDirty;
}

void PlayerInfo::SetDirty(bool dirty) {
    sDirty = dirty;
}

const SavePayload& PlayerInfo::GetPayload() {
    return sPayload;
}

void PlayerInfo::SetPayload(const SavePayload& payload) {
    sPayload = payload;
    sDirty = true;
    SyncToLegacy();
}

void PlayerInfo::Reset() {
    memset(&sPayload, 0, sizeof(SavePayload));

    sPayload.coins = 0;
    uint32_t defaultVehicleHash = HashId("jeep");
    uint32_t defaultStageHash = HashId("countryside");

    sPayload.selectedVehicleIdHash = defaultVehicleHash;
    sPayload.selectedStageIdHash = defaultStageHash;

    sPayload.vehicleCount = 1;
    sPayload.vehicles[0].idHash = defaultVehicleHash;
    sPayload.vehicles[0].unlocked = 1;
    sPayload.vehicles[0].levelEngine = 0;
    sPayload.vehicles[0].levelSuspension = 0;
    sPayload.vehicles[0].levelTires = 0;
    sPayload.vehicles[0].level4th = 0;

    sPayload.stageCount = 1;
    sPayload.stages[0].idHash = defaultStageHash;
    sPayload.stages[0].unlocked = 1;
    sPayload.stages[0].bestDistance = 0;

    sDirty = true;
    SyncToLegacy();
}

void PlayerInfo::SyncToLegacy() {
    Coins = (int)sPayload.coins;

    SavedVehicleEntry* v = GetVehicleEntry(sPayload.selectedVehicleIdHash);
    if (!v) v = GetVehicleEntry(HashId("jeep"));

    if (v) {
        EngineLevel = v->levelEngine;
        SuspensionLevel = v->levelSuspension;
        TiresLevel = v->levelTires;
        AWDLevel = v->level4th;
    } else {
        EngineLevel = SuspensionLevel = TiresLevel = AWDLevel = 0;
    }

    const char* stageTags[STAGE_COUNT] = {
        "countryside", "desert", "arctic", "moon",
        "highway", "cave", "mars", "forest",
        "mountain", "mudpool", "volcano", "beach",
        "rollercoaster", "night", "rooftops", "junkyard",
        "construction", "rainbow", "seasons", "bootcamp",
        "xmas", "alienplanet", "arcticcave", "ragnarok"
    };
    for (int i = 0; i < STAGE_COUNT; ++i) {
        SavedStageEntry* s = GetStageEntry(HashId(stageTags[i]));
        HighScores[i] = s ? s->bestDistance : 0;
    }
}

void PlayerInfo::SyncFromLegacy() {
    if (sPayload.coins != Coins) {
        sPayload.coins = Coins;
        sDirty = true;
    }

    SavedVehicleEntry* v = GetOrCreateVehicleEntry(sPayload.selectedVehicleIdHash);
    if (v) {
        if (v->levelEngine != EngineLevel || v->levelSuspension != SuspensionLevel ||
            v->levelTires != TiresLevel || v->level4th != AWDLevel) {
            v->levelEngine = (uint8_t)EngineLevel;
            v->levelSuspension = (uint8_t)SuspensionLevel;
            v->levelTires = (uint8_t)TiresLevel;
            v->level4th = (uint8_t)AWDLevel;
            sDirty = true;
        }
    }

    const char* stageTags[STAGE_COUNT] = {
        "countryside", "desert", "arctic", "moon",
        "highway", "cave", "mars", "forest",
        "mountain", "mudpool", "volcano", "beach",
        "rollercoaster", "night", "rooftops", "junkyard",
        "construction", "rainbow", "seasons", "bootcamp",
        "xmas", "alienplanet", "arcticcave", "ragnarok"
    };
    for (int i = 0; i < STAGE_COUNT; ++i) {
        SavedStageEntry* s = GetOrCreateStageEntry(HashId(stageTags[i]));
        if (s && s->bestDistance != HighScores[i]) {
            s->bestDistance = HighScores[i];
            sDirty = true;
        }
    }
}

static bool ReadSaveFile(const char* filepath, SavePayload* outPayload) {
    FILE* f = fopen(filepath, "rb");
    if (!f) return false;

    SaveHeader header;
    if (fread(&header, sizeof(SaveHeader), 1, f) != 1) {
        fclose(f);
        return false;
    }

    if (memcmp(header.magic, "HCRS", 4) != 0 || header.version != 1 || header.payloadSize != sizeof(SavePayload)) {
        fclose(f);
        return false;
    }

    SavePayload payload;
    if (fread(&payload, sizeof(SavePayload), 1, f) != 1) {
        fclose(f);
        return false;
    }
    fclose(f);

    uint32_t calcCrc = PlayerInfo::CalculateCRC32(&payload, sizeof(SavePayload));
    if (calcCrc != header.crc32) {
        return false;
    }

    *outPayload = payload;
    return true;
}

bool PlayerInfo::Load(const char* basePath) {
    char binPath[256], bakPath[256], tmpPath[256], datPath[256];
    BuildFilePaths(basePath, binPath, bakPath, tmpPath, datPath, sizeof(binPath));

    // 1. Try primary binary save
    SavePayload loadedPayload;
    bool loaded = false;
    if (ReadSaveFile(binPath, &loadedPayload)) {
        sPayload = loadedPayload;
        sDirty = false;
        loaded = true;
    } else if (ReadSaveFile(bakPath, &loadedPayload)) {
        sPayload = loadedPayload;
        sDirty = true; // Mark dirty so primary save will be re-created on next save point
        loaded = true;
        printf("[SAVE] Primary save missing or corrupted. Recovered from %s\n", bakPath);
    }

    if (loaded) {
        // H10.4 & H11 migration: fingerscrew (0xDC088A10) -> screwfinger (0xB61B2D9E)
        const uint32_t OLD_FINGERSCREW_HASH = 0xDC088A10u;
        const uint32_t NEW_SCREWFINGER_HASH = 0xB61B2D9Eu;
        if (sPayload.selectedVehicleIdHash == OLD_FINGERSCREW_HASH) {
            sPayload.selectedVehicleIdHash = NEW_SCREWFINGER_HASH;
            sDirty = true;
        }

        for (uint16_t i = 0; i < sPayload.vehicleCount; ++i) {
            if (sPayload.vehicles[i].idHash == OLD_FINGERSCREW_HASH) {
                // Check if canonical NEW_SCREWFINGER_HASH slot already exists
                int canonicalIdx = -1;
                for (uint16_t k = 0; k < sPayload.vehicleCount; ++k) {
                    if (sPayload.vehicles[k].idHash == NEW_SCREWFINGER_HASH && k != i) {
                        canonicalIdx = (int)k;
                        break;
                    }
                }

                if (canonicalIdx != -1) {
                    // Case b: Both slots exist -> merge into canonical slot, remove old slot
                    SavedVehicleEntry& dst = sPayload.vehicles[canonicalIdx];
                    const SavedVehicleEntry& src = sPayload.vehicles[i];
                    if (src.unlocked) dst.unlocked = 1;
                    if (src.levelEngine > dst.levelEngine) dst.levelEngine = src.levelEngine;
                    if (src.levelSuspension > dst.levelSuspension) dst.levelSuspension = src.levelSuspension;
                    if (src.levelTires > dst.levelTires) dst.levelTires = src.levelTires;
                    if (src.level4th > dst.level4th) dst.level4th = src.level4th;

                    // Remove old slot i by shifting tail
                    for (uint16_t j = i; j + 1 < sPayload.vehicleCount; ++j) {
                        sPayload.vehicles[j] = sPayload.vehicles[j + 1];
                    }
                    sPayload.vehicleCount--;
                    sDirty = true;
                    --i; // Re-evaluate current slot index after tail shift
                } else {
                    // Case a: Only old slot exists -> simply rename idHash
                    sPayload.vehicles[i].idHash = NEW_SCREWFINGER_HASH;
                    sDirty = true;
                }
            }
        }
        SyncToLegacy();
        return true;
    }

    // 3. Try migrating legacy 'HCR1' save.dat
    FILE* df = fopen(datPath, "rb");
    if (df) {
        int magic = 0;
        int legacyCoins = 0;
        int eng = 0, susp = 0, tires = 0, awd = 0;
        const int LEGACY_STAGE_COUNT = 4;
        int scores[LEGACY_STAGE_COUNT] = { 0 };

        bool readOk = true;
        if (fread(&magic, sizeof(int), 1, df) != 1 || magic != 0x48435231) readOk = false;
        if (readOk && fread(&legacyCoins, sizeof(int), 1, df) != 1) readOk = false;
        if (readOk && fread(&eng, sizeof(int), 1, df) != 1) readOk = false;
        if (readOk && fread(&susp, sizeof(int), 1, df) != 1) readOk = false;
        if (readOk && fread(&tires, sizeof(int), 1, df) != 1) readOk = false;
        if (readOk && fread(&awd, sizeof(int), 1, df) != 1) readOk = false;
        if (readOk && fread(scores, sizeof(int), LEGACY_STAGE_COUNT, df) != LEGACY_STAGE_COUNT) readOk = false;
        fclose(df);

        if (readOk) {
            printf("[SAVE] Successfully migrating legacy %s ('HCR1') to HCRS format\n", datPath);
            Reset();
            sPayload.coins = legacyCoins;

            SavedVehicleEntry* v = GetOrCreateVehicleEntry(HashId("jeep"));
            if (v) {
                v->unlocked = 1;
                v->levelEngine = (uint8_t)eng;
                v->levelSuspension = (uint8_t)susp;
                v->levelTires = (uint8_t)tires;
                v->level4th = (uint8_t)awd;
            }

            const char* stageTags[4] = { "countryside", "desert", "arctic", "moon" };
            for (int i = 0; i < LEGACY_STAGE_COUNT; ++i) {
                SavedStageEntry* s = GetOrCreateStageEntry(HashId(stageTags[i]));
                if (s) { s->unlocked = 1; s->bestDistance = scores[i]; }
            }

            sDirty = true;
            SyncToLegacy();
            Save(basePath, true);
            return true;
        }
    }

    // 4. Fresh default state if no valid saves exist
    Reset();
    sDirty = true;
    SyncToLegacy();
    return false;
}

bool PlayerInfo::Save(const char* basePath, bool force) {
    SyncFromLegacy();
    if (!force && !sDirty) {
        return true; // No disk I/O needed when clean
    }

    char binPath[256], bakPath[256], tmpPath[256], datPath[256];
    BuildFilePaths(basePath, binPath, bakPath, tmpPath, datPath, sizeof(binPath));

    // 1. Write to temporary file
    FILE* f = fopen(tmpPath, "wb");
    if (!f) return false;

    SaveHeader header;
    header.magic[0] = 'H';
    header.magic[1] = 'C';
    header.magic[2] = 'R';
    header.magic[3] = 'S';
    header.version = 1;
    header.flags = 0;
    header.payloadSize = sizeof(SavePayload);
    header.crc32 = CalculateCRC32(&sPayload, sizeof(SavePayload));

    size_t w1 = fwrite(&header, sizeof(SaveHeader), 1, f);
    size_t w2 = fwrite(&sPayload, sizeof(SavePayload), 1, f);
    fflush(f);
    int err = fclose(f);
    if (w1 != 1 || w2 != 1 || err != 0) {
        remove(tmpPath);
        return false;
    }

    // 2. Validate written tmp file before modifying permanent saves
    SavePayload verifyPayload;
    if (!ReadSaveFile(tmpPath, &verifyPayload)) {
        remove(tmpPath);
        return false;
    }

    // 3. Rotate existing save.bin to save.bak (handle FAT32 overwrite limitation)
    FILE* checkBin = fopen(binPath, "rb");
    if (checkBin) {
        fclose(checkBin);
        remove(bakPath);
        rename(binPath, bakPath);
    }

    // 4. Commit tmp file to save.bin
    remove(binPath);
    if (rename(tmpPath, binPath) != 0) {
        return false;
    }

    sDirty = false;
    return true;
}
