#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <assert.h>
#include "graphics/Texture.h"

static int gFailedTests = 0;

#define ASSERT_TRUE(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("  [FAIL] %s (line %d): %s\n", __FUNCTION__, __LINE__, msg); \
            gFailedTests++; \
            return; \
        } \
    } while (0)

#define ASSERT_EQ(a, b, msg) \
    do { \
        if ((a) != (b)) { \
            printf("  [FAIL] %s (line %d): %s (expected %lld, got %lld)\n", \
                   __FUNCTION__, __LINE__, msg, (long long)(b), (long long)(a)); \
            gFailedTests++; \
            return; \
        } \
    } while (0)

static void TestHeaderPacking() {
    printf("Running TestHeaderPacking...\n");
    ASSERT_EQ(sizeof(PSPTexHeader), 16, "PSPTexHeader struct must be exactly 16 bytes");
    printf("  [PASS] Header packing verified (16 bytes)\n");
}

static void TestSwizzleDeswizzleRoundtrip8888() {
    printf("Running TestSwizzleDeswizzleRoundtrip8888...\n");
    const int w = 64;
    const int h = 32;
    const int bpp = 4;
    const size_t totalBytes = w * h * bpp;

    uint8_t* linear = (uint8_t*)malloc(totalBytes);
    uint8_t* swizzled = (uint8_t*)malloc(totalBytes);
    uint8_t* restored = (uint8_t*)malloc(totalBytes);
    ASSERT_TRUE(linear && swizzled && restored, "Memory allocation failed");

    // Populate with unique test pattern
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int idx = (y * w + x) * bpp;
            linear[idx + 0] = (uint8_t)((x * 17 + y * 31) & 0xFF);
            linear[idx + 1] = (uint8_t)((x * 53 + y * 11) & 0xFF);
            linear[idx + 2] = (uint8_t)((x ^ y ^ 0xA5) & 0xFF);
            linear[idx + 3] = (uint8_t)((x + y) & 0xFF);
        }
    }

    Texture::Swizzle(linear, swizzled, w, h, bpp);

    // Swizzled buffer must rearrange the linear memory layout
    ASSERT_TRUE(memcmp(linear, swizzled, totalBytes) != 0, "Swizzled data must differ from linear layout");

    Texture::Deswizzle(swizzled, restored, w, h, bpp);

    // Roundtrip must be 100% pixel-exact
    ASSERT_TRUE(memcmp(linear, restored, totalBytes) == 0, "Deswizzle roundtrip must be pixel-exact (RGBA8888)");

    free(linear);
    free(swizzled);
    free(restored);
    printf("  [PASS] Swizzle/Deswizzle roundtrip RGBA8888 pixel-exact (64x32)\n");
}

static void TestSwizzleDeswizzleRoundtrip5551() {
    printf("Running TestSwizzleDeswizzleRoundtrip5551...\n");
    const int w = 128;
    const int h = 64;
    const int bpp = 2;
    const size_t totalBytes = w * h * bpp;

    uint8_t* linear = (uint8_t*)malloc(totalBytes);
    uint8_t* swizzled = (uint8_t*)malloc(totalBytes);
    uint8_t* restored = (uint8_t*)malloc(totalBytes);
    ASSERT_TRUE(linear && swizzled && restored, "Memory allocation failed");

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int idx = (y * w + x) * bpp;
            uint16_t val = (uint16_t)(((x * 7 + y * 13) & 0x1F) |
                                     (((x ^ y) & 0x1F) << 5) |
                                     (((x + y * 3) & 0x1F) << 10) |
                                     ((x > y ? 1 : 0) << 15));
            linear[idx + 0] = (uint8_t)(val & 0xFF);
            linear[idx + 1] = (uint8_t)((val >> 8) & 0xFF);
        }
    }

    Texture::Swizzle(linear, swizzled, w, h, bpp);
    ASSERT_TRUE(memcmp(linear, swizzled, totalBytes) != 0, "Swizzled 16-bit data must differ from linear");

    Texture::Deswizzle(swizzled, restored, w, h, bpp);
    ASSERT_TRUE(memcmp(linear, restored, totalBytes) == 0, "Deswizzle roundtrip must be pixel-exact (RGBA5551)");

    free(linear);
    free(swizzled);
    free(restored);
    printf("  [PASS] Swizzle/Deswizzle roundtrip RGBA5551 pixel-exact (128x64)\n");
}

