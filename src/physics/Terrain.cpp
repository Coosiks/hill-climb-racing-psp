#include "Terrain.h"
#include "core/Config.h"
#include <math.h>
#include <stdio.h>

#if defined(__PSP__)
#include <psprtc.h>
static u64 sTotalChunkFullGenTicks = 0;
static int sCompletedChunkCount = 0;
static u64 sMaxPerFrameGenTicks = 0;
static u64 sTotalPerFrameGenTicks = 0;
static int sPerFrameGenCount = 0;
#endif

static inline float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline double ApplyBridgeChasmDip(float x, double h, double depth) {
    float modX = fmodf(x, 120.0f);
    if (modX < 0.0f) modX += 120.0f;
    if (x >= 30.0f && modX >= 39.0f && modX <= 51.0f) {
        double u = (double)(modX - 39.0f) / 12.0;
        h -= sin(u * 3.14159265358979323846) * depth;
    }
    return h;
}

static inline double ApplyMudBasinDip(float x, double h, float startOff, float endOff, double depth) {
    float modX = fmodf(x, 60.0f);
    if (modX < 0.0f) modX += 60.0f;
    if (x >= 15.0f && modX >= startOff && modX <= endOff) {
        double u = (double)(modX - startOff) / (double)(endOff - startOff);
        h -= sin(u * 3.14159265358979323846) * depth;
    }
    return h;
}

Terrain::Terrain(b2World* world, StageType stageType)
    : mWorld(world), mStageType(stageType),
      mRearWallBody(NULL), mItemCount(0), mLastItemSpawnX(15.0f), mNextFuelX(90.0f),
      mQueueHead(0), mQueueTail(0), mQueueCount(0) {

    for (int i = 0; i < DEFERRED_QUEUE_CAPACITY; ++i) {
        mDestroyQueue[i] = NULL;
    }

    // Load textures based on stage config
#if defined(__PSP__)
    u32 t0 = sceKernelGetSystemTimeLow();
#endif
    int stageIdx = (mStageType >= 0 && mStageType < STAGE_COUNT) ? mStageType : STAGE_COUNTRYSIDE;
    if (stageIdx < Config::StageCount) {
        mStageConfig = &Config::Stages[stageIdx];
    } else {
        mStageConfig = &Config::Stages[STAGE_COUNTRYSIDE];
    }

    mNextFuelX = (mStageConfig && mStageConfig->fuel_interval > 0.0f) ? mStageConfig->fuel_interval : 90.0f;

    if (mStageConfig->ground_texture[0]) {
        mTexGround.LoadPNG(mStageConfig->ground_texture);
    } else {
        mTexGround.LoadPNG("assets/terrain-ground.png");
    }

    if (mStageConfig->surface_texture[0]) {
        mTexSurface.LoadPNG(mStageConfig->surface_texture);
    } else {
        mTexSurface.LoadPNG("assets/terrain-surface.png");
    }

    if (mStageType == STAGE_ROOFTOPS) {
        mTexRooftopWall.LoadPNG("assets/building-01-body.png");
    }

    if (mStageConfig && mStageConfig->has_mud_pools != 0) {
        if (mStageType == STAGE_VOLCANO || mStageType == STAGE_RAGNAROK) {
            mTexLiquid.LoadPNG("assets/lava.png");
        } else if (mStageType == STAGE_NUCLEAR) {
            mTexLiquid.LoadPNG("assets/nuclear-slime.png");
        } else {
            mTexLiquid.LoadPNG("assets/mud.png");
        }
    }

    // Collectibles
    mTexCoin5.LoadPNG("assets/Coin5.png");
    mTexCoin25.LoadPNG("assets/Coin25.png");
    mTexCoin100.LoadPNG("assets/Coin100.png");
    mTexCoin500.LoadPNG("assets/Coin500.png");
    mTexFuel.LoadPNG("assets/fuel-canister.png");
    mTexBridge.LoadPNG("assets/BridgeWoodBlock.png");
#if defined(__PSP__)
    u32 t1 = sceKernelGetSystemTimeLow();
    printf("[PERF_ASSETS] Terrain textures loaded in %u us (%.2f ms)\n", (unsigned int)(t1 - t0), (float)(t1 - t0) / 1000.0f);
#endif

    InitChunkPool();
    InitBridgePool();
    UpdateChunkWindow(0);
    // Synchronously generate initial loading window: chunks -1, 0, 1, 2
    for (int i = 0; i < NUM_CHUNK_SLOTS; ++i) {
        if (mChunks[i].active && !mChunks[i].ready && mChunks[i].chunkIndex <= 2) {
            CompleteChunkImmediately(i);
        }
    }
#if defined(__PSP__)
    // Reset runtime stats so synchronous startup loading is not counted
    sMaxPerFrameGenTicks = 0;
    sTotalPerFrameGenTicks = 0;
    sPerFrameGenCount = 0;
#endif
}

Terrain::~Terrain() {
    // 0. Destroy all active bridges in the pool
    DestroyAllBridges();

    // 1. Destroy chunk pool bodies
    for (int i = 0; i < NUM_CHUNK_SLOTS; ++i) {
        if (mChunks[i].fixture && mChunks[i].body) {
            mChunks[i].body->DestroyFixture(mChunks[i].fixture);
            mChunks[i].fixture = NULL;
        }
        if (mChunks[i].body) {
            mWorld->DestroyBody(mChunks[i].body);
            mChunks[i].body = NULL;
        }
        mChunks[i].active = false;
        mChunks[i].chunkIndex = -1;
    }

    // 2. Destroy rear wall body
    if (mRearWallBody) {
        mWorld->DestroyBody(mRearWallBody);
        mRearWallBody = NULL;
    }

    // 3. Clear deferred queue
    mQueueHead = 0;
    mQueueTail = 0;
    mQueueCount = 0;
    for (int i = 0; i < DEFERRED_QUEUE_CAPACITY; ++i) {
        mDestroyQueue[i] = NULL;
    }

    // 4. Destroy active item bodies
    for (int i = 0; i < mItemCount; ++i) {
        if (mItems[i].body) {
            mWorld->DestroyBody(mItems[i].body);
            mItems[i].body = NULL;
        }
    }
    mItemCount = 0;
}

b2Body* Terrain::SpawnItem(float x, float y, int type) {
    if (mItemCount >= MAX_COLLECTIBLES) return NULL;

    b2BodyDef bd;
    bd.type = b2_staticBody;
    bd.position.Set(x, y);
    b2Body* b = mWorld->CreateBody(&bd);

    b2CircleShape circle;
    circle.m_radius = (type >= 1000) ? 0.5f : 0.4f;

    b2FixtureDef fd;
    fd.shape = &circle;
    fd.isSensor = true;
    fd.filter.categoryBits = CAT_ITEM;
    fd.filter.maskBits = CAT_CHASSIS | CAT_WHEEL;
    fd.userData = (void*)(uintptr_t)type;

    b->CreateFixture(&fd);

    mItems[mItemCount].x = x;
    mItems[mItemCount].y = y;
    mItems[mItemCount].type = type;
    mItems[mItemCount].body = b;
    mItems[mItemCount].collected = false;
    b->SetUserData(&mItems[mItemCount]);
    mItemCount++;

    return b;
}

void Terrain::EnqueueDestroy(b2Body* body) {
    if (!body) return;
    if (mQueueCount >= DEFERRED_QUEUE_CAPACITY) return;

    for (int i = 0; i < mQueueCount; ++i) {
        int idx = (mQueueHead + i) % DEFERRED_QUEUE_CAPACITY;
        if (mDestroyQueue[idx] == body) return; // already queued
    }

    mDestroyQueue[mQueueTail] = body;
    mQueueTail = (mQueueTail + 1) % DEFERRED_QUEUE_CAPACITY;
    mQueueCount++;
}

void Terrain::ProcessDeferredDestroy(float carX) {
    // 1. Process deferred destruction ring buffer
    while (mQueueCount > 0) {
        b2Body* body = mDestroyQueue[mQueueHead];
        mQueueHead = (mQueueHead + 1) % DEFERRED_QUEUE_CAPACITY;
        mQueueCount--;

        if (body) {
            for (int i = 0; i < mItemCount; ++i) {
                if (mItems[i].body == body) {
                    mItems[i].body = NULL;
                    mItems[i].collected = true;
                    mItems[i] = mItems[mItemCount - 1];
                    mItemCount--;
                    if (i < mItemCount && mItems[i].body) {
                        mItems[i].body->SetUserData(&mItems[i]);
                    }
                    break;
                }
            }
            mWorld->DestroyBody(body);
        }
    }

    // 2. Despawn items behind carX - 40.0f
    float despawnX = carX - 40.0f;
    for (int i = 0; i < mItemCount; ++i) {
        if (mItems[i].x < despawnX) {
            if (mItems[i].body) {
                for (int q = 0; q < mQueueCount; ++q) {
                    int qIdx = (mQueueHead + q) % DEFERRED_QUEUE_CAPACITY;
                    if (mDestroyQueue[qIdx] == mItems[i].body) {
                        mDestroyQueue[qIdx] = NULL;
                    }
                }
                mWorld->DestroyBody(mItems[i].body);
                mItems[i].body = NULL;
            }
            mItems[i] = mItems[mItemCount - 1];
            mItemCount--;
            if (i < mItemCount && mItems[i].body) {
                mItems[i].body->SetUserData(&mItems[i]);
            }
            i--; // re-check swapped item
        }
    }
}

bool Terrain::HasCeiling() const {
    return (mStageConfig && mStageConfig->has_ceiling != 0);
}

