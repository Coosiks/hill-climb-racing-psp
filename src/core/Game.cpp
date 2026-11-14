#include "Game.h"
#include "Config.h"
#include "graphics/ParticleSystem.h"
#include <math.h>
#include <stdio.h>
#include <stdarg.h>

#if defined(__PSP__)
#include <psprtc.h>
static u64 sTotalStepTicks = 0;
static int sStepCount = 0;
#endif

static bool sBenchmarkMode = false;
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

static uint64_t FNV1a_64(const void* data, size_t numBytes) {
    const uint8_t* ptr = (const uint8_t*)data;
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < numBytes; ++i) {
        hash ^= ptr[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static void RunHeightfieldHashBenchmark() {
    const int NUM_HASH_POINTS = 4001; // 2000m / 0.5m + 1
    float* heightBuffer = (float*)malloc(NUM_HASH_POINTS * sizeof(float));
    if (!heightBuffer) return;

    b2World dummyWorld(b2Vec2(0.0f, -10.0f));
    const char* stageNames[28] = {
        "Countryside", "Desert", "Arctic", "Moon",
        "Highway", "Cave", "Mars", "Forest",
        "Mountain", "Mudpool", "Volcano", "Beach",
        "Rollercoaster", "Night", "Rooftops", "Junkyard",
        "Construction", "Rainbow", "Seasons", "Bootcamp",
        "Xmas", "Alienplanet", "Arcticcave", "Ragnarok",
        "Factory", "Haunted", "Northpole", "Nuclear"
    };

    FILE* hf = fopen("heightfield_hashes_psp.txt", "w");
    if (!hf) hf = fopen("ms0:/PSP/GAME/HCR/heightfield_hashes_psp.txt", "w");

    if (hf) {
        fprintf(hf, "=== PSP Heightfield Hashes (4001 pts, 0-2000m, step 0.5m) ===\n");
    }

    for (int s = 0; s < 28; ++s) {
        Terrain terrain(&dummyWorld, (StageType)s);
        for (int i = 0; i < NUM_HASH_POINTS; ++i) {
            float x = (float)i * 0.5f;
            heightBuffer[i] = terrain.GetHeightAtX(x);
        }
        uint64_t hash = FNV1a_64(heightBuffer, NUM_HASH_POINTS * sizeof(float));
        printf("[HASH] Stage %-13s (4001 pts) -> FNV-1a: 0x%016llx\n",
               stageNames[s], (unsigned long long)hash);
        HCR_Log("[HASH] Stage %-13s (4001 pts) -> FNV-1a: 0x%016llx",
                stageNames[s], (unsigned long long)hash);
        if (hf) {
            fprintf(hf, "%s=0x%016llx\n", stageNames[s], (unsigned long long)hash);
        }
    }
    if (hf) fclose(hf);
    free(heightBuffer);
}

Game::Game()
    : mState(STATE_GARAGE), mCurrentStage(0), mCurrentVehicle(0),
      mGarageTab(0), mSelectedUpgradeSlot(0),
      mPhysics(NULL), mTerrain(NULL), mCar(NULL),
      mSessionCoins(0), mRaceCollectedCoins(0), mRaceTrickBonusCoins(0),
      mRaceDistanceBonusCoins(0), mIsNewRecord(false),
      mMaxDistance(0.0f), mGameOverReasonDriverDown(false),
      mStuntTimer(0.0f), mStuntName(NULL), mStuntCoins(0),
      mOutOfFuelStopTimer(0.0f), mOutOfFuelFallbackTimer(0.0f),
      mCameraX(0.0f), mCameraY(0.0f), mCameraZoom(1.0f),
      mLastTime(0.0), mAccumulator(0.0f), mRenderAlpha(1.0f) {
}

Game::~Game() {
    EndRace();
    AudioEngine::Shutdown();
}

#if defined(__PSP__)
static float sAssetLoadTimeUI = 0.0f;
static float sAssetLoadTimeTerrain = 0.0f;
static float sAssetLoadTimeCar = 0.0f;
#endif

void Game::LoadStagePreview() {
    mTexStagePreview.Destroy();
    if (mCurrentStage >= 0 && mCurrentStage < Config::StageCount) {
        char path[128];
        snprintf(path, sizeof(path), "assets/stage-icon-%s.png", Config::Stages[mCurrentStage].id);
        mTexStagePreview.LoadPNG(path);
    }
}

void Game::LoadVehiclePreview() {
    mTexVehiclePreview.Destroy();
    const VehicleConfig* vConfig = Config::GetVehicle(mCurrentVehicle);
    const char* prevPath = (vConfig && vConfig->preview_texture[0]) ? vConfig->preview_texture :
                           ((vConfig && vConfig->body_texture[0]) ? vConfig->body_texture : "assets/Car.png");
    mTexVehiclePreview.LoadPNG(prevPath);
}

bool Game::CanStartRace() const {
    if (mCurrentStage < 0 || mCurrentStage >= Config::StageCount) return false;
    if (mCurrentVehicle < 0 || mCurrentVehicle >= Config::VehicleCount) return false;
    uint32_t sHash = PlayerInfo::HashId(Config::Stages[mCurrentStage].id);
    if (!PlayerInfo::IsStageUnlocked(sHash)) return false;
    uint32_t vHash = PlayerInfo::HashId(Config::Vehicles[mCurrentVehicle].id);
    if (!PlayerInfo::IsVehicleUnlocked(vHash)) return false;
    return true;
}

void Game::CommitRaceResults() {
    PlayerInfo::AddCoins(mSessionCoins);
    mIsNewRecord = false;
    if (mCurrentStage >= 0 && mCurrentStage < Config::StageCount) {
        uint32_t stageHash = PlayerInfo::HashId(Config::Stages[mCurrentStage].id);
        int curBest = PlayerInfo::GetStageRecord(stageHash);
        if ((int)mMaxDistance > curBest) {
            mIsNewRecord = true;
            PlayerInfo::SetStageRecord(stageHash, (int)mMaxDistance);
        }
        if (mCurrentStage < STAGE_COUNT && (int)mMaxDistance > PlayerInfo::HighScores[mCurrentStage]) {
            PlayerInfo::HighScores[mCurrentStage] = (int)mMaxDistance;
        }
    }
    PlayerInfo::Save();
}

bool Game::Init() {
    PlayerInfo::Load();
    AudioEngine::Init();

#if defined(__PSP__)
    u32 t0 = sceKernelGetSystemTimeLow();
#endif

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

    mTexStageLocked.LoadPNG("assets/stage-icon-locked.png");

    // Sync active selection with Config
    uint32_t selStage = PlayerInfo::GetSelectedStageIdHash();
    for (int i = 0; i < Config::StageCount; ++i) {
        if (PlayerInfo::HashId(Config::Stages[i].id) == selStage) {
            mCurrentStage = i;
            break;
        }
    }
    uint32_t selVeh = PlayerInfo::GetSelectedVehicleIdHash();
    for (int i = 0; i < Config::VehicleCount; ++i) {
        if (PlayerInfo::HashId(Config::Vehicles[i].id) == selVeh) {
            mCurrentVehicle = i;
            break;
        }
    }

    LoadStagePreview();
    LoadVehiclePreview();

#if defined(__PSP__)
    u32 t1 = sceKernelGetSystemTimeLow();
    sAssetLoadTimeUI = (float)(t1 - t0) / 1000.0f;
    printf("[PERF_ASSETS] Game::Init UI textures loaded in %u us (%.2f ms)\n", (unsigned int)(t1 - t0), sAssetLoadTimeUI);
    HCR_Log("[PERF_ASSETS] Game::Init UI textures loaded in %u us (%.2f ms)", (unsigned int)(t1 - t0), sAssetLoadTimeUI);
#endif

    mState = STATE_GARAGE;
    return true;
}

void Game::StartRace() {
    if (!CanStartRace()) {
        AudioEngine::PlaySFX(SFX_CLICK);
        return;
    }
    HCR_Log("StartRace: begin");
    EndRace();

    HCR_Log("StartRace: creating PhysicsWorld");
    float grav = (mCurrentStage >= 0 && mCurrentStage < Config::StageCount) ? Config::Stages[mCurrentStage].gravity : Config::Stages[0].gravity;
    mPhysics = new PhysicsWorld(grav, this);

    HCR_Log("StartRace: creating Terrain");
#if defined(__PSP__)
    u32 tc0 = sceKernelGetSystemTimeLow();
#endif
    mTerrain = new Terrain(mPhysics->GetWorld(), (StageType)mCurrentStage);
#if defined(__PSP__)
    u32 tc1 = sceKernelGetSystemTimeLow();
    sAssetLoadTimeTerrain = (float)(tc1 - tc0) / 1000.0f;
    HCR_Log("[PERF_ASSETS] Terrain loaded in %u us (%.2f ms)", (unsigned int)(tc1 - tc0), sAssetLoadTimeTerrain);
#endif
    mPhysics->SetTerrain(mTerrain);

    HCR_Log("StartRace: creating PlayerCar");
#if defined(__PSP__)
    u32 tp0 = sceKernelGetSystemTimeLow();
#endif
    b2Vec2 startPos(0.0f, 2.5f);
    const VehicleConfig* vConfig = Config::GetVehicle(mCurrentVehicle);
    uint32_t vHash = PlayerInfo::HashId(vConfig ? vConfig->id : "jeep");
    SavedVehicleEntry* vEntry = PlayerInfo::GetVehicleEntry(vHash);
    int eng  = vEntry ? vEntry->levelEngine : PlayerInfo::EngineLevel;
    int susp = vEntry ? vEntry->levelSuspension : PlayerInfo::SuspensionLevel;
    int tire = vEntry ? vEntry->levelTires : PlayerInfo::TiresLevel;
    int awd  = vEntry ? vEntry->level4th : PlayerInfo::AWDLevel;

    mCar = new PlayerCar(mPhysics->GetWorld(), startPos, eng, susp, tire, awd, vConfig, mTerrain);
#if defined(__PSP__)
    u32 tp1 = sceKernelGetSystemTimeLow();
    sAssetLoadTimeCar = (float)(tp1 - tp0) / 1000.0f;
    HCR_Log("[PERF_ASSETS] PlayerCar loaded in %u us (%.2f ms)", (unsigned int)(tp1 - tp0), sAssetLoadTimeCar);
#endif

    mSessionCoins = 0;
    mRaceCollectedCoins = 0;
    mRaceTrickBonusCoins = 0;
    mRaceDistanceBonusCoins = 0;
    mIsNewRecord = false;
    mMaxDistance = 0.0f;
    mStuntTimer = 0.0f;
    mStuntName = NULL;
    mStuntCoins = 0;
    mOutOfFuelStopTimer = 0.0f;
    mOutOfFuelFallbackTimer = 0.0f;
    mCameraX = 0.0f;
    mCameraY = 2.0f;
    mLastTime = System::GetTimeSeconds();
    mAccumulator = 0.0f;
    mRenderAlpha = 1.0f;
    mState = STATE_PLAYING;

    ParticleSystem::Init();
    ParticleSystem::Reset();

    AudioEngine::PlaySFX(SFX_CLICK);
    HCR_Log("StartRace: complete, state = STATE_PLAYING");
}

void Game::EndRace() {
    AudioEngine::UpdateEngineSound(0.0f, false);
    mAccumulator = 0.0f;
    ParticleSystem::Reset();

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
        CommitRaceResults();
    }
}

void Game::OnCoinCollected(int value) {
    float mult = mTerrain ? mTerrain->GetCoinMultiplier() : 1.0f;
    int earned = (int)(value * mult);
    mRaceCollectedCoins += earned;
    mSessionCoins += earned;
    AudioEngine::PlaySFX(SFX_COIN);
}

void Game::OnFuelCollected() {
    if (mCar) {
        mCar->AddFuel(Config::Physics.max_fuel);
    }
    AudioEngine::PlaySFX(SFX_REFUEL);
}

void Game::Run() {
    mLastTime = System::GetTimeSeconds();
    mAccumulator = 0.0f;

    while (System::IsRunning()) {
        double currentTime = System::GetTimeSeconds();
        float frameTime = (float)(currentTime - mLastTime);
        mLastTime = currentTime;

        System::PollInput();

        switch (mState) {
            case STATE_GARAGE:
                mAccumulator = 0.0f;
                UpdateGarage();
                break;
            case STATE_PLAYING:
                UpdatePlaying(frameTime);
                break;
            case STATE_GAMEOVER:
                mAccumulator = 0.0f;
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

        Renderer2D::EndFrame();
        System::EndFrame();
    }
}

void Game::UpdateGarage() {
    static bool sCheckedBenchmark = false;
    if (!sCheckedBenchmark) {
        sCheckedBenchmark = true;
#if defined(__PSP__)
        RunHeightfieldHashBenchmark();
#endif
        FILE* bf = fopen("benchmark.txt", "r");
        if (!bf) bf = fopen("ms0:/PSP/GAME/HCR/benchmark.txt", "r");
        if (bf) {
            fclose(bf);
            sBenchmarkMode = true;
            printf("[PERF] benchmark.txt found: enabling automated benchmark mode\n");
            HCR_Log("[PERF] benchmark.txt found: enabling automated benchmark mode");
            RunHeightfieldHashBenchmark();
        }
    }
    if (sBenchmarkMode) {
        static int sGarageDelay = 0;
        if (++sGarageDelay >= 10) {
            StartRace();
            return;
        }
    }

    // Switch tabs with L / R Triggers
    if (System::ButtonsPressed & PSP_CTRL_LTRIGGER) {
        mGarageTab = (mGarageTab + 2) % 3;
        AudioEngine::PlaySFX(SFX_CLICK);
    }
    if (System::ButtonsPressed & PSP_CTRL_RTRIGGER) {
        mGarageTab = (mGarageTab + 1) % 3;
        AudioEngine::PlaySFX(SFX_CLICK);
    }

    if (mGarageTab == 0) {
        // Tab 0: Stage Selection
        if (System::ButtonsPressed & PSP_CTRL_LEFT) {
            if (Config::StageCount > 0) {
                mCurrentStage = (mCurrentStage - 1 + Config::StageCount) % Config::StageCount;
                LoadStagePreview();
                PlayerInfo::SetSelectedStageIdHash(PlayerInfo::HashId(Config::Stages[mCurrentStage].id));
                AudioEngine::PlaySFX(SFX_CLICK);
            }
        }
        if (System::ButtonsPressed & PSP_CTRL_RIGHT) {
            if (Config::StageCount > 0) {
                mCurrentStage = (mCurrentStage + 1) % Config::StageCount;
                LoadStagePreview();
                PlayerInfo::SetSelectedStageIdHash(PlayerInfo::HashId(Config::Stages[mCurrentStage].id));
                AudioEngine::PlaySFX(SFX_CLICK);
            }
        }
        if (System::ButtonsPressed & PSP_CTRL_CROSS) {
            uint32_t sHash = PlayerInfo::HashId(Config::Stages[mCurrentStage].id);
            if (!PlayerInfo::IsStageUnlocked(sHash)) {
                int cost = Config::Stages[mCurrentStage].unlock_cost;
                if (PlayerInfo::GetCoins() >= cost) {
                    PlayerInfo::AddCoins(-cost);
                    PlayerInfo::UnlockStage(sHash);
                    PlayerInfo::Save();
                    AudioEngine::PlaySFX(SFX_COIN);
                }
            } else {
                StartRace();
            }
        }
    } else if (mGarageTab == 1) {
        // Tab 1: Vehicle Selection
        if (System::ButtonsPressed & PSP_CTRL_LEFT) {
            if (Config::VehicleCount > 0) {
                mCurrentVehicle = (mCurrentVehicle - 1 + Config::VehicleCount) % Config::VehicleCount;
                LoadVehiclePreview();
                PlayerInfo::SetSelectedVehicleIdHash(PlayerInfo::HashId(Config::Vehicles[mCurrentVehicle].id));
                AudioEngine::PlaySFX(SFX_CLICK);
            }
        }
        if (System::ButtonsPressed & PSP_CTRL_RIGHT) {
            if (Config::VehicleCount > 0) {
                mCurrentVehicle = (mCurrentVehicle + 1) % Config::VehicleCount;
                LoadVehiclePreview();
                PlayerInfo::SetSelectedVehicleIdHash(PlayerInfo::HashId(Config::Vehicles[mCurrentVehicle].id));
                AudioEngine::PlaySFX(SFX_CLICK);
            }
        }
        if (System::ButtonsPressed & PSP_CTRL_CROSS) {
            uint32_t vHash = PlayerInfo::HashId(Config::Vehicles[mCurrentVehicle].id);
            if (!PlayerInfo::IsVehicleUnlocked(vHash)) {
                int cost = Config::Vehicles[mCurrentVehicle].unlock_cost;
                if (PlayerInfo::GetCoins() >= cost) {
                    PlayerInfo::AddCoins(-cost);
                    PlayerInfo::UnlockVehicle(vHash);
                    PlayerInfo::SetSelectedVehicleIdHash(vHash);
                    PlayerInfo::Save();
                    AudioEngine::PlaySFX(SFX_COIN);
                }
            } else {
                mGarageTab = 2; // Jump to Upgrades tab
                AudioEngine::PlaySFX(SFX_CLICK);
            }
        }
    } else if (mGarageTab == 2) {
        // Tab 2: Upgrades
        if (System::ButtonsPressed & PSP_CTRL_LEFT) {
            mSelectedUpgradeSlot = (mSelectedUpgradeSlot + 3) % 4;
            AudioEngine::PlaySFX(SFX_CLICK);
        }
        if (System::ButtonsPressed & PSP_CTRL_RIGHT) {
            mSelectedUpgradeSlot = (mSelectedUpgradeSlot + 1) % 4;
            AudioEngine::PlaySFX(SFX_CLICK);
        }

        int slotToUpgrade = -1;
        if (System::ButtonsPressed & PSP_CTRL_TRIANGLE) slotToUpgrade = 0;
        else if (System::ButtonsPressed & PSP_CTRL_SQUARE) slotToUpgrade = 1;
        else if (System::ButtonsPressed & PSP_CTRL_CIRCLE) slotToUpgrade = 2;
        else if (System::ButtonsPressed & PSP_CTRL_SELECT) slotToUpgrade = 3;
        else if (System::ButtonsPressed & PSP_CTRL_CROSS) slotToUpgrade = mSelectedUpgradeSlot;

        if (slotToUpgrade >= 0) {
            uint32_t vHash = PlayerInfo::HashId(Config::Vehicles[mCurrentVehicle].id);
            int curLevel = PlayerInfo::GetVehicleUpgrade(vHash, slotToUpgrade);
            int maxLevel = Config::Vehicles[mCurrentVehicle].max_upgrade_level;
            int cost = GetUpgradeCost(curLevel);
            if (PlayerInfo::GetCoins() >= cost && curLevel < maxLevel) {
                PlayerInfo::AddCoins(-cost);
                PlayerInfo::SetVehicleUpgrade(vHash, slotToUpgrade, curLevel + 1);
                PlayerInfo::Save();
                AudioEngine::PlaySFX(SFX_UPGRADE);
            }
        }
    }

    // Start Race prompt
    if (System::ButtonsPressed & PSP_CTRL_START) {
        StartRace();
    }
}

void Game::RenderGarage() {
    Renderer2D::SetupScreenSpace();

    // Dark dirt background
    Renderer2D::DrawFilledRect(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, 0xFF181512);

    // Title bar (0..34)
    Renderer2D::DrawFilledRect(0, 0, PSP_SCREEN_WIDTH, 34, 0xFF352518);
    mFont.DrawText("HILL CLIMB RACING", 16, 7, 0.45f, 0xFFFFFFFF);

    // Coins in top right
    char strBuf[64];
    snprintf(strBuf, sizeof(strBuf), "%lld", (long long)PlayerInfo::GetCoins());
    float coinTextW = mFont.GetTextWidth(strBuf, 0.45f);
    Renderer2D::DrawSpriteScreen(&mTexCoinIcon, PSP_SCREEN_WIDTH - 30 - coinTextW, 6, 22, 22);
    mFont.DrawText(strBuf, PSP_SCREEN_WIDTH - 4 - coinTextW, 7, 0.45f, 0xFF00E5FF);

    // Navigation Tabs bar (35..58)
    Renderer2D::DrawFilledRect(0, 35, PSP_SCREEN_WIDTH, 24, 0xFF261A10);
    const char* tabs[3] = { "[L] STAGES", "VEHICLES", "UPGRADES [R]" };
    float tabW = PSP_SCREEN_WIDTH / 3.0f;
    for (int t = 0; t < 3; ++t) {
        if (t == mGarageTab) {
            Renderer2D::DrawFilledRect(t * tabW, 35, tabW, 24, 0xFF5A3C24);
            mFont.DrawText(tabs[t], t * tabW + (tabW - mFont.GetTextWidth(tabs[t], 0.35f)) * 0.5f, 39, 0.35f, 0xFF00E5FF);
        } else {
            mFont.DrawText(tabs[t], t * tabW + (tabW - mFont.GetTextWidth(tabs[t], 0.32f)) * 0.5f, 40, 0.32f, 0xFF888888);
        }
    }

    if (mGarageTab == 0) {
        // Tab 0: Stage Selection
        Renderer2D::DrawFilledRect(15, 63, PSP_SCREEN_WIDTH - 30, 148, 0xFF352518);

        uint32_t sHash = PlayerInfo::HashId(Config::Stages[mCurrentStage].id);
        bool isUnlocked = PlayerInfo::IsStageUnlocked(sHash);

        // Preview Card (Lazy loaded)
        if (isUnlocked) {
            Renderer2D::DrawSpriteScreen(&mTexStagePreview, 25, 75, 110, 72);
        } else {
            Renderer2D::DrawSpriteScreen(&mTexStageLocked, 25, 75, 110, 72);
        }

        snprintf(strBuf, sizeof(strBuf), "STAGE: %s (%d/%d)", Config::Stages[mCurrentStage].name, mCurrentStage + 1, Config::StageCount);
        mFont.DrawText(strBuf, 145, 75, 0.42f, 0xFFFFFFFF);

        snprintf(strBuf, sizeof(strBuf), "GRAVITY: %.1f | FRICTION: %.2f | COIN BONUS: x%.2f",
                 Config::Stages[mCurrentStage].gravity, Config::Stages[mCurrentStage].friction, Config::Stages[mCurrentStage].coin_multiplier);
        mFont.DrawText(strBuf, 145, 100, 0.30f, 0xFFAAAAAA);

        int bestRecord = PlayerInfo::GetStageRecord(sHash);
        snprintf(strBuf, sizeof(strBuf), "BEST RECORD: %d METERS", bestRecord);
        mFont.DrawText(strBuf, 145, 122, 0.35f, 0xFF88DDFF);

        if (isUnlocked) {
            mFont.DrawText("STATUS: UNLOCKED", 145, 148, 0.35f, 0xFF55FF55);
            mFont.DrawText("< LEFT / RIGHT TO BROWSE STAGES >", 130, 185, 0.32f, 0xFFCCCCCC);
        } else {
            int cost = Config::Stages[mCurrentStage].unlock_cost;
            snprintf(strBuf, sizeof(strBuf), "UNLOCK COST: %d COINS", cost);
            mFont.DrawText(strBuf, 145, 148, 0.35f, 0xFFFFCC00);

            if (PlayerInfo::GetCoins() >= cost) {
                Renderer2D::DrawFilledRect(145, 172, 170, 28, 0xFF257518);
                mFont.DrawText("[X] UNLOCK STAGE", 160, 178, 0.35f, 0xFFFFFFFF);
            } else {
                Renderer2D::DrawFilledRect(145, 172, 170, 28, 0xFF552222);
                mFont.DrawText("NEED MORE COINS", 160, 178, 0.35f, 0xFFAAAAAA);
            }
        }
    } else if (mGarageTab == 1) {
        // Tab 1: Vehicle Selection
        Renderer2D::DrawFilledRect(15, 63, PSP_SCREEN_WIDTH - 30, 148, 0xFF352518);

        uint32_t vHash = PlayerInfo::HashId(Config::Vehicles[mCurrentVehicle].id);
        bool isUnlocked = PlayerInfo::IsVehicleUnlocked(vHash);

        Renderer2D::DrawSpriteScreen(&mTexVehiclePreview, 25, 75, 110, 72);

        snprintf(strBuf, sizeof(strBuf), "VEHICLE: %s (%d/%d)", Config::Vehicles[mCurrentVehicle].name, mCurrentVehicle + 1, Config::VehicleCount);
        mFont.DrawText(strBuf, 145, 75, 0.42f, 0xFFFFFFFF);

        snprintf(strBuf, sizeof(strBuf), "SPEED: %.1f | TORQUE: %.1f | SPRING: %.1f",
                 Config::Vehicles[mCurrentVehicle].base_motor_speed,
                 Config::Vehicles[mCurrentVehicle].base_motor_torque,
                 Config::Vehicles[mCurrentVehicle].base_spring_freq);
        mFont.DrawText(strBuf, 145, 100, 0.30f, 0xFFAAAAAA);

        if (isUnlocked) {
            mFont.DrawText("STATUS: UNLOCKED & EQUIPPED", 145, 125, 0.35f, 0xFF55FF55);
            Renderer2D::DrawFilledRect(145, 155, 170, 28, 0xFF355520);
            mFont.DrawText("[X] TUNE UPGRADES", 160, 161, 0.35f, 0xFFFFFFFF);
            mFont.DrawText("< LEFT / RIGHT TO BROWSE VEHICLES >", 120, 192, 0.32f, 0xFFCCCCCC);
        } else {
            int cost = Config::Vehicles[mCurrentVehicle].unlock_cost;
            snprintf(strBuf, sizeof(strBuf), "UNLOCK COST: %d COINS", cost);
            mFont.DrawText(strBuf, 145, 125, 0.35f, 0xFFFFCC00);

            if (PlayerInfo::GetCoins() >= cost) {
                Renderer2D::DrawFilledRect(145, 155, 170, 28, 0xFF257518);
                mFont.DrawText("[X] UNLOCK VEHICLE", 155, 161, 0.35f, 0xFFFFFFFF);
            } else {
                Renderer2D::DrawFilledRect(145, 155, 170, 28, 0xFF552222);
                mFont.DrawText("NEED MORE COINS", 160, 161, 0.35f, 0xFFAAAAAA);
            }
        }
    } else if (mGarageTab == 2) {
        // Tab 2: Upgrades Grid
        Renderer2D::DrawFilledRect(15, 63, PSP_SCREEN_WIDTH - 30, 26, 0xFF352518);
        snprintf(strBuf, sizeof(strBuf), "TUNING: %s", Config::Vehicles[mCurrentVehicle].name);
        mFont.DrawText(strBuf, 25, 68, 0.38f, 0xFFFFFFFF);

        float upgW = 104.0f;
        float upgH = 116.0f;
        float startX = 15.0f;
        float startY = 93.0f;
        float spacing = 115.0f;

        uint32_t vHash = PlayerInfo::HashId(Config::Vehicles[mCurrentVehicle].id);
        int maxLvl = Config::Vehicles[mCurrentVehicle].max_upgrade_level;

        const VehicleConfig* vConfig = Config::GetVehicle(mCurrentVehicle);
        const char* fourthName = "4WD";
        const char* fourthKey = "[SEL] 4WD";
        if (vConfig) {
            switch (vConfig->fourth_upgrade_type) {
                case 0: fourthName = "4WD"; fourthKey = "[SEL] 4WD"; break;
                case 1: fourthName = "FUEL"; fourthKey = "[SEL] FUEL"; break;
                case 2: fourthName = "AIR CTRL"; fourthKey = "[SEL] AIR"; break;
                case 3: fourthName = "DOWNFORCE"; fourthKey = "[SEL] DOWN"; break;
                case 4: fourthName = "STABILITY"; fourthKey = "[SEL] STAB"; break;
                case 5: fourthName = "BOOST"; fourthKey = "[SEL] BOOST"; break;
                default: fourthName = "SPECIAL"; fourthKey = "[SEL] SPEC"; break;
            }
        }

        struct UpgCard {
            const char* key;
            const char* name;
            int level;
            Texture* tex;
        } cards[4] = {
            { "[^] TRIANGLE", "ENGINE", PlayerInfo::GetVehicleUpgrade(vHash, 0), &mTexUpgEngine },
            { "[] SQUARE", "SUSPENSION", PlayerInfo::GetVehicleUpgrade(vHash, 1), &mTexUpgSusp },
            { "(O) CIRCLE", "TIRES", PlayerInfo::GetVehicleUpgrade(vHash, 2), &mTexUpgTires },
            { fourthKey, fourthName, PlayerInfo::GetVehicleUpgrade(vHash, 3), &mTexUpgAWD }
        };

        for (int i = 0; i < 4; ++i) {
            float cx = startX + i * spacing;
            unsigned int boxColor = (i == mSelectedUpgradeSlot) ? 0xFF553822 : 0xFF322316;
            Renderer2D::DrawFilledRect(cx, startY, upgW, upgH, boxColor);

            if (i == mSelectedUpgradeSlot) {
                // Highlight border
                float th = 2.0f;
                Renderer2D::DrawFilledRect(cx, startY, upgW, th, 0xFF00E5FF);
                Renderer2D::DrawFilledRect(cx, startY + upgH - th, upgW, th, 0xFF00E5FF);
                Renderer2D::DrawFilledRect(cx, startY, th, upgH, 0xFF00E5FF);
                Renderer2D::DrawFilledRect(cx + upgW - th, startY, th, upgH, 0xFF00E5FF);
            }

            // Icon
            Renderer2D::DrawSpriteScreen(cards[i].tex, cx + 6, startY + 6, 36, 36);

            // Key Hint
            mFont.DrawText(cards[i].key, cx + 46, startY + 12, 0.26f, 0xFF00CCFF);

            // Name
            mFont.DrawText(cards[i].name, cx + 8, startY + 48, 0.30f, 0xFFFFFFFF);

            // Level
            snprintf(strBuf, sizeof(strBuf), "LVL %d/%d", cards[i].level, maxLvl);
            mFont.DrawText(strBuf, cx + 8, startY + 68, 0.28f, 0xFFCCCCCC);

            // Cost
            if (cards[i].level >= maxLvl) {
                mFont.DrawText("MAX", cx + 8, startY + 88, 0.32f, 0xFF55FF55);
            } else {
                int cost = GetUpgradeCost(cards[i].level);
                snprintf(strBuf, sizeof(strBuf), "%d C", cost);
                unsigned int cColor = (PlayerInfo::GetCoins() >= cost) ? 0xFF00E5FF : 0xFF5555AA;
                mFont.DrawText(strBuf, cx + 8, startY + 88, 0.30f, cColor);
            }
        }
    }

    // Bottom Start prompt (218..262)
    if (CanStartRace()) {
        Renderer2D::DrawFilledRect(15, 218, PSP_SCREEN_WIDTH - 30, 44, 0xFF257518);
        mFont.DrawText("PRESS CROSS OR START TO RACE!", 95, 230, 0.45f, 0xFFFFFFFF);
    } else {
        Renderer2D::DrawFilledRect(15, 218, PSP_SCREEN_WIDTH - 30, 44, 0xFF652518);
        mFont.DrawText("STAGE OR VEHICLE LOCKED! UNLOCK FIRST", 65, 230, 0.38f, 0xFFFFAAAA);
    }
}

void Game::UpdatePhysicsStep(float dt) {
    if (!mCar || !mPhysics || !mTerrain) return;

    // Controls
    if (sBenchmarkMode) {
        mCar->Accelerate();
    } else if ((System::ButtonsHeld & PSP_CTRL_RTRIGGER) || (System::ButtonsHeld & PSP_CTRL_CROSS) || (System::ButtonsHeld & PSP_CTRL_RIGHT)) {
        mCar->Accelerate();
    } else if ((System::ButtonsHeld & PSP_CTRL_LTRIGGER) || (System::ButtonsHeld & PSP_CTRL_SQUARE) || (System::ButtonsHeld & PSP_CTRL_LEFT)) {
        mCar->Decelerate();
    } else {
        mCar->Idle();
    }

    // Thruster Jet control (Moonlander)
    if (sBenchmarkMode) {
        mCar->SetThruster(false);
    } else {
        bool thruster = (System::ButtonsHeld & (PSP_CTRL_TRIANGLE | PSP_CTRL_UP)) != 0;
        mCar->SetThruster(thruster);
    }

    // Capture previous transforms right before physics integration
    mCar->CapturePreviousTransforms();

    // Step physics (with PSP telemetry)
#if defined(__PSP__)
    u64 t0 = 0, t1 = 0;
    sceRtcGetCurrentTick(&t0);
    mPhysics->Step(dt);
    sceRtcGetCurrentTick(&t1);
    sTotalStepTicks += (t1 - t0);
    sStepCount++;
    if (sStepCount % 60 == 0) {
        u32 res = sceRtcGetTickResolution();
        if (res == 0) res = 1000000;
        double avgStepMs = ((double)sTotalStepTicks / (double)sStepCount) * 1000.0 / (double)res;
        printf("[PERF] Physics step avg: %.3f ms (%d steps)\n", avgStepMs, sStepCount);
        HCR_Log("[PERF] Physics step avg: %.3f ms (%d steps)", avgStepMs, sStepCount);
    }
#else
    mPhysics->Step(dt);
#endif

    b2Vec2 pos = mCar->GetPosition();
    mTerrain->ProcessDeferredDestroy(pos.x);

    mCar->Update(dt);
    mTerrain->Update(pos.x);

    // Update Distance
    if (pos.x > mMaxDistance) {
        mMaxDistance = pos.x;
    }

    // Stunt Tracking
    // 1. Flips in air
    int flip = mCar->PopFlips();
    if (flip > 0) {
        int bonus = Config::Physics.flip_bonus_coins;
        mRaceTrickBonusCoins += bonus;
        mSessionCoins += bonus;
        mStuntName = "BACKFLIP!";
        mStuntCoins = bonus;
        mStuntTimer = 2.0f;
        AudioEngine::PlaySFX(SFX_COIN);
    } else if (flip < 0) {
        int bonus = Config::Physics.flip_bonus_coins;
        mRaceTrickBonusCoins += bonus;
        mSessionCoins += bonus;
        mStuntName = "FRONTFLIP!";
        mStuntCoins = bonus;
        mStuntTimer = 2.0f;
        AudioEngine::PlaySFX(SFX_COIN);
    }

    // 2. Air Time bonus
    float airSec = 0.0f;
    int airBonus = mCar->PopAirTimeBonus(&airSec);
    if (airBonus > 0) {
        mRaceTrickBonusCoins += airBonus;
        mSessionCoins += airBonus;
        mStuntName = "AIR TIME!";
        mStuntCoins = airBonus;
        mStuntTimer = 2.0f;
        AudioEngine::PlaySFX(SFX_COIN);
    }

    // 3. Wheelie bonus
    float wheelieSec = 0.0f;
    int wheelieBonus = mCar->PopWheelieBonus(&wheelieSec);
    if (wheelieBonus > 0) {
        mRaceTrickBonusCoins += wheelieBonus;
        mSessionCoins += wheelieBonus;
        mStuntName = "WHEELIE!";
        mStuntCoins = wheelieBonus;
        mStuntTimer = 2.0f;
        AudioEngine::PlaySFX(SFX_COIN);
    }

    if (mStuntTimer > 0.0f) {
        mStuntTimer -= dt;
    }

    // Check Out of Fuel Coasting
    if (mCar->IsOutOfFuel()) {
        mOutOfFuelFallbackTimer += dt;
        float speed = mCar->GetSpeedKmH() / 3.6f;
        if (speed < 0.3f) {
            mOutOfFuelStopTimer += dt;
        } else {
            mOutOfFuelStopTimer = 0.0f; // still moving, keep coasting
        }

        if (mOutOfFuelStopTimer >= 1.0f || mOutOfFuelFallbackTimer >= 15.0f) {
            mGameOverReasonDriverDown = false;
            mState = STATE_GAMEOVER;
            CommitRaceResults();
            return;
        }
    } else {
        mOutOfFuelStopTimer = 0.0f;
        mOutOfFuelFallbackTimer = 0.0f;
    }

    // Particle Emissions (Wheel Dirt & Exhaust Smoke)
    if (mCar && !mCar->IsDriverDead()) {
        if (mCar->IsThrottleActive()) {
            if (mCar->IsRearWheelOnGround()) {
                b2Vec2 rwPos = mCar->GetRearWheelBody()->GetPosition();
                b2Vec2 rwVel = mCar->GetRearWheelBody()->GetLinearVelocity();
                float rwRadius = (mCar->GetConfig() && mCar->GetConfig()->rear_wheel_radius > 0.0f) ?
                                  mCar->GetConfig()->rear_wheel_radius : 0.40f;
                ParticleSystem::EmitWheelDirt(rwPos.x, rwPos.y - rwRadius, rwVel.x, rwVel.y, mCurrentStage);
            }
            if (mCar->GetConfig() && mCar->GetConfig()->drive_type == 2 && mCar->IsFrontWheelOnGround()) {
                b2Vec2 fwPos = mCar->GetFrontWheelBody()->GetPosition();
                b2Vec2 fwVel = mCar->GetFrontWheelBody()->GetLinearVelocity();
                float fwRadius = (mCar->GetConfig() && mCar->GetConfig()->front_wheel_radius > 0.0f) ?
                                  mCar->GetConfig()->front_wheel_radius : 0.40f;
                ParticleSystem::EmitWheelDirt(fwPos.x, fwPos.y - fwRadius, fwVel.x, fwVel.y, mCurrentStage);
            }
            // Exhaust smoke puff
            b2Vec2 cPos = mCar->GetChassisBody()->GetPosition();
            b2Vec2 cVel = mCar->GetChassisBody()->GetLinearVelocity();
            float rx = (mCar->GetConfig()) ? mCar->GetConfig()->rear_wheel_offset[0] : -1.0f;
            float ry = (mCar->GetConfig()) ? mCar->GetConfig()->rear_wheel_offset[1] + 0.2f : -0.3f;
            ParticleSystem::EmitExhaust(cPos.x + rx, cPos.y + ry, cVel.x, cVel.y);
        }
    }
    ParticleSystem::Update(dt);
}

void Game::UpdatePlaying(float frameTime) {
    if (!mCar || !mPhysics || !mTerrain) return;

    if (frameTime > 0.10f) frameTime = 0.10f; // Protection against spiral of death
    mAccumulator += frameTime;

    float fixedDt = Config::Physics.fixed_timestep;
    int maxSubSteps = Config::Physics.max_sub_steps;
    int substeps = 0;

    while (mAccumulator >= fixedDt && substeps < maxSubSteps) {
        UpdatePhysicsStep(fixedDt);
        mAccumulator -= fixedDt;
        substeps++;
        if (mState != STATE_PLAYING) {
            break;
        }
    }

    if (fixedDt > 0.0f) {
        mRenderAlpha = mAccumulator / fixedDt;
        if (mRenderAlpha > 1.0f) mRenderAlpha = 1.0f;
        if (mRenderAlpha < 0.0f) mRenderAlpha = 0.0f;
    } else {
        mRenderAlpha = 1.0f;
    }

    // Engine Audio
    if (mCar) {
        bool carActive = !mCar->IsDead() && !mCar->IsOutOfFuel();
        AudioEngine::UpdateEngineSound(mCar->GetEngineRev(), carActive, frameTime);
    }

    // Camera follow (tracks interpolated car position for jitter-free smoothness)
    if (mCar) {
        b2Vec2 interpPos = mCar->GetInterpolatedPosition(mRenderAlpha);
        float targetCamX = interpPos.x + Config::Physics.camera_lead_x;
        float targetCamY = interpPos.y + Config::Physics.camera_lead_y;

        mCameraX += (targetCamX - mCameraX) * Config::Physics.camera_smoothing;
        mCameraY += (targetCamY - mCameraY) * Config::Physics.camera_smoothing;

        float speed = mCar->GetSpeedKmH() / 3.6f;
        float targetZoom = Config::Physics.zoom_base - (speed * Config::Physics.zoom_speed_factor);
        if (targetZoom < Config::Physics.min_zoom) targetZoom = Config::Physics.min_zoom;
        if (targetZoom > Config::Physics.max_zoom) targetZoom = Config::Physics.max_zoom;
        mCameraZoom += (targetZoom - mCameraZoom) * Config::Physics.camera_smoothing;
    }

#if defined(__PSP__)
    if (sBenchmarkMode) {
        static int sBenchmarkFrames = 0;
        sBenchmarkFrames++;
        if (sBenchmarkFrames >= 400) { // ~6.6 seconds of active simulation
            u32 res = sceRtcGetTickResolution();
            if (res == 0) res = 1000000;
            double avgStepMs = (sStepCount > 0) ? (((double)sTotalStepTicks / sStepCount) * 1000.0 / res) : 0.0;
            float avgChunkMs = Terrain::GetAverageChunkGenMs();
            float maxPerFrameMs = Terrain::GetMaxPerFrameGenMs();
            float avgPerFrameMs = Terrain::GetAveragePerFrameGenMs();
            int chunkCount = Terrain::GetChunkGenCount();

            FILE* pf = fopen("perf_snapshot.txt", "w");
            if (!pf) pf = fopen("ms0:/PSP/GAME/HCR/perf_snapshot.txt", "w");
            if (pf) {
                fprintf(pf, "=== PPSSPP PSP-1000 Performance Snapshot (T-111 & T-119) ===\n");
                fprintf(pf, "Hardware Profile: PSP-1000 (32 MB RAM @ 333 MHz Allegrex MIPS)\n");
                fprintf(pf, "Asset Load Time UI: %.2f ms (13 textures + font)\n", sAssetLoadTimeUI);
                fprintf(pf, "Asset Load Time Terrain: %.2f ms (7 stage textures)\n", sAssetLoadTimeTerrain);
                fprintf(pf, "Asset Load Time PlayerCar: %.2f ms (4 vehicle textures)\n", sAssetLoadTimeCar);
                fprintf(pf, "Physics Step Avg: %.3f ms (%d steps measured)\n", avgStepMs, sStepCount);
                fprintf(pf, "Chunk Gen (Full) Avg: %.3f ms (%d chunks measured)\n", avgChunkMs, chunkCount);
                fprintf(pf, "Per-Frame Gen Max: %.3f ms\n", maxPerFrameMs);
                fprintf(pf, "Per-Frame Gen Avg: %.3f ms\n", avgPerFrameMs);
                fprintf(pf, "Car Distance Reached: %.1f m\n", mMaxDistance);
                fclose(pf);
            }
            HCR_Log("[PERF] Asset Load UI: %.2f ms, Terrain: %.2f ms, Car: %.2f ms", sAssetLoadTimeUI, sAssetLoadTimeTerrain, sAssetLoadTimeCar);
            HCR_Log("[PERF] Physics Step Avg: %.3f ms (%d steps)", avgStepMs, sStepCount);
            HCR_Log("[PERF] Chunk Gen (Full) Avg: %.3f ms (%d chunks)", avgChunkMs, chunkCount);
            HCR_Log("[PERF] Per-Frame Gen Max: %.3f ms", maxPerFrameMs);
            HCR_Log("[PERF] Per-Frame Gen Avg: %.3f ms", avgPerFrameMs);
            HCR_Log("[PERF] Car Distance Reached: %.1f m", mMaxDistance);
            printf("==================================================\n");
            printf("  PPSSPP PSP-1000 Performance Snapshot (T-119)\n");
            printf("  Physics Step Avg:     %.3f ms (%d steps)\n", avgStepMs, sStepCount);
            printf("  Chunk Gen (Full) Avg: %.3f ms (%d chunks)\n", avgChunkMs, chunkCount);
            printf("  Per-Frame Gen Max:    %.3f ms\n", maxPerFrameMs);
            printf("  Per-Frame Gen Avg:    %.3f ms\n", avgPerFrameMs);
            printf("  Car Distance:         %.1f m\n", mMaxDistance);
            printf("==================================================\n");
            sceKernelExitGame();
        }
    }
#endif
}

void Game::RenderPlaying() {
    // 0. Parallax Sky Gradient (Screen space 480x272)
    Renderer2D::SetupScreenSpace();
    uint32_t skyTop = 0xFFD87820;
    uint32_t skyBot = 0xFFFFF0D0;
    if (mCurrentStage >= 0 && mCurrentStage < Config::StageCount) {
        skyTop = Config::Stages[mCurrentStage].sky_color_top;
        skyBot = Config::Stages[mCurrentStage].sky_color_bottom;
    }
    Renderer2D::DrawGradientRect(0.0f, 0.0f, (float)PSP_SCREEN_WIDTH, (float)PSP_SCREEN_HEIGHT, skyTop, skyBot);

    // Subtle parallax horizon band scrolling slower than foreground
    float pX = -fmodf(mCameraX * 4.0f, (float)PSP_SCREEN_WIDTH);
    uint32_t hillColor = (skyBot & 0x00FFFFFF) | 0x30000000;
    Renderer2D::DrawFilledRect(pX, 170.0f, (float)PSP_SCREEN_WIDTH, 102.0f, hillColor);
    Renderer2D::DrawFilledRect(pX + (float)PSP_SCREEN_WIDTH, 170.0f, (float)PSP_SCREEN_WIDTH, 102.0f, hillColor);

    // 1. World Rendering
    Renderer2D::SetupCamera(mCameraX, mCameraY, mCameraZoom);

    if (mTerrain) {
        mTerrain->Render(mCameraX, mCameraY);
    }

    ParticleSystem::Render();

    if (mCar) {
        mCar->Render(mRenderAlpha);
    }

    // Night Mode overlay: ambient darkening FIRST, then headlight cone on top (H11 / T-125)
    if (mCurrentStage >= 0 && mCurrentStage < Config::StageCount && Config::Stages[mCurrentStage].is_night == 1) {
        Renderer2D::SetupScreenSpace();
        Renderer2D::DrawFilledRect(0.0f, 0.0f, (float)PSP_SCREEN_WIDTH, (float)PSP_SCREEN_HEIGHT, 0xB0080604);

        if (mCar) {
            b2Vec2 pos = mCar->GetPosition();
            float angle = mCar->GetAngle();
            float cosA = cosf(angle);
            float sinA = sinf(angle);
            float originX = pos.x + cosA * 1.2f - sinA * 0.1f;
            float originY = pos.y + sinA * 1.2f + cosA * 0.1f;
            Renderer2D::DrawLightCone(originX, originY, angle, 14.0f, 0.35f, 4.0f, 0x90A0FFFF, 0x00A0FFFF);
        }
    }

    // 2. HUD Rendering in Screen Space
    RenderHUD();
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
    // 0. Sky background
    Renderer2D::SetupScreenSpace();
    uint32_t skyTop = 0xFFD87820;
    uint32_t skyBot = 0xFFFFF0D0;
    if (mCurrentStage >= 0 && mCurrentStage < Config::StageCount) {
        skyTop = Config::Stages[mCurrentStage].sky_color_top;
        skyBot = Config::Stages[mCurrentStage].sky_color_bottom;
    }
    Renderer2D::DrawGradientRect(0.0f, 0.0f, (float)PSP_SCREEN_WIDTH, (float)PSP_SCREEN_HEIGHT, skyTop, skyBot);

    // Keep rendering world behind gameover overlay
    Renderer2D::SetupCamera(mCameraX, mCameraY, mCameraZoom);
    if (mTerrain) mTerrain->Render(mCameraX, mCameraY);
    ParticleSystem::Render();
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

    // Stats (T-033 detailed breakdown)
    char buf[64];
    snprintf(buf, sizeof(buf), "DISTANCE: %d METERS", (int)mMaxDistance);
    mFont.DrawText(buf, 80, 85, 0.40f, 0xFFFFFFFF);

    if (mIsNewRecord) {
        Renderer2D::DrawFilledRect(265, 83, 115, 18, 0xFF00AAFF);
        mFont.DrawText("NEW RECORD!", 272, 86, 0.34f, 0xFFFFFFFF);
    }

    snprintf(buf, sizeof(buf), "COINS: +%d", mRaceCollectedCoins);
    mFont.DrawText(buf, 80, 110, 0.38f, 0xFF00E5FF);

    snprintf(buf, sizeof(buf), "TRICKS: +%d", mRaceTrickBonusCoins);
    mFont.DrawText(buf, 80, 132, 0.38f, 0xFF55FF55);

    snprintf(buf, sizeof(buf), "TOTAL COINS: %lld", (long long)PlayerInfo::GetCoins());
    mFont.DrawText(buf, 80, 154, 0.38f, 0xFFE0E0E0);

    // Buttons
    // [X] Retry
    Renderer2D::DrawFilledRect(70, 188, 150, 32, 0xFF257518);
    mFont.DrawText("[X] RETRY", 100, 194, 0.38f, 0xFFFFFFFF);

    // [O] Garage
    Renderer2D::DrawFilledRect(260, 188, 150, 32, 0xFF453020);
    mFont.DrawText("[O] GARAGE", 285, 194, 0.38f, 0xFFFFFFFF);
}

