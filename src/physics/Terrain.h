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

#define MAX_COLLECTIBLES        64
#define DEFERRED_QUEUE_CAPACITY 64

#define CHUNK_WIDTH_METERS 30.0f
#define CHUNK_STEP_METERS  0.5f
#define VERTS_PER_CHUNK    61 // (30 / 0.5) + 1 vertices
#define NUM_CHUNK_SLOTS    6
#define MAX_RENDER_POINTS  256

struct TerrainChunk {
    int chunkIndex;
    b2Body* body;
    b2Fixture* fixture;
    b2Vec2 vertices[VERTS_PER_CHUNK];
    bool active;
};

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

    int GetActiveChunkCount() const;
    int GetTotalChunkSlots() const { return NUM_CHUNK_SLOTS; }
    b2Body* GetRearWallBody() const { return mRearWallBody; }

    void GenerateChunkVertices(int chunkIndex, b2Vec2* outVerts, b2Vec2* outPrev, b2Vec2* outNext) const;

private:
    b2World* mWorld;
    StageType mStageType;

    // Static chunk pool
    TerrainChunk mChunks[NUM_CHUNK_SLOTS];
    b2Body* mRearWallBody;

    // Render coordinates cache
    float mRenderX[MAX_RENDER_POINTS];
    float mRenderY[MAX_RENDER_POINTS];

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

    void InitChunkPool();
    void UpdateChunkWindow(int currentChunk);
    void LoadChunk(int slotIdx, int chunkIndex);
    void UnloadChunk(int slotIdx);
    void SpawnItemsAhead(float startX, float endX);
};

#endif // HCR_TERRAIN_H