float Terrain::GetStageFriction() const {
    if (mStageConfig) {
        return mStageConfig->friction;
    }
    int stageIdx = (mStageType >= 0 && mStageType < STAGE_COUNT) ? mStageType : STAGE_COUNTRYSIDE;
    if (stageIdx < Config::StageCount) {
        return Config::Stages[stageIdx].friction;
    }
    return Config::Stages[STAGE_COUNTRYSIDE].friction;
}

float Terrain::GetCoinMultiplier() const {
    if (mStageConfig) {
        return mStageConfig->coin_multiplier;
    }
    int stageIdx = (mStageType >= 0 && mStageType < STAGE_COUNT) ? mStageType : STAGE_COUNTRYSIDE;
    if (stageIdx < Config::StageCount) {
        return Config::Stages[stageIdx].coin_multiplier;
    }
    return Config::Stages[STAGE_COUNTRYSIDE].coin_multiplier;
}

float Terrain::GetPoolSurfaceY(float x) const {
    float cyc = floorf(x / 60.0f);
    float off0 = (mStageType == STAGE_VOLCANO) ? 20.0f : 18.0f;
    float off1 = (mStageType == STAGE_VOLCANO) ? 34.0f : 32.0f;
    float p0 = cyc * 60.0f + off0;
    float p1 = cyc * 60.0f + off1;
    return fminf(GetHeightAtX(p0), GetHeightAtX(p1)) - 0.14f;
}

