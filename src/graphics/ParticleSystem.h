#ifndef HCR_PARTICLE_SYSTEM_H
#define HCR_PARTICLE_SYSTEM_H

#include <stdint.h>
#include "graphics/Renderer2D.h"
#include "graphics/Texture.h"

#define MAX_PARTICLES 128

enum ParticleType {
    PARTICLE_DIRT,
    PARTICLE_EXHAUST
};

struct Particle {
    bool active;
    ParticleType type;
    float x, y;
    float vx, vy;
    float life;
    float maxLife;
    float startSize;
    float endSize;
    uint32_t color; // AABBGGRR in little-endian format
};

class ParticleSystem {
public:
    static void Init();
    static void Reset();
    static void Update(float dt);
    static void Render(); // Batches all active particles into 1 draw call

    static void SpawnParticle(float x, float y, float vx, float vy, float life,
                              float startSize, float endSize, uint32_t color,
                              ParticleType type = PARTICLE_DIRT);

    static void EmitWheelDirt(float wheelX, float wheelY, float wheelVx, float wheelVy, int stageType);
    static void EmitExhaust(float chassisX, float chassisY, float chassisVx, float chassisVy);

    static int GetActiveCount();
    static int GetMaxCapacity() { return MAX_PARTICLES; }
    static Texture* GetParticleTexture() { return &sParticleTexture; }

private:
    static Particle sParticles[MAX_PARTICLES];
    static int sNextSlot;
    static Texture sParticleTexture;
    static bool sInitialized;
};

#endif // HCR_PARTICLE_SYSTEM_H

