#include "AudioEngine.h"
#include <math.h>

#if defined(__PSP__)
#include "core/fpu.h"
#else
// Include stb_vorbis implementation only on non-PSP targets for fallback
#include "stb_vorbis.c"
#endif

SoundBuffer AudioEngine::mSfxBuffers[SFX_COUNT] = { {0} };
SoundBuffer AudioEngine::mEngineBuffer = { 0 };
float AudioEngine::mTargetPitch = 1.0f;
float AudioEngine::mEnginePitch = 1.0f;
bool AudioEngine::mEngineActive = false;
bool AudioEngine::mPaused = false;
float AudioEngine::mMusicVolume = 1.0f;
float AudioEngine::mSFXVolume = 1.0f;
int AudioEngine::mCurrentSfxToPlay = -1;

#if defined(__PSP__)
int AudioEngine::mChannelEngine = -1;
int AudioEngine::mChannelSFX = -1;
int AudioEngine::mAudioThreadId = -1;
volatile bool AudioEngine::mAudioRunning = false;
#define AUDIO_BUF_SAMPLES 512
#else
SDL_AudioDeviceID AudioEngine::mAudioDevice = 0;
#endif

bool AudioEngine::LoadWavFromMemory(const void* data, size_t size, SoundBuffer* outBuf) {
    if (!data || size < 44 || !outBuf) return false;
    const unsigned char* p = (const unsigned char*)data;
    if (memcmp(p, "RIFF", 4) != 0 || memcmp(p + 8, "WAVE", 4) != 0) {
        return false;
    }

    size_t offset = 12;
    int channels = 0;
    int sampleRate = 0;
    int bitsPerSample = 0;
    const unsigned char* pcmData = NULL;
    size_t dataLen = 0;

    while (offset + 8 <= size) {
        char chunkId[5] = {0};
        memcpy(chunkId, p + offset, 4);
        unsigned int chunkSize = *(const unsigned int*)(p + offset + 4);
        offset += 8;

        if (strcmp(chunkId, "fmt ") == 0 && chunkSize >= 16) {
            unsigned short format = *(const unsigned short*)(p + offset);
            if (format != 1) return false; // Uncompressed PCM only
            channels = *(const unsigned short*)(p + offset + 2);
            sampleRate = *(const unsigned int*)(p + offset + 4);
            bitsPerSample = *(const unsigned short*)(p + offset + 14);
        } else if (strcmp(chunkId, "data") == 0) {
            pcmData = p + offset;
            dataLen = chunkSize;
            if (offset + dataLen > size) dataLen = size - offset;
            break;
        }
        offset += chunkSize;
    }

    if (!pcmData || channels <= 0 || sampleRate <= 0 || bitsPerSample != 16 || dataLen == 0) {
        return false;
    }

    short* samples = (short*)malloc(dataLen);
    if (!samples) return false;
    memcpy(samples, pcmData, dataLen);

    outBuf->samples = samples;
    outBuf->channels = channels;
    outBuf->sampleRate = sampleRate;
    outBuf->sampleCount = (int)(dataLen / (channels * sizeof(short)));
    return true;
}

bool AudioEngine::LoadWav(const char* filepath, SoundBuffer* outBuf) {
    if (!filepath || !outBuf) return false;
    FILE* f = fopen(filepath, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz < 44) {
        fclose(f);
        return false;
    }

    void* buf = malloc(sz);
    if (!buf) {
        fclose(f);
        return false;
    }

    size_t readBytes = fread(buf, 1, sz, f);
    fclose(f);

    bool res = LoadWavFromMemory(buf, readBytes, outBuf);
    free(buf);
    return res;
}