// 1:1 mathematical reconstruction of Stage*::getHeightAtX(float) from libgame.so
float Terrain::GetHeightAtX(float x) const {
    float t = x * 1.5f + 10.0f;

    switch (mStageType) {
        case STAGE_DESERT: {
            double w1 = cos(sin((double)(t * 0.15243903f)) + (double)(t * 0.33333334f));
            float k1 = clampf(t * 0.0015f, 0.0f, 1.0f);
            double mod1 = cos((double)t + sin((double)(t * 0.175f)));
            double s = w1 * 0.35 + 0.30 * (double)k1 * mod1;

            double w2 = cos(sin((double)(t * 0.0515f)) + (double)(t * 0.11f));
            float k2 = clampf(t * 0.00075f, 0.0f, 1.0f);
            s += 2.5 * (double)k2 * w2;

            double w3 = sin(cos((double)(t * 0.0151f)) + (double)(t * 0.132f));
            float k3 = clampf(t * 0.00275f, 0.0f, 1.0f);
            s += 3.5 * (double)k3 * w3;

            double w4 = sin(cos((double)(t * 0.015590895f)) + (double)(t * 0.03333334f));
            float k4 = clampf(t * 0.00175f, 0.0f, 1.0f);
            s += 2.5 * (double)k4 * w4;

            float amp = 0.5f + clampf(t * 0.0005f, 0.0f, 1.5f);
            return (float)s * amp;
        }

        case STAGE_ARCTIC: {
            double w1 = 2.0 * cos((double)(t * 0.10f));
            float k1 = clampf(t * 0.0015f, 0.0f, 1.0f);
            double mod1 = cos((double)t + sin((double)(t * 0.3215f)));
            double s = w1 + 0.10 * (double)k1 * mod1;

            double w2 = cos(sin((double)(t * 0.165f)) + (double)(t * 0.180f));
            float k2 = clampf(t * 0.00145f, 0.0f, 1.0f);
            s += 1.5 * (double)k2 * w2;

            double w3 = cos(sin((double)(t * 0.115f)) + (double)(t * 0.0952f));
            float k3 = clampf(t * 0.00275f, 0.0f, 1.0f);
            s += 1.75 * (double)k3 * w3;

            double w4 = sin(cos((double)(t * 0.03111388f)) + (double)(t * 0.09090909f));
            s += 4.0 * (double)k2 * w4;

            float amp = 0.5f + clampf(t * 0.000625f, 0.0f, 2.0f);
            return (float)s * amp;
        }

        case STAGE_MOON: {
            double w1 = 2.0 * cos((double)(t * 0.125f));
            float k1 = clampf(t * 0.0015f, 0.0f, 1.0f);
            double mod1 = cos((double)t + sin((double)(t * 0.25f + 1.0f)));
            double s = w1 + 0.20 * (double)k1 * mod1;

            double w2 = cos(sin((double)(t * 0.45f)) + (double)(t * 0.14f));
            float k2 = clampf(t * 0.00075f, 0.0f, 2.0f);
            s += 2.0 * (double)k2 * w2;

            double w3 = cos(sin((double)(t * 0.095f)) + (double)(t * 0.0752f));
            float k3 = clampf(t * 0.00275f, 0.0f, 1.0f);
            s += 2.0 * (double)k3 * w3;

            double w4 = sin(cos((double)(t * 0.04730369f)) + (double)(t * 0.03333334f));
            float k4 = clampf(t * 0.00275f, 0.0f, 2.0f);
            s += 4.0 * (double)k4 * w4;

            float amp = 0.5f + clampf(t * 0.00066667f, 0.0f, 1.5f);
            return (float)s * amp;
        }

        case STAGE_HIGHWAY: {
            float sx = x + 3.0f;
            float v2 = cosf(((sx * 6.7f) / 80.0f + 3.14159f) * 4.0f);
            float v4 = cosf((sx * 6.7f) / 73.0f + 3.14159f) * 1.6f;
            float v6 = cosf(0.5f * ((sx * 6.7f) / 57.0f + 3.14159f));
            float v8 = v6 * 1.6f * clampf((2.5f * sx) / 300.0f, 0.0f, 1.0f);
            float v9 = v2 * 0.5f + v4 + v8;
            float v11 = cosf(((sx * 6.7f) / 78.0f + 3.14159f) / 3.0f);
            float v13 = 1.6f * v11 * clampf((2.5f * sx) / 600.0f, 0.0f, 1.0f);
            float v14 = cosf(((sx * 6.7f) / 57.0f + 3.14159f) / 7.0f);
            float v16 = v14 * 2.6f * clampf((2.5f * sx) / 1200.0f, 0.0f, 1.0f);
            float v18 = cosf(((sx * 6.7f) / 86.0f + 3.14159f) * 0.25f);
            float v20 = v18 * 1.35f * clampf((2.5f * sx) / 1400.0f, 0.0f, 1.0f);
            float v21 = v9 + v13 + v16 + v20;
            float v23 = sinf(((sx * 6.7f) / 67.0f + 3.14159f) * 0.25f);
            float v25 = v23 * 1.25f * clampf((2.5f * sx) / 1500.0f, 0.0f, 1.0f);
            float sum = v21 + v25;
            float v27 = sinf((2.5f * sx) / 225.0f) * 0.5f + 0.5f;
            float v29 = clampf(powf(v27, 1.0f), 0.0f, 1.0f) * (0.05f * sum - sum);
            return (clampf((2.5f * sx) / 1350.0f, 0.0f, 1.0f) + 1.35f) * (v29 + sum);
        }

        case STAGE_CAVE: {
            float tc = x * 1.5f + 10.0f;
            float v3 = cosf(tc / 7.21f + sinf(tc / 17.0f));
            float v5 = (sinf(tc / 185.0f) + 1.5f) * v3;
            float v9 = cosf(sinf(tc * 0.1265f) + tc) * 0.2f * sinf(tc / 200.0f);
            float v10 = clampf(x / 100.0f, 0.0f, 1.0f) * v9;
            float v13 = cosf(tc / 7.651f + sinf(tc / 25.521f));
            float v14 = (v13 + v13) * clampf(x / 900.0f, 0.0f, 1.0f);
            float v17 = cosf(tc / 11.518f + sinf(tc / 15.412f));
            float v18 = (v17 + v17) * clampf(x / 1900.0f, 0.0f, 1.0f);
            float v19 = v18 + v14 + v10 + v5;
            float v22 = sinf(tc / 35.0f + cosf(tc / 42.14f));
            float v23 = v22 * 5.0f * clampf(x / 2500.0f, 0.0f, 1.0f);
            float base = v23 - 1.0f + v19;
            float v25 = clampf(powf(sinf(x / 95.0f) * 0.5f + 0.5f, 1.0f), 0.0f, 1.0f);
            float v29 = v25 * (base * 0.05f - base) + base;
            return (clampf(tc / 1200.0f, 0.0f, 1.0f) + 0.75f) * v29;
        }

        case STAGE_MARS: {
            float tm = x * 1.75f + 10.0f;
            float v3 = cosf(tm / 9.21f + sinf(tm / 67.0f));
            float v5 = (sinf(tm / 185.0f) + 1.95f) * v3;
            float v9 = cosf(sinf(tm * 0.1265f) + tm) * 0.2f * sinf(tm / 200.0f);
            float v10 = clampf(x / 100.0f, 0.0f, 1.0f) * v9;
            float v13 = cosf(tm / 8.651f + sinf(tm / 33.521f));
            float v14 = v13 * 1.8f * clampf(x / 900.0f, 0.0f, 1.0f);
            float v17 = cosf(tm / 13.518f + sinf(tm / 22.412f));
            float v18 = v17 * 2.2f * clampf(x / 1900.0f, 0.0f, 1.0f);
            float v19 = v18 + v14 + v10 + v5;
            float v22 = sinf(tm / 42.5f + cosf(tm / 64.14f));
            float v23 = v22 * 5.0f * clampf(x / 2500.0f, 0.0f, 1.0f);
            float base = v23 - 1.0f + v19;
            float v25 = clampf(powf(sinf(x / 65.0f) * 0.5f + 0.5f, 1.0f), 0.0f, 1.0f);
            float v29 = v25 * (base * 0.05f - base) + base;
            return (clampf(tm / 1200.0f, 0.0f, 1.0f) + 0.75f) * v29;
        }

        case STAGE_FOREST: {
            float tf = t;
            float v2 = 2.0f * cosf(tf / 10.0f);
            float v3 = tf * 0.215f;
            float v6 = sinf(tf + sinf(v3)) * 0.2f * clampf(tf * 0.0015f, 0.0f, 1.0f);
            float v9 = cosf(tf * 0.25f + sinf(tf * 0.515f));
            float v10 = v9 * 0.25f * clampf(tf * 0.0015f, 0.0f, 1.0f);
            float v13 = tf * 0.16f + sinf(tf * 0.1515f) + 2.411f;
            float v15 = 2.0f * cosf(v13) * clampf(tf * 0.00075f, 0.0f, 1.0f);
            float v18 = tf * 0.0952f + sinf(v3) + 5.6f;
            float v20 = 2.0f * cosf(v18) * clampf(tf * 0.00275f, 0.0f, 1.0f);
            float v24 = sinf(tf * 0.03125f + cosf(tf / 31.14f));
            float v25 = v24 * 4.0f * clampf(tf * 0.00075f, 0.0f, 1.0f);
            double h = (double)((clampf(tf / 2000.0f, 0.0f, 1.5f) + 0.5f) * (v2 + v6 + v10 + v15 + v20 + v25 - 1.0f));
            return (float)ApplyBridgeChasmDip(x, h, 3.5);
        }

        case STAGE_MOUNTAIN: {
            float tm = t;
            float v3 = 2.0f * cosf(tm / 6.41f);
            float v7 = sinf(sinf(tm * 0.415f) + tm) * 0.2f * clampf(tm * 0.0015f, 0.0f, 1.0f);
            float v10 = cosf(tm * 0.655f + sinf(tm * 0.215f));
            float v11 = v10 * 0.25f * clampf(tm * 0.0015f, 0.0f, 1.0f);
            float v14 = tm * 0.16f + sinf(tm * 0.3515f) + 2.411f;
            float v16 = 2.0f * cosf(v14) * clampf(tm * 0.00075f, 0.0f, 1.0f);
            float v20 = tm * 0.1952f + sinf(tm * 1.215f) + 5.6f;
            float v22 = 2.0f * cosf(v20) * clampf(tm * 0.00275f, 0.0f, 1.0f);
            float sum = v3 + v7 + v11 + v16 + v22 + tm / 3.0f;
            double h = (double)(sum * (0.2f + clampf(tm / 4000.0f, 0.0f, 1.0f)));
            return (float)ApplyBridgeChasmDip(x, h, 3.8);
        }

        case STAGE_MUDPOOL: {
            float tm = t;
            float v2 = 2.0f * cosf(tm / 10.0f);
            float v3 = tm * 0.25f;
            float v6 = cosf(tm + sinf(v3)) * 0.2f * clampf(tm * 0.0013f, 0.0f, 1.0f);
            float v8 = cosf(tm * 0.17f + sinf(v3));
            float v10 = 2.0f * v8 * clampf(tm * 0.00035f, 0.0f, 1.0f);
            float v13 = cosf(tm * 0.0952f + sinf(tm * 0.15f));
            float v15 = 2.0f * v13 * clampf(tm * 0.00125f, 0.0f, 1.0f);
            float v19 = sinf(tm / 30.0f + cosf(tm / 32.14f));
            float v21 = v19 * 4.0f * clampf(tm * 0.0007f, 0.0f, 1.0f);
            double h = (double)((clampf(tm / 2000.0f, 0.0f, 1.5f) + 0.5f) * (v2 + v6 + v10 + v15 + v21 - 1.0f));
            return (float)ApplyMudBasinDip(x, h, 18.0f, 32.0f, 1.8);
        }

        case STAGE_VOLCANO: {
            float tv = t;
            float v3 = 2.0f * cosf(tv / 6.41f);
            float v7 = sinf(sinf(tv * 0.415f) + tv) * 0.2f * clampf(tv * 0.0015f, 0.0f, 1.0f);
            float v10 = cosf(tv * 0.855f + sinf(tv * 0.215f));
            float v11 = v10 * 0.25f * clampf(tv * 0.0015f, 0.0f, 1.0f);
            float v14 = tv * 0.30f + sinf(tv * 0.3515f) + 2.411f;
            float v16 = 2.0f * cosf(v14) * clampf(tv * 0.00075f, 0.0f, 1.0f);
            float v20 = tv * 0.2052f + sinf(tv * 1.215f) + 5.6f;
            float v22 = 2.0f * cosf(v20) * clampf(tv * 0.00275f, 0.0f, 1.0f);
            float sum = v3 + v7 + v11 + v16 + v22 + tv / 6.0f;
            double h = (double)(sum * (0.2f + clampf(tv / 4000.0f, 0.0f, 1.0f)));
            return (float)ApplyMudBasinDip(x, h, 20.0f, 34.0f, 1.5);
        }

        case STAGE_BEACH: {
            float tb = x * 1.3f + 10.0f;
            float v3 = cosf(tb * 0.25f + sinf(tb / 7.56f));
            float v5 = sinf(tb * 0.095f) + tb;
            float v7 = cosf(v5) * 0.7f * 0.3f * clampf(tb * 0.0015f, 0.0f, 1.0f);
            float v8 = v3 * 0.35f + v7;
            float v11 = cosf(tb * 0.11f + sinf(tb * 0.0315f));
            float v13 = 0.3f * v11 * 2.5f * clampf(tb * 0.00075f, 0.0f, 1.0f);
            float v16 = sinf(tb * 0.102f + cosf(tb * 0.0101f));
            float v18 = v16 * 3.5f * clampf(tb * 0.00275f, 0.0f, 1.0f);
            float v21 = sinf(tb / 50.0f + cosf(tb / 84.14f));
            float v23 = v21 * 2.5f * clampf(tb * 0.00175f, 0.0f, 1.0f);
            float v24 = v8 + v13 + v18 + (v23 - 1.0f);
            float v27 = 10.0f * sinf(tb / 100.0f) * 2.5f * clampf(tb * 0.001f, 0.0f, 1.0f);
            float v28 = v24 + v27;
            float v30 = v28 + (tb / 15.0f) * clampf(cosf(tb / 40.0f + sinf(tb / 70.0f)) * 0.5f + 0.5f, 0.0f, 1.0f);
            return (clampf(tb / 2000.0f, 0.0f, 1.0f) + 0.5f) * v30;
        }

        case STAGE_ROLLERCOASTER: {
            float v1 = clampf(sinf(x * 0.08f), 0.0f, 1.0f);
            float v2 = clampf(x * 0.04f, 0.0f, 1.0f) * (v1 * v1);
            float s3 = sinf(x * 0.03f);
            float v4 = (s3 * s3) * clampf(x * 0.08f, 0.0f, 1.0f);
            float over1k = fmaxf(0.0f, x - 1000.0f);
            float v7 = (over1k * 0.1f) * sinf(over1k * 0.01f) + (v2 + v4);
            return v7 + (x / 10.0f);
        }

        case STAGE_NIGHT: {
            float sx = x + 3.0f;
            float v2 = cosf(((sx * 6.7f) / 60.0f + 3.14159f) * 4.0f);
            float v5 = cosf((sx * 6.7f) / 53.0f + 3.14159f) * 2.6f;
            float v8 = cosf(((sx * 6.7f) / 37.0f + 3.14159f) * 0.5f) * 2.2f * clampf((2.5f * sx) / 300.0f, 0.0f, 1.0f);
            float v12 = cosf(((sx * 6.7f) / 98.0f + 3.14159f) / 3.0f) * 2.8f * clampf((2.5f * sx) / 600.0f, 0.0f, 1.0f);
            float v13 = v2 + v5 + v8 + v12;
            float v17 = 2.6f * cosf(((sx * 6.7f) / 87.0f + 3.14159f) / 7.0f) * clampf((2.5f * sx) / 1200.0f, 0.0f, 1.0f);
            float v21 = cosf(((sx * 6.7f) / 56.0f + 3.14159f) * 0.25f) * 2.35f * clampf((2.5f * sx) / 1400.0f, 0.0f, 1.0f);
            float v25 = sinf(((sx * 6.7f) / 27.0f + 3.14159f) * 0.25f) * 3.25f * clampf((2.5f * sx) / 2000.0f, 0.0f, 1.0f);
            float v26 = v13 + v17 + v21 + v25;
            float v30 = sinf(((sx * 6.7f) / 110.0f + 3.14159f) * 0.25f) * 6.25f * clampf((sx - 2000.0f) / 2000.0f, 0.0f, 1.0f);
            float v34 = sinf(((sx * 6.7f) / 73.0f + 3.14159f) * 0.25f) * 4.25f * clampf((sx - 1500.0f) / 1500.0f, 0.0f, 1.0f);
            float v38 = 5.25f * sinf(((sx * 6.7f) / 62.0f + 3.14159f) * 0.25f) * clampf((sx - 4000.0f) / 1000.0f, 0.0f, 1.0f);
            float v39 = v26 + v30 + v34 + v38;
            float v41 = sinf((2.5f * sx) / 225.0f) * 0.5f + 0.5f;
            float v42 = clampf(powf(v41, 1.0f), 0.0f, 1.0f) * (1.5f * v39 - v39);
            float v44 = (v42 + v39) * (clampf((2.5f * sx) / 1350.0f, 0.0f, 1.0f) + 1.35f);
            double v47 = (double)(clampf((2.5f * sx) / 500.0f, 0.0f, 1.0f) * v44) * 0.4;
            return (float)(v47 + (double)((6.7f * sx) / 90.0f));
        }

        case STAGE_ROOFTOPS: {
            if (x < 39.0f) return 0.0f;
            float modBridge = fmodf(x, 120.0f);
            if (modBridge < 0.0f) modBridge += 120.0f;
            if (modBridge >= 39.0f && modBridge <= 51.0f) {
                double u = (double)(modBridge - 39.0f) / 12.0;
                return (float)(-sin(u * 3.14159265358979323846) * 3.5);
            }
            if (x < 60.0f) return 0.0f;
            float cyc = floorf(x / 30.0f);
            float local = x - cyc * 30.0f;
            float roofH = sinf(cyc * 1.73f) * 2.2f + cosf(cyc * 0.61f) * 0.8f;
            float nextRoofH = sinf((cyc + 1.0f) * 1.73f) * 2.2f + cosf((cyc + 1.0f) * 0.61f) * 0.8f;
            if (local <= 22.5f) return roofH;
            if (local <= 23.0f) return roofH + (-18.0f - roofH) * ((local - 22.5f) / 0.5f);
            if (local < 27.5f) return -18.0f;
            if (local < 28.0f) return -18.0f + (nextRoofH - (-18.0f)) * ((local - 27.5f) / 0.5f);
            return nextRoofH;
        }

        case STAGE_JUNKYARD: {
            float tj = 1.3f * x + 10.0f;
            float v2 = 2.0f * cosf(tj / 10.0f);
            float v6 = cosf(sinf(tj * 0.25f) + tj) * 0.2f * clampf(tj * 0.0013f, 0.0f, 1.0f);
            float v9 = cosf(0.2f * tj + sinf(tj * 0.15f));
            float v11 = 2.0f * v9 * clampf(tj * 0.00075f, 0.0f, 1.0f);
            float v14 = cosf(tj * 0.12f + sinf(tj * 0.19f));
            float v16 = 2.0f * v14 * clampf(tj * 0.00275f, 0.0f, 1.0f);
            float v20 = sinf(tj / 25.0f + cosf(tj / 28.14f));
            float v21 = v20 * 4.0f * clampf(tj * 0.00075f, 0.0f, 1.0f);
            double h = (double)((clampf(tj / 2000.0f, 0.0f, 1.0f) + 0.5f) * (v2 + v6 + v11 + v16 + v21 - 1.0f));
            return (float)ApplyMudBasinDip(x, h, 18.0f, 32.0f, 1.6);
        }

        case STAGE_CONSTRUCTION: {
            double td = (double)x * 1.5 + 10.0;
            double w1 = 2.8 * sin(td * 0.078);
            double w2 = 1.5 * cos(td * 0.155);
            double w3 = 1.1 * sin(td * 0.295 + cos(td * 0.055));
            double h = (w1 + w2 + w3) * 1.25;
            return (float)ApplyBridgeChasmDip(x, h, 3.6);
        }

        case STAGE_RAINBOW: {
            float v1 = 2.0f * sinf(x * 0.15f);
            float v3 = clampf(x * 0.001f, 0.0f, 1.0f) * v1 + v1 + x * 0.05f;
            return clampf(x * 0.001f + 0.3f, 0.0f, 1.0f) * v3;
        }

        case STAGE_SEASONS: {
            float ts = x + 10.0f;
            float v1 = 2.0f * cosf(ts / 10.0f);
            float v3 = cosf(sinf(ts * 0.28f) + ts);
            float v8 = (v3 * 0.1f * clampf(ts * 0.0015f, 0.0f, 1.0f)) + v1;
            double v9 = (double)v8 + cos(0.3 * (double)ts + sin(0.1 * (double)ts));
            float v13 = cosf(ts * 0.2f + sinf(ts * 0.15f)) * 1.4f * clampf(ts * 0.0007f, 0.0f, 1.0f);
            float v14 = v13 + (float)v9;
            float v18 = cosf(ts * 0.12f + sinf(ts * 0.19f)) * 1.5f * clampf(ts * 0.0025f, 0.0f, 1.0f);
            float v20 = ts / 28.0f + cosf(ts / 29.14f);
            float v22 = 6.0f * sinf(v20) * clampf(ts * 0.00065f, 0.0f, 1.0f);
            float v23 = v22 - 1.0f + v18 + v14;
            return (clampf(ts / 3000.0f, 0.0f, 1.5f) + 0.5f) * v23 - 1.0f;
        }

        case STAGE_BOOTCAMP: {
            float tb = x + 10.0f;
            float v1 = 2.0f * cosf(tb / 10.0f);
            float v3 = cosf(sinf(tb * 0.28f) + tb);
            float v8 = (v3 * 0.1f * clampf(tb * 0.0015f, 0.0f, 1.0f)) + v1;
            double v9 = (double)v8 + cos(0.3 * (double)tb + sin(0.1 * (double)tb));
            float v13 = cosf(tb * 0.2f + sinf(tb * 0.15f)) * 1.4f * clampf(tb * 0.0007f, 0.0f, 1.0f);
            float v14 = v13 + (float)v9;
            float v18 = cosf(tb * 0.12f + sinf(tb * 0.19f)) * 1.5f * clampf(tb * 0.0025f, 0.0f, 1.0f);
            float v20 = tb / 28.0f + cosf(tb / 29.14f);
            float v22 = 6.0f * sinf(v20) * clampf(tb * 0.00065f, 0.0f, 1.0f);
            float v23 = v22 - 1.0f + v18 + v14;
            return (clampf(tb / 3000.0f, 0.0f, 1.5f) + 0.5f) * v23 - 1.0f;
        }

        case STAGE_XMAS: {
            float v1 = cosf(x / 37.0f + 1.13f);
            float v2 = clampf((x - 1000.0f) * 0.0073f, 0.0f, 5.0f);
            float v3 = x / 43.0f + sinf(x * 0.023f);
            float v4 = clampf(0.0081f * (x - 1500.0f), 0.0f, 5.0f);
            double v6 = (double)(x / 29.0f + 0.53f) + sin(0.21 * (double)x);
            double v9 = (double)(cosf(v3) * v4 + v1 * v2) + (double)clampf(x * 0.0051f, 0.0f, 3.0f) * cos(v6);
            float v12 = clampf(x * 0.0063f, 0.0f, 3.0f) * cosf(x / 31.0f + 0.91f) + (float)v9;
            float v13 = clampf((x - 1500.0f) / 1000.0f, 0.0f, 3.0f);
            float v16 = cosf(x / 13.0f + sinf(x / 5.0f)) * 1.5f + cosf(x / 11.0f) * 1.2f + v13 * v12;
            float v20 = clampf((float)(0.01 * (double)x), 0.0f, 3.0f) * cosf(x / 17.0f + sinf(x / 3.0f)) + v16;
            float v23 = clampf((float)((double)x * 0.005), 0.0f, 3.0f) * cosf(x / 23.0f);
            float v24 = clampf(x / 2000.0f, 0.0f, 3.0f) + 0.4f;
            return cosf(sinf(x * 0.25f) + x) * 0.2f + v24 * (v23 + v20);
        }

        case STAGE_ALIENPLANET: {
            float sx = 3.0f + x;
            float v2 = cosf(((sx * 22.7f) / 80.0f + 3.14159f) * 0.45f);
            float v4 = cosf(((sx * 22.7f) / 73.0f + 3.14159f) * 0.81f);
            float v6 = (sx * 22.7f) / 57.0f;
            float v8 = (2.5f * sx) / 300.0f;
            float v9 = cosf((v6 + 3.14159f) / 2.126f) * 1.6f * clampf(v8, 0.0f, 1.0f);
            float v12 = (2.5f * sx) / 600.0f;
            float v13 = cosf(((sx * 22.7f) / 78.0f + 3.14159f) / 2.895f) * 1.6f * clampf(v12, 0.0f, 1.0f);
            float v15 = (2.5f * sx) / 1200.0f;
            float v16 = cosf((v6 + 3.14159f) / 4.721f) * 2.6f * clampf(v15, 0.0f, 1.0f);
            float v20 = (2.5f * sx) / 1400.0f;
            float v21 = cosf(((sx * 22.7f) / 86.0f + 3.14159f) / 4.791f) * 1.35f * clampf(v20, 0.0f, 1.0f);
            float v24 = (2.5f * sx) / 1500.0f;
            float v25 = 1.25f * sinf(((sx * 22.7f) / 67.0f + 3.14159f) / 3.971f) * clampf(v24, 0.0f, 1.0f);
            float a = v2 * 0.5f + v4 * 1.6f + v9 + v13 + v16 + v21 + v25;

            float v27 = cosf(((sx * 22.7f) / 95.1f + 3.14159f) * 0.35f);
            float v29 = cosf(((sx * 22.7f) / 33.0f + 3.14159f) * 0.51f);
            float v33 = cosf(((sx * 22.7f) / 87.0f + 3.14159f) / 3.126f) * 1.6f * clampf(v8, 0.0f, 1.0f);
            float v36 = cosf(((sx * 22.7f) / 84.2f + 3.14159f) / 1.895f) * 1.6f * clampf(v12, 0.0f, 1.0f);
            float v39 = cosf(((sx * 22.7f) / 77.7f + 3.14159f) / 5.721f) * 2.6f * clampf(v15, 0.0f, 1.0f);
            float v43 = cosf(((sx * 22.7f) / 84.35f + 3.14159f) / 4.791f) * 1.35f * clampf(v20, 0.0f, 1.0f);
            float v47 = 1.25f * sinf(((sx * 22.7f) / 96.7f + 3.14159f) / 3.971f) * clampf(v24, 0.0f, 1.0f);
            float b = v27 * 0.5f + v29 * 1.6f + v33 + v36 + v39 + v43 + v47;

            float v48 = sinf((2.5f * sx) / 100.0f) * 0.5f + 0.5f;
            float v50 = v48 * (b - a) + a;
            return (clampf((2.5f * sx) / 2050.0f, 0.0f, 1.0f) + 0.95f) * v50;
        }

        case STAGE_ARCTICCAVE: {
            float v1 = cosf(x * 0.016f) * 10.0f;
            double v3 = (double)x * 0.2 + (double)v1;
            float v4 = (float)fabs((double)clampf(x / 1000.0f, 0.5f, 2.0f) * sin(v3));
            float tc = 10.0f + x * 1.5f;
            if (tc > 1569.0f) tc = 1569.0f + (tc - 1569.0f) * 0.875f;
            float v8 = cosf(tc / 7.21f + sinf(tc / 17.0f));
            float v10 = v8 * (sinf(tc / 185.0f) + 1.5f);
            float v14 = cosf(sinf(tc * 0.1565f) + tc) * 0.25f * sinf(tc / 200.0f);
            float v15 = clampf(x / 100.0f, 0.0f, 1.0f) * v14;
            float v19 = 2.0f * cosf(tc / 8.651f + sinf(tc / 28.521f)) * clampf(x / 900.0f, 0.0f, 1.0f);
            float v23 = 2.0f * cosf(tc / 13.518f + sinf(tc / 14.412f)) * clampf(x / 1900.0f, 0.0f, 1.0f);
            float v28 = sinf(tc / 37.0f + cosf(tc / 62.14f)) * 5.0f * clampf(x / 2500.0f, 0.0f, 1.0f);
            float v29 = v10 + v15 + v19 + v23 + v28 - 1.0f + v4;
            float v31 = clampf(powf(sinf(x / 95.0f) * 0.5f + 0.5f, 1.0f), 0.0f, 1.0f);
            float v32 = v31 * (0.05f * v29 - v29);
            return (clampf(tc / 1200.0f, 0.0f, 1.0f) + 0.75f) * (v32 + v29);
        }

        case STAGE_RAGNAROK: {
            float tr = x + 10.0f;
            float v1 = 2.0f * cosf(tr / 10.0f);
            float v4 = clampf(tr * 0.0025f, 0.0f, 1.0f);
            float v8 = cosf(sinf(tr * 0.23f) + tr) * 0.08f * v4 + v1;
            double v9 = (double)v8 + cos(0.25 * (double)tr + sin(0.1 * (double)tr));
            float v10 = sinf(tr * 0.19f);
            float v13 = cosf(tr * 0.22f + v10) * 1.3f * clampf(tr * 0.0017f, 0.0f, 1.0f);
            float v14 = v13 + (float)v9;
            float v20 = cosf(tr * 0.12f + v10) * 1.5f * v4;
            float v17 = tr / 23.0f + cosf(tr / 22.14f);
            double v21 = ((double)sinf(v17) * 6.4) * (double)clampf(tr * 0.00045f, 0.0f, 1.0f);
            double v22 = (double)(v20 + v14) + (v21 - 1.0);
            double h = (double)((clampf(tr / 2000.0f, 0.0f, 1.5f) + 0.35f) * (float)v22 - 1.0f);
            return (float)ApplyMudBasinDip(x, h, 18.0f, 32.0f, 1.8);
        }

        case STAGE_FACTORY: {
            float tf = x + 10.0f;
            float v1 = 2.0f * cosf(tf / 9.0f);
            float v3 = cosf(sinf(tf * 0.15f) + tf);
            float v5 = v1 + v3 * 0.21f * clampf(tf * 0.0013f, 0.0f, 1.0f);
            float v7 = tf * 0.14f + sinf(0.21f * tf);
            float v9 = cosf(v7) * 1.3f * clampf(tf * 0.00255f, 0.0f, 1.0f);
            float v11 = tf / 22.0f + cosf(tf / 25.14f);
            float v13 = 2.0f * sinf(v11) * clampf(tf * 0.00065f, 0.0f, 1.0f);
            float v14 = v5 + v9 + v13 - 1.0f;
            double h = (double)((clampf(tf / 2000.0f, 0.0f, 1.0f) + 0.64f) * v14 - 1.0f);
            return (float)ApplyBridgeChasmDip(x, h, 3.8);
        }

        case STAGE_HAUNTED: {
            float th = 1.3f * x + 10.0f;
            float v2 = 2.0f * cosf(th / 10.0f);
            float v7 = cosf(sinf(th * 0.24f) + th) * 0.15f * clampf(th * 0.0012f, 0.0f, 1.0f);
            float v11 = 2.0f * cosf(th * 0.21f + sinf(0.15f * th)) * clampf(th * 0.0008f, 0.0f, 1.0f);
            float v16 = 2.0f * cosf(th * 0.11f + sinf(th * 0.19f)) * clampf(th * 0.00245f, 0.0f, 1.0f);
            float v22 = sinf(th / 26.0f + cosf(th / 28.14f)) * 4.0f * clampf(th * 0.00059f, 0.0f, 1.0f);
            float v25 = (v2 + v7 + v11 + v16 + v22 - 1.0f) * (clampf(th / 2000.0f, 0.0f, 1.5f) + 0.4f);
            float v24 = th * 0.25f;
            float v28 = (sinf(v24) * cosf(th / 9.0f) + cosf(v24)) * 1.5f;
            float v29 = fabsf(sinf(x / 400.0f));
            float v32 = sinf(th / 200.0f) * 60.0f * clampf(th / 1500.0f, 0.0f, 1.5f);
            double h = (double)(v32 + v25 + v28 * v29);
            return (float)ApplyMudBasinDip(x, h, 18.0f, 32.0f, 1.8);
        }

        case STAGE_NORTHPOLE: {
            float tn = 1.3f * x + 10.0f;
            float v1 = fabsf(sinf(x / 800.0f));
            float v3 = 2.0f * cosf(tn / 10.0f);
            float v8 = 2.0f * cosf(tn * 0.01f + sinf(tn * 0.15f)) * clampf(tn * 0.0008f, 0.0f, 1.0f);
            float v13 = 2.0f * cosf(tn * 0.11f + sinf(tn * 0.09f)) * clampf(tn * 0.00045f, 0.0f, 1.0f);
            float v18 = clampf(500.0f / tn, 0.0f, 1.0f) * (2.0f * cosf(tn * 0.1f) * clampf(tn * 0.00145f, 0.0f, 1.0f));
            float v24 = sinf(tn / 26.0f + cosf(tn / 28.14f)) * 3.0f * clampf(tn * 0.00059f, 0.0f, 1.0f);
            float v26 = (v3 + v8 + v13 + v18 + v24 - 1.0f) * (clampf(tn / 2000.0f, 0.0f, 1.0f) + 0.4f);
            float tnOver = tn - 2000.0f;
            float v31 = (tnOver > 0.0f) ? (1.2f * cosf(tn * 0.2f + sinf(tn * 0.24f)) * clampf(tnOver * 0.045f, 0.0f, 1.0f)) : 0.0f;
            float v33 = cosf(tn * 0.25f) * 0.5f * v1;
            float v36 = sinf(tn / 330.0f) * 65.0f * clampf(tn / 1700.0f, 0.0f, 1.0f);
            return v36 + v33 + v26 + v31;
        }

        case STAGE_NUCLEAR: {
            float tn = x + 10.0f;
            float v1 = 2.0f * cosf(tn / 10.0f);
            float v5 = v1 + cosf(sinf(tn * 0.25f) + tn) * 0.2f * clampf(tn * 0.0015f, 0.0f, 1.0f);
            float v8 = clampf(tn * 0.00075f, 0.0f, 1.0f);
            float v9 = cosf(0.2f * tn + sinf(tn * 0.15f)) * 1.3f * v8;
            float v13 = 1.3f * cosf(tn * 0.12f + sinf(tn * 0.19f)) * clampf(tn * 0.00275f, 0.0f, 1.0f);
            float v16 = 2.0f * sinf(tn / 25.0f + cosf(tn / 28.14f));
            float v17 = v5 + v9 + v13 + v16 * v8 - 1.0f;
            double h = (double)((clampf(tn / 2000.0f, 0.0f, 1.0f) + 0.5f) * v17 - 1.0f);
            return (float)ApplyMudBasinDip(x, h, 18.0f, 32.0f, 1.8);
        }

        case STAGE_COUNTRYSIDE:
        default: {
            double w1 = 2.0 * cos((double)(t * 0.10f));
            float k1 = clampf(t * 0.0015f, 0.0f, 1.0f);
            double mod1 = cos((double)t + sin((double)(t * 0.25f)));
            double s = w1 + 0.20 * (double)k1 * mod1;

            double w2 = cos(sin((double)(t * 0.25f)) + (double)(t * 0.17f));
            float k2 = clampf(t * 0.00075f, 0.0f, 1.0f);
            s += 2.0 * (double)k2 * w2;

            double w3 = cos(sin((double)(t * 0.15f)) + (double)(t * 0.0952f));
            float k3 = clampf(t * 0.00275f, 0.0f, 1.0f);
            s += 2.0 * (double)k3 * w3;

            double w4 = sin(cos((double)(t * 0.03111388f)) + (double)(t * 0.03333334f));
            s += -1.0 + 4.0 * (double)k2 * w4;

            float amp = 0.5f + clampf(t * 0.0005f, 0.0f, 1.5f);
            return (float)s * amp;
        }
    }
}

