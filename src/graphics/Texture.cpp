#include <malloc.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"
#include "Texture.h"

int Texture::NextPowerOfTwo(int v) {
    int p = 1;
    while (p < v) {
        p <<= 1;
    }
    return p;
}

Texture::Texture()
    : mWidth(0), mHeight(0), mTextureWidth(0), mTextureHeight(0),
      mMaxU(1.0f), mMaxV(1.0f)
#if defined(__PSP__)
      , mData(NULL)
#else
      , mTexID(0)
#endif
{
}

Texture::~Texture() {
#if defined(__PSP__)
    if (mData) {
        free(mData);
        mData = NULL;
    }
#else
    if (mTexID) {
        glDeleteTextures(1, &mTexID);
        mTexID = 0;
    }
#endif
}

bool Texture::LoadPNG(const char* filepath) {
#if defined(__PSP__)
    if (mData) {
        free(mData);
        mData = NULL;
    }
#else
    if (mTexID) {
        glDeleteTextures(1, &mTexID);
        mTexID = 0;
    }
#endif

    int channels = 0;
    unsigned char* raw = stbi_load(filepath, &mWidth, &mHeight, &channels, 4);
    if (!raw) {
        printf("Failed to load texture: %s\n", filepath);
        return false;
    }

    mTextureWidth = NextPowerOfTwo(mWidth);
    mTextureHeight = NextPowerOfTwo(mHeight);

    // Clamp dimensions to 512 for PSP compatibility
    if (mTextureWidth > 512) mTextureWidth = 512;
    if (mTextureHeight > 512) mTextureHeight = 512;

    mMaxU = (float)mWidth / (float)mTextureWidth;
    mMaxV = (float)mHeight / (float)mTextureHeight;

#if defined(__PSP__)
    unsigned int potSize = mTextureWidth * mTextureHeight * 4;
    mData = (unsigned int*)memalign(16, potSize);
    if (!mData) {
        stbi_image_free(raw);
        return false;
    }

    memset(mData, 0, potSize);

    int copyW = (mWidth < mTextureWidth) ? mWidth : mTextureWidth;
    int copyH = (mHeight < mTextureHeight) ? mHeight : mTextureHeight;
    for (int y = 0; y < copyH; ++y) {
        memcpy((unsigned char*)mData + (y * mTextureWidth * 4), raw + (y * mWidth * 4), copyW * 4);
    }
    stbi_image_free(raw);

    // Flush D-Cache so hardware GE sees texture
    sceKernelDcacheWritebackInvalidateAll();

#else // PC OpenGL texture creation
    unsigned int potSize = mTextureWidth * mTextureHeight * 4;
    unsigned char* potBuffer = (unsigned char*)calloc(1, potSize);
    if (!potBuffer) {
        stbi_image_free(raw);
        return false;
    }

    int copyW = (mWidth < mTextureWidth) ? mWidth : mTextureWidth;
    int copyH = (mHeight < mTextureHeight) ? mHeight : mTextureHeight;
    for (int y = 0; y < copyH; ++y) {
        memcpy(potBuffer + (y * mTextureWidth * 4), raw + (y * mWidth * 4), copyW * 4);
    }
    stbi_image_free(raw);

    glGenTextures(1, &mTexID);
    glBindTexture(GL_TEXTURE_2D, mTexID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mTextureWidth, mTextureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, potBuffer);
    free(potBuffer);

#endif

    return true;
}

void Texture::Bind(int filter) {
#if defined(__PSP__)
    if (!mData) return;
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
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
