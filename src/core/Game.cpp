#include "Game.h"
#include "Config.h"
#include <math.h>
#include <stdio.h>
#include <stdarg.h>

static FILE* gLogFile = NULL;
static void HCR_Log(const char* fmt, ...) {
    if (!gLogFile) {
        gLogFile = fopen("hcr_log.txt", "w");
    }
    if (gLogFile) {
        va_list args;
        va_start(args, fmt);
        vfprintf(gLogFile, fmt, args);
        va_end(args);
        fprintf(gLogFile, "\n");
        fflush(gLogFile);
    }
}

Game::Game()
    : mState(STATE_GARAGE), mCurrentStage(STAGE_COUNTRYSIDE),
      mPhysics(NULL), mTerrain(NULL), mCar(NULL),
      mSessionCoins(0), mMaxDistance(0.0f), mGameOverReasonDriverDown(false),
      mStuntTimer(0.0f), mStuntName(NULL), mStuntCoins(0),
      mOutOfFuelStopTimer(0.0f), mOutOfFuelFallbackTimer(0.0f),
      mCameraX(0.0f), mCameraY(0.0f), mCameraZoom(1.0f) {
}

Game::~Game() {
    EndRace();
    AudioEngine::Shutdown();
}

bool Game::Init() {
    PlayerInfo::Load();
    AudioEngine::Init();

    mFont.Load("assets/fonts/gamefont.fnt", "assets/fonts/gamefont_0.png");

    mTexLogo.LoadPNG("assets/game-logo.png");
    mTexCoinIcon.LoadPNG("assets/coin-icon.png");
    mTexFuelIcon.LoadPNG("assets/fuel-icon.png");
    mTexPedalGas.LoadPNG("assets/lowres-pedal-gas-normal.png");
    mTexPedalBrake.LoadPNG("assets/lowres-pedal-brake-normal.png");

    mTexUpgEngine.LoadPNG("assets/MainMenuUpgradeEngine.png");
    mTexUpgSusp.LoadPNG("assets/MainMenuUpgradeSuspension.png");
    mTexUpgTires.LoadPNG("assets/MainMenuUpgradeTires.png");
    mTexUpgAWD.LoadPNG("assets/MainMenuUpgradeAWD.png");

    mTexStageIcons[STAGE_COUNTRYSIDE].LoadPNG("assets/stage-icon-countryside.png");
    mTexStageIcons[STAGE_DESERT].LoadPNG("assets/stage-icon-desert.png");
    mTexStageIcons[STAGE_ARCTIC].LoadPNG("assets/stage-icon-arctic.png");
    mTexStageIcons[STAGE_MOON].LoadPNG("assets/stage-icon-moon.png");

    mState = STATE_GARAGE;
    return true;
}

void Game::StartRace() {
    HCR_Log("StartRace: begin");
    EndRace();

    HCR_Log("StartRace: creating PhysicsWorld");
    float grav = (mCurrentStage >= 0 && mCurrentStage < STAGE_COUNT) ? Config::Stages[mCurrentStage].gravity : Config::Stages[STAGE_COUNTRYSIDE].gravity;
    mPhysics = new PhysicsWorld(grav, this);

    HCR_Log("StartRace: creating Terrain");
    mTerrain = new Terrain(mPhysics->GetWorld(), mCurrentStage);
    mPhysics->SetTerrain(mTerrain);

    HCR_Log("StartRace: creating PlayerCar");
    b2Vec2 startPos(0.0f, 2.5f);
    mCar = new PlayerCar(mPhysics->GetWorld(), startPos,
                         PlayerInfo::EngineLevel,
                         PlayerInfo::SuspensionLevel,
                         PlayerInfo::TiresLevel,
                         PlayerInfo::AWDLevel);

    mSessionCoins = 0;
    mMaxDistance = 0.0f;
    mStuntTimer = 0.0f;
    mStuntName = NULL;
    mStuntCoins = 0;
    mOutOfFuelStopTimer = 0.0f;
    mOutOfFuelFallbackTimer = 0.0f;
    mCameraX = 0.0f;
    mCameraY = 2.0f;
    mState = STATE_PLAYING;

    AudioEngine::PlaySFX(SFX_CLICK);
    HCR_Log("StartRace: complete, state = STATE_PLAYING");
}