float Terrain::GetCeilingHeightAtX(float x) const {
    float floorY = GetHeightAtX(x);
    float offset = (mStageConfig && mStageConfig->ceiling_offset > 0.0f) ? mStageConfig->ceiling_offset : 6.5f;
    // Cave ceiling undulations (extracted from libgame.so @ 0x14269e)
    float undulation = cosf(x / 12.76214f) * 0.75f + sinf(x * 0.05f) * 0.35f;
    float ceilY = floorY + offset + undulation;
    // Guaranteed minimum clearance >= 4.5m
    if (ceilY < floorY + 4.5f) {
        ceilY = floorY + 4.5f;
    }
    return ceilY;
}

void Terrain::InitBridgePool() {
    for (int i = 0; i < MAX_ACTIVE_BRIDGES; ++i) {
        mBridges[i].active = false;
        mBridges[i].chunkIndex = -1;
        mBridges[i].startX = 0.0f;
        mBridges[i].endX = 0.0f;
        mBridges[i].plankCount = 0;
        for (int p = 0; p < MAX_BRIDGE_PLANKS; ++p) {
            mBridges[i].planks[p] = NULL;
        }
        for (int j = 0; j <= MAX_BRIDGE_PLANKS; ++j) {
            mBridges[i].joints[j] = NULL;
        }
    }
}

