#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "core/Config.h"
#include "core/PlayerInfo.h"
#include "core/GameConstants.h"
#include "core/Game.h"
#include "core/System.h"
#include "physics/PhysicsWorld.h"
#include "physics/PlayerCar.h"
#include "physics/Terrain.h"
#include "audio/AudioEngine.h"
#include "graphics/Renderer2D.h"
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

static void GenerateSineWavFile(const char* filepath, int sampleRate, int numSamples) {
    size_t dataSize = numSamples * sizeof(short);
    size_t totalSize = 44 + dataSize;
    unsigned char* wav = (unsigned char*)malloc(totalSize);
    if (!wav) return;

    memcpy(wav, "RIFF", 4);
    unsigned int chunkSize = (unsigned int)(totalSize - 8);
    memcpy(wav + 4, &chunkSize, 4);
    memcpy(wav + 8, "WAVE", 4);

    memcpy(wav + 12, "fmt ", 4);
    unsigned int sub1Size = 16;
    memcpy(wav + 16, &sub1Size, 4);
    unsigned short audioFormat = 1;
    memcpy(wav + 20, &audioFormat, 2);
    unsigned short numChannels = 1;
    memcpy(wav + 22, &numChannels, 2);
    unsigned int sRate = sampleRate;
    memcpy(wav + 24, &sRate, 4);
    unsigned int byteRate = sampleRate * 1 * sizeof(short);
    memcpy(wav + 28, &byteRate, 4);
    unsigned short blockAlign = 1 * sizeof(short);
    memcpy(wav + 32, &blockAlign, 2);
    unsigned short bitsPerSample = 16;
    memcpy(wav + 34, &bitsPerSample, 2);

    memcpy(wav + 36, "data", 4);
    unsigned int dSize = (unsigned int)dataSize;
    memcpy(wav + 40, &dSize, 4);

    short* samples = (short*)(wav + 44);
    for (int i = 0; i < numSamples; ++i) {
        float t = (float)i / (float)sampleRate;
        samples[i] = (short)(sinf(2.0f * 3.14159265f * 440.0f * t) * 16000.0f);
    }

    FILE* f = fopen(filepath, "wb");
    if (f) {
        fwrite(wav, 1, totalSize, f);
        fclose(f);
    }
    free(wav);
}

static void TeleportCar(Game& game, float targetX) {
    PlayerCar* car = game.GetCar();
    if (!car || !car->GetChassisBody()) return;
    Terrain* terrain = game.GetTerrain();
    float groundY = terrain ? terrain->GetHeightAtX(targetX) : 0.0f;
    b2Vec2 cur = car->GetPosition();
    float targetY = groundY + 2.5f;
    b2Vec2 delta(targetX - cur.x, targetY - cur.y);

    car->GetChassisBody()->SetTransform(car->GetChassisBody()->GetPosition() + delta, 0.0f);
    if (car->GetRearWheelBody())
        car->GetRearWheelBody()->SetTransform(car->GetRearWheelBody()->GetPosition() + delta, 0.0f);
    if (car->GetFrontWheelBody())
        car->GetFrontWheelBody()->SetTransform(car->GetFrontWheelBody()->GetPosition() + delta, 0.0f);
    if (car->GetDriverHeadBody())
        car->GetDriverHeadBody()->SetTransform(car->GetDriverHeadBody()->GetPosition() + delta, 0.0f);

    car->GetChassisBody()->SetLinearVelocity(b2Vec2(0.0f, 0.0f));
    car->GetChassisBody()->SetAngularVelocity(0.0f);
    if (car->GetRearWheelBody()) {
        car->GetRearWheelBody()->SetLinearVelocity(b2Vec2(0.0f, 0.0f));
        car->GetRearWheelBody()->SetAngularVelocity(0.0f);
    }
    if (car->GetFrontWheelBody()) {
        car->GetFrontWheelBody()->SetLinearVelocity(b2Vec2(0.0f, 0.0f));
        car->GetFrontWheelBody()->SetAngularVelocity(0.0f);
    }
}

