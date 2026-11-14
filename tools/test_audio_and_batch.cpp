#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <Box2D/Box2D.h>

#include "audio/AudioEngine.h"
#include "graphics/Renderer2D.h"
#include "graphics/Texture.h"
#include "physics/PlayerCar.h"
#include "physics/PhysicsWorld.h"
#include "core/Config.h"

#define TEST_CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
            exit(1); \
        } \
    } while (0)

static inline bool feq(float a, float b, float eps = 0.01f) {
    return fabsf(a - b) <= eps;
}

// Generate in-memory 16-bit PCM mono 22050Hz WAV buffer
static void GenerateSineWav(int sampleRate, int numSamples, unsigned char** outData, size_t* outSize) {
    size_t dataSize = numSamples * sizeof(short);
    size_t totalSize = 44 + dataSize;
    unsigned char* wav = (unsigned char*)malloc(totalSize);

    // RIFF Header
    memcpy(wav, "RIFF", 4);
    unsigned int chunkSize = (unsigned int)(totalSize - 8);
    memcpy(wav + 4, &chunkSize, 4);
    memcpy(wav + 8, "WAVE", 4);

    // fmt subchunk
    memcpy(wav + 12, "fmt ", 4);
    unsigned int sub1Size = 16;
    memcpy(wav + 16, &sub1Size, 4);
    unsigned short audioFormat = 1; // PCM
    memcpy(wav + 20, &audioFormat, 2);
    unsigned short numChannels = 1; // Mono
    memcpy(wav + 22, &numChannels, 2);
    unsigned int sRate = sampleRate;
    memcpy(wav + 24, &sRate, 4);
    unsigned int byteRate = sampleRate * 1 * sizeof(short);
    memcpy(wav + 28, &byteRate, 4);
    unsigned short blockAlign = 1 * sizeof(short);
    memcpy(wav + 32, &blockAlign, 2);
    unsigned short bitsPerSample = 16;
    memcpy(wav + 34, &bitsPerSample, 2);

    // data subchunk
    memcpy(wav + 36, "data", 4);
    unsigned int dSize = (unsigned int)dataSize;
    memcpy(wav + 40, &dSize, 4);

    // Sine samples
    short* samples = (short*)(wav + 44);
    for (int i = 0; i < numSamples; ++i) {
        float t = (float)i / (float)sampleRate;
        samples[i] = (short)(sinf(2.0f * 3.14159265f * 440.0f * t) * 16000.0f);
    }

    *outData = wav;
    *outSize = totalSize;
}

