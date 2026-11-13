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
#endif

class Renderer2D {
public:
    static void Init();
    static void SetupCamera(float camX, float camY, float zoom = 1.0f);
    static void SetupScreenSpace();

    // Sprite drawing
    static void DrawSprite(Texture* tex, float worldX, float worldY, float width, float height, float angle = 0.0f, unsigned int color = 0xFFFFFFFF);
    static void DrawSpriteScreen(Texture* tex, float screenX, float screenY, float width, float height, unsigned int color = 0xFFFFFFFF);
    static void DrawQuad(Texture* tex, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, unsigned int color = 0xFFFFFFFF);

    // Primitive drawing
    static void DrawFilledRect(float x, float y, float w, float h, unsigned int color);

    // Terrain rendering
    static void DrawTerrainFill(Texture* groundTex, const float* pointsX, const float* pointsY, int count, float bottomY, unsigned int color = 0xFFFFFFFF);
    static void DrawTerrainSurface(Texture* surfaceTex, const float* pointsX, const float* pointsY, int count, float thickness, unsigned int color = 0xFFFFFFFF);
};

#endif // HCR_RENDERER2D_H
