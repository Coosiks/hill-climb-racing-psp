#include "Renderer2D.h"
#include <math.h>

#if defined(__PSP__)

void Renderer2D::Init() {
    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumOrtho(0.0f, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, 0.0f, -1.0f, 1.0f);

    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();

    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
}

void Renderer2D::SetupCamera(float camX, float camY, float zoom) {
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
    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumOrtho(0.0f, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, 0.0f, -1.0f, 1.0f);

    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();

    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
}

void Renderer2D::DrawSprite(Texture* tex, float worldX, float worldY, float width, float height, float angle, unsigned int color) {
    if (!tex) return;

    tex->Bind(GU_LINEAR);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);

    sceGumMatrixMode(GU_MODEL);
    sceGumPushMatrix();

    ScePspFVector3 pos = { worldX, worldY, 0.0f };
    sceGumTranslate(&pos);

    if (angle != 0.0f) {
        sceGumRotateZ(angle);
    }

    float hw = width * 0.5f;
    float hh = height * 0.5f;
    float maxU = tex->GetMaxU();
    float maxV = tex->GetMaxV();

    Vertex2D* v = (Vertex2D*)sceGuGetMemory(4 * sizeof(Vertex2D));
    if (!v) {
        sceGumPopMatrix();
        return;
    }

    v[0].u = 0.0f; v[0].v = 0.0f; v[0].color = color; v[0].x = -hw; v[0].y =  hh; v[0].z = 0.0f;
    v[1].u = maxU; v[1].v = 0.0f; v[1].color = color; v[1].x =  hw; v[1].y =  hh; v[1].z = 0.0f;
    v[2].u = 0.0f; v[2].v = maxV; v[2].color = color; v[2].x = -hw; v[2].y = -hh; v[2].z = 0.0f;
    v[3].u = maxU; v[3].v = maxV; v[3].color = color; v[3].x =  hw; v[3].y = -hh; v[3].z = 0.0f;

    sceGumDrawArray(GU_TRIANGLE_STRIP, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 4, 0, v);
    sceGumPopMatrix();
}

void Renderer2D::DrawSpriteScreen(Texture* tex, float screenX, float screenY, float width, float height, unsigned int color) {
    if (!tex) return;

    tex->Bind(GU_LINEAR);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);

    float maxU = tex->GetMaxU();
    float maxV = tex->GetMaxV();

    Vertex2D* v = (Vertex2D*)sceGuGetMemory(4 * sizeof(Vertex2D));
    if (!v) return;

    v[0].u = 0.0f; v[0].v = 0.0f; v[0].color = color; v[0].x = screenX;         v[0].y = screenY;          v[0].z = 0.0f;
    v[1].u = maxU; v[1].v = 0.0f; v[1].color = color; v[1].x = screenX + width; v[1].y = screenY;          v[1].z = 0.0f;
    v[2].u = 0.0f; v[2].v = maxV; v[2].color = color; v[2].x = screenX;         v[2].y = screenY + height; v[2].z = 0.0f;
    v[3].u = maxU; v[3].v = maxV; v[3].color = color; v[3].x = screenX + width; v[3].y = screenY + height; v[3].z = 0.0f;

    sceGumDrawArray(GU_TRIANGLE_STRIP, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 4, 0, v);
}

void Renderer2D::DrawQuad(Texture* tex, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, unsigned int color) {
    if (!tex) return;

    tex->Bind(GU_LINEAR);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);

    Vertex2D* v = (Vertex2D*)sceGuGetMemory(4 * sizeof(Vertex2D));
    if (!v) return;

    v[0].u = u0; v[0].v = v0; v[0].color = color; v[0].x = x0; v[0].y = y0; v[0].z = 0.0f;
    v[1].u = u1; v[1].v = v0; v[1].color = color; v[1].x = x1; v[1].y = y0; v[1].z = 0.0f;
    v[2].u = u0; v[2].v = v1; v[2].color = color; v[2].x = x0; v[2].y = y1; v[2].z = 0.0f;
    v[3].u = u1; v[3].v = v1; v[3].color = color; v[3].x = x1; v[3].y = y1; v[3].z = 0.0f;

    sceGumDrawArray(GU_TRIANGLE_STRIP, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 4, 0, v);
}

