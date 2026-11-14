#include "Renderer2D.h"
#include <math.h>
#include <string.h>

Vertex2D Renderer2D::sBatchVertices[MAX_BATCH_VERTS];
Texture* Renderer2D::sBatchTexture = NULL;
int Renderer2D::sBatchQuadCount = 0;
int Renderer2D::sDrawCallCount = 0;
int Renderer2D::sBatchedQuadCount = 0;
float Renderer2D::sCameraX = 0.0f;
float Renderer2D::sCameraY = 0.0f;
float Renderer2D::sCameraZoom = 1.0f;

float Renderer2D::GetCameraX() {
    return sCameraX;
}

float Renderer2D::GetCameraY() {
    return sCameraY;
}

float Renderer2D::GetCameraZoom() {
    return sCameraZoom;
}

void Renderer2D::WorldToScreen(float wx, float wy, float* outSx, float* outSy) {
    if (outSx) *outSx = PSP_SCREEN_WIDTH * 0.5f + (wx - sCameraX) * (PTM_RATIO * sCameraZoom);
    if (outSy) *outSy = PSP_SCREEN_HEIGHT * 0.5f - (wy - sCameraY) * (PTM_RATIO * sCameraZoom);
}

void Renderer2D::ResetMetrics() {
    sDrawCallCount = 0;
    sBatchedQuadCount = 0;
}

int Renderer2D::GetDrawCallCount() {
    return sDrawCallCount;
}

int Renderer2D::GetBatchedQuadCount() {
    return sBatchedQuadCount;
}

void Renderer2D::EndFrame() {
    FlushBatch();
}

void Renderer2D::AddQuadToBatch(Texture* tex,
                                float x0, float y0, float u0, float v0,
                                float x1, float y1, float u1, float v1,
                                float x2, float y2, float u2, float v2,
                                float x3, float y3, float u3, float v3,
                                unsigned int color) {
    if (!tex) return;

    if (sBatchTexture != tex || sBatchQuadCount >= MAX_BATCH_SPRITES) {
        FlushBatch();
    }

    sBatchTexture = tex;
    int base = sBatchQuadCount * 6;

    // Triangle 1: (0, 1, 2)
    sBatchVertices[base + 0].u = u0; sBatchVertices[base + 0].v = v0;
    sBatchVertices[base + 0].color = color;
    sBatchVertices[base + 0].x = x0; sBatchVertices[base + 0].y = y0; sBatchVertices[base + 0].z = 0.0f;

    sBatchVertices[base + 1].u = u1; sBatchVertices[base + 1].v = v1;
    sBatchVertices[base + 1].color = color;
    sBatchVertices[base + 1].x = x1; sBatchVertices[base + 1].y = y1; sBatchVertices[base + 1].z = 0.0f;

    sBatchVertices[base + 2].u = u2; sBatchVertices[base + 2].v = v2;
    sBatchVertices[base + 2].color = color;
    sBatchVertices[base + 2].x = x2; sBatchVertices[base + 2].y = y2; sBatchVertices[base + 2].z = 0.0f;

    // Triangle 2: (2, 1, 3)
    sBatchVertices[base + 3].u = u2; sBatchVertices[base + 3].v = v2;
    sBatchVertices[base + 3].color = color;
    sBatchVertices[base + 3].x = x2; sBatchVertices[base + 3].y = y2; sBatchVertices[base + 3].z = 0.0f;

    sBatchVertices[base + 4].u = u1; sBatchVertices[base + 4].v = v1;
    sBatchVertices[base + 4].color = color;
    sBatchVertices[base + 4].x = x1; sBatchVertices[base + 4].y = y1; sBatchVertices[base + 4].z = 0.0f;

    sBatchVertices[base + 5].u = u3; sBatchVertices[base + 5].v = v3;
    sBatchVertices[base + 5].color = color;
    sBatchVertices[base + 5].x = x3; sBatchVertices[base + 5].y = y3; sBatchVertices[base + 5].z = 0.0f;

    sBatchQuadCount++;
    sBatchedQuadCount++;
}

