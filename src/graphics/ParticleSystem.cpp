#include "graphics/ParticleSystem.h"
#include "core/GameConstants.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

Particle ParticleSystem::sParticles[MAX_PARTICLES];
int ParticleSystem::sNextSlot = 0;
Texture ParticleSystem::sParticleTexture;
bool ParticleSystem::sInitialized = false;

static inline float RandFloat(float minVal, float maxVal) {
    float r = (float)rand() / (float)RAND_MAX;
    return minVal + r * (maxVal - minVal);
}

void ParticleSystem::Init() {
    Reset();
    if (!sInitialized) {
        sParticleTexture.LoadPNG("assets/Tire.png");
        sInitialized = true;
    }
}

void ParticleSystem::Reset() {
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        sParticles[i].active = false;
        sParticles[i].life = 0.0f;
    }
    sNextSlot = 0;
}

void ParticleSystem::SpawnParticle(float x, float y, float vx, float vy, float life,
                                  float startSize, float endSize, uint32_t color,
                                  ParticleType type) {
    if (life <= 0.0f) return;

    // Zero-alloc fixed capacity ring buffer / find inactive slot
    int slot = -1;
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        int idx = (sNextSlot + i) % MAX_PARTICLES;
        if (!sParticles[idx].active) {
            slot = idx;
            break;
        }
    }
    if (slot == -1) {
        // Pool is full: overwrite oldest circular slot (deterministic FIFO behavior)
        slot = sNextSlot;
    }
    sNextSlot = (slot + 1) % MAX_PARTICLES;

    Particle& p = sParticles[slot];
    p.active = true;
    p.type = type;
    p.x = x;
    p.y = y;
    p.vx = vx;
    p.vy = vy;
    p.life = life;
    p.maxLife = life;
    p.startSize = startSize;
    p.endSize = endSize;
    p.color = color;
}

void ParticleSystem::Update(float dt) {
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        Particle& p = sParticles[i];
        if (!p.active) continue;

        p.life -= dt;
        if (p.life <= 0.0f) {
            p.active = false;
            continue;
        }

        p.x += p.vx * dt;
        p.y += p.vy * dt;

        if (p.type == PARTICLE_DIRT) {
            // Gravity pulls dirt particles down
            p.vy -= 9.8f * dt;
        } else if (p.type == PARTICLE_EXHAUST) {
            // Smoke drifts upwards and decelerates horizontally
            p.vy += 1.5f * dt;
            p.vx *= 0.95f;
        }
    }
}

void ParticleSystem::Render() {
    // All active particles are drawn with sParticleTexture, thus packed into a single batch
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        const Particle& p = sParticles[i];
        if (!p.active) continue;

        float t = 1.0f - (p.life / p.maxLife); // 0.0 -> 1.0
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;

        float size = p.startSize + t * (p.endSize - p.startSize);

        // Alpha fade out to 0
        uint8_t origA = (uint8_t)((p.color >> 24) & 0xFF);
        uint8_t a = (uint8_t)((float)origA * (1.0f - t));
        uint32_t color = (p.color & 0x00FFFFFF) | ((uint32_t)a << 24);

        Renderer2D::DrawSprite(&sParticleTexture, p.x, p.y, size, size, 0.0f, color);
    }
}