#if !defined(__PSP__)
bool AudioEngine::LoadOgg(const char* filepath, SoundBuffer* outBuf) {
    if (!outBuf) return false;

    short* decoded = NULL;
    int channels = 0;
    int sampleRate = 0;
    int sampleCount = stb_vorbis_decode_filename(filepath, &channels, &sampleRate, &decoded);

    if (sampleCount <= 0 || !decoded) {
        return false;
    }

    outBuf->samples = decoded;
    outBuf->sampleCount = sampleCount;
    outBuf->channels = channels;
    outBuf->sampleRate = sampleRate;
    return true;
}
#else
bool AudioEngine::LoadOgg(const char* filepath, SoundBuffer* outBuf) {
    (void)filepath;
    (void)outBuf;
    return false;
}
#endif

bool AudioEngine::LoadAudioFile(const char* baseNameWithoutExt, SoundBuffer* outBuf) {
    char pathWav[256];
    snprintf(pathWav, sizeof(pathWav), "%s.wav", baseNameWithoutExt);
    if (LoadWav(pathWav, outBuf)) {
        return true;
    }

    char pathOgg[256];
    snprintf(pathOgg, sizeof(pathOgg), "%s.ogg", baseNameWithoutExt);
    return LoadOgg(pathOgg, outBuf);
}

#if defined(__PSP__)

int AudioEngine::AudioThread(SceSize args, void* argp) {
    fpu_no_exceptions();

    int ch = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, AUDIO_BUF_SAMPLES, PSP_AUDIO_FORMAT_STEREO);
    if (ch < 0) return 0;
    mChannelEngine = ch;

    static short __attribute__((aligned(64))) mixbuf[2][AUDIO_BUF_SAMPLES * 2];
    int cur = 0;

    unsigned int enginePosFP = 0;
    int sfxCurrentIndex = -1;
    int sfxSamplePos = 0;

    while (mAudioRunning) {
        short* outputBuffer = mixbuf[cur];
        memset(outputBuffer, 0, AUDIO_BUF_SAMPLES * 2 * sizeof(short));

        if (!mPaused) {
            // Check for new SFX request
            if (mCurrentSfxToPlay >= 0 && mCurrentSfxToPlay < SFX_COUNT) {
                sfxCurrentIndex = mCurrentSfxToPlay;
                sfxSamplePos = 0;
                mCurrentSfxToPlay = -1;
            }

            // 1. Synthesize / Resample Engine Audio Loop
            if (mEngineActive && mEngineBuffer.samples && mEngineBuffer.sampleCount > 0) {
                float pitch = mEnginePitch;
                if (pitch < 0.50f) pitch = 0.50f;
                if (pitch > 2.00f) pitch = 2.00f;

                float rateRatio = (mEngineBuffer.sampleRate > 0) ? ((float)mEngineBuffer.sampleRate / 44100.0f) : 1.0f;
                unsigned int stepFP = (unsigned int)(pitch * rateRatio * 65536.0f);
                unsigned int maxPosFP = (unsigned int)(mEngineBuffer.sampleCount) << 16;
                short* engSamples = mEngineBuffer.samples;
                int engChannels = mEngineBuffer.channels;
                int volFactor = (int)(mMusicVolume * 180.0f);

                for (int i = 0; i < AUDIO_BUF_SAMPLES; ++i) {
                    int sIdx = (enginePosFP >> 16);
                    if (sIdx >= mEngineBuffer.sampleCount) sIdx = mEngineBuffer.sampleCount - 1;
                    short sample = (engChannels == 1) ? engSamples[sIdx] : engSamples[sIdx * 2];

                    sample = (short)((sample * volFactor) >> 8);

                    outputBuffer[i * 2 + 0] = sample;
                    outputBuffer[i * 2 + 1] = sample;

                    enginePosFP += stepFP;
                    if (enginePosFP >= maxPosFP) {
                        enginePosFP -= maxPosFP;
                    }
                }
            }

            // 2. Mix Active SFX
            if (sfxCurrentIndex >= 0 && sfxCurrentIndex < SFX_COUNT) {
                SoundBuffer& sfx = mSfxBuffers[sfxCurrentIndex];
                if (sfx.samples && sfxSamplePos < sfx.sampleCount) {
                    int sfxVol = (int)(mSFXVolume * 256.0f);
                    for (int i = 0; i < AUDIO_BUF_SAMPLES; ++i) {
                        if (sfxSamplePos >= sfx.sampleCount) {
                            sfxCurrentIndex = -1;
                            break;
                        }

                        short sfxSample = (sfx.channels == 1) ? sfx.samples[sfxSamplePos] : sfx.samples[sfxSamplePos * 2];
                        sfxSample = (short)((sfxSample * sfxVol) >> 8);

                        int left = outputBuffer[i * 2 + 0] + sfxSample;
                        int right = outputBuffer[i * 2 + 1] + sfxSample;

                        if (left > 32767) left = 32767;
                        if (left < -32768) left = -32768;
                        if (right > 32767) right = 32767;
                        if (right < -32768) right = -32768;

                        outputBuffer[i * 2 + 0] = (short)left;
                        outputBuffer[i * 2 + 1] = (short)right;

                        sfxSamplePos++;
                    }
                } else {
                    sfxCurrentIndex = -1;
                }
            }
        }

        sceAudioOutputPannedBlocking(mChannelEngine, PSP_AUDIO_VOLUME_MAX, PSP_AUDIO_VOLUME_MAX, outputBuffer);
        cur ^= 1;
    }

    if (mChannelEngine >= 0) {
        sceAudioChRelease(mChannelEngine);
        mChannelEngine = -1;
    }
    return 0;
}