#if defined(__PSP__)

void Renderer2D::Init() {
    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumOrtho(0.0f, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, 0.0f, -1.0f, 1.0f);

    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();

    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();

    sBatchQuadCount = 0;
    sBatchTexture = NULL;
    sDrawCallCount = 0;
    sBatchedQuadCount = 0;
}

void Renderer2D::SetupCamera(float camX, float camY, float zoom) {
    FlushBatch();

    sCameraX = camX;
    sCameraY = camY;
    sCameraZoom = zoom;

    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();

    float halfW = (PSP_SCREEN_WIDTH * 0.5f) / (PTM_RATIO * zoom);
    float halfH = (PSP_SCREEN_HEIGHT * 0.5f) / (PTM_RATIO * zoom);

    sceGumOrtho(-halfW, halfW, -halfH, halfH, -100.0f, 100.0f);

    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    ScePspFVector3 eye = { -camX, -camY, 0.0f };
    sceGumTranslate(&eye);

    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
}

void Renderer2D::SetupScreenSpace() {
    FlushBatch();

    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumOrtho(0.0f, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, 0.0f, -1.0f, 1.0f);

    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();

    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
}

void Renderer2D::FlushBatch() {
    if (sBatchQuadCount == 0 || !sBatchTexture) return;

    sDrawCallCount++;
    sBatchTexture->Bind(GU_LINEAR);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);

    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();

    int vertCount = sBatchQuadCount * 6;
    Vertex2D* v = (Vertex2D*)sceGuGetMemory(vertCount * sizeof(Vertex2D));
    if (v) {
        memcpy(v, sBatchVertices, vertCount * sizeof(Vertex2D));
        sceGumDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, vertCount, 0, v);
    }

    sBatchQuadCount = 0;
    sBatchTexture = NULL;
}

void Renderer2D::DrawSprite(Texture* tex, float worldX, float worldY, float width, float height, float angle, unsigned int color) {
    if (!tex) return;

    float hw = width * 0.5f;
    float hh = height * 0.5f;
    float maxU = tex->GetMaxU();
    float maxV = tex->GetMaxV();

    float cosA = 1.0f, sinA = 0.0f;
    if (angle != 0.0f) {
        cosA = cosf(angle);
        sinA = sinf(angle);
    }

    // Top-left, top-right, bottom-left, bottom-right
    float x0 = worldX + (-hw * cosA -  hh * sinA);
    float y0 = worldY + (-hw * sinA +  hh * cosA);

    float x1 = worldX + ( hw * cosA -  hh * sinA);
    float y1 = worldY + ( hw * sinA +  hh * cosA);

    float x2 = worldX + (-hw * cosA - -hh * sinA);
    float y2 = worldY + (-hw * sinA + -hh * cosA);

    float x3 = worldX + ( hw * cosA - -hh * sinA);
    float y3 = worldY + ( hw * sinA + -hh * cosA);

    AddQuadToBatch(tex,
                   x0, y0, 0.0f, 0.0f,
                   x1, y1, maxU, 0.0f,
                   x2, y2, 0.0f, maxV,
                   x3, y3, maxU, maxV,
                   color);
}

void Renderer2D::DrawSpriteScreen(Texture* tex, float screenX, float screenY, float width, float height, unsigned int color) {
    if (!tex) return;

    float maxU = tex->GetMaxU();
    float maxV = tex->GetMaxV();

    AddQuadToBatch(tex,
                   screenX,         screenY,          0.0f, 0.0f,
                   screenX + width, screenY,          maxU, 0.0f,
                   screenX,         screenY + height, 0.0f, maxV,
                   screenX + width, screenY + height, maxU, maxV,
                   color);
}

