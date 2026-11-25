#include "Texture.h"
#include <malloc.h>
#include <strings.h>

#if !defined(__PSP__)
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"
#endif

int Texture::NextPowerOfTwo(int v) {
    int p = 1;
    while (p < v) {
        p <<= 1;
    }
    return p;
}

void Texture::Swizzle(const uint8_t* srcLinear, uint8_t* dstSwizzled, int width, int height, int bpp) {
    if (!srcLinear || !dstSwizzled || width <= 0 || height <= 0 || bpp <= 0) return;

    int blockWidthPixels = 16 / bpp; // 4 for 32bpp (8888), 8 for 16bpp (5551)
    int blockHeight = 8;
    int numBlocksX = width / blockWidthPixels;
    int numBlocksY = height / blockHeight;
    int dstOffset = 0;

    for (int by = 0; by < numBlocksY; ++by) {
        for (int bx = 0; bx < numBlocksX; ++bx) {
            for (int y = 0; y < blockHeight; ++y) {
                int srcX = bx * blockWidthPixels;
                int srcY = by * blockHeight + y;
                int srcOffset = (srcY * width + srcX) * bpp;
                memcpy(dstSwizzled + dstOffset, srcLinear + srcOffset, 16);
                dstOffset += 16;
            }
        }
    }
}

void Texture::Deswizzle(const uint8_t* srcSwizzled, uint8_t* dstLinear, int width, int height, int bpp) {
    if (!srcSwizzled || !dstLinear || width <= 0 || height <= 0 || bpp <= 0) return;

    int blockWidthPixels = 16 / bpp;
    int blockHeight = 8;
    int numBlocksX = width / blockWidthPixels;
    int numBlocksY = height / blockHeight;
    int srcOffset = 0;

    for (int by = 0; by < numBlocksY; ++by) {
        for (int bx = 0; bx < numBlocksX; ++bx) {
            for (int y = 0; y < blockHeight; ++y) {
                int dstX = bx * blockWidthPixels;
                int dstY = by * blockHeight + y;
                int dstOffset = (dstY * width + dstX) * bpp;
                memcpy(dstLinear + dstOffset, srcSwizzled + srcOffset, 16);
                srcOffset += 16;
            }
        }
    }
}

Texture::Texture()
    : mWidth(0), mHeight(0), mTextureWidth(0), mTextureHeight(0),
      mMaxU(1.0f), mMaxV(1.0f), mFormat(GU_PSM_8888), mSwizzled(0), mDataSize(0)
#if defined(__PSP__)
      , mData(NULL)
#else
      , mTexID(0), mRawBuffer(NULL)
#endif
{
}

void Texture::Free() {
#if defined(__PSP__)
    if (mData) {
        free(mData);
        mData = NULL;
    }
#else
    if (mRawBuffer) {
        free(mRawBuffer);
        mRawBuffer = NULL;
    }
    if (mTexID) {
        glDeleteTextures(1, &mTexID);
        mTexID = 0;
    }
#endif
    mWidth = mHeight = mTextureWidth = mTextureHeight = 0;
    mMaxU = mMaxV = 1.0f;
    mDataSize = 0;
}

Texture::~Texture() {
    Free();
}

static void TileHorizontalRepeat(uint8_t* linearBuf, int origW, int potW, int potH, int bpp) {
    int srcBytes = origW * bpp;
    int rowBytes = potW * bpp;
    for (int y = 0; y < potH; ++y) {
        uint8_t* row = linearBuf + y * rowBytes;
        for (int x = origW; x < potW; x += origW) {
            memcpy(row + x * bpp, row, srcBytes);
        }
    }
}