bool AudioEngine::Init() {
    mEngineActive = false;
    mPaused = false;
    mEnginePitch = 1.0f;
    mTargetPitch = 1.0f;
    mMusicVolume = 1.0f;
    mSFXVolume = 1.0f;

    LoadAudioFile("assets/audio/engine",  &mEngineBuffer);
    LoadAudioFile("assets/audio/coin",    &mSfxBuffers[SFX_COIN]);
    LoadAudioFile("assets/audio/crack",   &mSfxBuffers[SFX_CRACK]);
    LoadAudioFile("assets/audio/refuel",  &mSfxBuffers[SFX_REFUEL]);
    LoadAudioFile("assets/audio/click",   &mSfxBuffers[SFX_CLICK]);
    if (!LoadAudioFile("assets/audio/upgrade", &mSfxBuffers[SFX_UPGRADE])) {
        // Fallback to click sound for upgrade
        if (mSfxBuffers[SFX_CLICK].samples && mSfxBuffers[SFX_CLICK].sampleCount > 0) {
            SoundBuffer& click = mSfxBuffers[SFX_CLICK];
            size_t bytes = click.sampleCount * click.channels * sizeof(short);
            mSfxBuffers[SFX_UPGRADE].samples = (short*)malloc(bytes);
            if (mSfxBuffers[SFX_UPGRADE].samples) {
                memcpy(mSfxBuffers[SFX_UPGRADE].samples, click.samples, bytes);
                mSfxBuffers[SFX_UPGRADE].sampleCount = click.sampleCount;
                mSfxBuffers[SFX_UPGRADE].channels = click.channels;
                mSfxBuffers[SFX_UPGRADE].sampleRate = click.sampleRate;
            }
        }
    }

    mAudioRunning = true;
    mAudioThreadId = sceKernelCreateThread("hcr_audio", AudioThread, 0x22, 0x10000, THREAD_ATTR_USER | THREAD_ATTR_VFPU, NULL);
    if (mAudioThreadId >= 0) {
        sceKernelStartThread(mAudioThreadId, 0, NULL);
    }
    return true;
}

void AudioEngine::Shutdown() {
    mAudioRunning = false;
    if (mAudioThreadId >= 0) {
        sceKernelWaitThreadEnd(mAudioThreadId, NULL);
        sceKernelDeleteThread(mAudioThreadId);
        mAudioThreadId = -1;
    }

    if (mEngineBuffer.samples) {
        free(mEngineBuffer.samples);
        mEngineBuffer.samples = NULL;
    }
    for (int i = 0; i < SFX_COUNT; ++i) {
        if (mSfxBuffers[i].samples) {
            free(mSfxBuffers[i].samples);
            mSfxBuffers[i].samples = NULL;
        }
    }
}