void Renderer2D::DrawQuad(Texture* tex, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, unsigned int color) {
    if (!tex) return;

    AddQuadToBatch(tex,
                   x0, y0, u0, v0,
                   x1, y0, u1, v0,
                   x0, y1, u0, v1,
                   x1, y1, u1, v1,
                   color);
}

void Renderer2D::DrawFilledRect(float x, float y, float w, float h, unsigned int color) {
    FlushBatch();

    sceGuDisable(GU_TEXTURE_2D);

    ColorVertex2D* v = (ColorVertex2D*)sceGuGetMemory(4 * sizeof(ColorVertex2D));
    if (v) {
        v[0].color = color; v[0].x = x;     v[0].y = y;     v[0].z = 0.0f;
        v[1].color = color; v[1].x = x + w; v[1].y = y;     v[1].z = 0.0f;
        v[2].color = color; v[2].x = x;     v[2].y = y + h; v[2].z = 0.0f;
        v[3].color = color; v[3].x = x + w; v[3].y = y + h; v[3].z = 0.0f;

        sceGumDrawArray(GU_TRIANGLE_STRIP, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 4, 0, v);
        sDrawCallCount++;
    }

    sceGuEnable(GU_TEXTURE_2D);
}

void Renderer2D::DrawGradientRect(float x, float y, float w, float h, unsigned int topColor, unsigned int bottomColor) {
    FlushBatch();

    sceGuDisable(GU_TEXTURE_2D);

    ColorVertex2D* v = (ColorVertex2D*)sceGuGetMemory(4 * sizeof(ColorVertex2D));
    if (v) {
        v[0].color = topColor;    v[0].x = x;     v[0].y = y;     v[0].z = 0.0f;
        v[1].color = topColor;    v[1].x = x + w; v[1].y = y;     v[1].z = 0.0f;
        v[2].color = bottomColor; v[2].x = x;     v[2].y = y + h; v[2].z = 0.0f;
        v[3].color = bottomColor; v[3].x = x + w; v[3].y = y + h; v[3].z = 0.0f;

        sceGumDrawArray(GU_TRIANGLE_STRIP, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 4, 0, v);
        sDrawCallCount++;
    }

    sceGuEnable(GU_TEXTURE_2D);
}

void Renderer2D::DrawLightCone(float originX, float originY, float angle, float length, float startWidth, float endWidth, unsigned int innerColor, unsigned int outerColor) {
    FlushBatch();

    float cosA = cosf(angle);
    float sinA = sinf(angle);
    float perpX = -sinA;
    float perpY = cosA;

    float p0x = originX + perpX * (startWidth * 0.5f);
    float p0y = originY + perpY * (startWidth * 0.5f);
    float p1x = originX - perpX * (startWidth * 0.5f);
    float p1y = originY - perpY * (startWidth * 0.5f);

    float endCenterX = originX + cosA * length;
    float endCenterY = originY + sinA * length;

    float p2x = endCenterX + perpX * (endWidth * 0.5f);
    float p2y = endCenterY + perpY * (endWidth * 0.5f);
    float p3x = endCenterX - perpX * (endWidth * 0.5f);
    float p3y = endCenterY - perpY * (endWidth * 0.5f);

    float s0x, s0y, s1x, s1y, s2x, s2y, s3x, s3y;
    WorldToScreen(p0x, p0y, &s0x, &s0y);
    WorldToScreen(p1x, p1y, &s1x, &s1y);
    WorldToScreen(p2x, p2y, &s2x, &s2y);
    WorldToScreen(p3x, p3y, &s3x, &s3y);

    sceGuDisable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0x00FFFFFF);

    ColorVertex2D* v = (ColorVertex2D*)sceGuGetMemory(4 * sizeof(ColorVertex2D));
    if (v) {
        v[0].color = innerColor; v[0].x = s0x; v[0].y = s0y; v[0].z = 0.0f;
        v[1].color = innerColor; v[1].x = s1x; v[1].y = s1y; v[1].z = 0.0f;
        v[2].color = outerColor; v[2].x = s2x; v[2].y = s2y; v[2].z = 0.0f;
        v[3].color = outerColor; v[3].x = s3x; v[3].y = s3y; v[3].z = 0.0f;

        sceGumDrawArray(GU_TRIANGLE_STRIP, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 4, 0, v);
        sDrawCallCount++;
    }

    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuEnable(GU_TEXTURE_2D);
}