bool Texture::LoadTex(const char* filepath) {
    if (!filepath) return false;
    Free();

    FILE* f = fopen(filepath, "rb");
    if (!f) {
        return false;
    }

    PSPTexHeader header;
    if (fread(&header, 1, sizeof(header), f) != sizeof(header)) {
        fclose(f);
        return false;
    }

    if (memcmp(header.magic, "PSPT", 4) != 0) {
        fclose(f);
        return false;
    }

    int bpp = 4;
    if (header.format == GU_PSM_8888) {
        bpp = 4;
    } else if (header.format == GU_PSM_5551 || header.format == GU_PSM_5650 || header.format == GU_PSM_4444) {
        bpp = 2;
    } else {
        fclose(f);
        return false;
    }

    if (header.pot_w == 0 || header.pot_h == 0 || header.pot_w > 512 || header.pot_h > 512) {
        fclose(f);
        return false;
    }

    size_t payloadSize = (size_t)header.pot_w * header.pot_h * bpp;
    bool shouldTileRepeat = (header.orig_w > 0 && header.orig_w < header.pot_w &&
                             (header.pot_w % header.orig_w) == 0 && header.orig_h == header.pot_h);

#if defined(__PSP__)
    if (mData) {
        free(mData);
        mData = NULL;
    }

    mData = memalign(16, payloadSize);
    if (!mData) {
        fclose(f);
        return false;
    }

    size_t bytesRead = fread(mData, 1, payloadSize, f);
    fclose(f);

    if (bytesRead != payloadSize) {
        free(mData);
        mData = NULL;
        return false;
    }

    if (shouldTileRepeat) {
        uint8_t* linearBuf = (uint8_t*)malloc(payloadSize);
        if (linearBuf) {
            if (header.swizzled) {
                Deswizzle((const uint8_t*)mData, linearBuf, header.pot_w, header.pot_h, bpp);
            } else {
                memcpy(linearBuf, mData, payloadSize);
            }
            TileHorizontalRepeat(linearBuf, header.orig_w, header.pot_w, header.pot_h, bpp);
            if (header.swizzled) {
                Swizzle(linearBuf, (uint8_t*)mData, header.pot_w, header.pot_h, bpp);
            } else {
                memcpy(mData, linearBuf, payloadSize);
            }
            free(linearBuf);
            header.orig_w = header.pot_w;
        }
    }

    sceKernelDcacheWritebackInvalidateAll();

#else
    if (mRawBuffer) {
        free(mRawBuffer);
        mRawBuffer = NULL;
    }
    if (mTexID) {
        glDeleteTextures(1, &mTexID);
        mTexID = 0;
    }

    mRawBuffer = malloc(payloadSize);
    if (!mRawBuffer) {
        fclose(f);
        return false;
    }

    size_t bytesRead = fread(mRawBuffer, 1, payloadSize, f);
    fclose(f);

    if (bytesRead != payloadSize) {
        free(mRawBuffer);
        mRawBuffer = NULL;
        return false;
    }

    // Build OpenGL texture for PC development/inspection
    uint8_t* linearBuf = (uint8_t*)malloc(payloadSize);
    if (linearBuf) {
        if (header.swizzled) {
            Deswizzle((const uint8_t*)mRawBuffer, linearBuf, header.pot_w, header.pot_h, bpp);
        } else {
            memcpy(linearBuf, mRawBuffer, payloadSize);
        }

        if (shouldTileRepeat) {
            TileHorizontalRepeat(linearBuf, header.orig_w, header.pot_w, header.pot_h, bpp);
            header.orig_w = header.pot_w;
        }

        glGenTextures(1, &mTexID);
        glBindTexture(GL_TEXTURE_2D, mTexID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        if (header.format == GU_PSM_8888) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, header.pot_w, header.pot_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, linearBuf);
        } else if (header.format == GU_PSM_5551) {
            uint8_t* rgba32 = (uint8_t*)malloc(header.pot_w * header.pot_h * 4);
            if (rgba32) {
                const uint16_t* src16 = (const uint16_t*)linearBuf;
                int totalPixels = header.pot_w * header.pot_h;
                for (int i = 0; i < totalPixels; ++i) {
                    uint16_t p = src16[i];
                    uint8_t r = ((p & 0x1F) * 255) / 31;
                    uint8_t g = (((p >> 5) & 0x1F) * 255) / 31;
                    uint8_t b = (((p >> 10) & 0x1F) * 255) / 31;
                    uint8_t a = (p & 0x8000) ? 255 : 0;
                    rgba32[i * 4 + 0] = r;
                    rgba32[i * 4 + 1] = g;
                    rgba32[i * 4 + 2] = b;
                    rgba32[i * 4 + 3] = a;
                }
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, header.pot_w, header.pot_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba32);
                free(rgba32);
            }
        }
        free(linearBuf);
    }
#endif

    mWidth = header.orig_w;
    mHeight = header.orig_h;
    mTextureWidth = header.pot_w;
    mTextureHeight = header.pot_h;
    mMaxU = (mTextureWidth > 0) ? (float)mWidth / (float)mTextureWidth : 1.0f;
    mMaxV = (mTextureHeight > 0) ? (float)mHeight / (float)mTextureHeight : 1.0f;
    mFormat = header.format;
    mSwizzled = header.swizzled;
    mDataSize = payloadSize;

    return true;
}

