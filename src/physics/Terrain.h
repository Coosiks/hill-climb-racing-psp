#ifndef HCR_TERRAIN_H
#define HCR_TERRAIN_H

#include <Box2D/Box2D.h>
#include "core/GameConstants.h"
#include "graphics/Texture.h"
#include "graphics/Renderer2D.h"

struct Collectible {
    float x, y;
    int type; // 5, 25, 100, 500 for coins, 1000 for fuel
    b2Body* body;
    bool collected;
};

#define MAX_TERRAIN_POINTS 8192
#define MAX_COLLECTIBLES   64
#define DEFERRED_QUEUE_CAPACITY 64

class Terrain {
public:
    Terrain(b2World* world, StageType stageType);
    ~Terrain();

    void Update(float playerX);
    void ProcessDeferredDestroy(float carX);
    void EnqueueDestroy(b2Body* body);
    b2Body* SpawnItem(float x, float y, int type);
    void Render(float cameraX, float cameraY);

    float GetHeightAtX(float x) const;
    float GetStageFriction() const;
    float GetCoinMultiplier() const;

    int GetItemCount() const { return mItemCount; }
    const Collectible* GetItems() const { return mItems; }

private:
    b2World* mWorld;
    StageType mStageType;

    // Sliding window of generated points
    float mPointsX[MAX_TERRAIN_POINTS];
    float mPointsY[MAX_TERRAIN_POINTS];
    int mPointCount;
    float mLastGeneratedX;
    float mFirstGeneratedX;

    // Box2D body for terrain chain
    b2Body* mTerrainBody;

    // Collectibles
    Collectible mItems[MAX_COLLECTIBLES];
    int mItemCount;
    float mLastItemSpawnX;

    // Deferred destroy ring buffer
    b2Body* mDestroyQueue[DEFERRED_QUEUE_CAPACITY];
    int mQueueHead;
    int mQueueTail;
    int mQueueCount;

    // Textures
    Texture mTexGround;
    Texture mTexSurface;
    Texture mTexCoin5;
    Texture mTexCoin25;
    Texture mTexCoin100;
    Texture mTexCoin500;
    Texture mTexFuel;

    void RebuildCollision();
    void SpawnItemsAhead(float startX, float endX);
};

#endif // HCR_TERRAIN_H