static void CleanupTestSaveFiles(const char* base) {
    char path[256];
    snprintf(path, sizeof(path), "%s.bin", base); remove(path);
    snprintf(path, sizeof(path), "%s.bak", base); remove(path);
    snprintf(path, sizeof(path), "%s.tmp", base); remove(path);
    snprintf(path, sizeof(path), "%s.dat", base); remove(path);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("=================================================================\n");
    printf("   HCR Regression Test 23: Audio Routing, Pause & Distance Bonus \n");
    printf("=================================================================\n\n");

    // -------------------------------------------------------------
    // Sub-test A: Volume Routing Invariants & Streaming Music Lifecycle
    // -------------------------------------------------------------
    printf("[Sub-test A] Audio Volume Routing Invariants & Streaming Music Lifecycle...\n");
    {
        AudioEngine::Init();

        // 1. Invariant verification: mMusicVolume scales ONLY music, mSFXVolume scales SFX/motor
        AudioEngine::SetSFXVolume(0.40f);
        TEST_CHECK(feq(AudioEngine::GetSFXVolume(), 0.40f), "GetSFXVolume mismatch");

        AudioEngine::SetMusicVolume(0.80f);
        TEST_CHECK(feq(AudioEngine::GetMusicVolume(), 0.80f), "GetMusicVolume mismatch");
        TEST_CHECK(feq(AudioEngine::GetSFXVolume(), 0.40f), "Setting music volume leaked into SFX volume");

        AudioEngine::SetSFXVolume(0.65f);
        TEST_CHECK(feq(AudioEngine::GetSFXVolume(), 0.65f), "GetSFXVolume update mismatch");
        TEST_CHECK(feq(AudioEngine::GetMusicVolume(), 0.80f), "Setting SFX volume leaked into music volume");

        // Volume clamping [0.0, 1.0]
        AudioEngine::SetSFXVolume(-0.75f);
        TEST_CHECK(feq(AudioEngine::GetSFXVolume(), 0.0f), "SFX volume lower bound clamping failed");
        AudioEngine::SetSFXVolume(1.85f);
        TEST_CHECK(feq(AudioEngine::GetSFXVolume(), 1.0f), "SFX volume upper bound clamping failed");

        AudioEngine::SetMusicVolume(-0.50f);
        TEST_CHECK(feq(AudioEngine::GetMusicVolume(), 0.0f), "Music volume lower bound clamping failed");
        AudioEngine::SetMusicVolume(2.20f);
        TEST_CHECK(feq(AudioEngine::GetMusicVolume(), 1.0f), "Music volume upper bound clamping failed");

        AudioEngine::SetSFXVolume(1.0f);
        AudioEngine::SetMusicVolume(1.0f);

        // 2. Missing music file headless robustness (must return false, no crash)
        bool missingOk = AudioEngine::PlayMusic("non_existent_stream_track.wav");
        TEST_CHECK(!missingOk, "PlayMusic on missing file should return false");
        TEST_CHECK(!AudioEngine::IsMusicPlaying(), "Music should not be marked playing for missing file");
        AudioEngine::UpdateMusicStream(); // must be safe no-op
        AudioEngine::StopMusic();         // must be safe no-op

        // 3. Streaming playback, ring buffer feed, pause/resume and looping
        const char* tempWav = "test_stream_temp.wav";
        GenerateSineWavFile(tempWav, 22050, 22050); // 1.0s mono sine wave

        bool streamOk = AudioEngine::PlayMusic(tempWav);
        TEST_CHECK(streamOk, "PlayMusic on valid WAV file failed");
        TEST_CHECK(AudioEngine::IsMusicPlaying(), "Music should be playing after PlayMusic");

        // Pump stream updates
        for (int i = 0; i < 10; ++i) {
            AudioEngine::UpdateMusicStream();
        }
        TEST_CHECK(AudioEngine::GetMusicUnderrunCount() == 0, "Music underrun occurred during stream");

        // Global Pause/Resume
        AudioEngine::PauseAll();
        TEST_CHECK(AudioEngine::IsPaused(), "AudioEngine should be paused");
        AudioEngine::ResumeAll();
        TEST_CHECK(!AudioEngine::IsPaused(), "AudioEngine should be resumed");

        AudioEngine::StopMusic();
        TEST_CHECK(!AudioEngine::IsMusicPlaying(), "Music should be stopped after StopMusic");

        remove(tempWav);
        printf("  -> Sub-test A PASS: Volume routing independent, streaming music lifecycle verified.\n\n");
    }

    // -------------------------------------------------------------
    // Sub-test B: STATE_PAUSE Physics/Timer Freeze & Resume Fidelity
    // -------------------------------------------------------------
    printf("[Sub-test B] STATE_PAUSE Physics/Timer Freeze & Resume Fidelity...\n");
    {
        Config::LoadAll();
        PlayerInfo::Reset();

        Game game;
        game.SetCurrentStage(0);   // Countryside
        game.SetCurrentVehicle(0); // Jeep
        game.StartRace();

        TEST_CHECK(game.GetState() == STATE_PLAYING, "Game state should be STATE_PLAYING");
        TEST_CHECK(game.GetCar() != NULL, "PlayerCar should be instantiated");
        TEST_CHECK(game.GetTerrain() != NULL, "Terrain should be instantiated");

        // Simulate 30 active frames (0.5s) to establish linear velocity and displacement
        for (int i = 0; i < 30; ++i) {
            game.GetCar()->Accelerate();
            game.UpdatePhysicsStep(1.0f / 60.0f);
        }

        b2Vec2 posBefore = game.GetCar()->GetPosition();
        b2Vec2 velBefore = game.GetCar()->GetChassisBody()->GetLinearVelocity();
        float distBefore = game.GetMaxDistance();

        // Engage PAUSE
        game.SetState(STATE_PAUSE);
        TEST_CHECK(game.GetState() == STATE_PAUSE, "Game state should be STATE_PAUSE");

        // Run 120 pause frames (~2.0 seconds)
        for (int i = 0; i < 120; ++i) {
            System::ButtonsPressed = 0;
            game.UpdatePause();

            b2Vec2 posNow = game.GetCar()->GetPosition();
            b2Vec2 velNow = game.GetCar()->GetChassisBody()->GetLinearVelocity();
            float distNow = game.GetMaxDistance();

            TEST_CHECK(feq(posNow.x, posBefore.x, 0.0001f), "Car X position drifted during pause");
            TEST_CHECK(feq(posNow.y, posBefore.y, 0.0001f), "Car Y position drifted during pause");
            TEST_CHECK(feq(velNow.x, velBefore.x, 0.0001f), "Car X velocity changed during pause");
            TEST_CHECK(feq(velNow.y, velBefore.y, 0.0001f), "Car Y velocity changed during pause");
            TEST_CHECK(feq(distNow, distBefore, 0.0001f), "Car distance drifted during pause");
        }

        // Test Pause Menu Navigation: RESUME (0) -> RESTART (1) -> GARAGE (2) -> RESTART (1)
        TEST_CHECK(game.GetPauseMenuIndex() == 0, "Initial pause menu index should be 0 (RESUME)");

        System::ButtonsPressed = PSP_CTRL_DOWN;
        game.UpdatePause();
        TEST_CHECK(game.GetPauseMenuIndex() == 1, "Pause menu index did not advance to 1 on DOWN");

        System::ButtonsPressed = PSP_CTRL_DOWN;
        game.UpdatePause();
        TEST_CHECK(game.GetPauseMenuIndex() == 2, "Pause menu index did not advance to 2 on DOWN");

        System::ButtonsPressed = PSP_CTRL_UP;
        game.UpdatePause();
        TEST_CHECK(game.GetPauseMenuIndex() == 1, "Pause menu index did not advance to 1 on UP");

        // Resume game via START button toggle
        System::ButtonsPressed = PSP_CTRL_START;
        game.UpdatePause();
        TEST_CHECK(game.GetState() == STATE_PLAYING, "Game state should return to STATE_PLAYING on START");

        // Step physics after unpause and ensure no explosive impulse spike
        game.UpdatePhysicsStep(1.0f / 60.0f);
        b2Vec2 posAfter = game.GetCar()->GetPosition();
        b2Vec2 velAfter = game.GetCar()->GetChassisBody()->GetLinearVelocity();

        TEST_CHECK(!isnan(posAfter.x) && !isnan(posAfter.y), "Position NaN after unpause");
        TEST_CHECK(!isnan(velAfter.x) && !isnan(velAfter.y), "Velocity NaN after unpause");

        float dVx = fabsf(velAfter.x - velBefore.x);
        float dVy = fabsf(velAfter.y - velBefore.y);
        TEST_CHECK(dVx < 1.0f && dVy < 1.0f, "Physics velocity spiked explosively on unpause");

        game.EndRace();
        printf("  -> Sub-test B PASS: STATE_PAUSE freezes simulation 100%%; unpause seamless.\n\n");
    }

    // -------------------------------------------------------------
    // Sub-test C: Distance Milestone Bonus Rewards & Persistence
    // -------------------------------------------------------------
    printf("[Sub-test C] Distance Milestone Bonus Rewards & Persistence...\n");
    {
        CleanupTestSaveFiles("save");
        Config::LoadAll();
        PlayerInfo::Reset();

        float step = Config::Physics.distance_bonus_step;
        int coinsPerStep = Config::Physics.distance_bonus_coins;
        TEST_CHECK(feq(step, 100.0f), "distance_bonus_step should be 100.0f");
        TEST_CHECK(coinsPerStep == 500, "distance_bonus_coins should be 500");

        uint64_t initialWallet = PlayerInfo::GetCoins();

        Game game;
        game.SetCurrentStage(0);
        game.SetCurrentVehicle(0);
        game.StartRace();

        TEST_CHECK(game.GetRaceDistanceBonusCoins() == 0, "Distance bonus coins must initialize to 0");
        TEST_CHECK(game.GetMaxDistance() == 0.0f, "Initial max distance must be 0.0f");

        // 1. Advance to 60m (before first 100m milestone)
        TeleportCar(game, 60.0f);
        game.UpdatePhysicsStep(1.0f / 60.0f);
        TEST_CHECK(game.GetRaceDistanceBonusCoins() == 0, "No milestone bonus expected at 60m");

        // 2. Cross 100m milestone (reach 105m) -> awards 500 coins
        TeleportCar(game, 105.0f);
        game.UpdatePhysicsStep(1.0f / 60.0f);
        TEST_CHECK(game.GetRaceDistanceBonusCoins() == 500, "Distance bonus at 105m should be 500");

        // 3. Cross 200m milestone (reach 215m) -> awards additional 500 coins (total 1000)
        TeleportCar(game, 215.0f);
        game.UpdatePhysicsStep(1.0f / 60.0f);
        TEST_CHECK(game.GetRaceDistanceBonusCoins() == 1000, "Distance bonus at 215m should be 1000");

        // 4. Leap past multiple milestones directly to 510m (crosses 300m, 400m, 500m) -> +1500 (total 2500)
        TeleportCar(game, 510.0f);
        game.UpdatePhysicsStep(1.0f / 60.0f);
        TEST_CHECK(game.GetRaceDistanceBonusCoins() == 2500, "Distance bonus at 510m should be 2500");

        // 5. Reverse movement (vehicle rolls back to 420m) -> distance bonus must NOT decrement
        TeleportCar(game, 420.0f);
        game.UpdatePhysicsStep(1.0f / 60.0f);
        TEST_CHECK(game.GetRaceDistanceBonusCoins() == 2500, "Distance bonus coins must not decrease on rollback");
        TEST_CHECK(game.GetMaxDistance() >= 510.0f, "Max distance must maintain peak distance");

        // 6. Coin pickups and trick bonuses (respects stage coin_multiplier)
        int collectedBefore = game.GetRaceCollectedCoins();
        float mult = game.GetTerrain() ? game.GetTerrain()->GetCoinMultiplier() : 1.0f;
        game.OnCoinCollected(100);
        game.OnCoinCollected(250);
        int expectedAdded = (int)(100 * mult) + (int)(250 * mult);
        TEST_CHECK(game.GetRaceCollectedCoins() == collectedBefore + expectedAdded, "Collected coins sum mismatch");

        // 7. Verify session coins invariant: mSessionCoins == collected + tricks + distance_bonus
        int expectedSessionCoins = game.GetRaceCollectedCoins() + game.GetRaceTrickBonusCoins() + game.GetRaceDistanceBonusCoins();
        TEST_CHECK(game.GetSessionCoins() == expectedSessionCoins, "Session coins invariant violated");
        TEST_CHECK(game.GetRaceDistanceBonusCoins() == 2500, "Distance bonus total mismatch");

        // 8. Commit results and verify PlayerInfo persistence
        game.CommitRaceResults();
        uint64_t walletAfter = PlayerInfo::GetCoins();
        TEST_CHECK(walletAfter == initialWallet + (uint64_t)expectedSessionCoins, "Wallet mismatch after CommitRaceResults");

        uint32_t stageHash = PlayerInfo::HashId(Config::Stages[0].id);
        int record = PlayerInfo::GetStageRecord(stageHash);
        TEST_CHECK(record >= 510, "Stage record in PlayerInfo not updated to peak distance");

        game.EndRace();
        CleanupTestSaveFiles("save");
        printf("  -> Sub-test C PASS: Distance milestone bonuses calculated & persisted correctly.\n\n");
    }

    printf("=================================================================\n");
    printf("   TEST 23 PASSED: All Audio, Pause & Bonus Checks Succeeded!    \n");
    printf("=================================================================\n");
    return 0;
}