void Renderer2D::DrawTerrainFill(Texture* groundTex, const float* pointsX, const float* pointsY, int count, float bottomY, unsigned int color) {
    if (!groundTex || count < 2) return;

    FlushBatch();

    groundTex->Bind(GU_REPEAT);
    sceGuTexWrap(GU_REPEAT, GU_REPEAT);

    int vertCount = count * 2;
    Vertex2D* v = (Vertex2D*)sceGuGetMemory(vertCount * sizeof(Vertex2D));
    if (!v) return;

    const float texScale = 0.25f;
    const float UV_OFFSET = 10000.0f;

    for (int i = 0; i < count; ++i) {
        float x = pointsX[i];
        float y = pointsY[i];

        float u = (x + UV_OFFSET) * texScale;
        float vTop = (y + UV_OFFSET) * texScale;
        float vBot = (bottomY + UV_OFFSET) * texScale;

        v[i * 2 + 0].u = u;
        v[i * 2 + 0].v = vTop;
        v[i * 2 + 0].color = color;
        v[i * 2 + 0].x = x;
        v[i * 2 + 0].y = y;
        v[i * 2 + 0].z = 0.0f;

        v[i * 2 + 1].u = u;
        v[i * 2 + 1].v = vBot;
        v[i * 2 + 1].color = color;
        v[i * 2 + 1].x = x;
        v[i * 2 + 1].y = bottomY;
        v[i * 2 + 1].z = 0.0f;
    }

    sceGumDrawArray(GU_TRIANGLE_STRIP, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, vertCount, 0, v);
    sDrawCallCount++;
}

void Renderer2D::DrawTerrainSurface(Texture* surfaceTex, const float* pointsX, const float* pointsY, int count, float thickness, unsigned int color) {
    if (!surfaceTex || count < 2) return;

    FlushBatch();

    surfaceTex->Bind(GU_REPEAT);
    sceGuTexWrap(GU_REPEAT, GU_CLAMP);

    int vertCount = count * 2;
    Vertex2D* v = (Vertex2D*)sceGuGetMemory(vertCount * sizeof(Vertex2D));
    if (!v) return;

    const float UV_OFFSET = 10000.0f;
    const float halfThick = thickness * 0.5f;

    for (int i = 0; i < count; ++i) {
        float x = pointsX[i];
        float y = pointsY[i];

        float u = (x + UV_OFFSET) * 0.5f;

        v[i * 2 + 0].u = u;
        v[i * 2 + 0].v = 0.0f;
        v[i * 2 + 0].color = color;
        v[i * 2 + 0].x = x;
        v[i * 2 + 0].y = y + halfThick;
        v[i * 2 + 0].z = 0.0f;

        v[i * 2 + 1].u = u;
        v[i * 2 + 1].v = 1.0f;
        v[i * 2 + 1].color = color;
        v[i * 2 + 1].x = x;
        v[i * 2 + 1].y = y - halfThick;
        v[i * 2 + 1].z = 0.0f;
    }

    sceGumDrawArray(GU_TRIANGLE_STRIP, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, vertCount, 0, v);
    sDrawCallCount++;
}

#else // PC OpenGL Implementation

void Renderer2D::Init() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);

    sBatchQuadCount = 0;
    sBatchTexture = NULL;
    sDrawCallCount = 0;
    sBatchedQuadCount = 0;
}

