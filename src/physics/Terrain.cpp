#include "Terrain.h"
#include <math.h>

static inline float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

Terrain::Terrain(b2World* world, StageType stageType)
    : mWorld(world), mStageType(stageType), mPointCount(0),
      mLastGeneratedX(0.0f), mFirstGeneratedX(0.0f),
      mTerrainBody(NULL), mItemCount(0), mLastItemSpawnX(15.0f),
      mQueueHead(0), mQueueTail(0), mQueueCount(0) {

    for (int i = 0; i < DEFERRED_QUEUE_CAPACITY; ++i) {
        mDestroyQueue[i] = NULL;
    }

    // Load textures based on stage
    const char* groundPath = "assets/terrain-ground.png";
    const char* surfacePath = "assets/terrain-surface.png";

    switch (mStageType) {
        case STAGE_DESERT:
            groundPath = "assets/desert-ground.png";
            surfacePath = "assets/desert-surface.png";
            break;
        case STAGE_ARCTIC:
            groundPath = "assets/arctic-ground.png";
            surfacePath = "assets/arctic-surface.png";
            break;
        case STAGE_MOON:
            groundPath = "assets/moon-ground.png";
            surfacePath = "assets/moon-surface.png";
            break;
        default:
            break;
    }

    mTexGround.LoadPNG(groundPath);
    mTexSurface.LoadPNG(surfacePath);

    // Collectibles
    mTexCoin5.LoadPNG("assets/Coin5.png");
    mTexCoin25.LoadPNG("assets/Coin25.png");
    mTexCoin100.LoadPNG("assets/Coin100.png");
    mTexCoin500.LoadPNG("assets/Coin500.png");
    mTexFuel.LoadPNG("assets/fuel-canister.png");

    // Create terrain static body
    b2BodyDef bd;
    bd.type = b2_staticBody;
    bd.position.Set(0.0f, 0.0f);
    mTerrainBody = mWorld->CreateBody(&bd);

    // Initial points around spawn (-20m to +50m) at exact 0.5m segment step
    float startX = -20.0f;
    float endX = 50.0f;
    float step = 0.5f;

    mFirstGeneratedX = startX;
    mLastGeneratedX = startX;

    while (mLastGeneratedX <= endX && mPointCount < MAX_TERRAIN_POINTS) {
        mPointsX[mPointCount] = mLastGeneratedX;
        mPointsY[mPointCount] = GetHeightAtX(mLastGeneratedX);
        mPointCount++;
        mLastGeneratedX += step;
    }

    RebuildCollision();
}