void Game::EndRace() {
    AudioEngine::UpdateEngineSound(0.0f, false);

    if (mCar) {
        delete mCar;
        mCar = NULL;
    }
    if (mTerrain) {
        delete mTerrain;
        mTerrain = NULL;
    }
    if (mPhysics) {
        delete mPhysics;
        mPhysics = NULL;
    }
}

void Game::OnDriverDown() {
    if (mState == STATE_PLAYING) {
        mGameOverReasonDriverDown = true;
        if (mCar) mCar->Kill();
        AudioEngine::PlaySFX(SFX_CRACK);
        mState = STATE_GAMEOVER;

        // Save progress
        PlayerInfo::Coins += mSessionCoins;
        if ((int)mMaxDistance > PlayerInfo::HighScores[mCurrentStage]) {
            PlayerInfo::HighScores[mCurrentStage] = (int)mMaxDistance;
        }
        PlayerInfo::Save();
    }
}

void Game::OnCoinCollected(int value) {
    float mult = mTerrain ? mTerrain->GetCoinMultiplier() : 1.0f;
    mSessionCoins += (int)(value * mult);
    AudioEngine::PlaySFX(SFX_COIN);
}

void Game::OnFuelCollected() {
    if (mCar) {
        mCar->AddFuel(Config::Physics.max_fuel);
    }
    AudioEngine::PlaySFX(SFX_REFUEL);
}

void Game::Run() {
    while (System::IsRunning()) {
        System::PollInput();

        switch (mState) {
            case STATE_GARAGE:
                UpdateGarage();
                break;
            case STATE_PLAYING:
                UpdatePlaying();
                break;
            case STATE_GAMEOVER:
                UpdateGameOver();
                break;
        }

        System::BeginFrame();

        switch (mState) {
            case STATE_GARAGE:
                RenderGarage();
                break;
            case STATE_PLAYING:
                RenderPlaying();
                break;
            case STATE_GAMEOVER:
                RenderGameOver();
                break;
        }

        System::EndFrame();
    }
}

void Game::UpdateGarage() {
    // Stage selection (Left / Right)
    if (System::ButtonsPressed & PSP_CTRL_LEFT) {
        int nextStage = (int)mCurrentStage - 1;
        if (nextStage < 0) nextStage = STAGE_COUNT - 1;
        mCurrentStage = (StageType)nextStage;
        AudioEngine::PlaySFX(SFX_CLICK);
    }
    if (System::ButtonsPressed & PSP_CTRL_RIGHT) {
        int nextStage = (int)mCurrentStage + 1;
        if (nextStage >= STAGE_COUNT) nextStage = 0;
        mCurrentStage = (StageType)nextStage;
        AudioEngine::PlaySFX(SFX_CLICK);
    }

    // Upgrades
    // Triangle: Engine
    if (System::ButtonsPressed & PSP_CTRL_TRIANGLE) {
        int cost = GetUpgradeCost(PlayerInfo::EngineLevel);
        if (PlayerInfo::Coins >= cost && PlayerInfo::EngineLevel < 20) {
            PlayerInfo::Coins -= cost;
            PlayerInfo::EngineLevel++;
            AudioEngine::PlaySFX(SFX_CLICK);
            PlayerInfo::Save();
        }
    }

    // Square: Suspension
    if (System::ButtonsPressed & PSP_CTRL_SQUARE) {
        int cost = GetUpgradeCost(PlayerInfo::SuspensionLevel);
        if (PlayerInfo::Coins >= cost && PlayerInfo::SuspensionLevel < 20) {
            PlayerInfo::Coins -= cost;
            PlayerInfo::SuspensionLevel++;
            AudioEngine::PlaySFX(SFX_CLICK);
            PlayerInfo::Save();
        }
    }

    // Circle: Tires
    if (System::ButtonsPressed & PSP_CTRL_CIRCLE) {
        int cost = GetUpgradeCost(PlayerInfo::TiresLevel);
        if (PlayerInfo::Coins >= cost && PlayerInfo::TiresLevel < 20) {
            PlayerInfo::Coins -= cost;
            PlayerInfo::TiresLevel++;
            AudioEngine::PlaySFX(SFX_CLICK);
            PlayerInfo::Save();
        }
    }

    // Select: 4WD
    if (System::ButtonsPressed & PSP_CTRL_SELECT) {
        int cost = GetUpgradeCost(PlayerInfo::AWDLevel);
        if (PlayerInfo::Coins >= cost && PlayerInfo::AWDLevel < 20) {
            PlayerInfo::Coins -= cost;
            PlayerInfo::AWDLevel++;
            AudioEngine::PlaySFX(SFX_CLICK);
            PlayerInfo::Save();
        }
    }

    // Start Race (Cross or Start)
    if ((System::ButtonsPressed & PSP_CTRL_CROSS) || (System::ButtonsPressed & PSP_CTRL_START)) {
        StartRace();
    }
}

