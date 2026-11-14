#ifndef HCR_TEXTURE_H
#define HCR_TEXTURE_H

#if defined(__PSP__)
#include <pspgu.h>
#include <pspkernel.h>
#else
#include <SDL2/SDL_opengl.h>
#ifndef GU_PSM_5650
#define GU_PSM_5650 0
#endif
#ifndef GU_PSM_5551
#define GU_PSM_5551 1
#endif
#ifndef GU_PSM_4444
#define GU_PSM_4444 2
#endif
#ifndef GU_PSM_8888
#define GU_PSM_8888 3
#endif

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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma pack(push, 1)
struct PSPTexHeader {
    char magic[4];       // "PSPT"
    uint16_t orig_w;     // Original image width
    uint16_t orig_h;     // Original image height
    uint16_t pot_w;      // Padded power-of-two width (<= 512)
    uint16_t pot_h;      // Padded power-of-two height (<= 512)
    uint16_t format;     // GU_PSM: 0=5650, 1=5551, 2=4444, 3=8888
    uint16_t swizzled;   // 1 if swizzled for PSP GE, 0 if linear
};
#pragma pack(pop)

class Texture {
public:
    Texture();
    ~Texture();

    bool Load(const char* filepath);
    bool LoadPNG(const char* filepath); // Backwards-compatible wrapper calling Load()
    bool LoadTex(const char* filepath);
    void Bind(int filter = GU_LINEAR);

    int GetWidth() const { return mWidth; }
    int GetHeight() const { return mHeight; }
    int GetTextureWidth() const { return mTextureWidth; }
    int GetTextureHeight() const { return mTextureHeight; }
    float GetMaxU() const { return mMaxU; }
    float GetMaxV() const { return mMaxV; }
    int GetFormat() const { return mFormat; }
    bool IsSwizzled() const { return mSwizzled != 0; }
    size_t GetDataSize() const { return mDataSize; }
    const void* GetData() const {
#if defined(__PSP__)
        return mData;
#else
        return mRawBuffer;
#endif
    }

    static int NextPowerOfTwo(int v);
    static void Swizzle(const uint8_t* srcLinear, uint8_t* dstSwizzled, int width, int height, int bpp);
    static void Deswizzle(const uint8_t* srcSwizzled, uint8_t* dstLinear, int width, int height, int bpp);

private:
    int mWidth;          // Original image width
    int mHeight;         // Original image height
    int mTextureWidth;   // Power-of-two width
    int mTextureHeight;  // Power-of-two height
    float mMaxU;
    float mMaxV;
    int mFormat;         // GU_PSM_8888 or GU_PSM_5551
    int mSwizzled;       // 1 if swizzled, 0 if linear
    size_t mDataSize;    // Size of pixel buffer in bytes

#if defined(__PSP__)
    void* mData;         // 16-byte aligned pixel buffer for PSP GE
#else
    GLuint mTexID;
    void* mRawBuffer;    // Raw payload buffer retained for inspection
    bool LoadPNG_PC(const char* filepath);
#endif
};

#endif // HCR_TEXTURE_H