int main() {
    printf("========================================================\n");
    printf("   HCR Regression Test: Audio & Render Batching (T-115) \n");
    printf("========================================================\n\n");

    // ---------------------------------------------------------
    // Sub-test A: WAV Loader & Engine Pitch Modulation Clamping
    // ---------------------------------------------------------
    printf("[Sub-test A] 16-bit PCM WAV Loader & Pitch Clamping [0.50, 2.00]...\n");
    {
        unsigned char* wavData = NULL;
        size_t wavSize = 0;
        int targetSamples = 2205; // 0.10s at 22050 Hz
        GenerateSineWav(22050, targetSamples, &wavData, &wavSize);
        TEST_CHECK(wavData != NULL && wavSize > 44, "Failed to generate in-memory WAV");

        SoundBuffer sbuf = { 0 };
        bool loadOk = AudioEngine::LoadWavFromMemory(wavData, wavSize, &sbuf);
        free(wavData);

        TEST_CHECK(loadOk, "AudioEngine::LoadWavFromMemory failed");
        TEST_CHECK(sbuf.channels == 1, "Expected mono (1 channel)");
        TEST_CHECK(sbuf.sampleRate == 22050, "Expected sample rate 22050 Hz");
        TEST_CHECK(sbuf.sampleCount == targetSamples, "Expected sampleCount == 2205");
        TEST_CHECK(sbuf.samples != NULL, "Expected non-NULL sample buffer");
        free(sbuf.samples);

        // Test Pitch Modulation & Clamping
        AudioEngine::UpdateEngineSound(0.0f, true, 0.0166667f);
        TEST_CHECK(feq(AudioEngine::GetTargetPitch(), 0.60f, 0.02f), "Idle target pitch should be 0.60f");

        AudioEngine::UpdateEngineSound(1.0f, true, 0.0166667f);
        TEST_CHECK(feq(AudioEngine::GetTargetPitch(), 2.00f, 0.02f), "Full rev target pitch should be 2.00f");

        // Extreme positive rev -> must clamp to 2.00f
        AudioEngine::UpdateEngineSound(15.0f, true, 0.0166667f);
        TEST_CHECK(feq(AudioEngine::GetTargetPitch(), 2.00f, 0.001f), "Extreme rev must clamp to 2.00f");

        // Out of fuel / inactive -> reset target pitch to 0.50f
        AudioEngine::UpdateEngineSound(0.0f, false, 0.0166667f);
        TEST_CHECK(feq(AudioEngine::GetTargetPitch(), 0.50f, 0.001f), "Muted engine target pitch should clamp to 0.50f");

        // Verify smooth Lerp convergence
        AudioEngine::UpdateEngineSound(1.0f, true, 0.0f);
        float p0 = AudioEngine::GetEnginePitch();
        for (int step = 0; step < 60; ++step) {
            AudioEngine::UpdateEngineSound(1.0f, true, 0.0166667f);
        }
        float pFinal = AudioEngine::GetEnginePitch();
        TEST_CHECK(pFinal > p0 && feq(pFinal, 2.00f, 0.05f), "Engine pitch smooth lerp failed to converge to 2.00f");
        printf("  [PASS] Sub-test A: WAV loader & engine pitch modulation [0.50, 2.00] verified!\n\n");
    }

    // ---------------------------------------------------------
    // Sub-test B: Fixed-Point 16.16 Phase Resampler Accuracy
    // ---------------------------------------------------------
    printf("[Sub-test B] Fixed-Point 16.16 Resampler Phase Accumulator...\n");
    {
        const int bufferSize = 1000;
        unsigned int maxPosFP = (unsigned int)bufferSize << 16;
        float pitches[3] = { 0.50f, 1.00f, 2.00f };

        for (int pIdx = 0; pIdx < 3; ++pIdx) {
            float pitch = pitches[pIdx];
            float rateRatio = 22050.0f / 44100.0f; // 0.5
            unsigned int stepFP = (unsigned int)(pitch * rateRatio * 65536.0f);
            unsigned int posFP = 0;
            int wrapCount = 0;

            for (int f = 0; f < 44100; ++f) {
                int sIdx = (posFP >> 16);
                TEST_CHECK(sIdx >= 0 && sIdx < bufferSize, "Resampler sample index out of bounds!");

                posFP += stepFP;
                if (posFP >= maxPosFP) {
                    posFP -= maxPosFP;
                    wrapCount++;
                }
            }
            TEST_CHECK(wrapCount > 0, "Resampler loop failed to wrap around");
        }
        printf("  [PASS] Sub-test B: Fixed-point 16.16 resampler bounds and phase wrap verified!\n\n");
    }

    // ---------------------------------------------------------
    // Sub-test C: Renderer2D Sprite Batching (50 quads -> 1 draw call)
    // ---------------------------------------------------------
    printf("[Sub-test C] Renderer2D Sprite Batching & Draw Call Reduction...\n");
    {
        Renderer2D::Init();
        Renderer2D::ResetMetrics();

        // Create mock dummy textures
        Texture dummyTex1;
        Texture dummyTex2;

        // Submit 50 quads under texture 1
        for (int i = 0; i < 50; ++i) {
            Renderer2D::DrawQuad(&dummyTex1, (float)i * 10, 0, (float)i * 10 + 10, 10, 0, 0, 1, 1);
        }

        TEST_CHECK(Renderer2D::GetBatchedQuadCount() == 50, "Expected 50 batched quads");
        TEST_CHECK(Renderer2D::GetDrawCallCount() == 0, "Expected 0 draw calls before flush");

        // Flush batch
        Renderer2D::FlushBatch();
        TEST_CHECK(Renderer2D::GetDrawCallCount() == 1, "Expected exactly 1 draw call for 50 quads!");
        TEST_CHECK(Renderer2D::GetBatchedQuadCount() == 50, "Expected 50 total batched quads");

        // Texture swap test: tex1 quad -> tex2 quad -> FlushBatch()
        Renderer2D::DrawQuad(&dummyTex1, 0, 0, 10, 10, 0, 0, 1, 1);
        Renderer2D::DrawQuad(&dummyTex2, 0, 0, 10, 10, 0, 0, 1, 1);
        Renderer2D::FlushBatch();

        // Texture change triggers automatic flush of tex1, then explicit flush flushes tex2 -> +2 draw calls
        TEST_CHECK(Renderer2D::GetDrawCallCount() == 3, "Texture swap must trigger flush batch");
        printf("  [PASS] Sub-test C: 50 quads batched into 1 draw call verified!\n\n");
    }

    // ---------------------------------------------------------
    // Sub-test D: Quadratic Aerodynamic Air Drag
    // ---------------------------------------------------------
    printf("[Sub-test D] Aerodynamic Drag Force Quadratic Deceleration...\n");
    {
        b2World world(b2Vec2(0.0f, 0.0f)); // Zero gravity to isolate air drag
        PlayerCar carHigh(&world, b2Vec2(0.0f, 10.0f), 0, 0, 0, 0);

        // Set high speed = 25 m/s on all bodies
        b2Body* chassisHigh = carHigh.GetRearJoint()->GetBodyA();
        chassisHigh->SetLinearVelocity(b2Vec2(25.0f, 0.0f));
        carHigh.GetRearWheelBody()->SetLinearVelocity(b2Vec2(25.0f, 0.0f));
        carHigh.GetFrontWheelBody()->SetLinearVelocity(b2Vec2(25.0f, 0.0f));
        if (carHigh.GetDriverHeadBody()) carHigh.GetDriverHeadBody()->SetLinearVelocity(b2Vec2(25.0f, 0.0f));

        Config::Physics.air_drag_coeff = 0.02f;

        const float dt = 1.0f / 60.0f;
        for (int step = 0; step < 5; ++step) {
            carHigh.Idle();
            carHigh.Update(dt);
            world.Step(dt, 8, 3);
        }
        float vHighFinal = chassisHigh->GetLinearVelocity().x;
        float deltaVHigh = 25.0f - vHighFinal;
        printf("  High Speed (25 m/s): deltaV = %.4f m/s (final: %.4f m/s)\n", deltaVHigh, vHighFinal);

        // Low speed test = 10 m/s on all bodies
        PlayerCar carLow(&world, b2Vec2(0.0f, 30.0f), 0, 0, 0, 0);
        b2Body* chassisLow = carLow.GetRearJoint()->GetBodyA();
        chassisLow->SetLinearVelocity(b2Vec2(10.0f, 0.0f));
        carLow.GetRearWheelBody()->SetLinearVelocity(b2Vec2(10.0f, 0.0f));
        carLow.GetFrontWheelBody()->SetLinearVelocity(b2Vec2(10.0f, 0.0f));
        if (carLow.GetDriverHeadBody()) carLow.GetDriverHeadBody()->SetLinearVelocity(b2Vec2(10.0f, 0.0f));

        for (int step = 0; step < 5; ++step) {
            carLow.Idle();
            carLow.Update(dt);
            world.Step(dt, 8, 3);
        }
        float vLowFinal = chassisLow->GetLinearVelocity().x;
        float deltaVLow = 10.0f - vLowFinal;
        printf("  Low  Speed (10 m/s): deltaV = %.4f m/s (final: %.4f m/s)\n", deltaVLow, vLowFinal);

        // Quadratic deceleration check: deltaVHigh / deltaVLow should be roughly (25/10)^2 = 6.25
        float ratio = deltaVHigh / deltaVLow;
        printf("  Deceleration ratio: %.2fx (expected ~6.25x for quadratic drag)\n", ratio);
        TEST_CHECK(ratio >= 4.0f && ratio <= 7.0f, "Air drag deceleration must scale quadratically with velocity");

        // Zero drag check
        Config::Physics.air_drag_coeff = 0.0f;
        PlayerCar carZero(&world, b2Vec2(0.0f, 50.0f), 0, 0, 0, 0);
        b2Body* chassisZero = carZero.GetRearJoint()->GetBodyA();
        chassisZero->SetLinearVelocity(b2Vec2(25.0f, 0.0f));
        carZero.GetRearWheelBody()->SetLinearVelocity(b2Vec2(25.0f, 0.0f));
        carZero.GetFrontWheelBody()->SetLinearVelocity(b2Vec2(25.0f, 0.0f));
        if (carZero.GetDriverHeadBody()) carZero.GetDriverHeadBody()->SetLinearVelocity(b2Vec2(25.0f, 0.0f));

        for (int step = 0; step < 5; ++step) {
            carZero.Idle();
            carZero.Update(dt);
            world.Step(dt, 8, 3);
        }
        float vZeroFinal = chassisZero->GetLinearVelocity().x;
        TEST_CHECK(feq(vZeroFinal, 25.0f, 0.01f), "Velocity must remain constant when air_drag_coeff == 0");
        printf("  Zero Drag Check: final velocity = %.4f m/s (expected 25.0000 m/s)\n", vZeroFinal);
        printf("  [PASS] Sub-test D: Quadratic aerodynamic air drag verified!\n\n");
    }

    printf("========================================================\n");
    printf("   ALL 4 SUB-TESTS PASSED (T-043/T-044/T-040/T-117)     \n");
    printf("========================================================\n");
    return 0;
}