void Game::RenderGarage() {
    Renderer2D::SetupScreenSpace();

    // Dark dirt background
    Renderer2D::DrawFilledRect(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, 0xFF181512);

    // Title bar
    Renderer2D::DrawFilledRect(0, 0, PSP_SCREEN_WIDTH, 38, 0xFF352518);
    mFont.DrawText("HILL CLIMB RACING", 16, 7, 0.45f, 0xFFFFFFFF);

    // Coins in top right
    char strBuf[64];
    snprintf(strBuf, sizeof(strBuf), "%d", PlayerInfo::Coins);
    float coinTextW = mFont.GetTextWidth(strBuf, 0.45f);
    Renderer2D::DrawSpriteScreen(&mTexCoinIcon, PSP_SCREEN_WIDTH - 30 - coinTextW, 8, 22, 22);
    mFont.DrawText(strBuf, PSP_SCREEN_WIDTH - 4 - coinTextW, 8, 0.45f, 0xFF00E5FF);

    // Stage Selection panel
    Renderer2D::DrawFilledRect(15, 45, PSP_SCREEN_WIDTH - 30, 62, 0xFF422E1F);

    // Stage Icon
    Renderer2D::DrawSpriteScreen(&mTexStageIcons[mCurrentStage], 22, 48, 70, 56);

    const char* stageNames[STAGE_COUNT] = {
        "STAGE: COUNTRYSIDE",
        "STAGE: DESERT",
        "STAGE: ARCTIC",
        "STAGE: MOON"
    };
    mFont.DrawText(stageNames[mCurrentStage], 102, 50, 0.42f, 0xFFFFFFFF);

    snprintf(strBuf, sizeof(strBuf), "RECORD: %dm", PlayerInfo::HighScores[mCurrentStage]);
    mFont.DrawText(strBuf, 102, 78, 0.35f, 0xFF88DDFF);

    mFont.DrawText("< LEFT / RIGHT >", 320, 65, 0.35f, 0xFFAAAAAA);

    // Upgrades Grid (4 items)
    float upgW = 104.0f;
    float upgH = 92.0f;
    float startX = 15.0f;
    float startY = 115.0f;
    float spacing = 115.0f;

    struct UpgCard {
        const char* key;
        const char* name;
        int level;
        Texture* tex;
    } cards[4] = {
        { "[^] TRIANGLE", "ENGINE", PlayerInfo::EngineLevel, &mTexUpgEngine },
        { "[] SQUARE", "SUSPENSION", PlayerInfo::SuspensionLevel, &mTexUpgSusp },
        { "(O) CIRCLE", "TIRES", PlayerInfo::TiresLevel, &mTexUpgTires },
        { "[SEL] 4WD", "4WD", PlayerInfo::AWDLevel, &mTexUpgAWD }
    };

    for (int i = 0; i < 4; ++i) {
        float cx = startX + i * spacing;
        Renderer2D::DrawFilledRect(cx, startY, upgW, upgH, 0xFF322316);

        // Icon
        Renderer2D::DrawSpriteScreen(cards[i].tex, cx + 6, startY + 6, 36, 36);

        // Key Hint
        mFont.DrawText(cards[i].key, cx + 46, startY + 12, 0.28f, 0xFF00CCFF);

        // Name
        mFont.DrawText(cards[i].name, cx + 8, startY + 48, 0.30f, 0xFFFFFFFF);

        // Level
        snprintf(strBuf, sizeof(strBuf), "LVL %d/20", cards[i].level);
        mFont.DrawText(strBuf, cx + 8, startY + 63, 0.28f, 0xFFCCCCCC);

        // Cost
        if (cards[i].level >= 20) {
            mFont.DrawText("MAX", cx + 8, startY + 77, 0.30f, 0xFF55FF55);
        } else {
            int cost = GetUpgradeCost(cards[i].level);
            snprintf(strBuf, sizeof(strBuf), "%d C", cost);
            unsigned int cColor = (PlayerInfo::Coins >= cost) ? 0xFF00E5FF : 0xFF5555AA;
            mFont.DrawText(strBuf, cx + 8, startY + 77, 0.30f, cColor);
        }
    }

    // Bottom Start prompt
    Renderer2D::DrawFilledRect(15, 220, PSP_SCREEN_WIDTH - 30, 42, 0xFF257518);
    mFont.DrawText("PRESS CROSS OR START TO RACE!", 95, 230, 0.45f, 0xFFFFFFFF);
}