Terrain::~Terrain() {
    mQueueHead = 0;
    mQueueTail = 0;
    mQueueCount = 0;
    for (int i = 0; i < DEFERRED_QUEUE_CAPACITY; ++i) {
        mDestroyQueue[i] = NULL;
    }
    for (int i = 0; i < mItemCount; ++i) {
        if (mItems[i].body) {
            mWorld->DestroyBody(mItems[i].body);
            mItems[i].body = NULL;
        }
    }
    mItemCount = 0;
    if (mTerrainBody) {
        mWorld->DestroyBody(mTerrainBody);
        mTerrainBody = NULL;
    }
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

float Terrain::GetStageFriction() const {
    switch (mStageType) {
        case STAGE_DESERT: return FRICTION_DESERT;
        case STAGE_ARCTIC: return FRICTION_ARCTIC;
        case STAGE_MOON:   return FRICTION_MOON;
        default:           return FRICTION_COUNTRYSIDE;
    }
}

float Terrain::GetCoinMultiplier() const {
    switch (mStageType) {
        case STAGE_DESERT: return COIN_MULT_DESERT;
        case STAGE_ARCTIC: return COIN_MULT_ARCTIC;
        case STAGE_MOON:   return COIN_MULT_MOON;
        default:           return COIN_MULT_COUNTRYSIDE;
    }
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

void Terrain::RebuildCollision() {
    if (!mTerrainBody) return;

    // Remove existing fixtures
    b2Fixture* f = mTerrainBody->GetFixtureList();
    while (f) {
        b2Fixture* next = f->GetNext();
        mTerrainBody->DestroyFixture(f);
        f = next;
    }

    if (mPointCount < 2) return;

    float friction = GetStageFriction();

    // Create edge segments
    b2EdgeShape edge;
    b2FixtureDef fd;
    fd.shape = &edge;
    fd.friction = friction;
    fd.restitution = 0.0f;
    fd.filter.categoryBits = CAT_TERRAIN;
    fd.filter.maskBits = CAT_CHASSIS | CAT_WHEEL | CAT_DRIVER;

    for (int i = 0; i < mPointCount - 1; ++i) {
        b2Vec2 v1(mPointsX[i], mPointsY[i]);
        b2Vec2 v2(mPointsX[i + 1], mPointsY[i + 1]);
        edge.Set(v1, v2);
        mTerrainBody->CreateFixture(&fd);
    }

    // Rear boundary wall so player cannot reverse off the left edge
    b2EdgeShape wall;
    wall.Set(b2Vec2(mPointsX[0], -15.0f), b2Vec2(mPointsX[0], 35.0f));
    fd.shape = &wall;
    mTerrainBody->CreateFixture(&fd);
}

void Terrain::SpawnItemsAhead(float startX, float endX) {
    if (endX - mLastItemSpawnX < 30.0f) return;

    // Spawn pattern
    float spawnX = mLastItemSpawnX + 35.0f;
    mLastItemSpawnX = spawnX;

    // Decide whether to spawn fuel or coin pack
    bool spawnFuel = ((int)(spawnX) % 90 < 35);

    if (spawnFuel && mItemCount < MAX_COLLECTIBLES) {
        float y = GetHeightAtX(spawnX) + 1.2f;
        SpawnItem(spawnX, y, 1000); // 1000 = Fuel
    } else {
        // Spawn coin arc (3 coins)
        int coinVal = (spawnX > 300.0f) ? 100 : (spawnX > 100.0f ? 25 : 5);
        for (int k = 0; k < 3 && mItemCount < MAX_COLLECTIBLES; ++k) {
            float cx = spawnX + k * 1.5f;
            float cy = GetHeightAtX(cx) + 1.0f + sinf(k * 1.0f) * 0.4f;
            SpawnItem(cx, cy, coinVal);
        }
    }
}

void Terrain::Update(float playerX) {
    float aheadLimit = playerX + 45.0f;

    // Generate new points ahead with exact 0.5m segment step
    float step = 0.5f;
    bool added = false;
    float friction = GetStageFriction();

    while (mLastGeneratedX <= aheadLimit && mPointCount < MAX_TERRAIN_POINTS) {
        mPointsX[mPointCount] = mLastGeneratedX;
        mPointsY[mPointCount] = GetHeightAtX(mLastGeneratedX);
        mPointCount++;
        mLastGeneratedX += step;
        added = true;

        // Add edge fixture directly
        if (mTerrainBody && mPointCount >= 2) {
            b2EdgeShape edge;
            b2Vec2 v1(mPointsX[mPointCount - 2], mPointsY[mPointCount - 2]);
            b2Vec2 v2(mPointsX[mPointCount - 1], mPointsY[mPointCount - 1]);
            edge.Set(v1, v2);

            b2FixtureDef fd;
            fd.shape = &edge;
            fd.friction = friction;
            fd.restitution = 0.0f;
            fd.filter.categoryBits = CAT_TERRAIN;
            fd.filter.maskBits = CAT_CHASSIS | CAT_WHEEL | CAT_DRIVER;
            mTerrainBody->CreateFixture(&fd);
        }
    }

    if (added) {
        SpawnItemsAhead(playerX, aheadLimit);
    }
}

void Terrain::Render(float cameraX, float cameraY) {
    if (mPointCount < 2) return;

    // Find the visible slice of terrain points on screen (fast culling)
    float minVisX = cameraX - 12.0f;
    float maxVisX = cameraX + 22.0f;

    int startIdx = 0;
    while (startIdx < mPointCount - 1 && mPointsX[startIdx + 1] < minVisX) {
        startIdx++;
    }

    int endIdx = startIdx;
    while (endIdx < mPointCount - 1 && mPointsX[endIdx] < maxVisX) {
        endIdx++;
    }
    if (endIdx < mPointCount - 1) {
        endIdx++; // one extra point to avoid gap at edge of screen
    }

    int visibleCount = endIdx - startIdx + 1;
    if (visibleCount < 2) return;

    float bottomY = cameraY - 14.0f; // well below visible screen

    // 1. Draw ground fill for visible slice only (fast & locked to world UVs)
    Renderer2D::DrawTerrainFill(&mTexGround, &mPointsX[startIdx], &mPointsY[startIdx], visibleCount, bottomY);

    // 2. Draw surface ribbon for visible slice only
    Renderer2D::DrawTerrainSurface(&mTexSurface, &mPointsX[startIdx], &mPointsY[startIdx], visibleCount, 0.6f);

    // 3. Draw Collectibles (only if within visible range)
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
}