static void TestSyntheticTex8888() {
    printf("Running TestSyntheticTex8888...\n");
    const char* testFile = "tools/test_synth_8888.tex";
    const uint16_t origW = 200;
    const uint16_t origH = 100;
    const uint16_t potW = 256;
    const uint16_t potH = 128;
    const uint16_t format = GU_PSM_8888;
    const uint16_t swizzled = 1;
    const size_t payloadSize = (size_t)potW * potH * 4;

    PSPTexHeader header;
    memcpy(header.magic, "PSPT", 4);
    header.orig_w = origW;
    header.orig_h = origH;
    header.pot_w = potW;
    header.pot_h = potH;
    header.format = format;
    header.swizzled = swizzled;

    uint8_t* payload = (uint8_t*)calloc(1, payloadSize);
    ASSERT_TRUE(payload != NULL, "calloc failed");

    FILE* f = fopen(testFile, "wb");
    ASSERT_TRUE(f != NULL, "Cannot open synthetic test file for writing");
    fwrite(&header, 1, sizeof(header), f);
    fwrite(payload, 1, payloadSize, f);
    fclose(f);
    free(payload);

    Texture tex;
    bool loaded = tex.LoadTex(testFile);
    remove(testFile);

    ASSERT_TRUE(loaded, "Failed to load valid synthetic 8888 .tex file");
    ASSERT_EQ(tex.GetWidth(), origW, "Original width mismatch");
    ASSERT_EQ(tex.GetHeight(), origH, "Original height mismatch");
    ASSERT_EQ(tex.GetTextureWidth(), potW, "POT width mismatch");
    ASSERT_EQ(tex.GetTextureHeight(), potH, "POT height mismatch");
    ASSERT_EQ(tex.GetFormat(), format, "Format mismatch");
    ASSERT_TRUE(tex.IsSwizzled(), "Swizzled flag mismatch");
    ASSERT_EQ(tex.GetDataSize(), payloadSize, "Payload size mismatch");
    ASSERT_TRUE(fabsf(tex.GetMaxU() - ((float)origW / (float)potW)) < 0.0001f, "MaxU mismatch");
    ASSERT_TRUE(fabsf(tex.GetMaxV() - ((float)origH / (float)potH)) < 0.0001f, "MaxV mismatch");

    printf("  [PASS] Synthetic 8888 .tex loaded and validated\n");
}

static void TestSyntheticTex5551() {
    printf("Running TestSyntheticTex5551...\n");
    const char* testFile = "tools/test_synth_5551.tex";
    const uint16_t origW = 480;
    const uint16_t origH = 272;
    const uint16_t potW = 512;
    const uint16_t potH = 512;
    const uint16_t format = GU_PSM_5551;
    const uint16_t swizzled = 1;
    const size_t payloadSize = (size_t)potW * potH * 2;

    PSPTexHeader header;
    memcpy(header.magic, "PSPT", 4);
    header.orig_w = origW;
    header.orig_h = origH;
    header.pot_w = potW;
    header.pot_h = potH;
    header.format = format;
    header.swizzled = swizzled;

    uint8_t* payload = (uint8_t*)calloc(1, payloadSize);
    ASSERT_TRUE(payload != NULL, "calloc failed");

    FILE* f = fopen(testFile, "wb");
    ASSERT_TRUE(f != NULL, "Cannot open synthetic test file for writing");
    fwrite(&header, 1, sizeof(header), f);
    fwrite(payload, 1, payloadSize, f);
    fclose(f);
    free(payload);

    Texture tex;
    bool loaded = tex.LoadTex(testFile);
    remove(testFile);

    ASSERT_TRUE(loaded, "Failed to load valid synthetic 5551 .tex file");
    ASSERT_EQ(tex.GetWidth(), origW, "Original width mismatch");
    ASSERT_EQ(tex.GetHeight(), origH, "Original height mismatch");
    ASSERT_EQ(tex.GetTextureWidth(), potW, "POT width mismatch");
    ASSERT_EQ(tex.GetTextureHeight(), potH, "POT height mismatch");
    ASSERT_EQ(tex.GetFormat(), format, "Format mismatch");
    ASSERT_TRUE(tex.IsSwizzled(), "Swizzled flag mismatch");
    ASSERT_EQ(tex.GetDataSize(), payloadSize, "Payload size mismatch (512x512x2 = 524288 B)");

    printf("  [PASS] Synthetic 5551 .tex loaded and validated\n");
}