void Game::UpdatePlaying() {
    if (!mCar || !mPhysics || !mTerrain) return;

    static int sUpdateCount = 0;
    if (sUpdateCount < 5) HCR_Log("UpdatePlaying frame %d: start", sUpdateCount);

    // Controls
    if ((System::ButtonsHeld & PSP_CTRL_RTRIGGER) || (System::ButtonsHeld & PSP_CTRL_CROSS) || (System::ButtonsHeld & PSP_CTRL_RIGHT)) {
        mCar->Accelerate();
    } else if ((System::ButtonsHeld & PSP_CTRL_LTRIGGER) || (System::ButtonsHeld & PSP_CTRL_SQUARE) || (System::ButtonsHeld & PSP_CTRL_LEFT)) {
        mCar->Decelerate();
    } else {
        mCar->Idle();
    }

    // Step physics
    if (sUpdateCount < 5) HCR_Log("UpdatePlaying frame %d: before Physics Step", sUpdateCount);
    mPhysics->Step(FIXED_TIMESTEP);
    if (sUpdateCount < 5) HCR_Log("UpdatePlaying frame %d: after Physics Step", sUpdateCount);

    b2Vec2 pos = mCar->GetPosition();
    mTerrain->ProcessDeferredDestroy(pos.x);

    mCar->Update(FIXED_TIMESTEP);
    if (sUpdateCount < 5) HCR_Log("UpdatePlaying frame %d: after Car Update", sUpdateCount);

    mTerrain->Update(pos.x);
    if (sUpdateCount < 5) HCR_Log("UpdatePlaying frame %d: after Terrain Update", sUpdateCount);

    // Update Distance
    if (pos.x > mMaxDistance) {
        mMaxDistance = pos.x;
    }

    // Stunt Tracking (Flips in air)
    int flip = mCar->PopFlips();
    if (flip > 0) {
        mSessionCoins += Config::Physics.flip_bonus_coins;
        mStuntName = "BACKFLIP!";
        mStuntCoins = Config::Physics.flip_bonus_coins;
        mStuntTimer = 2.0f;
        AudioEngine::PlaySFX(SFX_COIN);
    } else if (flip < 0) {
        mSessionCoins += Config::Physics.flip_bonus_coins;
        mStuntName = "FRONTFLIP!";
        mStuntCoins = Config::Physics.flip_bonus_coins;
        mStuntTimer = 2.0f;
        AudioEngine::PlaySFX(SFX_COIN);
    }

    if (mStuntTimer > 0.0f) {
        mStuntTimer -= FIXED_TIMESTEP;
    }

    // Check Out of Fuel Coasting
    if (mCar->IsOutOfFuel()) {
        mOutOfFuelFallbackTimer += FIXED_TIMESTEP;
        float speed = mCar->GetSpeedKmH() / 3.6f;
        if (speed < 0.3f) {
            mOutOfFuelStopTimer += FIXED_TIMESTEP;
        } else {
            mOutOfFuelStopTimer = 0.0f; // still moving, keep coasting
        }

        if (mOutOfFuelStopTimer >= 1.0f || mOutOfFuelFallbackTimer >= 15.0f) {
            mGameOverReasonDriverDown = false;
            mState = STATE_GAMEOVER;
            PlayerInfo::Coins += mSessionCoins;
            if ((int)mMaxDistance > PlayerInfo::HighScores[mCurrentStage]) {
                PlayerInfo::HighScores[mCurrentStage] = (int)mMaxDistance;
            }
            PlayerInfo::Save();
            return;
        }
    } else {
        mOutOfFuelStopTimer = 0.0f;
        mOutOfFuelFallbackTimer = 0.0f;
    }

    // Engine Audio
    AudioEngine::UpdateEngineSound(mCar->GetEngineRev(), true);
    if (sUpdateCount < 5) HCR_Log("UpdatePlaying frame %d: after Audio Update", sUpdateCount);

    // Camera follow (lead forward and smoothing from Config)
    float targetCamX = pos.x + Config::Physics.camera_lead_x;
    float targetCamY = pos.y + Config::Physics.camera_lead_y;

    mCameraX += (targetCamX - mCameraX) * Config::Physics.camera_smoothing;
    mCameraY += (targetCamY - mCameraY) * Config::Physics.camera_smoothing;

    float speed = mCar->GetSpeedKmH() / 3.6f;
    float targetZoom = Config::Physics.zoom_base - (speed * Config::Physics.zoom_speed_factor);
    if (targetZoom < Config::Physics.min_zoom) targetZoom = Config::Physics.min_zoom;
    if (targetZoom > Config::Physics.max_zoom) targetZoom = Config::Physics.max_zoom;
    mCameraZoom += (targetZoom - mCameraZoom) * Config::Physics.camera_smoothing;
    if (sUpdateCount < 5) HCR_Log("UpdatePlaying frame %d: done", sUpdateCount++);
}