#else // PC SDL Audio Implementation

static unsigned int sPCEnginePosFP = 0;
static int sPCSfxCurrentIndex = -1;
static int sPCSfxSamplePos = 0;

void AudioEngine::AudioCallback(void* userdata, Uint8* stream, int len) {
    short* outputBuffer = (short*)stream;
    int samplesCount = len / (2 * sizeof(short));
    memset(outputBuffer, 0, len);

    if (mPaused) return;

    if (mCurrentSfxToPlay >= 0 && mCurrentSfxToPlay < SFX_COUNT) {
        sPCSfxCurrentIndex = mCurrentSfxToPlay;
        sPCSfxSamplePos = 0;
        mCurrentSfxToPlay = -1;
    }

    // 1. Engine sound
    if (mEngineActive && mEngineBuffer.samples && mEngineBuffer.sampleCount > 0) {
        float pitch = mEnginePitch;
        if (pitch < 0.50f) pitch = 0.50f;
        if (pitch > 2.00f) pitch = 2.00f;

        float rateRatio = (mEngineBuffer.sampleRate > 0) ? ((float)mEngineBuffer.sampleRate / 44100.0f) : 1.0f;
        unsigned int stepFP = (unsigned int)(pitch * rateRatio * 65536.0f);
        unsigned int maxPosFP = (unsigned int)(mEngineBuffer.sampleCount) << 16;
        short* engSamples = mEngineBuffer.samples;
        int engChannels = mEngineBuffer.channels;
        int volFactor = (int)(mMusicVolume * 180.0f);

        for (int i = 0; i < samplesCount; ++i) {
            int sIdx = (sPCEnginePosFP >> 16);
            if (sIdx >= mEngineBuffer.sampleCount) sIdx = mEngineBuffer.sampleCount - 1;
            short sample = (engChannels == 1) ? engSamples[sIdx] : engSamples[sIdx * 2];
            sample = (short)((sample * volFactor) >> 8);

            outputBuffer[i * 2 + 0] = sample;
            outputBuffer[i * 2 + 1] = sample;

            sPCEnginePosFP += stepFP;
            if (sPCEnginePosFP >= maxPosFP) {
                sPCEnginePosFP -= maxPosFP;
            }
        }
    }

    // 2. SFX
    if (sPCSfxCurrentIndex >= 0 && sPCSfxCurrentIndex < SFX_COUNT) {
        SoundBuffer& sfx = mSfxBuffers[sPCSfxCurrentIndex];
        if (sfx.samples && sPCSfxSamplePos < sfx.sampleCount) {
            int sfxVol = (int)(mSFXVolume * 256.0f);
            for (int i = 0; i < samplesCount; ++i) {
                if (sPCSfxSamplePos >= sfx.sampleCount) {
                    sPCSfxCurrentIndex = -1;
                    break;
                }

                short sfxSample = (sfx.channels == 1) ? sfx.samples[sPCSfxSamplePos] : sfx.samples[sPCSfxSamplePos * 2];
                sfxSample = (short)((sfxSample * sfxVol) >> 8);
                int left = outputBuffer[i * 2 + 0] + sfxSample;
                int right = outputBuffer[i * 2 + 1] + sfxSample;

                if (left > 32767) left = 32767;
                if (left < -32768) left = -32768;
                if (right > 32767) right = 32767;
                if (right < -32768) right = -32768;

                outputBuffer[i * 2 + 0] = (short)left;
                outputBuffer[i * 2 + 1] = (short)right;

                sPCSfxSamplePos++;
            }
        } else {
            sPCSfxCurrentIndex = -1;
        }
    }
}

