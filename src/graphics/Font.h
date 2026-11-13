#ifndef HCR_FONT_H
#define HCR_FONT_H

#include "Texture.h"
#include "Renderer2D.h"

struct FontGlyph {
    float u0, v0, u1, v1;
    float width, height;
    float xoffset, yoffset;
    float xadvance;
    bool valid;
};

class Font {
public:
    Font();
    ~Font();

    bool Load(const char* fntPath, const char* pngPath);
    void DrawText(const char* text, float x, float y, float scale = 0.5f, unsigned int color = 0xFFFFFFFF);
    float GetTextWidth(const char* text, float scale = 0.5f);

private:
    Texture mTexture;
    FontGlyph mGlyphs[256];
    float mLineHeight;
    float mBase;
};

#endif // HCR_FONT_H

