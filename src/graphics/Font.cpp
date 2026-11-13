#include "Font.h"
#include <stdio.h>
#include <string.h>

Font::Font() : mLineHeight(64.0f), mBase(53.0f) {
    memset(mGlyphs, 0, sizeof(mGlyphs));
}

Font::~Font() {
}

bool Font::Load(const char* fntPath, const char* pngPath) {
    if (!mTexture.LoadPNG(pngPath)) {
        printf("Failed to load font texture: %s\n", pngPath);
        return false;
    }

    FILE* f = fopen(fntPath, "r");
    if (!f) {
        printf("Failed to open font descriptor: %s\n", fntPath);
        return false;
    }

    char line[256];
    float texW = (float)mTexture.GetTextureWidth();
    float texH = (float)mTexture.GetTextureHeight();
    if (texW <= 0.0f) texW = 512.0f;
    if (texH <= 0.0f) texH = 512.0f;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "common", 6) == 0) {
            int lh = 64, b = 53;
            sscanf(line, "common lineHeight=%d base=%d", &lh, &b);
            mLineHeight = (float)lh;
            mBase = (float)b;
        } else if (strncmp(line, "char id=", 8) == 0) {
            int id = 0, x = 0, y = 0, w = 0, h = 0, xo = 0, yo = 0, xa = 0;
            if (sscanf(line, "char id=%d x=%d y=%d width=%d height=%d xoffset=%d yoffset=%d xadvance=%d",
                       &id, &x, &y, &w, &h, &xo, &yo, &xa) >= 8) {
                if (id >= 0 && id < 256) {
                    mGlyphs[id].u0 = (float)x / texW;
                    mGlyphs[id].v0 = (float)y / texH;
                    mGlyphs[id].u1 = (float)(x + w) / texW;
                    mGlyphs[id].v1 = (float)(y + h) / texH;
                    mGlyphs[id].width = (float)w;
                    mGlyphs[id].height = (float)h;
                    mGlyphs[id].xoffset = (float)xo;
                    mGlyphs[id].yoffset = (float)yo;
                    mGlyphs[id].xadvance = (float)xa;
                    mGlyphs[id].valid = true;
                }
            }
        }
    }

    fclose(f);
    return true;
}

void Font::DrawText(const char* text, float startX, float startY, float scale, unsigned int color) {
    if (!text || mTexture.GetWidth() == 0) return;

    float curX = startX;
    float curY = startY;

    int len = strlen(text);
    for (int i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c == '\n') {
            curX = startX;
            curY += mLineHeight * scale;
            continue;
        }

        const FontGlyph& g = mGlyphs[c];
        if (!g.valid) {
            curX += 16.0f * scale;
            continue;
        }

        float gx = curX + g.xoffset * scale;
        float gy = curY + g.yoffset * scale;
        float gw = g.width * scale;
        float gh = g.height * scale;

        Renderer2D::DrawQuad(&mTexture, gx, gy, gx + gw, gy + gh, g.u0, g.v0, g.u1, g.v1, color);

        curX += g.xadvance * scale;
    }
}

float Font::GetTextWidth(const char* text, float scale) {
    if (!text) return 0.0f;

    float maxW = 0.0f;
    float curW = 0.0f;

    int len = strlen(text);
    for (int i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c == '\n') {
            if (curW > maxW) maxW = curW;
            curW = 0.0f;
            continue;
        }

        const FontGlyph& g = mGlyphs[c];
        if (g.valid) {
            curW += g.xadvance * scale;
        } else {
            curW += 16.0f * scale;
        }
    }

    if (curW > maxW) maxW = curW;
    return maxW;
}
