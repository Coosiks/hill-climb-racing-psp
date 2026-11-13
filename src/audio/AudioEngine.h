#ifndef HCR_AUDIO_ENGINE_H
#define HCR_AUDIO_ENGINE_H

#if defined(__PSP__)
#include <pspaudio.h>
#include <pspkernel.h>
#else
#include <SDL2/SDL.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum SoundEffect {
    SFX_COIN = 0,
    SFX_CRACK,
    SFX_REFUEL,
    SFX_CLICK,
    SFX_COUNT
};

struct SoundBuffer {
    short* samples;
    int sampleCount;
    int channels;
    int sampleRate;
};

class AudioEngine {
public:
    static bool Init();
    static void Shutdown();

    static void PlaySFX(SoundEffect sfx);
    static void UpdateEngineSound(float rev, bool active);

private:
    static SoundBuffer mSfxBuffers[SFX_COUNT];
    static SoundBuffer mEngineBuffer;

    static float mTargetPitch;
    static bool mEngineActive;
    static int mCurrentSfxToPlay;

    static bool LoadOgg(const char* filepath, SoundBuffer* outBuf);

#if defined(__PSP__)
    static int mChannelEngine;
    static int mChannelSFX;
    static int mAudioThreadId;
    static volatile bool mAudioRunning;
    static int AudioThread(SceSize args, void* argp);
#else
    static SDL_AudioDeviceID mAudioDevice;
    static void AudioCallback(void* userdata, Uint8* stream, int len);
#endif
};

#endif // HCR_AUDIO_ENGINE_H
