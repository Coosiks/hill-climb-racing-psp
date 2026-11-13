#ifndef HCR_TEXTURE_H
#define HCR_TEXTURE_H

#if defined(__PSP__)
#include <pspgu.h>
#include <pspkernel.h>
#else
#include <SDL2/SDL_opengl.h>
#ifndef GU_LINEAR
#define GU_LINEAR GL_LINEAR
#endif
#ifndef GU_NEAREST
#define GU_NEAREST GL_NEAREST
#endif
#ifndef GU_REPEAT
#define GU_REPEAT GL_REPEAT
#endif
#ifndef GU_CLAMP
#define GU_CLAMP GL_CLAMP_TO_EDGE
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

class Texture {
public:
    Texture();
    ~Texture();

    bool LoadPNG(const char* filepath);
    void Bind(int filter = GU_LINEAR);

    int GetWidth() const { return mWidth; }
    int GetHeight() const { return mHeight; }
    int GetTextureWidth() const { return mTextureWidth; }
    int GetTextureHeight() const { return mTextureHeight; }
    float GetMaxU() const { return mMaxU; }
    float GetMaxV() const { return mMaxV; }

    static int NextPowerOfTwo(int v);

private:
    int mWidth;          // Original image width
    int mHeight;         // Original image height
    int mTextureWidth;   // Power-of-two width
    int mTextureHeight;  // Power-of-two height
    float mMaxU;
    float mMaxV;

#if defined(__PSP__)
    unsigned int* mData; // 16-byte aligned 32-bit RGBA pixel data
#else
    GLuint mTexID;
#endif
};

#endif // HCR_TEXTURE_H
