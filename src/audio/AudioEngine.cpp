#include "AudioEngine.h"
#include <math.h>

#if defined(__PSP__)
#include "core/fpu.h"
#endif

// Include stb_vorbis implementation
#include "stb_vorbis.c"

SoundBuffer AudioEngine::mSfxBuffers[SFX_COUNT] = { {0} };
SoundBuffer AudioEngine::mEngineBuffer = { 0 };
float AudioEngine::mTargetPitch = 1.0f;
bool AudioEngine::mEngineActive = false;
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

bool AudioEngine::LoadOgg(const char* filepath, SoundBuffer* outBuf) {
    if (!outBuf) return false;

    short* decoded = NULL;
    int channels = 0;
    int sampleRate = 0;
    int sampleCount = stb_vorbis_decode_filename(filepath, &channels, &sampleRate, &decoded);

    if (sampleCount <= 0 || !decoded) {
        printf("Failed to decode ogg: %s\n", filepath);
        return false;
    }

    outBuf->samples = decoded;
    outBuf->sampleCount = sampleCount;
    outBuf->channels = channels;
    outBuf->sampleRate = sampleRate;
    return true;
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

        // Check for new SFX request
        if (mCurrentSfxToPlay >= 0 && mCurrentSfxToPlay < SFX_COUNT) {
            sfxCurrentIndex = mCurrentSfxToPlay;
            sfxSamplePos = 0;
            mCurrentSfxToPlay = -1;
        }

        memset(outputBuffer, 0, AUDIO_BUF_SAMPLES * 2 * sizeof(short));

        // 1. Synthesize / Resample Engine Audio Loop
        if (mEngineActive && mEngineBuffer.samples && mEngineBuffer.sampleCount > 0) {
            float pitch = mTargetPitch;
            if (pitch < 0.5f) pitch = 0.5f;
            if (pitch > 2.0f) pitch = 2.0f;

            unsigned int stepFP = (unsigned int)(pitch * 65536.0f);
            unsigned int maxPosFP = (unsigned int)(mEngineBuffer.sampleCount) << 16;
            short* engSamples = mEngineBuffer.samples;
            int engChannels = mEngineBuffer.channels;

            for (int i = 0; i < AUDIO_BUF_SAMPLES; ++i) {
                int sIdx = (enginePosFP >> 16);
                short sample = 0;
                if (engChannels == 1) {
                    sample = engSamples[sIdx];
                } else {
                    sample = engSamples[sIdx * 2];
                }

                // Master volume scaling (70%)
                sample = (sample * 180) >> 8;

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
                for (int i = 0; i < AUDIO_BUF_SAMPLES; ++i) {
                    if (sfxSamplePos >= sfx.sampleCount) {
                        sfxCurrentIndex = -1;
                        break;
                    }

                    short sfxSample = 0;
                    if (sfx.channels == 1) {
                        sfxSample = sfx.samples[sfxSamplePos];
                    } else {
                        sfxSample = sfx.samples[sfxSamplePos * 2];
                    }

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
    LoadOgg("assets/audio/engine.ogg", &mEngineBuffer);
    LoadOgg("assets/audio/coin.ogg",   &mSfxBuffers[SFX_COIN]);
    LoadOgg("assets/audio/crack.ogg",  &mSfxBuffers[SFX_CRACK]);
    LoadOgg("assets/audio/refuel.ogg", &mSfxBuffers[SFX_REFUEL]);
    LoadOgg("assets/audio/click.ogg",  &mSfxBuffers[SFX_CLICK]);

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

    if (mCurrentSfxToPlay >= 0 && mCurrentSfxToPlay < SFX_COUNT) {
        sPCSfxCurrentIndex = mCurrentSfxToPlay;
        sPCSfxSamplePos = 0;
        mCurrentSfxToPlay = -1;
    }

    memset(outputBuffer, 0, len);

    // 1. Engine sound
    if (mEngineActive && mEngineBuffer.samples && mEngineBuffer.sampleCount > 0) {
        float pitch = mTargetPitch;
        if (pitch < 0.5f) pitch = 0.5f;
        if (pitch > 2.0f) pitch = 2.0f;

        unsigned int stepFP = (unsigned int)(pitch * 65536.0f);
        unsigned int maxPosFP = (unsigned int)(mEngineBuffer.sampleCount) << 16;
        short* engSamples = mEngineBuffer.samples;
        int engChannels = mEngineBuffer.channels;

        for (int i = 0; i < samplesCount; ++i) {
            int sIdx = (sPCEnginePosFP >> 16);
            short sample = (engChannels == 1) ? engSamples[sIdx] : engSamples[sIdx * 2];
            sample = (sample * 180) >> 8;

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
            for (int i = 0; i < samplesCount; ++i) {
                if (sPCSfxSamplePos >= sfx.sampleCount) {
                    sPCSfxCurrentIndex = -1;
                    break;
                }

                short sfxSample = (sfx.channels == 1) ? sfx.samples[sPCSfxSamplePos] : sfx.samples[sPCSfxSamplePos * 2];
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
    LoadOgg("assets/audio/engine.ogg", &mEngineBuffer);
    LoadOgg("assets/audio/coin.ogg",   &mSfxBuffers[SFX_COIN]);
    LoadOgg("assets/audio/crack.ogg",  &mSfxBuffers[SFX_CRACK]);
    LoadOgg("assets/audio/refuel.ogg", &mSfxBuffers[SFX_REFUEL]);
    LoadOgg("assets/audio/click.ogg",  &mSfxBuffers[SFX_CLICK]);

    SDL_AudioSpec want, have;
    memset(&want, 0, sizeof(want));
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    want.callback = AudioCallback;

    mAudioDevice = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (mAudioDevice > 0) {
        SDL_PauseAudioDevice(mAudioDevice, 0); // Unpause
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

void AudioEngine::UpdateEngineSound(float rev, bool active) {
    mEngineActive = active;
    mTargetPitch = 0.65f + rev * 0.95f;
}