void ParticleSystem::EmitWheelDirt(float wheelX, float wheelY, float wheelVx, float wheelVy, int stageType) {
    // Stage-specific dirt color (AABBGGRR in little-endian / RGBA8888)
    uint32_t dirtColor;
    switch (stageType) {
        case STAGE_DESERT:
            dirtColor = 0xD050A0D0; // Golden sand
            break;
        case STAGE_ARCTIC:
            dirtColor = 0xD0FFFFFF; // White snow
            break;
        case STAGE_MOON:
            dirtColor = 0xD0A0A0A0; // Pale grey lunar regolith
            break;
        case STAGE_HIGHWAY:
            dirtColor = 0xC0404040; // Dark tire smoke / asphalt grit
            break;
        case STAGE_CAVE:
            dirtColor = 0xD0203040; // Dark rocky subterranean dirt
            break;
        case STAGE_MARS:
            dirtColor = 0xD02040C0; // Crimson martian dust
            break;
        case STAGE_FOREST:
            dirtColor = 0xD0204030; // Dark forest loam / moss
            break;
        case STAGE_MOUNTAIN:
            dirtColor = 0xD0506070; // Alpine rock / slate grey
            break;
        case STAGE_MUDPOOL:
            dirtColor = 0xD0102030; // Dark viscous swamp mud
            break;
        case STAGE_VOLCANO:
            dirtColor = 0xD01020D0; // Glowing volcanic cinder / magma ember
            break;
        case STAGE_BEACH:
            dirtColor = 0xD060C0E0; // Warm sunlit dune sand
            break;
        case STAGE_ROLLERCOASTER:
            dirtColor = 0xD0803080; // Magenta / purple amusement sparks
            break;
        case STAGE_NIGHT:
            dirtColor = 0xD0202020; // Deep midnight gloom dust
            break;
        case STAGE_ROOFTOPS:
            dirtColor = 0xD0404050; // Urban brick / tar gravel
            break;
        case STAGE_JUNKYARD:
            dirtColor = 0xD0203030; // Corroded rust / scrap sludge
            break;
        case STAGE_CONSTRUCTION:
            dirtColor = 0xD0507090; // Concrete mortar / dust
            break;
        case STAGE_RAINBOW:
            dirtColor = 0xD0D090E0; // Shimmering pastel rainbow dust
            break;
        case STAGE_SEASONS:
            dirtColor = 0xD0306850; // Forest green / autumn earth
            break;
        case STAGE_BOOTCAMP:
            dirtColor = 0xD0405830; // Olive army drill dust
            break;
        case STAGE_XMAS:
            dirtColor = 0xD0F0F8FF; // Bright white snow dust
            break;
        case STAGE_ALIENPLANET:
            dirtColor = 0xD0903080; // Violet extraterrestrial dust
            break;
        case STAGE_ARCTICCAVE:
            dirtColor = 0xD0D0E8FF; // Ice cavern crystals
            break;
        case STAGE_RAGNAROK:
            dirtColor = 0xD02020A0; // Volcanic ash & embers
            break;
        case STAGE_FACTORY:
            dirtColor = 0xD0506070; // Industrial soot / steel dust
            break;
        case STAGE_HAUNTED:
            dirtColor = 0xD0201828; // Spooky cemetery soil / dark swamp mire
            break;
        case STAGE_NORTHPOLE:
            dirtColor = 0xD0F5FAFF; // Glacial blue-white powder snow
            break;
        case STAGE_NUCLEAR:
            dirtColor = 0xD0208030; // Irradiated radioactive toxic sludge
            break;
        case STAGE_COUNTRYSIDE:
        default:
            dirtColor = 0xD0306080; // Earth brown dirt
            break;
    }

    float vx = -wheelVx * 0.4f + RandFloat(-1.2f, 1.2f);
    float vy = RandFloat(1.0f, 3.5f);
    float life = RandFloat(0.30f, 0.60f);
    float startSize = RandFloat(0.12f, 0.20f);
    float endSize = RandFloat(0.25f, 0.40f);

    SpawnParticle(wheelX, wheelY, vx, vy, life, startSize, endSize, dirtColor, PARTICLE_DIRT);
}

void ParticleSystem::EmitExhaust(float chassisX, float chassisY, float chassisVx, float chassisVy) {
    float vx = chassisVx * 0.2f - RandFloat(0.5f, 1.5f);
    float vy = RandFloat(0.2f, 1.0f);
    float life = RandFloat(0.35f, 0.70f);
    float startSize = RandFloat(0.10f, 0.16f);
    float endSize = RandFloat(0.35f, 0.55f);
    uint32_t smokeColor = 0x80808080; // Translucent smoke grey

    SpawnParticle(chassisX, chassisY, vx, vy, life, startSize, endSize, smokeColor, PARTICLE_EXHAUST);
}

int ParticleSystem::GetActiveCount() {
    int count = 0;
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        if (sParticles[i].active) count++;
    }
    return count;
}

