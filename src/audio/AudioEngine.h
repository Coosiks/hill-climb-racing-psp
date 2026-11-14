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
    SFX_UPGRADE,
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
    static void UpdateEngineSound(float rev, bool active, float dt = 0.0166667f);

    static void PauseAll();
    static void ResumeAll();
    static bool IsPaused();

    static void SetMusicVolume(float vol);
    static void SetSFXVolume(float vol);
    static float GetMusicVolume();
    static float GetSFXVolume();

    static float GetEnginePitch();
    static float GetTargetPitch();
    static bool IsEngineActive();

    // Fast 16-bit PCM WAV loader (zero runtime decode overhead)
    static bool LoadWav(const char* filepath, SoundBuffer* outBuf);

    // Memory buffer WAV loader (useful for tests and embedded buffers)
    static bool LoadWavFromMemory(const void* data, size_t size, SoundBuffer* outBuf);

private:
    static SoundBuffer mSfxBuffers[SFX_COUNT];
    static SoundBuffer mEngineBuffer;

    static float mTargetPitch;
    static float mEnginePitch;
    static bool mEngineActive;
    static bool mPaused;
    static float mMusicVolume;
    static float mSFXVolume;
    static int mCurrentSfxToPlay;

    static bool LoadOgg(const char* filepath, SoundBuffer* outBuf);
    static bool LoadAudioFile(const char* baseNameWithoutExt, SoundBuffer* outBuf);

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
