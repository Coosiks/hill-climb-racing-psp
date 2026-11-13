#include "PlayerInfo.h"
#include <stdio.h>
#include <string.h>

int PlayerInfo::Coins = 1000; // Start with 1000 coins for initial fun
int PlayerInfo::EngineLevel = 0;
int PlayerInfo::SuspensionLevel = 0;
int PlayerInfo::TiresLevel = 0;
int PlayerInfo::AWDLevel = 0;
int PlayerInfo::HighScores[STAGE_COUNT] = { 0, 0, 0, 0 };

#define SAVE_FILE_PATH "save.dat"

void PlayerInfo::Reset() {
    Coins = 1000;
    EngineLevel = 0;
    SuspensionLevel = 0;
    TiresLevel = 0;
    AWDLevel = 0;
    for (int i = 0; i < STAGE_COUNT; ++i) {
        HighScores[i] = 0;
    }
}

bool PlayerInfo::Load() {
    FILE* f = fopen(SAVE_FILE_PATH, "rb");
    if (!f) return false;

    int magic = 0;
    if (fread(&magic, sizeof(int), 1, f) != 1 || magic != 0x48435231) { // 'HCR1'
        fclose(f);
        return false;
    }

    fread(&Coins, sizeof(int), 1, f);
    fread(&EngineLevel, sizeof(int), 1, f);
    fread(&SuspensionLevel, sizeof(int), 1, f);
    fread(&TiresLevel, sizeof(int), 1, f);
    fread(&AWDLevel, sizeof(int), 1, f);
    fread(HighScores, sizeof(int), STAGE_COUNT, f);

    fclose(f);
    return true;
}

bool PlayerInfo::Save() {
    FILE* f = fopen(SAVE_FILE_PATH, "wb");
    if (!f) return false;

    int magic = 0x48435231;
    fwrite(&magic, sizeof(int), 1, f);
    fwrite(&Coins, sizeof(int), 1, f);
    fwrite(&EngineLevel, sizeof(int), 1, f);
    fwrite(&SuspensionLevel, sizeof(int), 1, f);
    fwrite(&TiresLevel, sizeof(int), 1, f);
    fwrite(&AWDLevel, sizeof(int), 1, f);
    fwrite(HighScores, sizeof(int), STAGE_COUNT, f);

    fclose(f);
    return true;
}

