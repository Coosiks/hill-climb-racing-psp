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

    // Race management & testability helpers (T-033)
    void StartRace();
    void EndRace();
    void CommitRaceResults();
    void UpdatePhysicsStep(float dt);

    int GetRaceCollectedCoins() const { return mRaceCollectedCoins; }
    int GetRaceTrickBonusCoins() const { return mRaceTrickBonusCoins; }
    int GetRaceDistanceBonusCoins() const { return mRaceDistanceBonusCoins; }
    int GetSessionCoins() const { return mSessionCoins; }
    bool IsNewRecord() const { return mIsNewRecord; }
    float GetMaxDistance() const { return mMaxDistance; }
    void SetMaxDistance(float dist) { mMaxDistance = dist; }

    GameState GetState() const { return mState; }
    void SetState(GameState state) { mState = state; }
    int GetCurrentStage() const { return mCurrentStage; }
    void SetCurrentStage(int stage) { mCurrentStage = stage; }
    int GetCurrentVehicle() const { return mCurrentVehicle; }
    void SetCurrentVehicle(int veh) { mCurrentVehicle = veh; }

    PlayerCar* GetCar() const { return mCar; }
    Terrain* GetTerrain() const { return mTerrain; }
    PhysicsWorld* GetPhysics() const { return mPhysics; }

private:
    GameState mState;
    int mCurrentStage;
    int mCurrentVehicle;
    int mGarageTab;
    int mSelectedUpgradeSlot;

    PhysicsWorld* mPhysics;
    Terrain* mTerrain;
    PlayerCar* mCar;

    // Session stats (T-033 detailed breakdown)
    int mSessionCoins;
    int mRaceCollectedCoins;
    int mRaceTrickBonusCoins;
    int mRaceDistanceBonusCoins;
    bool mIsNewRecord;
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
    Texture mTexStagePreview;
    Texture mTexStageLocked;
    Texture mTexVehiclePreview;

    Font mFont;

    void LoadStagePreview();
    void LoadVehiclePreview();
    bool CanStartRace() const;

    void UpdateGarage();
    void RenderGarage();

    void UpdatePlaying(float frameTime);
    void RenderPlaying();

    void UpdateGameOver();
    void RenderGameOver();

    void RenderHUD();
};

#endif // HCR_GAME_H