void Renderer2D::DrawFilledRect(float x, float y, float w, float h, unsigned int color) {
    sceGuDisable(GU_TEXTURE_2D);

    ColorVertex2D* v = (ColorVertex2D*)sceGuGetMemory(4 * sizeof(ColorVertex2D));
    if (v) {
        v[0].color = color; v[0].x = x;     v[0].y = y;     v[0].z = 0.0f;
        v[1].color = color; v[1].x = x + w; v[1].y = y;     v[1].z = 0.0f;
        v[2].color = color; v[2].x = x;     v[2].y = y + h; v[2].z = 0.0f;
        v[3].color = color; v[3].x = x + w; v[3].y = y + h; v[3].z = 0.0f;

        sceGumDrawArray(GU_TRIANGLE_STRIP, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 4, 0, v);
    }

    sceGuEnable(GU_TEXTURE_2D);
}

void Renderer2D::DrawTerrainFill(Texture* groundTex, const float* pointsX, const float* pointsY, int count, float bottomY, unsigned int color) {
    if (!groundTex || count < 2) return;

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
}

void Renderer2D::DrawTerrainSurface(Texture* surfaceTex, const float* pointsX, const float* pointsY, int count, float thickness, unsigned int color) {
    if (!surfaceTex || count < 2) return;

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
}

#else // PC OpenGL Implementation

void Renderer2D::Init() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
}

void Renderer2D::SetupCamera(float camX, float camY, float zoom) {
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
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0f, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, 0.0f, -1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static inline void SetGLColor(unsigned int c) {
    glColor4ub(c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF, (c >> 24) & 0xFF);
}

void Renderer2D::DrawSprite(Texture* tex, float worldX, float worldY, float width, float height, float angle, unsigned int color) {
    if (!tex) return;

    tex->Bind(GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPushMatrix();
    glTranslatef(worldX, worldY, 0.0f);
    if (angle != 0.0f) {
        glRotatef(angle * (180.0f / 3.14159265f), 0.0f, 0.0f, 1.0f);
    }

    float hw = width * 0.5f;
    float hh = height * 0.5f;
    float maxU = tex->GetMaxU();
    float maxV = tex->GetMaxV();

    SetGLColor(color);
    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(-hw,  hh);
    glTexCoord2f(maxU, 0.0f); glVertex2f( hw,  hh);
    glTexCoord2f(0.0f, maxV); glVertex2f(-hw, -hh);
    glTexCoord2f(maxU, maxV); glVertex2f( hw, -hh);
    glEnd();

    glPopMatrix();
}

void Renderer2D::DrawSpriteScreen(Texture* tex, float screenX, float screenY, float width, float height, unsigned int color) {
    if (!tex) return;

    tex->Bind(GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    float maxU = tex->GetMaxU();
    float maxV = tex->GetMaxV();

    SetGLColor(color);
    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(screenX,         screenY);
    glTexCoord2f(maxU, 0.0f); glVertex2f(screenX + width, screenY);
    glTexCoord2f(0.0f, maxV); glVertex2f(screenX,         screenY + height);
    glTexCoord2f(maxU, maxV); glVertex2f(screenX + width, screenY + height);
    glEnd();
}

void Renderer2D::DrawQuad(Texture* tex, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, unsigned int color) {
    if (!tex) return;

    tex->Bind(GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    SetGLColor(color);
    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(u0, v0); glVertex2f(x0, y0);
    glTexCoord2f(u1, v0); glVertex2f(x1, y0);
    glTexCoord2f(u0, v1); glVertex2f(x0, y1);
    glTexCoord2f(u1, v1); glVertex2f(x1, y1);
    glEnd();
}

void Renderer2D::DrawFilledRect(float x, float y, float w, float h, unsigned int color) {
    glDisable(GL_TEXTURE_2D);
    SetGLColor(color);

    glBegin(GL_TRIANGLE_STRIP);
    glVertex2f(x,     y);
    glVertex2f(x + w, y);
    glVertex2f(x,     y + h);
    glVertex2f(x + w, y + h);
    glEnd();

    glEnable(GL_TEXTURE_2D);
}

void Renderer2D::DrawTerrainFill(Texture* groundTex, const float* pointsX, const float* pointsY, int count, float bottomY, unsigned int color) {
    if (!groundTex || count < 2) return;

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
}

void Renderer2D::DrawTerrainSurface(Texture* surfaceTex, const float* pointsX, const float* pointsY, int count, float thickness, unsigned int color) {
    if (!surfaceTex || count < 2) return;

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
}

#endif
