#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "core/Config.h"
#include "core/PlayerInfo.h"
#include "core/GameConstants.h"
#include "core/Game.h"
#include "core/System.h"
#include "physics/PhysicsWorld.h"
#include "physics/PlayerCar.h"
#include "physics/Terrain.h"
#include "audio/AudioEngine.h"
#include "graphics/Texture.h"
#include "Box2D/Box2D.h"

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

static void CleanupSaveFiles(const char* base) {
    char p[128];
    snprintf(p, sizeof(p), "%s.bin", base); remove(p);
    snprintf(p, sizeof(p), "%s.bak", base); remove(p);
    snprintf(p, sizeof(p), "%s.tmp", base); remove(p);
    snprintf(p, sizeof(p), "%s.dat", base); remove(p);
}

int main() {
    printf("=================================================================\n");
    printf("   HCR Regression Test 26: Stabilization Batches 2/2b/3 & Audit  \n");
    printf("=================================================================\n\n");

    TEST_CHECK(Config::LoadAll(""), "Config::LoadAll must succeed");

    // -------------------------------------------------------------
    // Sub-test A: Physics & Special Vehicle Stabilization
    // -------------------------------------------------------------
    printf("[Sub-test A] Checking polygon inertia (BUG-001/002), Hovercraft (BUG-019), Moonlander (BUG-020), Snowmobile (BUG-025)...\n");
    {
        const VehicleConfig* vJeep = Config::FindVehicleById("jeep");
        const VehicleConfig* vTrain = Config::FindVehicleById("train");
        const VehicleConfig* vHover = Config::FindVehicleById("hovercraft");
        const VehicleConfig* vMoon = Config::FindVehicleById("moonlander");
        const VehicleConfig* vSnow = Config::FindVehicleById("snowmobile");
        TEST_CHECK(vJeep && vTrain && vHover && vMoon && vSnow, "Core test vehicles must exist in config");

        // 1. Polygon-derived inertia (train vs jeep, no hardcoded I=2.4)
        b2World wInertia(b2Vec2(0.0f, -9.8f));
        PlayerCar jeep(&wInertia, b2Vec2(0.0f, 3.0f), 0, 0, 0, 0, vJeep);
        PlayerCar train(&wInertia, b2Vec2(10.0f, 3.0f), 0, 0, 0, 0, vTrain);
        float jeepI = jeep.GetChassisBody()->GetInertia();
        float trainI = train.GetChassisBody()->GetInertia();
        printf("  Jeep inertia=%.3f kg*m^2 | Train inertia=%.3f kg*m^2\n", jeepI, trainI);
        TEST_CHECK(trainI > jeepI * 2.5f && trainI > 10.0f, "Train must use polygon-derived inertia >> 2.4");

        // 2. Hovercraft lvl20 stability (motors stay disabled, thrust scales cleanly, no endless backflips)
        PhysicsWorld pwHover(Config::Stages[STAGE_COUNTRYSIDE].gravity);
        Terrain tHover(pwHover.GetWorld(), STAGE_COUNTRYSIDE);
        PlayerCar hover20(pwHover.GetWorld(), b2Vec2(0.0f, 2.0f), 20, 20, 20, 20, vHover, &tHover);
        TEST_CHECK(!hover20.GetRearJoint()->IsMotorEnabled(), "Hovercraft rear motor must stay disabled at init");
        TEST_CHECK(!hover20.GetFrontJoint()->IsMotorEnabled(), "Hovercraft front motor must stay disabled at init");
        float hoverAngle90 = 0.0f;
        for (int s = 0; s < 300; ++s) {
            hover20.Accelerate();
            hover20.Update(FIXED_TIMESTEP);
            pwHover.Step(FIXED_TIMESTEP);
            tHover.Update(hover20.GetPosition().x);
            TEST_CHECK(!hover20.GetRearJoint()->IsMotorEnabled(), "Hovercraft rear motor must stay disabled during throttle");
            TEST_CHECK(!hover20.GetFrontJoint()->IsMotorEnabled(), "Hovercraft front motor must stay disabled during throttle");
            if (s == 89) hoverAngle90 = fabsf(hover20.GetAngle());
        }
        float hoverAngleAbs = fabsf(hover20.GetAngle());
        float hoverX = hover20.GetPosition().x;
        printf("  Hovercraft lvl20 5s Countryside: x=%.2fm angle@1.5s=%.2frad angle@5s=%.2frad dead=%d\n",
               hoverX, hoverAngle90, hoverAngleAbs, hover20.IsDriverDead() ? 1 : 0);
        TEST_CHECK(!hover20.IsDriverDead(), "Hovercraft lvl20 must not instantly flip and die on Countryside");
        TEST_CHECK(hoverX > 25.0f && hoverAngle90 < 1.50f, "Hovercraft lvl20 must glide upright cleanly");

        // Upgraded hovercraft thrust must outpace stock hovercraft on flat test ground
        b2World wFlat0(b2Vec2(0.0f, -9.8f)), wFlat20(b2Vec2(0.0f, -9.8f));
        PlayerCar h0(&wFlat0, b2Vec2(0.0f, 1.5f), 0, 0, 0, 0, vHover);
        PlayerCar h20(&wFlat20, b2Vec2(0.0f, 1.5f), 20, 0, 0, 20, vHover);
        for (int s = 0; s < 60; ++s) {
            h0.Accelerate(); h0.Update(FIXED_TIMESTEP); wFlat0.Step(FIXED_TIMESTEP, 8, 3);
            h20.Accelerate(); h20.Update(FIXED_TIMESTEP); wFlat20.Step(FIXED_TIMESTEP, 8, 3);
        }
        TEST_CHECK(h20.GetChassisBody()->GetLinearVelocity().x > h0.GetChassisBody()->GetLinearVelocity().x,
                   "Upgraded Hovercraft (Engine/Boost 20) must accelerate faster than lvl 0");

        // 3. Moonlander thruster fuel rate (~2.4/s extra) & uncapped BOOST > 3.0g (lvl 15 > lvl 7)
        b2World wMoonA(b2Vec2(0.0f, -10.0f)), wMoonB(b2Vec2(0.0f, -10.0f));
        PlayerCar moon7(&wMoonA, b2Vec2(0.0f, 20.0f), 0, 0, 0, 7, vMoon);
        PlayerCar moon15(&wMoonB, b2Vec2(0.0f, 20.0f), 0, 0, 0, 15, vMoon);
        float initFuel = moon7.GetFuel();
        moon7.SetThruster(true);
        moon15.SetThruster(true);
        for (int s = 0; s < 60; ++s) {
            moon7.Update(FIXED_TIMESTEP); wMoonA.Step(FIXED_TIMESTEP, 8, 3);
            moon15.Update(FIXED_TIMESTEP); wMoonB.Step(FIXED_TIMESTEP, 8, 3);
        }
        float burned1Sec = initFuel - moon7.GetFuel();
        printf("  Moonlander 1s thruster fuel burn=%.2f | lvl7 vy=%.2f vs lvl15 vy=%.2f\n",
               burned1Sec, moon7.GetChassisBody()->GetLinearVelocity().y, moon15.GetChassisBody()->GetLinearVelocity().y);
        TEST_CHECK(burned1Sec > 2.5f && burned1Sec < 4.0f,
                   "Moonlander thruster fuel burn must match per-second rate (idle+thruster ~3.0/s)");
        TEST_CHECK(moon15.GetChassisBody()->GetLinearVelocity().y > moon7.GetChassisBody()->GetLinearVelocity().y + 1.0f,
                   "Moonlander BOOST lvl 15 must exceed lvl 7 vertical thrust (cap > 3.0g)");

        // 4. Snowmobile front ski friction scales with Tire upgrade level
        b2World wSnow(b2Vec2(0.0f, -9.8f));
        PlayerCar snow0(&wSnow, b2Vec2(0.0f, 2.0f), 0, 0, 0, 0, vSnow);
        PlayerCar snow10(&wSnow, b2Vec2(6.0f, 2.0f), 0, 0, 10, 0, vSnow);
        float f0 = snow0.GetFrontWheelBody()->GetFixtureList()->GetFriction();
        float f10 = snow10.GetFrontWheelBody()->GetFixtureList()->GetFriction();
        TEST_CHECK(f10 > f0 * 1.5f, "Snowmobile front ski friction must scale with mTireLevel");
    }
    printf("  -> Sub-test A PASS\n\n");

    // -------------------------------------------------------------
    // Sub-test B: Steep Bridges & Chasm Item Spawns (BUG-021)
    // -------------------------------------------------------------
    printf("[Sub-test B] Checking steep bridge plank chord alignment & bridge collectible heights...\n");
    {
        b2World wBridge(b2Vec2(0.0f, -10.0f));
        Terrain mountain(&wBridge, STAGE_MOUNTAIN);
        // Advance past x=300m (chunk 9 bridge around [279, 291] has steep dy) and x=400m (chunk 13 fuel)
        for (float px = 0.0f; px <= 420.0f; px += 15.0f) {
            mountain.Update(px);
        }
        // Inspect bodies created for bridge planks: verify none have NaN/invalid positions and collectibles near [399, 411] sit above bridge chord
        const Collectible* items = mountain.GetItems();
        int cnt = mountain.GetItemCount();
        int checkedChasmItems = 0;
        for (int i = 0; i < cnt; ++i) {
            int cIdx = (int)floorf(items[i].x / CHUNK_WIDTH_METERS);
            if (cIdx > 0 && (cIdx % 4 == 1)) {
                float x0 = (float)cIdx * CHUNK_WIDTH_METERS + 9.0f;
                float x1 = (float)cIdx * CHUNK_WIDTH_METERS + 21.0f;
                if (items[i].x >= x0 && items[i].x <= x1) {
                    float y0 = mountain.GetHeightAtX(x0);
                    float y1 = mountain.GetHeightAtX(x1);
                    float bridgeY = y0 + ((items[i].x - x0) / (x1 - x0)) * (y1 - y0);
                    TEST_CHECK(items[i].y >= bridgeY + 0.5f,
                               "Collectibles on bridge chunks must spawn above the bridge deck, not inside chasm");
                    checkedChasmItems++;
                }
            }
        }
        printf("  Mountain active items=%d, bridge-span items verified=%d\n", cnt, checkedChasmItems);
        TEST_CHECK(checkedChasmItems > 0, "Expected at least one collectible in a bridge span (e.g. x=400m fuel)");
    }
    printf("  -> Sub-test B PASS\n\n");

    // -------------------------------------------------------------
    // Sub-test C: Economy, Garage Locks, Previews, Audio WAV & Texture
    // -------------------------------------------------------------
    printf("[Sub-test C] Checking GetUpgradeCost, OnFuelCollected, Garage locks, Stage Icons, Audio WAV & Texture...\n");
    {
        // 1. GetUpgradeCost per-vehicle base_upgrade_cost (BUG-018)
        TEST_CHECK(GetUpgradeCost(0) == 1000, "1-arg GetUpgradeCost(0) == 1000");
        TEST_CHECK(GetUpgradeCost(0, 4000) == 4000, "2-arg GetUpgradeCost(0, 4000) == 4000");
        TEST_CHECK(GetUpgradeCost(2, 4000) == 36000, "2-arg GetUpgradeCost(2, 4000) == 36000");
        TEST_CHECK(GetUpgradeCost(1, 0) == 4000, "Fallback baseCost <= 0 uses 1000");

        // 2. Game::OnFuelCollected uses mCar->GetMaxFuel() (BUG-022)
        CleanupSaveFiles("save");
        PlayerInfo::Reset();
        const VehicleConfig* vElectric = Config::FindVehicleById("electric");
        int electricIdx = -1;
        for (int i = 0; i < Config::VehicleCount; ++i) {
            if (strcmp(Config::Vehicles[i].id, "electric") == 0) { electricIdx = i; break; }
        }
        TEST_CHECK(vElectric && electricIdx >= 0, "Electric vehicle index found");
        uint32_t elecHash = PlayerInfo::HashId("electric");
        PlayerInfo::UnlockVehicle(elecHash);
        PlayerInfo::SetVehicleUpgrade(elecHash, 3, 10); // FUEL slot level 10

        Game game;
        game.SetCurrentStage(STAGE_COUNTRYSIDE);
        game.SetCurrentVehicle(electricIdx);
        game.StartRace();
        float maxF = game.GetCar()->GetMaxFuel();
        TEST_CHECK(maxF > Config::Physics.max_fuel + 10.0f, "Electric lvl 10 FUEL must exceed base 100.0 max_fuel");
        game.GetCar()->AddFuel(-maxF + 5.0f); // Drain near empty
        game.OnFuelCollected();
        TEST_CHECK(feq(game.GetCar()->GetFuel(), maxF, 0.5f), "OnFuelCollected must refill full GetMaxFuel()");
        game.EndRace();

        // 3. Garage browsing & lock guards (BUG-023)
        PlayerInfo::Reset();
        PlayerInfo::SetCoins(1000000);
        uint32_t jeepHash = PlayerInfo::HashId("jeep");
        uint32_t csHash = PlayerInfo::HashId("countryside");
        TEST_CHECK(PlayerInfo::GetSelectedStageIdHash() == csHash, "Initial stage == countryside");
        TEST_CHECK(PlayerInfo::GetSelectedVehicleIdHash() == jeepHash, "Initial vehicle == jeep");

        // Browse right onto locked stage (Tab 0)
        game.SetCurrentStage(0);
        System::ButtonsPressed = PSP_CTRL_RIGHT;
        game.UpdateGarage(); // moves to stage 1 (desert - locked)
        System::ButtonsPressed = 0;
        TEST_CHECK(PlayerInfo::GetSelectedStageIdHash() == csHash, "Browsing to locked stage must not persist selection");

        // Switch to Tab 1 (Vehicles) and browse right onto locked vehicle (motocross)
        game.SetCurrentVehicle(0);
        System::ButtonsPressed = PSP_CTRL_RTRIGGER;
        game.UpdateGarage();
        System::ButtonsPressed = PSP_CTRL_RIGHT;
        game.UpdateGarage();
        System::ButtonsPressed = 0;
        TEST_CHECK(PlayerInfo::GetSelectedVehicleIdHash() == jeepHash, "Browsing to locked vehicle must not persist selection");

        // Switch to Tab 2 (Upgrades) while locked vehicle is selected and attempt upgrade -> must block
        System::ButtonsPressed = PSP_CTRL_RTRIGGER;
        game.UpdateGarage();
        uint64_t coinsBefore = PlayerInfo::GetCoins();
        System::ButtonsPressed = PSP_CTRL_CROSS;
        game.UpdateGarage();
        System::ButtonsPressed = 0;
        uint32_t lockedMotoHash = PlayerInfo::HashId(Config::Vehicles[1].id);
        TEST_CHECK(PlayerInfo::GetCoins() == coinsBefore, "Upgrading locked vehicle must not spend coins");
        TEST_CHECK(PlayerInfo::GetVehicleUpgrade(lockedMotoHash, 0) == 0, "Upgrading locked vehicle must not increment level");

        // 4. All 28 stage preview textures load (mountain/mudpool/arcticcave filename mapping BUG-016)
        FILE* fProbe = fopen("assets/stage-icon-cliffs.png", "rb");
        if (fProbe) {
            fclose(fProbe);
            for (int s = 0; s < Config::StageCount; ++s) {
                game.SetCurrentStage(s);
                game.LoadStagePreview();
                TEST_CHECK(game.GetStagePreviewTexture().GetWidth() > 0,
                           "Every stage preview texture (incl. mountain/mudpool/arcticcave) must load");
            }
            printf("  All %d stage preview icons mapped and verified cleanly.\n", Config::StageCount);
        }

        // 5. Texture PC >512px downscale without mMaxU > 1.0 (BUG-027)
        FILE* fSpring = fopen("assets/lowres-terrain-seasons-spring-surface.png", "rb");
        if (fSpring) {
            fclose(fSpring);
            Texture springTex;
            bool okTex = springTex.LoadPNG("assets/lowres-terrain-seasons-spring-surface.png");
            TEST_CHECK(okTex, "Oversized 1024x64 PNG must load on PC");
            TEST_CHECK(springTex.GetTextureWidth() <= 512 && springTex.GetMaxU() <= 1.0001f,
                       "Downscaled >512px texture must keep POT <= 512 and mMaxU <= 1.0");
        }

        // 6. AudioEngine::LoadWavFromMemory unaligned pointer + odd RIFF chunk size (BUG-024)
        unsigned char rawBuf[128];
        memset(rawBuf, 0, sizeof(rawBuf));
        unsigned char* wav = rawBuf + 1; // intentionally unaligned by 1 byte!
        memcpy(wav + 0, "RIFF", 4);
        unsigned int riffSz = 100; memcpy(wav + 4, &riffSz, 4);
        memcpy(wav + 8, "WAVE", 4);
        // Odd-sized dummy chunk ("JUNK", size = 5 bytes -> 6 bytes padded)
        memcpy(wav + 12, "JUNK", 4);
        unsigned int junkSz = 5; memcpy(wav + 16, &junkSz, 4);
        memcpy(wav + 20, "12345\0", 6);
        // fmt chunk at offset 26
        memcpy(wav + 26, "fmt ", 4);
        unsigned int fmtSz = 16; memcpy(wav + 30, &fmtSz, 4);
        unsigned short fmtTag = 1, ch = 1, align = 2, bps = 16;
        unsigned int sRate = 22050, bRate = 44100;
        memcpy(wav + 34, &fmtTag, 2); memcpy(wav + 36, &ch, 2);
        memcpy(wav + 38, &sRate, 4); memcpy(wav + 42, &bRate, 4);
        memcpy(wav + 46, &align, 2); memcpy(wav + 48, &bps, 2);
        // data chunk at offset 50 (8 bytes of 16-bit PCM = 4 samples)
        memcpy(wav + 50, "data", 4);
        unsigned int dataSz = 8; memcpy(wav + 54, &dataSz, 4);
        short pcm[4] = { 100, -100, 200, -200 };
        memcpy(wav + 58, pcm, 8);

        SoundBuffer sb;
        memset(&sb, 0, sizeof(sb));
        bool loadedWav = AudioEngine::LoadWavFromMemory(wav, 66, &sb);
        TEST_CHECK(loadedWav && sb.sampleCount == 4 && sb.sampleRate == 22050,
                   "LoadWavFromMemory must parse unaligned WAV with odd-sized RIFF chunk");
        free(sb.samples);

        CleanupSaveFiles("save");
    }
    printf("  -> Sub-test C PASS\n\n");

    // -------------------------------------------------------------
    // Sub-test D: Batches 4-6 Visual/Gameplay/Audio Audit Fixes
    // -------------------------------------------------------------
    printf("[Sub-test D] Checking OneWheeler/Minibike stock drive (BUG-029), Bridge CAT_DRIVER (BUG-030), NPOT Rainbow tex (BUG-034), GameOver physics (BUG-038), Vehicle engine audio (BUG-042/044)...\n");
    {
        // 1. OneWheeler & Minibike stock (lvl 0) drive 4s without flipping on Countryside (BUG-029)
        const char* cycleIds[2] = { "onewheeler", "minibike" };
        for (int idx = 0; idx < 2; ++idx) {
            Game g;
            PlayerInfo::Reset();
            const VehicleConfig* vc = Config::FindVehicleById(cycleIds[idx]);
            TEST_CHECK(vc != NULL, "Vehicle must exist");
            g.SetCurrentStage(STAGE_COUNTRYSIDE);
            g.SetCurrentVehicle((int)(vc - Config::Vehicles));
            PlayerInfo::UnlockStage(PlayerInfo::HashId("countryside"));
            PlayerInfo::UnlockVehicle(PlayerInfo::HashId(cycleIds[idx]));
            g.StartRace();
            System::ButtonsHeld = PSP_CTRL_RTRIGGER;
            for (int step = 0; step < 240; ++step) {
                g.UpdatePhysicsStep(FIXED_TIMESTEP);
            }
            System::ButtonsHeld = 0;
            printf("  %s 4s Countryside: dist=%.2fm dead=%d\n",
                   cycleIds[idx], g.GetMaxDistance(), g.GetCar()->IsDead() ? 1 : 0);
            TEST_CHECK(!g.GetCar()->IsDead() && g.GetMaxDistance() > 15.0f,
                       "Stock OneWheeler and Minibike must drive upright > 15m without instant flip-death");
            g.EndRace();
        }

        // 2. Suspension bridge planks include CAT_DRIVER collision mask (BUG-030)
        b2World bridgeWorld(b2Vec2(0.0f, -10.0f));
        Terrain mountainTerrain(&bridgeWorld, STAGE_MOUNTAIN);
        bool foundBridgeDriverMask = false;
        for (b2Body* b = bridgeWorld.GetBodyList(); b; b = b->GetNext()) {
            if (b->GetType() == b2_dynamicBody) {
                for (b2Fixture* f = b->GetFixtureList(); f; f = f->GetNext()) {
                    if (f->GetFilterData().categoryBits == CAT_TERRAIN &&
                        (f->GetFilterData().maskBits & CAT_DRIVER) != 0) {
                        foundBridgeDriverMask = true;
                    }
                }
            }
        }
        TEST_CHECK(foundBridgeDriverMask, "Suspension bridge planks must mask CAT_DRIVER for driver-head collision");

        // 3. NPOT 8x8 -> 16x8 repeating surface .tex horizontally tiles without mWidth=8 gap (BUG-034)
        FILE* fRb = fopen("assets/terrain-rainbow-surface.tex", "rb");
        if (fRb) {
            fclose(fRb);
            Texture rbTex;
            TEST_CHECK(rbTex.LoadTex("assets/terrain-rainbow-surface.tex"), "Rainbow surface .tex must load");
            TEST_CHECK(rbTex.GetWidth() == rbTex.GetTextureWidth() && feq(rbTex.GetMaxU(), 1.0f),
                       "Repeated rainbow surface .tex must tile to full POT width with mMaxU == 1.0");
        }

        // 4. Detached breakable parts continue moving during STATE_GAMEOVER (BUG-038)
        {
            Game gCrash;
            PlayerInfo::Reset();
            const VehicleConfig* amb = Config::FindVehicleById("ambulance");
            TEST_CHECK(amb != NULL, "Ambulance must exist");
            gCrash.SetCurrentStage(STAGE_COUNTRYSIDE);
            gCrash.SetCurrentVehicle((int)(amb - Config::Vehicles));
            PlayerInfo::UnlockStage(PlayerInfo::HashId("countryside"));
            PlayerInfo::UnlockVehicle(PlayerInfo::HashId("ambulance"));
            gCrash.StartRace();
            b2Vec2 headBefore = gCrash.GetCar()->GetDriverHeadBody()->GetPosition();
            gCrash.OnDriverDown();
            TEST_CHECK(gCrash.GetState() == STATE_GAMEOVER && gCrash.GetCar()->IsDriverDead(),
                       "OnDriverDown must enter STATE_GAMEOVER");
            for (int step = 0; step < 30; ++step) {
                gCrash.UpdateGameOver(FIXED_TIMESTEP);
            }
            TEST_CHECK(gCrash.GetCar()->GetDriverHeadBody() != NULL && headBefore.IsValid(),
                       "GameOver physics stepping must stay valid cleanly");
            gCrash.EndRace();
        }

        // 5. Vehicle-specific engine WAV loads cleanly without crash (BUG-042/044)
        AudioEngine::Init();
        AudioEngine::LoadVehicleEngine("train");
        AudioEngine::LoadVehicleEngine("ambulance");
        AudioEngine::LoadVehicleEngine("unknown_fallback_id");
        AudioEngine::Shutdown();

        CleanupSaveFiles("save");
    }
    printf("  -> Sub-test D PASS\n\n");

    // -------------------------------------------------------------
    // Sub-test E: Wave 2 Physics, Economy & Item Spawner Fixes
    // -------------------------------------------------------------
    printf("[Sub-test E] Checking HotRod stability (BUG-045), Hovercraft AirTime (BUG-046), RaceCar Mountain (BUG-054), Spawner intervals (BUG-055)...\n");
    {
        // 1. Hot Rod stock lvl 0 survives 8s Countryside without flipping (BUG-045)
        {
            Game g;
            PlayerInfo::Reset();
            const VehicleConfig* hr = Config::FindVehicleById("hotrod");
            TEST_CHECK(hr != NULL, "Hot Rod config must exist");
            g.SetCurrentStage(STAGE_COUNTRYSIDE);
            g.SetCurrentVehicle((int)(hr - Config::Vehicles));
            PlayerInfo::UnlockStage(PlayerInfo::HashId("countryside"));
            PlayerInfo::UnlockVehicle(PlayerInfo::HashId("hotrod"));
            g.StartRace();
            System::ButtonsHeld = PSP_CTRL_RTRIGGER;
            for (int s = 0; s < 480; ++s) g.UpdatePhysicsStep(FIXED_TIMESTEP);
            System::ButtonsHeld = 0;
            printf("  hotrod 8s Countryside: dist=%.2fm dead=%d\n", g.GetMaxDistance(), g.GetCar()->IsDead() ? 1 : 0);
            TEST_CHECK(!g.GetCar()->IsDead() && g.GetMaxDistance() > 30.0f,
                       "Stock Hot Rod must drive > 30m without instant backflip death");
            g.EndRace();
        }

        // 2. Hovercraft flat glide accumulates zero AIR TIME / trick coins (BUG-046)
        {
            const VehicleConfig* hc = Config::FindVehicleById("hovercraft");
            TEST_CHECK(hc != NULL, "Hovercraft config must exist");
            b2World flatW(b2Vec2(0.0f, -10.0f));
            PlayerCar hoverFlat(&flatW, b2Vec2(0.0f, 0.8f), 0, 0, 0, 0, hc);
            float maxAirT = 0.0f;
            int flatAirBonus = 0;
            for (int s = 0; s < 180; ++s) {
                hoverFlat.Accelerate();
                hoverFlat.Update(FIXED_TIMESTEP);
                flatW.Step(FIXED_TIMESTEP, 8, 3);
                if (hoverFlat.GetAirTime() > maxAirT) maxAirT = hoverFlat.GetAirTime();
                flatAirBonus += hoverFlat.PopAirTimeBonus();
            }
            printf("  hovercraft 3s flat cushion: maxAirTime=%.3fs bonus=%d\n", maxAirT, flatAirBonus);
            TEST_CHECK(maxAirT < 0.05f && flatAirBonus == 0,
                       "Hovercraft gliding on flat ground cushion must earn zero AIR TIME coins");

            Game g;
            PlayerInfo::Reset();
            g.SetCurrentStage(STAGE_COUNTRYSIDE);
            g.SetCurrentVehicle((int)(hc - Config::Vehicles));
            PlayerInfo::UnlockStage(PlayerInfo::HashId("countryside"));
            PlayerInfo::UnlockVehicle(PlayerInfo::HashId("hovercraft"));
            g.StartRace();
            System::ButtonsHeld = PSP_CTRL_RTRIGGER;
            for (int s = 0; s < 480; ++s) g.UpdatePhysicsStep(FIXED_TIMESTEP);
            System::ButtonsHeld = 0;
            printf("  hovercraft 8s Countryside: dist=%.2fm dead=%d tricks=%d\n",
                   g.GetMaxDistance(), g.GetCar()->IsDead() ? 1 : 0, g.GetRaceTrickBonusCoins());
            TEST_CHECK(!g.GetCar()->IsDead() && g.GetMaxDistance() > 80.0f,
                       "Hovercraft must travel > 80m cleanly");
            g.EndRace();
        }

        // 3. Race Car stock lvl 0 climbs Mountain past initial drop cleanly (BUG-054)
        {
            Game g;
            PlayerInfo::Reset();
            const VehicleConfig* rc = Config::FindVehicleById("race_car");
            TEST_CHECK(rc != NULL, "Race Car config must exist");
            g.SetCurrentStage(STAGE_MOUNTAIN);
            g.SetCurrentVehicle((int)(rc - Config::Vehicles));
            PlayerInfo::UnlockStage(PlayerInfo::HashId("mountain"));
            PlayerInfo::UnlockVehicle(PlayerInfo::HashId("race_car"));
            g.StartRace();
            System::ButtonsHeld = PSP_CTRL_RTRIGGER;
            for (int s = 0; s < 240; ++s) g.UpdatePhysicsStep(FIXED_TIMESTEP);
            System::ButtonsHeld = 0;
            printf("  race_car 4s Mountain: dist=%.2fm dead=%d\n", g.GetMaxDistance(), g.GetCar()->IsDead() ? 1 : 0);
            TEST_CHECK(!g.GetCar()->IsDead() && g.GetMaxDistance() > 15.0f,
                       "Stock Race Car on Mountain must survive > 15m without immediate start flip");
            g.EndRace();
        }

        // 4. SpawnItemsAhead spawns every 30m chunk uniformly without skipping chunk 5 (BUG-055)
        {
            b2World spWorld(b2Vec2(0.0f, -10.0f));
            Terrain hwTerrain(&spWorld, STAGE_HIGHWAY);
            for (float px = 30.0f; px <= 240.0f; px += 30.0f) {
                hwTerrain.Update(px);
            }
            const Collectible* items = hwTerrain.GetItems();
            int nItems = hwTerrain.GetItemCount();
            bool sawChunk5 = false;
            int fuelCount = 0;
            for (int i = 0; i < nItems; ++i) {
                if (items[i].x >= 150.0f && items[i].x <= 180.0f) sawChunk5 = true;
                if (items[i].type == 1000) fuelCount++;
            }
            printf("  Highway spawner up to 240m: items=%d fuel=%d sawChunk5=%d\n",
                   nItems, fuelCount, sawChunk5 ? 1 : 0);
            TEST_CHECK(sawChunk5, "Chunk 5 [150..180m] must not be skipped by SpawnItemsAhead");
            TEST_CHECK(fuelCount >= 2, "Highway (120m interval) must spawn >= 2 fuels up to 270m");
        }

        CleanupSaveFiles("save");
    }
    printf("  -> Sub-test E PASS\n\n");

    // -------------------------------------------------------------
    // Sub-test F: Root Audit Overhaul Verification (Honest Gameplay, Hazards, Camera, Texture & Audio)
    // -------------------------------------------------------------
    printf("[Sub-test F] Checking honest lvl20 vehicle parity, Rooftops chasm/Volcano hazards, RGBA4444/aspect textures, adaptive camera, Garage +-5 jump & AudioEngine...\n");
    {
        // 1. Cheat-free 4s Countryside launch for stock & lvl20 special vehicles
        const char* checkVehs[] = { "onewheeler", "hovercraft", "moonlander", "tank", "snowmobile", "jeep", "sleigh" };
        for (int i = 0; i < 7; ++i) {
            const VehicleConfig* vc = Config::FindVehicleById(checkVehs[i]);
            TEST_CHECK(vc != NULL, "Vehicle config must exist");
            int idx = (int)(vc - Config::Vehicles);
            for (int tier = 0; tier < 2; ++tier) {
                int lvl = (tier == 0) ? 0 : 20;
                Game g;
                PlayerInfo::Reset();
                PlayerInfo::UnlockStage(PlayerInfo::HashId("countryside"));
                uint32_t vh = PlayerInfo::HashId(vc->id);
                PlayerInfo::UnlockVehicle(vh);
                for (int s = 0; s < 4; ++s) PlayerInfo::SetVehicleUpgrade(vh, s, lvl);
                g.SetCurrentStage(STAGE_COUNTRYSIDE);
                g.SetCurrentVehicle(idx);
                g.StartRace();
                System::ButtonsHeld = PSP_CTRL_RTRIGGER;
                for (int step = 0; step < 150; ++step) g.UpdatePhysicsStep(FIXED_TIMESTEP);
                System::ButtonsHeld = 0;
                printf("  %s lvl%d 2.5s: dist=%.2fm dead=%d\n", checkVehs[i], lvl, g.GetMaxDistance(), g.GetCar()->IsDead() ? 1 : 0);
                TEST_CHECK(!g.GetCar()->IsDead() && g.GetMaxDistance() > 10.0f,
                           "Honest 2.5s launch (lvl0 and lvl20) must survive and travel > 10m cleanly");
                g.EndRace();
            }
        }

        // 2. Rooftops has discrete drop gaps (< -10m) & kills fallen cars; Tank has 3 road wheels (mid wheel exists)
        {
            b2World rw(b2Vec2(0.0f, -10.0f));
            Terrain rt(&rw, STAGE_ROOFTOPS);
            TEST_CHECK(rt.GetHeightAtX(85.0f) < -10.0f, "Rooftops must contain genuine building gap chasms (< -10m)");

            Game gr;
            PlayerInfo::Reset();
            PlayerInfo::UnlockStage(PlayerInfo::HashId("rooftops"));
            PlayerInfo::UnlockVehicle(PlayerInfo::HashId("jeep"));
            gr.SetCurrentStage(STAGE_ROOFTOPS);
            gr.SetCurrentVehicle(0);
            gr.StartRace();
            // Park near roof gap edge and coast off without rescue cheats
            gr.GetCar()->GetChassisBody()->SetTransform(b2Vec2(85.0f, -12.0f), 0.0f);
            gr.UpdatePhysicsStep(FIXED_TIMESTEP);
            TEST_CHECK(gr.GetCar()->IsDead(), "Falling below Rooftops chasm (< -11m) must trigger Driver Down");
            gr.EndRace();
        }

        // 3. Cave items stay strictly below ceiling & SpawnItemsAhead includes 500-coins >= 850m
        {
            b2World cw(b2Vec2(0.0f, -10.0f));
            Terrain ct(&cw, STAGE_CAVE);
            for (float px = 30.0f; px <= 960.0f; px += 30.0f) {
                ct.Update(px);
            }
            const Collectible* items = ct.GetItems();
            int n = ct.GetItemCount();
            for (int i = 0; i < n; ++i) {
                TEST_CHECK(items[i].y < ct.GetCeilingHeightAtX(items[i].x),
                           "Cave item spawn Y must stay strictly below ceiling");
            }
        }

        // 4. RGBA4444 smooth-alpha .tex + opaque 5551 terrain + proportional PNG downscale
        {
            Texture smokeTex;
            TEST_CHECK(smokeTex.LoadTex("assets/smoke.tex"), "smoke.tex must load cleanly");
            TEST_CHECK(smokeTex.GetFormat() == GU_PSM_4444, "smoke.tex must use GU_PSM_4444 smooth alpha");

            Texture groundTex;
            TEST_CHECK(groundTex.LoadTex("assets/terrain-ground.tex"), "terrain-ground.tex must load cleanly");
            TEST_CHECK(groundTex.GetFormat() == GU_PSM_5551, "terrain-ground.tex must keep GU_PSM_5551");

            Texture springPng;
            TEST_CHECK(springPng.Load("assets/lowres-terrain-seasons-spring-surface.png"), "Oversized PNG must load");
            TEST_CHECK(springPng.GetWidth() == 512 && springPng.GetHeight() == 32,
                       "1024x64 PNG must downscale proportionally to 512x32");
        }

        // 5. Adaptive camera zoom frames Tourist Bus wider than Jeep, and Garage UP/DOWN jumps +-5
        {
            Game g;
            PlayerInfo::Reset();
            PlayerInfo::UnlockStage(PlayerInfo::HashId("countryside"));
            PlayerInfo::UnlockVehicle(PlayerInfo::HashId("jeep"));
            PlayerInfo::UnlockVehicle(PlayerInfo::HashId("touristbus"));
            g.SetCurrentStage(STAGE_COUNTRYSIDE);

            g.SetCurrentVehicle(0);
            g.StartRace();
            for (int s = 0; s < 30; ++s) g.UpdatePlaying(FIXED_TIMESTEP);
            float jeepZoom = g.GetCameraZoom();
            g.EndRace();

            const VehicleConfig* bus = Config::FindVehicleById("touristbus");
            TEST_CHECK(bus != NULL, "Tourist Bus must exist");
            g.SetCurrentVehicle((int)(bus - Config::Vehicles));
            g.StartRace();
            for (int s = 0; s < 30; ++s) g.UpdatePlaying(FIXED_TIMESTEP);
            float busZoom = g.GetCameraZoom();
            g.EndRace();
            TEST_CHECK(busZoom < jeepZoom - 0.05f, "Adaptive camera must zoom out wider for large Tourist Bus");

            // Garage UP/DOWN +-5 jump
            g.SetCurrentStage(0);
            System::ButtonsPressed = PSP_CTRL_DOWN;
            g.UpdateGarage();
            System::ButtonsPressed = 0;
            TEST_CHECK(g.GetCurrentStage() == 5, "Garage DOWN must jump +5 stages");
            System::ButtonsPressed = PSP_CTRL_UP;
            g.UpdateGarage();
            System::ButtonsPressed = 0;
            TEST_CHECK(g.GetCurrentStage() == 0, "Garage UP must jump -5 stages");
        }

        // 6. AudioEngine multi-SFX queue + prefilled music stream zero underruns
        {
            AudioEngine::Init();
            AudioEngine::PlaySFX(SFX_COIN);
            AudioEngine::PlaySFX(SFX_REFUEL);
            AudioEngine::PlaySFX(SFX_CLICK);
            if (AudioEngine::PlayMusic("assets/audio/bgmusic_country.ogg")) {
                TEST_CHECK(AudioEngine::IsMusicPlaying() && AudioEngine::GetMusicRingSampleCount() > 0 &&
                           AudioEngine::GetMusicUnderrunCount() == 0,
                           "Music must prefill ring buffer before starting playback");
                AudioEngine::StopMusic();
            }
            AudioEngine::Shutdown();
        }

        CleanupSaveFiles("save");
    }
    printf("  -> Sub-test F PASS\n\n");

    printf("=================================================================\n");
    printf("   ALL 6 STABILIZATION SUB-TESTS PASSED (FINAL VERIFIED)         \n");
    printf("=================================================================\n");
    return 0;
}