int Terrain::GetActiveBridgeCount() const {
    int count = 0;
    for (int i = 0; i < MAX_ACTIVE_BRIDGES; ++i) {
        if (mBridges[i].active) count++;
    }
    return count;
}

void Terrain::DestroyBridge(int bridgeIdx) {
    if (bridgeIdx < 0 || bridgeIdx >= MAX_ACTIVE_BRIDGES) return;
    SuspensionBridge& b = mBridges[bridgeIdx];
    if (!b.active) return;

    for (int j = 0; j <= b.plankCount; ++j) {
        if (b.joints[j]) {
            mWorld->DestroyJoint(b.joints[j]);
            b.joints[j] = NULL;
        }
    }
    for (int p = 0; p < b.plankCount; ++p) {
        if (b.planks[p]) {
            mWorld->DestroyBody(b.planks[p]);
            b.planks[p] = NULL;
        }
    }

    b.active = false;
    b.chunkIndex = -1;
    b.plankCount = 0;
}

void Terrain::DestroyBridgeForChunk(int chunkIndex) {
    for (int i = 0; i < MAX_ACTIVE_BRIDGES; ++i) {
        if (mBridges[i].active && mBridges[i].chunkIndex == chunkIndex) {
            DestroyBridge(i);
        }
    }
}