#if !defined(__PSP__)
bool Texture::LoadPNG_PC(const char* filepath) {
    if (mTexID) {
        glDeleteTextures(1, &mTexID);
        mTexID = 0;
    }
    if (mRawBuffer) {
        free(mRawBuffer);
        mRawBuffer = NULL;
    }

    int channels = 0;
    unsigned char* raw = stbi_load(filepath, &mWidth, &mHeight, &channels, 4);
    if (!raw) {
        return false;
    }

    int srcW = mWidth;
    int srcH = mHeight;
    int dstW = (srcW > 512) ? 512 : srcW;
    int dstH = (srcH > 512) ? 512 : srcH;

    mWidth = dstW;
    mHeight = dstH;
    mTextureWidth = NextPowerOfTwo(dstW);
    mTextureHeight = NextPowerOfTwo(dstH);
    if (mTextureWidth > 512) mTextureWidth = 512;
    if (mTextureHeight > 512) mTextureHeight = 512;

    mMaxU = (float)dstW / (float)mTextureWidth;
    mMaxV = (float)dstH / (float)mTextureHeight;
    mFormat = GU_PSM_8888;
    mSwizzled = 0;

    unsigned int potSize = mTextureWidth * mTextureHeight * 4;
    mDataSize = potSize;
    unsigned char* potBuffer = (unsigned char*)calloc(1, potSize);
    if (!potBuffer) {
        stbi_image_free(raw);
        return false;
    }

    if (dstW == srcW && dstH == srcH) {
        for (int y = 0; y < dstH; ++y) {
            memcpy(potBuffer + (y * mTextureWidth * 4), raw + (y * srcW * 4), dstW * 4);
        }
    } else {
        // Bilinear/nearest box downscale so >512px textures fit within PSP 512px hardware limit without cropping
        for (int y = 0; y < dstH; ++y) {
            int sy0 = (y * srcH) / dstH;
            int sy1 = ((y + 1) * srcH) / dstH;
            if (sy1 <= sy0) sy1 = sy0 + 1;
            if (sy1 > srcH) sy1 = srcH;
            for (int x = 0; x < dstW; ++x) {
                int sx0 = (x * srcW) / dstW;
                int sx1 = ((x + 1) * srcW) / dstW;
                if (sx1 <= sx0) sx1 = sx0 + 1;
                if (sx1 > srcW) sx1 = srcW;
                unsigned int r = 0, g = 0, b = 0, a = 0, count = 0;
                for (int sy = sy0; sy < sy1; ++sy) {
                    for (int sx = sx0; sx < sx1; ++sx) {
                        const unsigned char* sp = raw + (sy * srcW + sx) * 4;
                        r += sp[0]; g += sp[1]; b += sp[2]; a += sp[3];
                        count++;
                    }
                }
                unsigned char* dp = potBuffer + (y * mTextureWidth + x) * 4;
                dp[0] = (unsigned char)(r / count);
                dp[1] = (unsigned char)(g / count);
                dp[2] = (unsigned char)(b / count);
                dp[3] = (unsigned char)(a / count);
            }
        }
    }
    stbi_image_free(raw);

    glGenTextures(1, &mTexID);
    glBindTexture(GL_TEXTURE_2D, mTexID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mTextureWidth, mTextureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, potBuffer);

    mRawBuffer = potBuffer;
    return true;
}
#endif

bool Texture::Load(const char* filepath) {
    if (!filepath) return false;

#if defined(__PSP__)
    // On PSP, transparently map .png extension to .tex
    char texPath[256];
    strncpy(texPath, filepath, sizeof(texPath) - 1);
    texPath[sizeof(texPath) - 1] = '\0';

    size_t len = strlen(texPath);
    if (len >= 4 && strcasecmp(texPath + len - 4, ".png") == 0) {
        strcpy(texPath + len - 4, ".tex");
    }

    if (LoadTex(texPath)) {
        return true;
    }
    if (strcmp(texPath, filepath) != 0 && LoadTex(filepath)) {
        return true;
    }

    printf("Failed to load texture: %s\n", texPath);
    return false;

#else
    // On PC:
    size_t len = strlen(filepath);
    if (len >= 4 && strcasecmp(filepath + len - 4, ".tex") == 0) {
        return LoadTex(filepath);
    }

    // Try loading PNG if it exists
    FILE* fp = fopen(filepath, "rb");
    if (fp) {
        fclose(fp);
        return LoadPNG_PC(filepath);
    }

    // If PNG is missing, try corresponding .tex file
    char texPath[256];
    strncpy(texPath, filepath, sizeof(texPath) - 1);
    texPath[sizeof(texPath) - 1] = '\0';
    if (len >= 4 && strcasecmp(texPath + len - 4, ".png") == 0) {
        strcpy(texPath + len - 4, ".tex");
        if (LoadTex(texPath)) {
            return true;
        }
    }

    printf("Failed to load texture: %s\n", filepath);
    return false;
#endif
}

bool Texture::LoadPNG(const char* filepath) {
    return Load(filepath);
}

void Texture::Bind(int filter) {
#if defined(__PSP__)
    if (!mData) return;
    sceGuTexMode(mFormat, 0, 0, mSwizzled ? 1 : 0);
    sceGuTexImage(0, mTextureWidth, mTextureHeight, mTextureWidth, mData);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(filter, filter);
#else
    if (!mTexID) return;
    glBindTexture(GL_TEXTURE_2D, mTexID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
#endif
}
