#ifndef HCR_PLAYER_INFO_H
#define HCR_PLAYER_INFO_H

#include "GameConstants.h"

class PlayerInfo {
public:
    static int Coins;
    static int EngineLevel;
    static int SuspensionLevel;
    static int TiresLevel;
    static int AWDLevel;
    static int HighScores[STAGE_COUNT];

    static void Reset();
    static bool Load();
    static bool Save();
};

#endif // HCR_PLAYER_INFO_H