bool AudioEngine::Init() {
    mEngineActive = false;
    mPaused = false;
    mEnginePitch = 1.0f;
    mTargetPitch = 1.0f;
    mMusicVolume = 1.0f;
    mSFXVolume = 1.0f;

    LoadAudioFile("assets/audio/engine",  &mEngineBuffer);
    LoadAudioFile("assets/audio/coin",    &mSfxBuffers[SFX_COIN]);
    LoadAudioFile("assets/audio/crack",   &mSfxBuffers[SFX_CRACK]);
    LoadAudioFile("assets/audio/refuel",  &mSfxBuffers[SFX_REFUEL]);
    LoadAudioFile("assets/audio/click",   &mSfxBuffers[SFX_CLICK]);
    if (!LoadAudioFile("assets/audio/upgrade", &mSfxBuffers[SFX_UPGRADE])) {
        if (mSfxBuffers[SFX_CLICK].samples && mSfxBuffers[SFX_CLICK].sampleCount > 0) {
            SoundBuffer& click = mSfxBuffers[SFX_CLICK];
            size_t bytes = click.sampleCount * click.channels * sizeof(short);
            mSfxBuffers[SFX_UPGRADE].samples = (short*)malloc(bytes);
            if (mSfxBuffers[SFX_UPGRADE].samples) {
                memcpy(mSfxBuffers[SFX_UPGRADE].samples, click.samples, bytes);
                mSfxBuffers[SFX_UPGRADE].sampleCount = click.sampleCount;
                mSfxBuffers[SFX_UPGRADE].channels = click.channels;
                mSfxBuffers[SFX_UPGRADE].sampleRate = click.sampleRate;
            }
        }
    }

    SDL_AudioSpec want, have;
    memset(&want, 0, sizeof(want));
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    want.callback = AudioCallback;

    mAudioDevice = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (mAudioDevice > 0) {
        SDL_PauseAudioDevice(mAudioDevice, 0); // Unpause device
    }
    return true;
}

void AudioEngine::Shutdown() {
    if (mAudioDevice > 0) {
        SDL_CloseAudioDevice(mAudioDevice);
        mAudioDevice = 0;
    }

    if (mEngineBuffer.samples) {
        free(mEngineBuffer.samples);
        mEngineBuffer.samples = NULL;
    }
    for (int i = 0; i < SFX_COUNT; ++i) {
        if (mSfxBuffers[i].samples) {
            free(mSfxBuffers[i].samples);
            mSfxBuffers[i].samples = NULL;
        }
    }
}

#endif

void AudioEngine::PlaySFX(SoundEffect sfx) {
    if (sfx >= 0 && sfx < SFX_COUNT) {
        mCurrentSfxToPlay = sfx;
    }
}

void AudioEngine::UpdateEngineSound(float rev, bool active, float dt) {
    mEngineActive = active;
    if (!active) {
        mTargetPitch = 0.50f;
    } else {
        float p = 0.60f + rev * 1.40f;
        if (p < 0.50f) p = 0.50f;
        if (p > 2.00f) p = 2.00f;
        mTargetPitch = p;
    }

    // Smooth lerp
    float lerpSpeed = 8.0f;
    mEnginePitch += (mTargetPitch - mEnginePitch) * (lerpSpeed * dt);
    if (mEnginePitch < 0.50f) mEnginePitch = 0.50f;
    if (mEnginePitch > 2.00f) mEnginePitch = 2.00f;
}

void AudioEngine::PauseAll() {
    mPaused = true;
}

void AudioEngine::ResumeAll() {
    mPaused = false;
}

bool AudioEngine::IsPaused() {
    return mPaused;
}

void AudioEngine::SetMusicVolume(float vol) {
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;
    mMusicVolume = vol;
}

void AudioEngine::SetSFXVolume(float vol) {
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;
    mSFXVolume = vol;
}

float AudioEngine::GetMusicVolume() {
    return mMusicVolume;
}

float AudioEngine::GetSFXVolume() {
    return mSFXVolume;
}

float AudioEngine::GetEnginePitch() {
    return mEnginePitch;
}

float AudioEngine::GetTargetPitch() {
    return mTargetPitch;
}

bool AudioEngine::IsEngineActive() {
    return mEngineActive;
}
