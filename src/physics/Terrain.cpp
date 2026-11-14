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

Terrain::Terrain(b2World* world, StageType stageType)
    : mWorld(world), mStageType(stageType),
      mRearWallBody(NULL), mItemCount(0), mLastItemSpawnX(15.0f),
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
            // Extracted from libgame.so @ 0x148f0a (StageHighway)
            // Fast, smooth undulating asphalt waves
            double w1 = 0.5 * sin((double)(t * 0.335f + 3.14159f) * 4.0);
            double w2 = 1.6 * sin((double)(t * 0.09178f + 3.14159f));
            double w3 = 1.6 * sin(0.5 * (double)(t * 0.11754f + 3.14159f));
            float k = clampf(t * 0.001f, 0.0f, 1.0f);
            double s = w1 * 0.4 + w2 * 0.6 + w3 * (0.5 + 0.5 * (double)k);
            return (float)s * 1.5f;
        }

        case STAGE_CAVE: {
            // Extracted from libgame.so @ 0x148776 (StageCave)
            // Underground rocky floor with bumpy profile
            double w1 = cos((double)(t / 17.0f));
            double w2 = sin((double)(t / 7.21f) + w1);
            double w3 = cos((double)(t / 185.0f));
            double w4 = (w3 + 1.5) * w2;
            double w5 = sin((double)t + cos((double)(t * 0.1265f)));
            double w6 = cos((double)(t / 200.0f)) * 0.2;
            double s = w4 * 0.5 + w5 * 0.35 + w6 * 0.8;
            return (float)s;
        }

        case STAGE_MARS: {
            // Extracted from libgame.so @ 0x147626 (StageMars)
            // Martian terrain with craters and steep slopes
            float tm = x * 1.75f + 10.0f;
            double w1 = cos((double)(tm / 67.0f));
            double w2 = sin((double)(tm / 9.21f) + w1);
            double w3 = cos((double)(tm / 185.0f));
            double w4 = (w3 + 1.95) * w2;
            double w5 = sin((double)tm + cos((double)(tm * 0.1265f)));
            double s = w4 * 0.65 + w5 * 0.45;
            return (float)s * 1.4f;
        }

        case STAGE_FOREST: {
            // Extracted from libgame.so @ 0x148b88 (StageForest)
            // Forest terrain with rolling hills and bridge chasms
            double w1 = sin((double)(t / 10.0f));
            double w2 = cos((double)(t * 0.215f));
            double w3 = cos((double)t + w2);
            double w4 = cos((double)(t * 0.515f));
            double s = w1 * 1.2 + w3 * 0.8 + w4 * 0.4;
            float h = (float)s;

            // Bridge chasms every 4 chunks (120m) starting at chunk 1: span [39.0, 51.0] (10 planks x 1.2m)
            float modX = fmodf(x, 120.0f);
            if (modX < 0.0f) modX += 120.0f;
            if (x >= 30.0f && modX >= 39.0f && modX <= 51.0f) {
                float u = (modX - 39.0f) / 12.0f;
                float dip = sinf(u * 3.14159265f) * 3.5f;
                h -= dip;
            }
            return h;
        }

        case STAGE_MOUNTAIN: {
            // Steep alpine peaks and ridges with deep valleys
            double w1 = 3.5 * sin((double)(t * 0.065f));
            double w2 = 2.0 * cos((double)(t * 0.145f) + sin((double)(t * 0.035f)));
            double w3 = 1.2 * sin((double)(t * 0.285f));
            float k = clampf(t * 0.001f, 0.0f, 1.5f);
            double s = (w1 + w2 + w3) * (0.8 + 0.4 * (double)k);
            float h = (float)s * 1.6f;

            // Bridge chasms every 4 chunks (120m) starting at chunk 1: span [39.0, 51.0] (10 planks x 1.2m)
            float modX = fmodf(x, 120.0f);
            if (modX < 0.0f) modX += 120.0f;
            if (x >= 30.0f && modX >= 39.0f && modX <= 51.0f) {
                float u = (modX - 39.0f) / 12.0f;
                float dip = sinf(u * 3.14159265f) * 3.8f;
                h -= dip;
            }
            return h;
        }

        case STAGE_MUDPOOL: {
            // Rolling muddy hollows and ridges
            double w1 = 1.8 * sin((double)(t * 0.085f));
            double w2 = 1.4 * cos((double)(t * 0.175f));
            double w3 = 0.8 * sin((double)t * 0.35f + w2);
            double s = w1 + w2 * 0.7 + w3 * 0.4;
            float h = (float)s * 1.3f;

            // In mud basins [18.0, 32.0] of each 60m cycle, carve a muddy hollow depression
            float modX = fmodf(x, 60.0f);
            if (modX < 0.0f) modX += 60.0f;
            if (x >= 15.0f && modX >= 18.0f && modX <= 32.0f) {
                float u = (modX - 18.0f) / 14.0f;
                float dip = sinf(u * 3.14159265f) * 1.8f;
                h -= dip;
            }
            return h;
        }

        case STAGE_VOLCANO: {
            // Jagged volcanic slopes and caldera rims
            double w1 = 2.5 * sin((double)(t * 0.095f));
            double w2 = 1.8 * sin((double)(t * 0.22f) + cos((double)(t * 0.08f)));
            double w3 = 1.0 * cos((double)(t * 0.42f));
            double s = w1 + w2 * 0.8 + w3 * 0.5;
            float h = (float)s * 1.4f;

            // Lava hollow depressions [20.0, 34.0] of each 60m cycle
            float modX = fmodf(x, 60.0f);
            if (modX < 0.0f) modX += 60.0f;
            if (x >= 15.0f && modX >= 20.0f && modX <= 34.0f) {
                float u = (modX - 20.0f) / 14.0f;
                float dip = sinf(u * 3.14159265f) * 1.5f;
                h -= dip;
            }
            return h;
        }

        case STAGE_BEACH: {
            // Smooth sandy dunes and gentle oceanfront rolls
            double w1 = 2.2 * sin((double)(t * 0.072f));
            double w2 = 1.5 * cos((double)(t * 0.135f));
            double w3 = 0.7 * sin((double)(t * 0.275f));
            double s = w1 * 1.1 + w2 * 0.8 + w3 * 0.4;
            return (float)s * 1.25f;
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
    b.plankCount = 10;

    b2PolygonShape plankBox;
    plankBox.SetAsBox(BRIDGE_PLANK_WIDTH * 0.5f, BRIDGE_PLANK_HEIGHT * 0.5f);

    b2FixtureDef pfd;
    pfd.shape = &plankBox;
    pfd.density = 1.5f;
    pfd.friction = 0.70f;
    pfd.restitution = 0.0f;
    pfd.filter.categoryBits = CAT_TERRAIN;
    pfd.filter.maskBits = CAT_CHASSIS | CAT_WHEEL;

    for (int i = 0; i < b.plankCount; ++i) {
        float px = startX + ((float)i + 0.5f) * BRIDGE_PLANK_WIDTH;
        float py = startY + (((float)i + 0.5f) / (float)b.plankCount) * (endY - startY);

        b2BodyDef pbd;
        pbd.type = b2_dynamicBody;
        pbd.position.Set(px, py);
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
        b2Vec2 anchor(startX + (float)i * BRIDGE_PLANK_WIDTH,
                      startY + ((float)i / (float)b.plankCount) * (endY - startY));
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
    if (endX - mLastItemSpawnX < 30.0f) return;

    float spawnX = mLastItemSpawnX + 35.0f;
    mLastItemSpawnX = spawnX;

    float fuelInterval = (mStageConfig && mStageConfig->fuel_interval > 0.0f) ? mStageConfig->fuel_interval : 90.0f;
    int intervalInt = (int)fuelInterval;
    if (intervalInt <= 0) intervalInt = 90;
    bool spawnFuel = ((int)(spawnX) % intervalInt < 35);

    if (spawnFuel && mItemCount < MAX_COLLECTIBLES) {
        float y = GetHeightAtX(spawnX) + 1.2f;
        SpawnItem(spawnX, y, 1000); // 1000 = Fuel
    } else {
        int coinVal = (spawnX > 300.0f) ? 100 : (spawnX > 100.0f ? 25 : 5);
        for (int k = 0; k < 3 && mItemCount < MAX_COLLECTIBLES; ++k) {
            float cx = spawnX + (float)k * 1.5f;
            float cy = GetHeightAtX(cx) + 1.0f + sinf((float)k * 1.0f) * 0.4f;
            SpawnItem(cx, cy, coinVal);
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
        float bottomY = cameraY - 14.0f;
        Renderer2D::DrawTerrainFill(&mTexGround, mRenderX, mRenderY, renderCount, bottomY);
        Renderer2D::DrawTerrainSurface(&mTexSurface, mRenderX, mRenderY, renderCount, 0.6f);

        if (HasCeiling()) {
            Renderer2D::DrawTerrainSurface(&mTexSurface, mCeilRenderX, mCeilRenderY, renderCount, 0.6f);
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
            for (int p = 0; p < mBridges[b].plankCount; ++p) {
                b2Body* plank = mBridges[b].planks[p];
                if (plank) {
                    b2Vec2 pos = plank->GetPosition();
                    float angle = plank->GetAngle();
                    Renderer2D::DrawSprite(&mTexBridge, pos.x, pos.y, BRIDGE_PLANK_WIDTH, BRIDGE_PLANK_HEIGHT, angle);
                }
            }
        }
    }
}