void Terrain::DestroyAllBridges() {
    for (int i = 0; i < MAX_ACTIVE_BRIDGES; ++i) {
        DestroyBridge(i);
    }
}

bool Terrain::CreateBridge(int chunkIndex, float startX, float endX, float startY, float endY, b2Body* anchorBody) {
    if (!anchorBody) return false;

    int slot = -1;
    for (int i = 0; i < MAX_ACTIVE_BRIDGES; ++i) {
        if (!mBridges[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return false;

    SuspensionBridge& b = mBridges[slot];
    b.active = true;
    b.chunkIndex = chunkIndex;
    b.startX = startX;
    b.endX = endX;
    b.startY = startY;
    b.endY = endY;
    b.plankCount = 10;

    float dx = endX - startX;
    float dy = endY - startY;
    float spanLen = sqrtf(dx * dx + dy * dy);
    float slopeAngle = atan2f(dy, dx);
    float plankW = spanLen / (float)b.plankCount;
    if (plankW < BRIDGE_PLANK_WIDTH) plankW = BRIDGE_PLANK_WIDTH;
    b.plankWidth = plankW;

    b2PolygonShape plankBox;
    plankBox.SetAsBox(plankW * 0.5f, BRIDGE_PLANK_HEIGHT * 0.5f);

    b2FixtureDef pfd;
    pfd.shape = &plankBox;
    pfd.density = 1.5f;
    pfd.friction = 0.70f;
    pfd.restitution = 0.0f;
    pfd.filter.categoryBits = CAT_TERRAIN;
    pfd.filter.maskBits = CAT_CHASSIS | CAT_WHEEL | CAT_DRIVER;

    for (int i = 0; i < b.plankCount; ++i) {
        float t = ((float)i + 0.5f) / (float)b.plankCount;
        float px = startX + t * dx;
        float py = startY + t * dy;

        b2BodyDef pbd;
        pbd.type = b2_dynamicBody;
        pbd.position.Set(px, py);
        pbd.angle = slopeAngle;
        pbd.linearDamping = 0.5f;
        pbd.angularDamping = 0.5f;
        b.planks[i] = mWorld->CreateBody(&pbd);
        b.planks[i]->CreateFixture(&pfd);
    }

    b2RevoluteJointDef jd;
    jd.collideConnected = false;
    jd.enableLimit = true;
    jd.lowerAngle = -0.25f;
    jd.upperAngle =  0.25f;

    // Left anchor
    jd.Initialize(anchorBody, b.planks[0], b2Vec2(startX, startY));
    b.joints[0] = (b2RevoluteJoint*)mWorld->CreateJoint(&jd);

    // Intermediate joints
    for (int i = 1; i < b.plankCount; ++i) {
        float t = (float)i / (float)b.plankCount;
        b2Vec2 anchor(startX + t * dx, startY + t * dy);
        jd.Initialize(b.planks[i - 1], b.planks[i], anchor);
        b.joints[i] = (b2RevoluteJoint*)mWorld->CreateJoint(&jd);
    }

    // Right anchor
    jd.Initialize(b.planks[b.plankCount - 1], anchorBody, b2Vec2(endX, endY));
    b.joints[b.plankCount] = (b2RevoluteJoint*)mWorld->CreateJoint(&jd);

    return true;
}

void Terrain::MaybeSpawnBridge(int slotIdx) {
    if (!mStageConfig || mStageConfig->has_bridges == 0) {
        return;
    }
    TerrainChunk& chunk = mChunks[slotIdx];
    if (chunk.chunkIndex > 0 && (chunk.chunkIndex % 4 == 1)) {
        float chunkStart = (float)chunk.chunkIndex * CHUNK_WIDTH_METERS;
        float x0 = chunkStart + 9.0f;
        float x1 = chunkStart + 21.0f;
        float y0 = GetHeightAtX(x0);
        float y1 = GetHeightAtX(x1);
        CreateBridge(chunk.chunkIndex, x0, x1, y0, y1, chunk.body);
    }
}

void Terrain::InitChunkPool() {
    // 1. Rear boundary wall: dedicated static body
    b2BodyDef wallBd;
    wallBd.type = b2_staticBody;
    wallBd.position.Set(0.0f, 0.0f);
    mRearWallBody = mWorld->CreateBody(&wallBd);

    b2EdgeShape wall;
    wall.Set(b2Vec2(-20.0f, -15.0f), b2Vec2(-20.0f, 35.0f));
    b2FixtureDef wfd;
    wfd.shape = &wall;
    wfd.friction = 0.0f;
    wfd.filter.categoryBits = CAT_TERRAIN;
    wfd.filter.maskBits = CAT_CHASSIS | CAT_WHEEL | CAT_DRIVER;
    mRearWallBody->CreateFixture(&wfd);

    // 2. Chunk pool: static bodies
    b2BodyDef chunkBd;
    chunkBd.type = b2_staticBody;
    chunkBd.position.Set(0.0f, 0.0f);

    for (int i = 0; i < NUM_CHUNK_SLOTS; ++i) {
        mChunks[i].chunkIndex = -1;
        mChunks[i].body = mWorld->CreateBody(&chunkBd);
        mChunks[i].fixture = NULL;
        mChunks[i].ceilingFixture = NULL;
        mChunks[i].active = false;
        mChunks[i].ready = false;
        mChunks[i].verticesComputed = 0;
        mChunks[i].accumulatedTicks = 0;
        for (int v = 0; v < VERTS_PER_CHUNK; ++v) {
            mChunks[i].vertices[v].SetZero();
            mChunks[i].ceilingVertices[v].SetZero();
        }
        mChunks[i].prevVertex.SetZero();
        mChunks[i].nextVertex.SetZero();
        mChunks[i].prevCeilingVertex.SetZero();
        mChunks[i].nextCeilingVertex.SetZero();
    }
}

void Terrain::GenerateChunkVertices(int chunkIndex, b2Vec2* outVerts, b2Vec2* outPrev, b2Vec2* outNext) const {
    float startX = (float)chunkIndex * CHUNK_WIDTH_METERS;
    for (int i = 0; i < VERTS_PER_CHUNK; ++i) {
        float x = startX + (float)i * CHUNK_STEP_METERS;
        outVerts[i].Set(x, GetHeightAtX(x));
    }
    if (outPrev) {
        float prevX = startX - CHUNK_STEP_METERS;
        outPrev->Set(prevX, GetHeightAtX(prevX));
    }
    if (outNext) {
        float nextX = startX + (float)(VERTS_PER_CHUNK - 1) * CHUNK_STEP_METERS + CHUNK_STEP_METERS;
        outNext->Set(nextX, GetHeightAtX(nextX));
    }
}



float Terrain::GetAverageChunkGenMs() {
#if defined(__PSP__)
    if (sCompletedChunkCount <= 0) return 0.0f;
    u32 res = sceRtcGetTickResolution();
    if (res == 0) res = 1000000;
    return (float)(((double)sTotalChunkFullGenTicks / (double)sCompletedChunkCount) * 1000.0 / (double)res);
#else
    return 0.0f;
#endif
}

float Terrain::GetMaxPerFrameGenMs() {
#if defined(__PSP__)
    u32 res = sceRtcGetTickResolution();
    if (res == 0) res = 1000000;
    return (float)(((double)sMaxPerFrameGenTicks) * 1000.0 / (double)res);
#else
    return 0.0f;
#endif
}

float Terrain::GetAveragePerFrameGenMs() {
#if defined(__PSP__)
    if (sPerFrameGenCount <= 0) return 0.0f;
    u32 res = sceRtcGetTickResolution();
    if (res == 0) res = 1000000;
    return (float)(((double)sTotalPerFrameGenTicks / (double)sPerFrameGenCount) * 1000.0 / (double)res);
#else
    return 0.0f;
#endif
}

int Terrain::GetChunkGenCount() {
#if defined(__PSP__)
    return sCompletedChunkCount;
#else
    return 0;
#endif
}

void Terrain::StartChunkGeneration(int slotIdx, int chunkIndex) {
    TerrainChunk& slot = mChunks[slotIdx];
    if (slot.active) {
        DestroyBridgeForChunk(slot.chunkIndex);
    }
    if (slot.active && slot.body) {
        if (slot.fixture) {
            slot.body->DestroyFixture(slot.fixture);
            slot.fixture = NULL;
        }
        if (slot.ceilingFixture) {
            slot.body->DestroyFixture(slot.ceilingFixture);
            slot.ceilingFixture = NULL;
        }
    }
    slot.chunkIndex = chunkIndex;
    slot.active = true;
    slot.ready = false;
    slot.verticesComputed = 0;
    slot.accumulatedTicks = 0;
}

bool Terrain::AdvanceGeneration(int slotIdx, int budget) {
    TerrainChunk& slot = mChunks[slotIdx];
    if (!slot.active || slot.ready) return true;

#if defined(__PSP__)
    u64 t0 = 0, t1 = 0;
    sceRtcGetCurrentTick(&t0);
#endif

    float startX = (float)slot.chunkIndex * CHUNK_WIDTH_METERS;

    // If starting vertex computation, also compute prevVertex (and ceiling)
    if (slot.verticesComputed == 0) {
        float prevX = startX - CHUNK_STEP_METERS;
        slot.prevVertex.Set(prevX, GetHeightAtX(prevX));
        if (HasCeiling()) {
            slot.prevCeilingVertex.Set(prevX, GetCeilingHeightAtX(prevX));
        }
    }

    int endIdx = slot.verticesComputed + budget;
    if (endIdx > VERTS_PER_CHUNK) {
        endIdx = VERTS_PER_CHUNK;
    }

    for (int i = slot.verticesComputed; i < endIdx; ++i) {
        float x = startX + (float)i * CHUNK_STEP_METERS;
        slot.vertices[i].Set(x, GetHeightAtX(x));
        if (HasCeiling()) {
            slot.ceilingVertices[i].Set(x, GetCeilingHeightAtX(x));
        }
    }
    slot.verticesComputed = endIdx;

    if (slot.verticesComputed >= VERTS_PER_CHUNK) {
        // Compute nextVertex
        float nextX = startX + (float)(VERTS_PER_CHUNK - 1) * CHUNK_STEP_METERS + CHUNK_STEP_METERS;
        slot.nextVertex.Set(nextX, GetHeightAtX(nextX));

        // Atomic b2ChainShape creation + fixture
        b2ChainShape chain;
        chain.CreateChain(slot.vertices, VERTS_PER_CHUNK);
        chain.SetPrevVertex(slot.prevVertex);
        chain.SetNextVertex(slot.nextVertex);

        b2FixtureDef fd;
        fd.shape = &chain;
        fd.friction = GetStageFriction();
        fd.restitution = 0.0f;
        fd.filter.categoryBits = CAT_TERRAIN;
        fd.filter.maskBits = CAT_CHASSIS | CAT_WHEEL | CAT_DRIVER;
        slot.fixture = slot.body->CreateFixture(&fd);

        if (HasCeiling()) {
            slot.nextCeilingVertex.Set(nextX, GetCeilingHeightAtX(nextX));
            b2ChainShape ceilChain;
            ceilChain.CreateChain(slot.ceilingVertices, VERTS_PER_CHUNK);
            ceilChain.SetPrevVertex(slot.prevCeilingVertex);
            ceilChain.SetNextVertex(slot.nextCeilingVertex);

            b2FixtureDef cfd;
            cfd.shape = &ceilChain;
            cfd.friction = GetStageFriction();
            cfd.restitution = 0.0f;
            cfd.filter.categoryBits = CAT_TERRAIN;
            cfd.filter.maskBits = CAT_CHASSIS | CAT_WHEEL | CAT_DRIVER;
            slot.ceilingFixture = slot.body->CreateFixture(&cfd);
        }

        slot.ready = true;

#if defined(__PSP__)
        sceRtcGetCurrentTick(&t1);
        u64 sliceTicks = (t1 >= t0) ? (t1 - t0) : 0;
        slot.accumulatedTicks += sliceTicks;
        sTotalChunkFullGenTicks += slot.accumulatedTicks;
        sCompletedChunkCount++;
        u32 res = sceRtcGetTickResolution();
        if (res == 0) res = 1000000;
        double fullMs = ((double)slot.accumulatedTicks) * 1000.0 / (double)res;
        printf("[PERF] Chunk %d ready! Total CPU time: %.3f ms (avg: %.3f ms over %d chunks)\n",
               slot.chunkIndex, fullMs,
               ((double)sTotalChunkFullGenTicks / sCompletedChunkCount) * 1000.0 / (double)res,
               sCompletedChunkCount);
#endif

        // Spawn items ahead for newly ready chunk
        float chunkStart = (float)slot.chunkIndex * CHUNK_WIDTH_METERS;
        float chunkEnd = chunkStart + CHUNK_WIDTH_METERS;
        if (chunkEnd > mLastItemSpawnX + 30.0f) {
            SpawnItemsAhead(chunkStart, chunkEnd);
        }

        // Spawn bridge if stage has bridges enabled
        if (mStageConfig && mStageConfig->has_bridges == 1) {
            MaybeSpawnBridge(slotIdx);
        }

        return true;
    }

#if defined(__PSP__)
    sceRtcGetCurrentTick(&t1);
    u64 sliceTicks = (t1 >= t0) ? (t1 - t0) : 0;
    slot.accumulatedTicks += sliceTicks;
#endif

    return false;
}

void Terrain::CompleteChunkImmediately(int slotIdx) {
    TerrainChunk& slot = mChunks[slotIdx];
    if (!slot.active || slot.ready) return;
    AdvanceGeneration(slotIdx, VERTS_PER_CHUNK + 1);
}

void Terrain::UnloadChunk(int slotIdx) {
    TerrainChunk& slot = mChunks[slotIdx];
    if (slot.active) {
        DestroyBridgeForChunk(slot.chunkIndex);
    }
    if (slot.active && slot.body) {
        if (slot.fixture) {
            slot.body->DestroyFixture(slot.fixture);
            slot.fixture = NULL;
        }
        if (slot.ceilingFixture) {
            slot.body->DestroyFixture(slot.ceilingFixture);
            slot.ceilingFixture = NULL;
        }
    }
    slot.chunkIndex = -1;
    slot.active = false;
    slot.ready = false;
    slot.verticesComputed = 0;
    slot.accumulatedTicks = 0;
}

void Terrain::UpdateChunkWindow(int currentChunk) {
    int minChunk = currentChunk - 1;
    if (minChunk < -1) minChunk = -1; // -1 covers [-30m, 0m] behind spawn
    int maxChunk = currentChunk + 3; // Proactive ahead chunks

    // 1. Unload chunks out of window
    for (int i = 0; i < NUM_CHUNK_SLOTS; ++i) {
        if (mChunks[i].active) {
            if (mChunks[i].chunkIndex < minChunk || mChunks[i].chunkIndex > maxChunk) {
                UnloadChunk(i);
            }
        }
    }

    // 2. Start missing in-window chunks
    for (int c = minChunk; c <= maxChunk; ++c) {
        bool alreadyExists = false;
        for (int i = 0; i < NUM_CHUNK_SLOTS; ++i) {
            if (mChunks[i].active && mChunks[i].chunkIndex == c) {
                alreadyExists = true;
                break;
            }
        }
        if (!alreadyExists) {
            int freeSlot = -1;
            for (int i = 0; i < NUM_CHUNK_SLOTS; ++i) {
                if (!mChunks[i].active) {
                    freeSlot = i;
                    break;
                }
            }
            if (freeSlot >= 0) {
                StartChunkGeneration(freeSlot, c);
            }
        }
    }

    // 3. Safeguard: ensure current and next chunk are ready
    for (int i = 0; i < NUM_CHUNK_SLOTS; ++i) {
        if (mChunks[i].active && !mChunks[i].ready) {
            if (mChunks[i].chunkIndex <= currentChunk + 1) {
                CompleteChunkImmediately(i);
            }
        }
    }
}

int Terrain::GetActiveChunkCount() const {
    int count = 0;
    for (int i = 0; i < NUM_CHUNK_SLOTS; ++i) {
        if (mChunks[i].active) count++;
    }
    return count;
}

void Terrain::SpawnItemsAhead(float startX, float endX) {
    float fuelInterval = (mStageConfig && mStageConfig->fuel_interval > 0.0f) ? mStageConfig->fuel_interval : 90.0f;
    if (fuelInterval <= 0.0f) fuelInterval = 90.0f;

    while (mLastItemSpawnX + 30.0f <= endX) {
        float spawnX = mLastItemSpawnX + 30.0f;
        mLastItemSpawnX = spawnX;
        if (mItemCount >= MAX_COLLECTIBLES) continue;

        bool spawnFuel = (spawnX >= mNextFuelX);
        if (spawnFuel) {
            while (mNextFuelX <= spawnX) {
                mNextFuelX += fuelInterval;
            }
        }

        if (spawnFuel) {
            float y = GetHeightAtX(spawnX);
            if (mStageType == STAGE_ROOFTOPS && y < -8.0f) {
                y = GetHeightAtX(floorf(spawnX / 30.0f) * 30.0f + 12.0f);
            }
            if (mStageConfig && mStageConfig->has_bridges == 1) {
                int cIdx = (int)floorf(spawnX / CHUNK_WIDTH_METERS);
                if (cIdx > 0 && (cIdx % 4 == 1)) {
                    float x0 = (float)cIdx * CHUNK_WIDTH_METERS + 9.0f;
                    float x1 = (float)cIdx * CHUNK_WIDTH_METERS + 21.0f;
                    if (spawnX >= x0 && spawnX <= x1) {
                        float y0 = GetHeightAtX(x0);
                        float y1 = GetHeightAtX(x1);
                        float bridgeY = y0 + ((spawnX - x0) / (x1 - x0)) * (y1 - y0);
                        if (bridgeY > y) y = bridgeY;
                    }
                }
            }
            float fuelY = y + 1.2f;
            if (HasCeiling()) {
                float ceilMax = GetCeilingHeightAtX(spawnX) - 1.1f;
                if (fuelY > ceilMax) fuelY = ceilMax;
            }
            SpawnItem(spawnX, fuelY, 1000); // 1000 = Fuel
        } else {
            int coinVal = (spawnX > 800.0f) ? 500 : (spawnX > 350.0f ? 100 : (spawnX > 120.0f ? 25 : 5));
            for (int k = 0; k < 3 && mItemCount < MAX_COLLECTIBLES; ++k) {
                float cx = spawnX + (float)k * 1.5f;
                float baseH = GetHeightAtX(cx);
                if (mStageType == STAGE_ROOFTOPS && baseH < -8.0f) {
                    // Place coin arc cleanly across the building jump instead of inside deep chasm
                    baseH = GetHeightAtX(floorf(cx / 30.0f) * 30.0f + 12.0f) + 0.5f;
                }
                if (mStageConfig && mStageConfig->has_bridges == 1) {
                    int cIdx = (int)floorf(cx / CHUNK_WIDTH_METERS);
                    if (cIdx > 0 && (cIdx % 4 == 1)) {
                        float x0 = (float)cIdx * CHUNK_WIDTH_METERS + 9.0f;
                        float x1 = (float)cIdx * CHUNK_WIDTH_METERS + 21.0f;
                        if (cx >= x0 && cx <= x1) {
                            float y0 = GetHeightAtX(x0);
                            float y1 = GetHeightAtX(x1);
                            float bridgeY = y0 + ((cx - x0) / (x1 - x0)) * (y1 - y0);
                            if (bridgeY > baseH) baseH = bridgeY;
                        }
                    }
                }
                float cy = baseH + 1.0f + sinf((float)k * 1.0f) * 0.4f;
                if (HasCeiling()) {
                    float ceilMax = GetCeilingHeightAtX(cx) - 0.9f;
                    if (cy > ceilMax) cy = ceilMax;
                }
                SpawnItem(cx, cy, coinVal);
            }
        }
    }
}

void Terrain::Update(float playerX) {
    int currentChunk = (int)floorf(playerX / CHUNK_WIDTH_METERS);
    UpdateChunkWindow(currentChunk);

    // Amortized generation: advance highest-priority chunk (active, !ready, lowest chunkIndex)
    int candidateSlot = -1;
    int lowestChunkIndex = 999999;
    for (int i = 0; i < NUM_CHUNK_SLOTS; ++i) {
        if (mChunks[i].active && !mChunks[i].ready) {
            if (mChunks[i].chunkIndex < lowestChunkIndex) {
                lowestChunkIndex = mChunks[i].chunkIndex;
                candidateSlot = i;
            }
        }
    }

    if (candidateSlot >= 0) {
#if defined(__PSP__)
        u64 t0 = 0, t1 = 0;
        sceRtcGetCurrentTick(&t0);
#endif
        AdvanceGeneration(candidateSlot, CHUNK_GEN_BUDGET);
#if defined(__PSP__)
        sceRtcGetCurrentTick(&t1);
        u64 frameTicks = (t1 >= t0) ? (t1 - t0) : 0;
        if (frameTicks > sMaxPerFrameGenTicks) {
            sMaxPerFrameGenTicks = frameTicks;
        }
        sTotalPerFrameGenTicks += frameTicks;
        sPerFrameGenCount++;
#endif
    }
}

void Terrain::Render(float cameraX, float cameraY) {
    float minVisX = cameraX - 12.0f;
    float maxVisX = cameraX + 22.0f;

    int startChunk = (int)floorf(minVisX / CHUNK_WIDTH_METERS);
    int endChunk = (int)floorf(maxVisX / CHUNK_WIDTH_METERS);

    int renderCount = 0;
    for (int c = startChunk; c <= endChunk; ++c) {
        for (int s = 0; s < NUM_CHUNK_SLOTS; ++s) {
            if (mChunks[s].active && mChunks[s].ready && mChunks[s].chunkIndex == c) {
                for (int i = 0; i < VERTS_PER_CHUNK; ++i) {
                    if (renderCount > 0 && mRenderX[renderCount - 1] == mChunks[s].vertices[i].x) {
                        continue; // skip duplicate border vertex
                    }
                    if (renderCount < MAX_RENDER_POINTS) {
                        mRenderX[renderCount] = mChunks[s].vertices[i].x;
                        mRenderY[renderCount] = mChunks[s].vertices[i].y;
                        if (HasCeiling()) {
                            mCeilRenderX[renderCount] = mChunks[s].ceilingVertices[i].x;
                            mCeilRenderY[renderCount] = mChunks[s].ceilingVertices[i].y;
                        }
                        renderCount++;
                    }
                }
                break;
            }
        }
    }

    if (renderCount >= 2) {
        // Visual mud / lava / nuclear-slime pools drawn BEFORE terrain fill so diagonal banks mask edges cleanly (BUG-052)
        if (mStageConfig && mStageConfig->has_mud_pools != 0 && mTexLiquid.GetWidth() > 0) {
            float off0 = (mStageType == STAGE_VOLCANO) ? 20.0f : 18.0f;
            float off1 = (mStageType == STAGE_VOLCANO) ? 34.0f : 32.0f;
            int cycStart = (int)floorf(minVisX / 60.0f);
            int cycEnd = (int)floorf(maxVisX / 60.0f);
            float maxU = mTexLiquid.GetMaxU();
            float maxV = mTexLiquid.GetMaxV();
            for (int c = cycStart; c <= cycEnd; ++c) {
                float p0 = (float)c * 60.0f + off0;
                float p1 = (float)c * 60.0f + off1;
                if (p0 < 15.0f || p1 < minVisX || p0 > maxVisX) continue;

                float bankY = GetPoolSurfaceY(p0);
                float minPoolY = bankY - 2.4f;
                // Tile liquid cleanly in 3.5m columns so GU_CLAMP never stretches edges
                const float tileW = 3.5f;
                for (float tx = p0; tx < p1; tx += tileW) {
                    float nx = fminf(tx + tileW, p1);
                    float u1 = ((nx - tx) / tileW) * maxU;
                    Renderer2D::DrawQuad(&mTexLiquid, tx, bankY, nx, minPoolY, 0.0f, 0.0f, u1, maxV, 0xFFFFFFFF);
                }
            }
            Renderer2D::FlushBatch();
        }

        Texture* fillTex = (mStageType == STAGE_ROOFTOPS && mTexRooftopWall.GetWidth() > 0) ? &mTexRooftopWall : &mTexGround;
        float bottomY = cameraY - 14.0f;
        Renderer2D::DrawTerrainFill(fillTex, mRenderX, mRenderY, renderCount, bottomY);
        Renderer2D::DrawTerrainSurface(&mTexSurface, mRenderX, mRenderY, renderCount, 0.6f);

        if (HasCeiling()) {
            float topY = cameraY + 14.0f;
            Renderer2D::DrawTerrainFill(&mTexGround, mCeilRenderX, mCeilRenderY, renderCount, topY);
            Renderer2D::DrawTerrainSurface(&mTexSurface, mCeilRenderX, mCeilRenderY, renderCount, 0.6f, 0xFFFFFFFF, true);
        }
    }

    // Draw Collectibles (only if within visible range)
    for (int i = 0; i < mItemCount; ++i) {
        if (!mItems[i].collected && mItems[i].x >= minVisX && mItems[i].x <= maxVisX) {
            Texture* tex = &mTexCoin5;
            if (mItems[i].type == 1000) tex = &mTexFuel;
            else if (mItems[i].type == 500) tex = &mTexCoin500;
            else if (mItems[i].type == 100) tex = &mTexCoin100;
            else if (mItems[i].type == 25)  tex = &mTexCoin25;

            float size = (mItems[i].type == 1000) ? 1.0f : 0.75f;
            Renderer2D::DrawSprite(tex, mItems[i].x, mItems[i].y, size, size);
        }
    }

    // Draw Suspension Bridges (batched)
    for (int b = 0; b < MAX_ACTIVE_BRIDGES; ++b) {
        if (mBridges[b].active) {
            float plankW = (mBridges[b].plankWidth > 0.0f) ? mBridges[b].plankWidth : BRIDGE_PLANK_WIDTH;
            for (int p = 0; p < mBridges[b].plankCount; ++p) {
                b2Body* plank = mBridges[b].planks[p];
                if (plank) {
                    b2Vec2 pos = plank->GetPosition();
                    float angle = plank->GetAngle();
                    Renderer2D::DrawSprite(&mTexBridge, pos.x, pos.y, plankW, BRIDGE_PLANK_HEIGHT, angle);
                }
            }
        }
    }
}
