#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <Box2D/Box2D.h>

#include "core/Config.h"
#include "core/GameConstants.h"
#include "physics/PhysicsWorld.h"
#include "physics/PlayerCar.h"
#include "physics/Terrain.h"
#include "graphics/Renderer2D.h"
#include "graphics/ParticleSystem.h"

#define TEST_CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
            return 1; \
        } \
    } while (0)

int main() {
    printf("=================================================================\n");
    printf("   HCR Regression Test: Particles & Stage Gating (Test 17)       \n");
    printf("=================================================================\n\n");

    // Load configs from production cfg
    bool loaded = Config::LoadAll("");
    if (!loaded) {
        loaded = Config::LoadAll("config/");
    }
    TEST_CHECK(loaded, "Failed to load production config files");
    TEST_CHECK(Config::StageCount == 8, "Expected 8 stages in production config");

    // -------------------------------------------------------------
    // Sub-test A: Stage 0..7 Config & Feature Gating
    // -------------------------------------------------------------
    printf("[Sub-test A] Verifying 8 stages config mapping, sky colors, and feature gating...\n");
    {
        for (int s = 0; s < 8; ++s) {
            const StageConfig& cfg = Config::Stages[s];
            PhysicsWorld physics(cfg.gravity);
            Terrain terrain(physics.GetWorld(), (StageType)s);

            const StageConfig* terrainCfg = terrain.GetStageConfig();
            TEST_CHECK(terrainCfg != NULL, "Terrain must hold non-null StageConfig pointer");
            TEST_CHECK(strcmp(terrainCfg->id, cfg.id) == 0, "Stage ID mismatch");
            TEST_CHECK(terrainCfg->gravity == cfg.gravity, "Stage gravity mismatch");
            TEST_CHECK(terrainCfg->friction == cfg.friction, "Stage friction mismatch");
            TEST_CHECK(terrainCfg->coin_multiplier == cfg.coin_multiplier, "Stage coin multiplier mismatch");

            // Sky gradient colors check (T-041)
            TEST_CHECK(terrainCfg->sky_color_top != 0, "Stage sky_color_top must be non-zero");
            TEST_CHECK(terrainCfg->sky_color_bottom != 0, "Stage sky_color_bottom must be non-zero");

            // Cave ceiling gating
            bool expectedCeiling = (cfg.has_ceiling == 1);
            TEST_CHECK(terrain.HasCeiling() == expectedCeiling, "Terrain ceiling state does not match config has_ceiling");
            if (s == STAGE_CAVE) {
                TEST_CHECK(terrain.HasCeiling() == true, "Cave stage must have ceiling enabled");
            } else {
                TEST_CHECK(terrain.HasCeiling() == false, "Non-cave stage must not have ceiling enabled");
            }

            // Bridge gating (H8 / T-021)
            int bridgeCount = terrain.GetActiveBridgeCount();
            if (s == STAGE_FOREST) {
                TEST_CHECK(bridgeCount == 1, "Forest stage must have exactly 1 active bridge initially");
                const SuspensionBridge* b = terrain.GetBridge(0);
                TEST_CHECK(b != NULL && b->active, "Forest bridge 0 must be active");
                TEST_CHECK(b->plankCount == 10, "Forest bridge must have 10 planks");
            } else {
                TEST_CHECK(bridgeCount == 0, "Non-forest stage must have 0 active bridges");
                // Explicit defensive call check
                terrain.MaybeSpawnBridge(1);
                TEST_CHECK(terrain.GetActiveBridgeCount() == 0, "Defensive gating must prevent bridge spawning on non-forest stage");
            }

            printf("  Stage %d [%-11s]: grav=%.2f, fric=%.2f, mult=%.2f, sky=[0x%08X -> 0x%08X], ceil=%d, bridges=%d -> OK\n",
                   s, cfg.id, cfg.gravity, cfg.friction, cfg.coin_multiplier,
                   cfg.sky_color_top, cfg.sky_color_bottom, terrain.HasCeiling() ? 1 : 0, terrain.GetActiveBridgeCount());
        }
        printf("  [PASS] Sub-test A: Stage 0..7 config & feature gating verified!\n\n");
    }

    // -------------------------------------------------------------
    // Sub-test B: Zero-Alloc Particle Pool & Batching
    // -------------------------------------------------------------
    printf("[Sub-test B] Verifying zero-alloc particle pool, ring buffer clamp & single draw call...\n");
    {
        TEST_CHECK(ParticleSystem::GetMaxCapacity() == 128, "Particle pool max capacity must be exactly 128");

        ParticleSystem::Init();
        ParticleSystem::Reset();
        TEST_CHECK(ParticleSystem::GetActiveCount() == 0, "Active particle count must be 0 after reset");

        // Spawn 500 particles in a loop (overflow stress test)
        for (int i = 0; i < 500; ++i) {
            ParticleSystem::SpawnParticle((float)i * 0.1f, 0.0f, 1.0f, 1.0f, 0.5f, 0.15f, 0.35f, 0xFFFFFFFF, PARTICLE_DIRT);
        }
        int activeAfter500 = ParticleSystem::GetActiveCount();
        printf("  Spawned 500 particles into 128-capacity pool: active count = %d (clamped to max capacity)\n", activeAfter500);
        TEST_CHECK(activeAfter500 == 128, "Active particle count must clamp at MAX_PARTICLES (128)");

        // Batch rendering test
        Renderer2D::Init();
        Renderer2D::ResetMetrics();
        TEST_CHECK(Renderer2D::GetBatchedQuadCount() == 0, "Batched quads should be 0 before render");
        TEST_CHECK(Renderer2D::GetDrawCallCount() == 0, "Draw calls should be 0 before render");

        ParticleSystem::Render();
        TEST_CHECK(Renderer2D::GetBatchedQuadCount() == 128, "All 128 particles must be queued into Renderer2D batch");
        TEST_CHECK(Renderer2D::GetDrawCallCount() == 0, "Draw calls should still be 0 before flush");

        Renderer2D::FlushBatch();
        TEST_CHECK(Renderer2D::GetBatchedQuadCount() == 128, "Batched quad count metric must record 128 quads");
        TEST_CHECK(Renderer2D::GetDrawCallCount() == 1, "All 128 particles must be flushed in exactly 1 draw call");
        printf("  Batch render: 128 particle quads submitted -> flushed in %d draw call!\n", Renderer2D::GetDrawCallCount());

        // Lifetime decay test (35 frames at 60 Hz = 0.5833 s > 0.50 s max life)
        const float dt = 1.0f / 60.0f;
        for (int frame = 0; frame < 35; ++frame) {
            ParticleSystem::Update(dt);
        }
        int activeAfterDecay = ParticleSystem::GetActiveCount();
        printf("  Advanced 35 frames (0.583 s): active count = %d (expected 0, zero leaks)\n", activeAfterDecay);
        TEST_CHECK(activeAfterDecay == 0, "All particles must cleanly expire after lifetime");

        // Specific emitter tests: Wheel Dirt & Exhaust Smoke
        ParticleSystem::EmitWheelDirt(12.0f, 4.0f, 2.5f, 0.5f, STAGE_COUNTRYSIDE);
        TEST_CHECK(ParticleSystem::GetActiveCount() == 1, "Wheel dirt emitter must activate 1 particle");

        ParticleSystem::EmitExhaust(10.0f, 4.5f, 2.5f, 0.0f);
        TEST_CHECK(ParticleSystem::GetActiveCount() == 2, "Exhaust smoke emitter must activate 1 particle");
        printf("  Emitters activated: Dirt + Exhaust -> active count = %d\n", ParticleSystem::GetActiveCount());

        // Let emitters decay
        for (int frame = 0; frame < 60; ++frame) {
            ParticleSystem::Update(dt);
        }
        TEST_CHECK(ParticleSystem::GetActiveCount() == 0, "Emitter particles must cleanly expire after 60 frames");
        printf("  [PASS] Sub-test B: Zero-alloc pool, batching and decay verified!\n\n");
    }

    printf("=================================================================\n");
    printf("   ALL 2 SUB-TESTS PASSED (Test 17: T-041 / T-042)               \n");
    printf("=================================================================\n");
    return 0;
}