void Renderer2D::SetupCamera(float camX, float camY, float zoom) {
    FlushBatch();

    sCameraX = camX;
    sCameraY = camY;
    sCameraZoom = zoom;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    float halfW = (PSP_SCREEN_WIDTH * 0.5f) / (PTM_RATIO * zoom);
    float halfH = (PSP_SCREEN_HEIGHT * 0.5f) / (PTM_RATIO * zoom);

    glOrtho(-halfW, halfW, -halfH, halfH, -100.0f, 100.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(-camX, -camY, 0.0f);
}

void Renderer2D::SetupScreenSpace() {
    FlushBatch();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0f, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, 0.0f, -1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static inline void SetGLColor(unsigned int c) {
    glColor4ub(c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF, (c >> 24) & 0xFF);
}

void Renderer2D::FlushBatch() {
    if (sBatchQuadCount == 0 || !sBatchTexture) return;

    sDrawCallCount++;
    sBatchTexture->Bind(GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBegin(GL_TRIANGLES);
    int totalVerts = sBatchQuadCount * 6;
    for (int i = 0; i < totalVerts; ++i) {
        const Vertex2D& v = sBatchVertices[i];
        SetGLColor(v.color);
        glTexCoord2f(v.u, v.v);
        glVertex2f(v.x, v.y);
    }
    glEnd();

    sBatchQuadCount = 0;
    sBatchTexture = NULL;
}

void Renderer2D::DrawSprite(Texture* tex, float worldX, float worldY, float width, float height, float angle, unsigned int color) {
    if (!tex) return;

    float hw = width * 0.5f;
    float hh = height * 0.5f;
    float maxU = tex->GetMaxU();
    float maxV = tex->GetMaxV();

    float cosA = 1.0f, sinA = 0.0f;
    if (angle != 0.0f) {
        cosA = cosf(angle);
        sinA = sinf(angle);
    }

    float x0 = worldX + (-hw * cosA -  hh * sinA);
    float y0 = worldY + (-hw * sinA +  hh * cosA);

    float x1 = worldX + ( hw * cosA -  hh * sinA);
    float y1 = worldY + ( hw * sinA +  hh * cosA);

    float x2 = worldX + (-hw * cosA - -hh * sinA);
    float y2 = worldY + (-hw * sinA + -hh * cosA);

    float x3 = worldX + ( hw * cosA - -hh * sinA);
    float y3 = worldY + ( hw * sinA + -hh * cosA);

    AddQuadToBatch(tex,
                   x0, y0, 0.0f, 0.0f,
                   x1, y1, maxU, 0.0f,
                   x2, y2, 0.0f, maxV,
                   x3, y3, maxU, maxV,
                   color);
}

void Renderer2D::DrawSpriteScreen(Texture* tex, float screenX, float screenY, float width, float height, unsigned int color) {
    if (!tex) return;

    float maxU = tex->GetMaxU();
    float maxV = tex->GetMaxV();

    AddQuadToBatch(tex,
                   screenX,         screenY,          0.0f, 0.0f,
                   screenX + width, screenY,          maxU, 0.0f,
                   screenX,         screenY + height, 0.0f, maxV,
                   screenX + width, screenY + height, maxU, maxV,
                   color);
}

void Renderer2D::DrawQuad(Texture* tex, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, unsigned int color) {
    if (!tex) return;

    AddQuadToBatch(tex,
                   x0, y0, u0, v0,
                   x1, y0, u1, v0,
                   x0, y1, u0, v1,
                   x1, y1, u1, v1,
                   color);
}

void Renderer2D::DrawFilledRect(float x, float y, float w, float h, unsigned int color) {
    FlushBatch();

    glDisable(GL_TEXTURE_2D);
    SetGLColor(color);

    glBegin(GL_TRIANGLE_STRIP);
    glVertex2f(x,     y);
    glVertex2f(x + w, y);
    glVertex2f(x,     y + h);
    glVertex2f(x + w, y + h);
    glEnd();

    glEnable(GL_TEXTURE_2D);
    sDrawCallCount++;
}

void Renderer2D::DrawGradientRect(float x, float y, float w, float h, unsigned int topColor, unsigned int bottomColor) {
    FlushBatch();

    glDisable(GL_TEXTURE_2D);

    glBegin(GL_TRIANGLE_STRIP);
    SetGLColor(topColor);
    glVertex2f(x,     y);
    glVertex2f(x + w, y);
    SetGLColor(bottomColor);
    glVertex2f(x,     y + h);
    glVertex2f(x + w, y + h);
    glEnd();

    glEnable(GL_TEXTURE_2D);
    sDrawCallCount++;
}

void Renderer2D::DrawLightCone(float originX, float originY, float angle, float length, float startWidth, float endWidth, unsigned int innerColor, unsigned int outerColor) {
    FlushBatch();

    float cosA = cosf(angle);
    float sinA = sinf(angle);
    float perpX = -sinA;
    float perpY = cosA;

    float p0x = originX + perpX * (startWidth * 0.5f);
    float p0y = originY + perpY * (startWidth * 0.5f);
    float p1x = originX - perpX * (startWidth * 0.5f);
    float p1y = originY - perpY * (startWidth * 0.5f);

    float endCenterX = originX + cosA * length;
    float endCenterY = originY + sinA * length;

    float p2x = endCenterX + perpX * (endWidth * 0.5f);
    float p2y = endCenterY + perpY * (endWidth * 0.5f);
    float p3x = endCenterX - perpX * (endWidth * 0.5f);
    float p3y = endCenterY - perpY * (endWidth * 0.5f);

    float s0x, s0y, s1x, s1y, s2x, s2y, s3x, s3y;
    WorldToScreen(p0x, p0y, &s0x, &s0y);
    WorldToScreen(p1x, p1y, &s1x, &s1y);
    WorldToScreen(p2x, p2y, &s2x, &s2y);
    WorldToScreen(p3x, p3y, &s3x, &s3y);

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glBegin(GL_TRIANGLE_STRIP);
    SetGLColor(innerColor);
    glVertex2f(s0x, s0y);
    glVertex2f(s1x, s1y);
    SetGLColor(outerColor);
    glVertex2f(s2x, s2y);
    glVertex2f(s3x, s3y);
    glEnd();

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    sDrawCallCount++;
}

void Renderer2D::DrawTerrainFill(Texture* groundTex, const float* pointsX, const float* pointsY, int count, float bottomY, unsigned int color) {
    if (!groundTex || count < 2) return;

    FlushBatch();

    groundTex->Bind(GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    const float texScale = 0.25f;
    const float UV_OFFSET = 10000.0f;

    SetGLColor(color);
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i < count; ++i) {
        float x = pointsX[i];
        float y = pointsY[i];

        float u = (x + UV_OFFSET) * texScale;
        float vTop = (y + UV_OFFSET) * texScale;
        float vBot = (bottomY + UV_OFFSET) * texScale;

        glTexCoord2f(u, vTop); glVertex2f(x, y);
        glTexCoord2f(u, vBot); glVertex2f(x, bottomY);
    }
    glEnd();
    sDrawCallCount++;
}

void Renderer2D::DrawTerrainSurface(Texture* surfaceTex, const float* pointsX, const float* pointsY, int count, float thickness, unsigned int color) {
    if (!surfaceTex || count < 2) return;

    FlushBatch();

    surfaceTex->Bind(GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    const float UV_OFFSET = 10000.0f;
    const float halfThick = thickness * 0.5f;

    SetGLColor(color);
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i < count; ++i) {
        float x = pointsX[i];
        float y = pointsY[i];

        float u = (x + UV_OFFSET) * 0.5f;

        glTexCoord2f(u, 0.0f); glVertex2f(x, y + halfThick);
        glTexCoord2f(u, 1.0f); glVertex2f(x, y - halfThick);
    }
    glEnd();
    sDrawCallCount++;
}

#endif
