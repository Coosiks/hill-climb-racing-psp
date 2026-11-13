#ifndef HCR_GAME_H
#define HCR_GAME_H

#include "GameConstants.h"
#include "System.h"
#include "PlayerInfo.h"
#include "physics/PhysicsWorld.h"
#include "physics/Terrain.h"
#include "physics/PlayerCar.h"
#include "audio/AudioEngine.h"
#include "graphics/Renderer2D.h"
#include "graphics/Texture.h"
#include "graphics/Font.h"

enum GameState {
    STATE_GARAGE,
    STATE_PLAYING,
    STATE_GAMEOVER
};

class Game : public GameListener {
public:
    Game();
    ~Game();

    bool Init();
    void Run();

    // GameListener callbacks
    virtual void OnDriverDown();
    virtual void OnCoinCollected(int value);
    virtual void OnFuelCollected();

private:
    GameState mState;
    StageType mCurrentStage;

    PhysicsWorld* mPhysics;
    Terrain* mTerrain;
    PlayerCar* mCar;

    // Session stats
    int mSessionCoins;
    float mMaxDistance;
    bool mGameOverReasonDriverDown;

    // Stunt bonus banner
    float mStuntTimer;
    const char* mStuntName;
    int mStuntCoins;
    float mOutOfFuelStopTimer;
    float mOutOfFuelFallbackTimer;

    // Camera
    float mCameraX;
    float mCameraY;
    float mCameraZoom;

    // Timestep & Accumulator
    double mLastTime;
    float mAccumulator;
    float mRenderAlpha;

    // UI Textures
    Texture mTexLogo;
    Texture mTexCoinIcon;
    Texture mTexFuelIcon;
    Texture mTexPedalGas;
    Texture mTexPedalBrake;
    Texture mTexUpgEngine;
    Texture mTexUpgSusp;
    Texture mTexUpgTires;
    Texture mTexUpgAWD;
    Texture mTexStageIcons[STAGE_COUNT];

    Font mFont;

    void StartRace();
    void EndRace();

    void UpdateGarage();
    void RenderGarage();

    void UpdatePlaying(float frameTime);
    void UpdatePhysicsStep(float dt);
    void RenderPlaying();

    void UpdateGameOver();
    void RenderGameOver();

    void RenderHUD();
};

#endif // HCR_GAME_H

