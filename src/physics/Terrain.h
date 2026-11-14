#ifndef HCR_TERRAIN_H
#define HCR_TERRAIN_H

#include <Box2D/Box2D.h>
#include <stdint.h>
#include "core/GameConstants.h"
#include "core/Config.h"
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
#define CHUNK_GEN_BUDGET   8  // N vertices computed per step (design/TODO-TUNE)

#define MAX_ACTIVE_BRIDGES 4
#define MAX_BRIDGE_PLANKS  12
#define BRIDGE_PLANK_WIDTH 1.20f
#define BRIDGE_PLANK_HEIGHT 0.20f

struct SuspensionBridge {
    bool active;
    int chunkIndex;
    float startX;
    float endX;
    int plankCount;
    b2Body* planks[MAX_BRIDGE_PLANKS];
    b2RevoluteJoint* joints[MAX_BRIDGE_PLANKS + 1];
};

struct TerrainChunk {
    int chunkIndex;
    b2Body* body;
    b2Fixture* fixture;
    b2Fixture* ceilingFixture;
    b2Vec2 vertices[VERTS_PER_CHUNK];
    b2Vec2 ceilingVertices[VERTS_PER_CHUNK];
    b2Vec2 prevVertex;
    b2Vec2 nextVertex;
    b2Vec2 prevCeilingVertex;
    b2Vec2 nextCeilingVertex;
    int verticesComputed;
    uint64_t accumulatedTicks;
    bool ready;
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
    float GetCeilingHeightAtX(float x) const;
    bool HasCeiling() const;
    float GetStageFriction() const;
    float GetCoinMultiplier() const;
    const StageConfig* GetStageConfig() const { return mStageConfig; }

    int GetItemCount() const { return mItemCount; }
    const Collectible* GetItems() const { return mItems; }

    static float GetAverageChunkGenMs();
    static float GetMaxPerFrameGenMs();
    static float GetAveragePerFrameGenMs();
    static int GetChunkGenCount();

    int GetActiveChunkCount() const;
    int GetTotalChunkSlots() const { return NUM_CHUNK_SLOTS; }
    b2Body* GetRearWallBody() const { return mRearWallBody; }
    int GetActiveBridgeCount() const;
    const SuspensionBridge* GetBridge(int index) const { return (index >= 0 && index < MAX_ACTIVE_BRIDGES) ? &mBridges[index] : NULL; }
    void MaybeSpawnBridge(int slotIdx);

    void GenerateChunkVertices(int chunkIndex, b2Vec2* outVerts, b2Vec2* outPrev, b2Vec2* outNext) const;

private:
    b2World* mWorld;
    StageType mStageType;
    const StageConfig* mStageConfig;

    // Static chunk pool
    TerrainChunk mChunks[NUM_CHUNK_SLOTS];
    b2Body* mRearWallBody;

    // Render coordinates cache
    float mRenderX[MAX_RENDER_POINTS];
    float mRenderY[MAX_RENDER_POINTS];
    float mCeilRenderX[MAX_RENDER_POINTS];
    float mCeilRenderY[MAX_RENDER_POINTS];

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
    Texture mTexBridge;

    // Suspension bridge pool
    SuspensionBridge mBridges[MAX_ACTIVE_BRIDGES];

    void InitBridgePool();
    void DestroyBridge(int bridgeIdx);
    void DestroyBridgeForChunk(int chunkIndex);
    void DestroyAllBridges();
    bool CreateBridge(int chunkIndex, float startX, float endX, float startY, float endY, b2Body* anchorBody);

    void InitChunkPool();
    void UpdateChunkWindow(int currentChunk);
    void StartChunkGeneration(int slotIdx, int chunkIndex);
    bool AdvanceGeneration(int slotIdx, int budget);
    void CompleteChunkImmediately(int slotIdx);
    void UnloadChunk(int slotIdx);
    void SpawnItemsAhead(float startX, float endX);
};

#endif // HCR_TERRAIN_H