static void TestRejectionInvalidHeaders() {
    printf("Running TestRejectionInvalidHeaders...\n");
    Texture tex;

    // 1. Nonexistent file
    ASSERT_TRUE(!tex.LoadTex("nonexistent_file_xyz.tex"), "Must reject non-existent file");

    // 2. Corrupt magic
    const char* badFile = "tools/test_bad_magic.tex";
    PSPTexHeader badH;
    memcpy(badH.magic, "NOPE", 4);
    badH.orig_w = 64; badH.orig_h = 64;
    badH.pot_w = 64; badH.pot_h = 64;
    badH.format = GU_PSM_8888; badH.swizzled = 0;

    FILE* f = fopen(badFile, "wb");
    ASSERT_TRUE(f != NULL, "fopen failed");
    fwrite(&badH, 1, sizeof(badH), f);
    fclose(f);

    ASSERT_TRUE(!tex.LoadTex(badFile), "Must reject invalid magic");
    remove(badFile);

    // 3. Oversized texture (>512)
    const char* overFile = "tools/test_oversize.tex";
    badH.magic[0] = 'P'; badH.magic[1] = 'S'; badH.magic[2] = 'P'; badH.magic[3] = 'T';
    badH.pot_w = 1024; badH.pot_h = 1024;
    f = fopen(overFile, "wb");
    ASSERT_TRUE(f != NULL, "fopen failed");
    fwrite(&badH, 1, sizeof(badH), f);
    fclose(f);

    ASSERT_TRUE(!tex.LoadTex(overFile), "Must reject POT dimension > 512");
    remove(overFile);

    // 4. Truncated payload
    const char* truncFile = "tools/test_trunc.tex";
    badH.pot_w = 64; badH.pot_h = 64;
    f = fopen(truncFile, "wb");
    ASSERT_TRUE(f != NULL, "fopen failed");
    fwrite(&badH, 1, sizeof(badH), f);
    // Write only 10 bytes instead of 64*64*4 = 16384 bytes
    uint8_t dummy[10] = {0};
    fwrite(dummy, 1, sizeof(dummy), f);
    fclose(f);

    ASSERT_TRUE(!tex.LoadTex(truncFile), "Must reject truncated payload");
    remove(truncFile);

    printf("  [PASS] Invalid headers rejected cleanly\n");
}

static void TestExistingAssetsIfPresent() {
    printf("Running TestExistingAssetsIfPresent...\n");
    FILE* f = fopen("assets/Car.tex", "rb");
    if (!f) {
        printf("  [SKIP] assets/Car.tex not found on disk (clean repository)\n");
        return;
    }
    fclose(f);

    Texture carTex;
    bool carLoaded = carTex.LoadTex("assets/Car.tex");
    ASSERT_TRUE(carLoaded, "Failed to load assets/Car.tex");
    ASSERT_EQ(carTex.GetWidth(), 256, "Car.tex orig width");
    ASSERT_EQ(carTex.GetHeight(), 128, "Car.tex orig height");
    ASSERT_EQ(carTex.GetFormat(), GU_PSM_8888, "Car.tex format must be RGBA8888");
    ASSERT_TRUE(carTex.IsSwizzled(), "Car.tex must be swizzled");
    ASSERT_EQ(carTex.GetDataSize(), 256 * 128 * 4, "Car.tex payload size (131072 B)");

    Texture groundTex;
    bool groundLoaded = groundTex.LoadTex("assets/terrain-ground.tex");
    ASSERT_TRUE(groundLoaded, "Failed to load assets/terrain-ground.tex");
    ASSERT_EQ(groundTex.GetWidth(), 512, "terrain-ground.tex orig width");
    ASSERT_EQ(groundTex.GetHeight(), 512, "terrain-ground.tex orig height");
    ASSERT_EQ(groundTex.GetFormat(), GU_PSM_5551, "terrain-ground.tex format must be RGBA5551 (auto mode)");
    ASSERT_TRUE(groundTex.IsSwizzled(), "terrain-ground.tex must be swizzled");
    ASSERT_EQ(groundTex.GetDataSize(), 512 * 512 * 2, "terrain-ground.tex payload size (524288 B)");

    printf("  [PASS] Converted assets on disk validated (Car.tex & terrain-ground.tex)\n");
}

int main() {
    printf("==================================================\n");
    printf("   Texture Pipeline & Swizzling Test (T-111)\n");
    printf("==================================================\n");

    TestHeaderPacking();
    TestSwizzleDeswizzleRoundtrip8888();
    TestSwizzleDeswizzleRoundtrip5551();
    TestSyntheticTex8888();
    TestSyntheticTex5551();
    TestRejectionInvalidHeaders();
    TestExistingAssetsIfPresent();

    if (gFailedTests > 0) {
        printf("\nFAILED: %d texture tests failed!\n", gFailedTests);
        return 1;
    }

    printf("\nALL TEXTURE TESTS PASSED!\n");
    return 0;
}
