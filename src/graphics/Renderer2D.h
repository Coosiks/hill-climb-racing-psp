#ifndef HCR_RENDERER2D_H
#define HCR_RENDERER2D_H

#include "Texture.h"
#include "core/GameConstants.h"

#if defined(__PSP__)
#include <pspgu.h>
#include <pspgum.h>
struct Vertex2D {
    float u, v;
    unsigned int color;
    float x, y, z;
};

struct ColorVertex2D {
    unsigned int color;
    float x, y, z;
};
#else
#include <GL/gl.h>
struct Vertex2D {
    float u, v;
    unsigned int color;
    float x, y, z;
};
#endif

class Renderer2D {
public:
    static void Init();
    static void SetupCamera(float camX, float camY, float zoom = 1.0f);
    static void SetupScreenSpace();

    // Sprite drawing (automatically batched)
    static void DrawSprite(Texture* tex, float worldX, float worldY, float width, float height, float angle = 0.0f, unsigned int color = 0xFFFFFFFF);
    static void DrawSpriteScreen(Texture* tex, float screenX, float screenY, float width, float height, unsigned int color = 0xFFFFFFFF);
    static void DrawQuad(Texture* tex, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, unsigned int color = 0xFFFFFFFF);

    // Primitive drawing
    static void DrawFilledRect(float x, float y, float w, float h, unsigned int color);
    static void DrawGradientRect(float x, float y, float w, float h, unsigned int topColor, unsigned int bottomColor);

    // Terrain rendering
    static void DrawTerrainFill(Texture* groundTex, const float* pointsX, const float* pointsY, int count, float bottomY, unsigned int color = 0xFFFFFFFF);
    static void DrawTerrainSurface(Texture* surfaceTex, const float* pointsX, const float* pointsY, int count, float thickness, unsigned int color = 0xFFFFFFFF);

    // Batching & inspection
    static void FlushBatch();
    static void EndFrame();
    static int GetDrawCallCount();
    static int GetBatchedQuadCount();
    static void ResetMetrics();

private:
    static const int MAX_BATCH_SPRITES = 256;
    static const int MAX_BATCH_VERTS = MAX_BATCH_SPRITES * 6;

    static Vertex2D sBatchVertices[MAX_BATCH_VERTS];
    static Texture* sBatchTexture;
    static int sBatchQuadCount;
    static int sDrawCallCount;
    static int sBatchedQuadCount;

    static void AddQuadToBatch(Texture* tex,
                               float x0, float y0, float u0, float v0,
                               float x1, float y1, float u1, float v1,
                               float x2, float y2, float u2, float v2,
                               float x3, float y3, float u3, float v3,
                               unsigned int color);
};

#endif // HCR_RENDERER2D_H