void Game::RenderPlaying() {
    static int sRenderCount = 0;
    if (sRenderCount < 5) HCR_Log("RenderPlaying frame %d: start", sRenderCount);

    // 1. World Rendering
    Renderer2D::SetupCamera(mCameraX, mCameraY, mCameraZoom);

    if (mTerrain) {
        mTerrain->Render(mCameraX, mCameraY);
    }
    if (sRenderCount < 5) HCR_Log("RenderPlaying frame %d: terrain rendered", sRenderCount);

    if (mCar) {
        mCar->Render();
    }
    if (sRenderCount < 5) HCR_Log("RenderPlaying frame %d: car rendered", sRenderCount);

    // 2. HUD Rendering in Screen Space
    RenderHUD();
    if (sRenderCount < 5) HCR_Log("RenderPlaying frame %d: HUD rendered", sRenderCount++);
}

void Game::RenderHUD() {
    Renderer2D::SetupScreenSpace();

    // 1. Fuel Gauge (top left)
    Renderer2D::DrawFilledRect(10, 8, 160, 28, 0x90000000);
    Renderer2D::DrawSpriteScreen(&mTexFuelIcon, 14, 11, 18, 22);

    float fuelPct = (mCar ? mCar->GetFuel() : 0.0f) / Config::Physics.max_fuel;
    if (fuelPct < 0.0f) fuelPct = 0.0f;
    if (fuelPct > 1.0f) fuelPct = 1.0f;

    Renderer2D::DrawFilledRect(36, 14, 126, 16, 0xFF222222);
    unsigned int fuelColor = (fuelPct > 0.4f) ? 0xFF22C022 : (fuelPct > 0.2f ? 0xFF22C0C0 : 0xFF2222C0);
    Renderer2D::DrawFilledRect(38, 16, 122.0f * fuelPct, 12, fuelColor);

    // 2. Distance Meter (top center)
    Renderer2D::DrawFilledRect(190, 8, 100, 28, 0x90000000);
    char distBuf[32];
    snprintf(distBuf, sizeof(distBuf), "%dm", (int)mMaxDistance);
    float distW = mFont.GetTextWidth(distBuf, 0.42f);
    mFont.DrawText(distBuf, 190 + (100 - distW) * 0.5f, 12, 0.42f, 0xFFFFFFFF);

    // 3. Coins (top right)
    Renderer2D::DrawFilledRect(PSP_SCREEN_WIDTH - 120, 8, 110, 28, 0x90000000);
    Renderer2D::DrawSpriteScreen(&mTexCoinIcon, PSP_SCREEN_WIDTH - 114, 11, 20, 20);
    char coinBuf[32];
    snprintf(coinBuf, sizeof(coinBuf), "%d", mSessionCoins);
    mFont.DrawText(coinBuf, PSP_SCREEN_WIDTH - 88, 12, 0.42f, 0xFF00E5FF);

    // Stunt Banner (Backflip / Frontflip)
    if (mStuntTimer > 0.0f && mStuntName) {
        float tw = mFont.GetTextWidth(mStuntName, 0.55f);
        mFont.DrawText(mStuntName, (PSP_SCREEN_WIDTH - tw) * 0.5f, 55, 0.55f, 0xFF00E5FF);

        char cBuf[32];
        snprintf(cBuf, sizeof(cBuf), "+%d COINS", mStuntCoins);
        float cw = mFont.GetTextWidth(cBuf, 0.38f);
        mFont.DrawText(cBuf, (PSP_SCREEN_WIDTH - cw) * 0.5f, 78, 0.38f, 0xFFFFFFFF);
    }

    // 4. Bottom Pedals (Visual Feedback)
    bool brakePressed = (System::ButtonsHeld & (PSP_CTRL_LTRIGGER | PSP_CTRL_SQUARE | PSP_CTRL_LEFT)) != 0;
    bool gasPressed   = (System::ButtonsHeld & (PSP_CTRL_RTRIGGER | PSP_CTRL_CROSS | PSP_CTRL_RIGHT)) != 0;

    unsigned int brakeColor = brakePressed ? 0xFFFFFFFF : 0x80FFFFFF;
    Renderer2D::DrawSpriteScreen(&mTexPedalBrake, 20, PSP_SCREEN_HEIGHT - 65, 55, 55, brakeColor);

    unsigned int gasColor = gasPressed ? 0xFFFFFFFF : 0x80FFFFFF;
    Renderer2D::DrawSpriteScreen(&mTexPedalGas, PSP_SCREEN_WIDTH - 75, PSP_SCREEN_HEIGHT - 65, 55, 55, gasColor);
}

void Game::UpdateGameOver() {
    AudioEngine::UpdateEngineSound(0.0f, false);

    // Press Cross to restart race
    if (System::ButtonsPressed & PSP_CTRL_CROSS) {
        StartRace();
    }

    // Press Circle or Start to return to Garage
    if ((System::ButtonsPressed & PSP_CTRL_CIRCLE) || (System::ButtonsPressed & PSP_CTRL_START)) {
        EndRace();
        mState = STATE_GARAGE;
        AudioEngine::PlaySFX(SFX_CLICK);
    }
}

void Game::RenderGameOver() {
    // Keep rendering world behind gameover overlay
    Renderer2D::SetupCamera(mCameraX, mCameraY, mCameraZoom);
    if (mTerrain) mTerrain->Render(mCameraX, mCameraY);
    if (mCar) mCar->Render();

    // Dark overlay banner
    Renderer2D::SetupScreenSpace();
    Renderer2D::DrawFilledRect(50, 35, 380, 200, 0xD0111111);

    // Banner header
    unsigned int headerColor = mGameOverReasonDriverDown ? 0xFF2222C0 : 0xFF22A0C0;
    Renderer2D::DrawFilledRect(50, 35, 380, 40, headerColor);

    const char* titleText = mGameOverReasonDriverDown ? "DRIVER DOWN!" : "OUT OF FUEL!";
    float tw = mFont.GetTextWidth(titleText, 0.55f);
    mFont.DrawText(titleText, 50 + (380 - tw) * 0.5f, 42, 0.55f, 0xFFFFFFFF);

    // Stats
    char buf[64];
    snprintf(buf, sizeof(buf), "DISTANCE: %d METERS", (int)mMaxDistance);
    mFont.DrawText(buf, 80, 95, 0.42f, 0xFFFFFFFF);

    snprintf(buf, sizeof(buf), "COINS EARNED: +%d", mSessionCoins);
    mFont.DrawText(buf, 80, 125, 0.42f, 0xFF00E5FF);

    snprintf(buf, sizeof(buf), "BEST RECORD: %d METERS", PlayerInfo::HighScores[mCurrentStage]);
    mFont.DrawText(buf, 80, 155, 0.38f, 0xFF88DDFF);

    // Buttons
    // [X] Retry
    Renderer2D::DrawFilledRect(70, 190, 150, 32, 0xFF257518);
    mFont.DrawText("[X] RETRY", 100, 196, 0.38f, 0xFFFFFFFF);

    // [O] Garage
    Renderer2D::DrawFilledRect(260, 190, 150, 32, 0xFF453020);
    mFont.DrawText("[O] GARAGE", 285, 196, 0.38f, 0xFFFFFFFF);
}

