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
volatile int AudioEngine::mSfxQueue[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
volatile int AudioEngine::mSfxQueueHead = 0;
volatile int AudioEngine::mSfxQueueTail = 0;

struct SfxVoice {
    int sfxIndex;
    unsigned int posFP;
    unsigned int stepFP;
    bool active;
};
#define MAX_SFX_VOICES 6
static SfxVoice sSfxVoices[MAX_SFX_VOICES] = { {0} };
static unsigned int sSharedEnginePosFP = 0;

static void MixEngineAndVoices(short* outputBuffer, int samplesCount,
                               const SoundBuffer& engBuf, float engPitch, bool engActive,
                               const SoundBuffer* sfxBuffers, float sfxVolume,
                               volatile int* qBuf, volatile int& qHead, volatile int& qTail,
                               SfxVoice* voices, unsigned int& enginePosFP) {
    // Drain pending SFX trigger queue into active voices (round-robin reuse if full)
    while (qTail != qHead) {
        int sfxIdx = qBuf[qTail];
        qTail = (qTail + 1) & 7;
        if (sfxIdx >= 0 && sfxIdx < SFX_COUNT && sfxBuffers[sfxIdx].samples && sfxBuffers[sfxIdx].sampleCount > 0) {
            int slot = -1;
            unsigned int maxAgePos = 0;
            int oldestSlot = 0;
            for (int v = 0; v < MAX_SFX_VOICES; ++v) {
                if (!voices[v].active) { slot = v; break; }
                if (voices[v].posFP >= maxAgePos) { maxAgePos = voices[v].posFP; oldestSlot = v; }
            }
            if (slot < 0) slot = oldestSlot;
            float rate = (sfxBuffers[sfxIdx].sampleRate > 0) ? ((float)sfxBuffers[sfxIdx].sampleRate / 44100.0f) : 0.5f;
            voices[slot].sfxIndex = sfxIdx;
            voices[slot].posFP = 0;
            voices[slot].stepFP = (unsigned int)(rate * 65536.0f);
            if (voices[slot].stepFP == 0) voices[slot].stepFP = 32768;
            voices[slot].active = true;
        }
    }

    // 1. Mix Engine loop with linear interpolation
    if (engActive && engBuf.samples && engBuf.sampleCount > 1) {
        float pitch = engPitch;
        if (pitch < 0.50f) pitch = 0.50f;
        if (pitch > 2.00f) pitch = 2.00f;
        float rateRatio = (engBuf.sampleRate > 0) ? ((float)engBuf.sampleRate / 44100.0f) : 1.0f;
        unsigned int stepFP = (unsigned int)(pitch * rateRatio * 65536.0f);
        unsigned int maxPosFP = (unsigned int)(engBuf.sampleCount) << 16;
        const short* engSamples = engBuf.samples;
        int engChannels = engBuf.channels;
        int volFactor = (int)(sfxVolume * 180.0f);

        for (int i = 0; i < samplesCount; ++i) {
            int idx0 = (int)(enginePosFP >> 16);
            if (idx0 >= engBuf.sampleCount) idx0 %= engBuf.sampleCount;
            int idx1 = (idx0 + 1 < engBuf.sampleCount) ? (idx0 + 1) : 0;
            unsigned int frac = enginePosFP & 0xFFFF;
            int s0 = (engChannels == 1) ? engSamples[idx0] : engSamples[idx0 * 2];
            int s1 = (engChannels == 1) ? engSamples[idx1] : engSamples[idx1 * 2];
            int interp = s0 + (((s1 - s0) * (int)frac) >> 16);
            short sample = (short)((interp * volFactor) >> 8);
            outputBuffer[i * 2 + 0] = sample;
            outputBuffer[i * 2 + 1] = sample;
            enginePosFP += stepFP;
            if (enginePosFP >= maxPosFP) enginePosFP -= maxPosFP;
        }
    }

    // 2. Mix concurrent SFX voices with sample-rate matching + linear interpolation
    int sfxVol = (int)(sfxVolume * 256.0f);
    for (int v = 0; v < MAX_SFX_VOICES; ++v) {
        if (!voices[v].active) continue;
        const SoundBuffer& sfx = sfxBuffers[voices[v].sfxIndex];
        if (!sfx.samples || sfx.sampleCount <= 0) { voices[v].active = false; continue; }
        unsigned int limitFP = (unsigned int)sfx.sampleCount << 16;
        for (int i = 0; i < samplesCount; ++i) {
            if (voices[v].posFP >= limitFP) { voices[v].active = false; break; }
            int idx0 = (int)(voices[v].posFP >> 16);
            int idx1 = (idx0 + 1 < sfx.sampleCount) ? (idx0 + 1) : idx0;
            unsigned int frac = voices[v].posFP & 0xFFFF;
            int s0 = (sfx.channels == 1) ? sfx.samples[idx0] : sfx.samples[idx0 * 2];
            int s1 = (sfx.channels == 1) ? sfx.samples[idx1] : sfx.samples[idx1 * 2];
            int sfxSample = ((s0 + (((s1 - s0) * (int)frac) >> 16)) * sfxVol) >> 8;
            int left = outputBuffer[i * 2 + 0] + sfxSample;
            int right = outputBuffer[i * 2 + 1] + sfxSample;
            if (left > 32767) left = 32767; else if (left < -32768) left = -32768;
            if (right > 32767) right = 32767; else if (right < -32768) right = -32768;
            outputBuffer[i * 2 + 0] = (short)left;
            outputBuffer[i * 2 + 1] = (short)right;
            voices[v].posFP += voices[v].stepFP;
        }
    }
}

#if defined(__PSP__)
int AudioEngine::mChannelEngine = -1;
int AudioEngine::mChannelSFX = -1;
int AudioEngine::mAudioThreadId = -1;
volatile bool AudioEngine::mAudioRunning = false;
#define AUDIO_BUF_SAMPLES 512
#else
SDL_AudioDeviceID AudioEngine::mAudioDevice = 0;
#endif

#define MUSIC_RING_BUF_SAMPLES 32768
static FILE* sMusicFile = NULL;
static char sCurrentMusicPath[128] = { 0 };
static long sMusicDataStart = 0;
static long sMusicDataBytes = 0;
static long sMusicFileBytesRead = 0;
static short sMusicRingBuf[MUSIC_RING_BUF_SAMPLES];
static volatile int sMusicRingHead = 0;
static volatile int sMusicRingTail = 0;
static volatile bool sMusicPlaying = false;
static int sMusicSampleRate = 22050;
static int sMusicChannels = 1;
static unsigned int sMusicPosFP = 0;
static volatile int sMusicUnderrunCount = 0;
static volatile bool sAudioInMusicMix = false;
static volatile bool sAudioSwappingEngine = false;
static volatile bool sAudioInEngineMix = false;
#if !defined(__PSP__)
static stb_vorbis* sOggMusicStream = NULL;
#endif

static inline unsigned int ReadU32LE(const unsigned char* p) {
    unsigned int v = 0;
    memcpy(&v, p, sizeof(v));
    return v;
}

static inline unsigned short ReadU16LE(const unsigned char* p) {
    unsigned short v = 0;
    memcpy(&v, p, sizeof(v));
    return v;
}

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
        unsigned int chunkSize = ReadU32LE(p + offset + 4);
        offset += 8;
        if (offset > size) break;

        size_t avail = size - offset;
        size_t boundedSize = (chunkSize <= avail) ? (size_t)chunkSize : avail;

        if (strcmp(chunkId, "fmt ") == 0 && boundedSize >= 16) {
            unsigned short format = ReadU16LE(p + offset);
            if (format != 1) return false; // Uncompressed PCM only
            channels = ReadU16LE(p + offset + 2);
            sampleRate = (int)ReadU32LE(p + offset + 4);
            bitsPerSample = ReadU16LE(p + offset + 14);
        } else if (strcmp(chunkId, "data") == 0) {
            pcmData = p + offset;
            dataLen = boundedSize;
            break;
        }
        size_t paddedSize = ((size_t)chunkSize + 1u) & ~1u;
        if (paddedSize > avail) break;
        offset += paddedSize;
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

    while (mAudioRunning) {
        short* outputBuffer = mixbuf[cur];
        memset(outputBuffer, 0, AUDIO_BUF_SAMPLES * 2 * sizeof(short));

        if (!mPaused) {
            bool canMixEngine = (!sAudioSwappingEngine);
            if (mEngineActive && canMixEngine) { sAudioInEngineMix = true; __sync_synchronize(); }
            MixEngineAndVoices(outputBuffer, AUDIO_BUF_SAMPLES,
                               mEngineBuffer, mEnginePitch, mEngineActive && canMixEngine,
                               mSfxBuffers, mSFXVolume,
                               mSfxQueue, mSfxQueueHead, mSfxQueueTail,
                               sSfxVoices, sSharedEnginePosFP);
            sAudioInEngineMix = false;
            __sync_synchronize();

            // 3. Mix Streaming Music Channel (T-132, H17c lock-free SPSC)
            if (sMusicPlaying && mMusicVolume > 0.001f) {
                sAudioInMusicMix = true;
                __sync_synchronize();
                int head = sMusicRingHead;
                int tail = sMusicRingTail;
                int avail = (head - tail + MUSIC_RING_BUF_SAMPLES) % MUSIC_RING_BUF_SAMPLES;

                if (avail > 0) {
                    float musicRatio = (sMusicSampleRate > 0) ? ((float)sMusicSampleRate / 44100.0f) : 0.5f;
                    unsigned int musicStepFP = (unsigned int)(musicRatio * 65536.0f);
                    int musicVol = (int)(mMusicVolume * 256.0f);

                    for (int i = 0; i < AUDIO_BUF_SAMPLES; ++i) {
                        head = sMusicRingHead;
                        avail = (head - tail + MUSIC_RING_BUF_SAMPLES) % MUSIC_RING_BUF_SAMPLES;

                        if (avail <= 1) {
                            sMusicUnderrunCount++;
                            break;
                        }

                        short musicSample = sMusicRingBuf[tail];
                        musicSample = (short)((musicSample * musicVol) >> 8);

                        int left = outputBuffer[i * 2 + 0] + musicSample;
                        int right = outputBuffer[i * 2 + 1] + musicSample;

                        if (left > 32767) left = 32767;
                        if (left < -32768) left = -32768;
                        if (right > 32767) right = 32767;
                        if (right < -32768) right = -32768;

                        outputBuffer[i * 2 + 0] = (short)left;
                        outputBuffer[i * 2 + 1] = (short)right;

                        sMusicPosFP += musicStepFP;
                        while (sMusicPosFP >= 65536) {
                            sMusicPosFP -= 65536;
                            tail = (tail + 1) % MUSIC_RING_BUF_SAMPLES;
                            __sync_synchronize();
                            sMusicRingTail = tail;
                            head = sMusicRingHead;
                            avail = (head - tail + MUSIC_RING_BUF_SAMPLES) % MUSIC_RING_BUF_SAMPLES;
                            if (avail <= 0) break;
                        }
                    }
                }
                sAudioInMusicMix = false;
                __sync_synchronize();
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
    if (!LoadAudioFile("assets/audio/upgrade", &mSfxBuffers[SFX_UPGRADE]) &&
        !LoadAudioFile("assets/audio/wrench", &mSfxBuffers[SFX_UPGRADE])) {
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
    LoadAudioFile("assets/audio/thruster", &mSfxBuffers[SFX_THRUSTER]);

    mAudioRunning = true;
    mAudioThreadId = sceKernelCreateThread("hcr_audio", AudioThread, 0x22, 0x10000, THREAD_ATTR_USER | THREAD_ATTR_VFPU, NULL);
    if (mAudioThreadId >= 0) {
        sceKernelStartThread(mAudioThreadId, 0, NULL);
    }
    return true;
}

void AudioEngine::Shutdown() {
    StopMusic();
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

void AudioEngine::AudioCallback(void* userdata, Uint8* stream, int len) {
    (void)userdata;
    short* outputBuffer = (short*)stream;
    int samplesCount = len / (2 * sizeof(short));
    memset(outputBuffer, 0, len);

    if (mPaused) return;

    MixEngineAndVoices(outputBuffer, samplesCount,
                       mEngineBuffer, mEnginePitch, mEngineActive,
                       mSfxBuffers, mSFXVolume,
                       mSfxQueue, mSfxQueueHead, mSfxQueueTail,
                       sSfxVoices, sSharedEnginePosFP);

    // 3. Mix Streaming Music Channel (T-132, H17c lock-free SPSC)
    if (sMusicPlaying && mMusicVolume > 0.001f) {
        int head = sMusicRingHead;
        int tail = sMusicRingTail;
        int avail = (head - tail + MUSIC_RING_BUF_SAMPLES) % MUSIC_RING_BUF_SAMPLES;

        if (avail > 0) {
            float musicRatio = (sMusicSampleRate > 0) ? ((float)sMusicSampleRate / 44100.0f) : 0.5f;
            unsigned int musicStepFP = (unsigned int)(musicRatio * 65536.0f);
            int musicVol = (int)(mMusicVolume * 256.0f);

            for (int i = 0; i < samplesCount; ++i) {
                head = sMusicRingHead;
                avail = (head - tail + MUSIC_RING_BUF_SAMPLES) % MUSIC_RING_BUF_SAMPLES;

                if (avail <= 1) {
                    sMusicUnderrunCount++;
                    break;
                }

                short musicSample = sMusicRingBuf[tail];
                musicSample = (short)((musicSample * musicVol) >> 8);

                int left = outputBuffer[i * 2 + 0] + musicSample;
                int right = outputBuffer[i * 2 + 1] + musicSample;

                if (left > 32767) left = 32767;
                if (left < -32768) left = -32768;
                if (right > 32767) right = 32767;
                if (right < -32768) right = -32768;

                outputBuffer[i * 2 + 0] = (short)left;
                outputBuffer[i * 2 + 1] = (short)right;

                sMusicPosFP += musicStepFP;
                while (sMusicPosFP >= 65536) {
                    sMusicPosFP -= 65536;
                    tail = (tail + 1) % MUSIC_RING_BUF_SAMPLES;
                    __sync_synchronize();
                    sMusicRingTail = tail;
                    head = sMusicRingHead;
                    avail = (head - tail + MUSIC_RING_BUF_SAMPLES) % MUSIC_RING_BUF_SAMPLES;
                    if (avail <= 0) break;
                }
            }
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
    if (!LoadAudioFile("assets/audio/upgrade", &mSfxBuffers[SFX_UPGRADE]) &&
        !LoadAudioFile("assets/audio/wrench", &mSfxBuffers[SFX_UPGRADE])) {
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
    LoadAudioFile("assets/audio/thruster", &mSfxBuffers[SFX_THRUSTER]);

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
    StopMusic();
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
        int nextHead = (mSfxQueueHead + 1) & 7;
        if (nextHead != mSfxQueueTail) {
            mSfxQueue[mSfxQueueHead] = sfx;
            __sync_synchronize();
            mSfxQueueHead = nextHead;
        }
    }
}

void AudioEngine::LoadVehicleEngine(const char* vehicleId) {
    struct EngineMap { const char* id; const char* asset; };
    static const EngineMap kMap[] = {
        { "ambulance",     "assets/audio/engine-ambulance5" },
        { "bigfinger",     "assets/audio/engine-bigmonster" },
        { "dieseltruck",   "assets/audio/engine-diesel" },
        { "truck",         "assets/audio/engine-diesel" },
        { "firetruck",     "assets/audio/engine-diesel" },
        { "electric",      "assets/audio/engine-electric" },
        { "hippievan",     "assets/audio/engine-hippievan" },
        { "hotrod",        "assets/audio/engine-hotrod" },
        { "hovercraft",    "assets/audio/engine-hovercraft" },
        { "monster_truck", "assets/audio/engine-monster" },
        { "moonlander",    "assets/audio/engine-moonlander2" },
        { "onewheeler",    "assets/audio/engine-onewheeler" },
        { "policecar",     "assets/audio/engine-police" },
        { "rallycar",      "assets/audio/engine-rallycar" },
        { "train",         "assets/audio/engine-train" },
        { "dragster",      "assets/audio/engine-roar" },
        { "race_car",      "assets/audio/engine-muscle" }
    };

    const char* targetBase = "assets/audio/engine";
    if (vehicleId) {
        for (size_t i = 0; i < sizeof(kMap) / sizeof(kMap[0]); ++i) {
            if (strcmp(vehicleId, kMap[i].id) == 0) {
                targetBase = kMap[i].asset;
                break;
            }
        }
    }

    SoundBuffer nextBuf = { 0 };
    if (!LoadAudioFile(targetBase, &nextBuf) && strcmp(targetBase, "assets/audio/engine") != 0) {
        LoadAudioFile("assets/audio/engine", &nextBuf);
    }
    if (!nextBuf.samples || nextBuf.sampleCount <= 0) return;

#if defined(__PSP__)
    sAudioSwappingEngine = true;
    __sync_synchronize();
    for (int wait = 0; wait < 40 && sAudioInEngineMix; ++wait) {
        sceKernelDelayThread(500);
    }
    short* oldSamples = mEngineBuffer.samples;
    mEngineBuffer = nextBuf;
    __sync_synchronize();
    sAudioSwappingEngine = false;
    if (oldSamples) free(oldSamples);
#else
    if (mAudioDevice > 0) SDL_LockAudioDevice(mAudioDevice);
    short* oldSamples = mEngineBuffer.samples;
    mEngineBuffer = nextBuf;
    if (mAudioDevice > 0) SDL_UnlockAudioDevice(mAudioDevice);
    if (oldSamples) free(oldSamples);
#endif
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

// -----------------------------------------------------------------------------
// Streaming Music API (T-132, H17c lock-free SPSC)
// -----------------------------------------------------------------------------
void AudioEngine::UpdateMusicStream(bool prefill) {
#if !defined(__PSP__)
    if ((sMusicPlaying || prefill) && sOggMusicStream) {
        int head = sMusicRingHead;
        int tail = sMusicRingTail;
        int freeSamples = (tail - head - 1 + MUSIC_RING_BUF_SAMPLES) % MUSIC_RING_BUF_SAMPLES;
        if (freeSamples < 1024) return;
        while (freeSamples >= 512 && (sMusicPlaying || prefill)) {
            short tmp[1024];
            int toGet = freeSamples > 1024 ? 1024 : freeSamples;
            int got = stb_vorbis_get_samples_short_interleaved(sOggMusicStream, 1, tmp, toGet);
            if (got <= 0) {
                stb_vorbis_seek_start(sOggMusicStream);
                got = stb_vorbis_get_samples_short_interleaved(sOggMusicStream, 1, tmp, toGet);
                if (got <= 0) break;
            }
            for (int k = 0; k < got; ++k) {
                sMusicRingBuf[head] = tmp[k];
                head = (head + 1) % MUSIC_RING_BUF_SAMPLES;
            }
            __sync_synchronize();
            sMusicRingHead = head;
            tail = sMusicRingTail;
            freeSamples = (tail - head - 1 + MUSIC_RING_BUF_SAMPLES) % MUSIC_RING_BUF_SAMPLES;
        }
        return;
    }
#endif
    if ((!sMusicPlaying && !prefill) || !sMusicFile) return;

    int head = sMusicRingHead;
    int tail = sMusicRingTail;
    int freeSamples = (tail - head - 1 + MUSIC_RING_BUF_SAMPLES) % MUSIC_RING_BUF_SAMPLES;
    if (freeSamples < 1024) return;

    while (freeSamples >= 512 && (sMusicPlaying || prefill)) {
        int toRead = freeSamples > 2048 ? 2048 : freeSamples;
        int bytesToRead = toRead * (int)sizeof(short);

        long remainingBytes = sMusicDataBytes - sMusicFileBytesRead;
        if (remainingBytes <= 0) {
            fseek(sMusicFile, sMusicDataStart, SEEK_SET);
            sMusicFileBytesRead = 0;
            remainingBytes = sMusicDataBytes;
        }

        if (bytesToRead > remainingBytes) {
            bytesToRead = (int)remainingBytes;
            toRead = bytesToRead / (int)sizeof(short);
        }
        if (toRead <= 0) break;

        int spaceToEnd = MUSIC_RING_BUF_SAMPLES - head;
        int chunk1 = toRead < spaceToEnd ? toRead : spaceToEnd;
        size_t read1 = fread(&sMusicRingBuf[head], sizeof(short), chunk1, sMusicFile);
        head = (head + (int)read1) % MUSIC_RING_BUF_SAMPLES;
        sMusicFileBytesRead += (long)(read1 * sizeof(short));
        freeSamples -= (int)read1;

        if (chunk1 < toRead && read1 == (size_t)chunk1) {
            int chunk2 = toRead - chunk1;
            size_t read2 = fread(&sMusicRingBuf[head], sizeof(short), chunk2, sMusicFile);
            head = (head + (int)read2) % MUSIC_RING_BUF_SAMPLES;
            sMusicFileBytesRead += (long)(read2 * sizeof(short));
            freeSamples -= (int)read2;
        }

        __sync_synchronize();
        sMusicRingHead = head;

        if (read1 == 0) {
            fseek(sMusicFile, sMusicDataStart, SEEK_SET);
            sMusicFileBytesRead = 0;
            break;
        }

        tail = sMusicRingTail;
        freeSamples = (tail - head - 1 + MUSIC_RING_BUF_SAMPLES) % MUSIC_RING_BUF_SAMPLES;
    }
}

static FILE* TryOpenWavFile(const char* path) {
    if (!path || path[0] == '\0') return NULL;
    FILE* fp = fopen(path, "rb");
    if (!fp) return NULL;
    char hdr[12];
    if (fread(hdr, 1, 12, fp) == 12 && memcmp(hdr, "RIFF", 4) == 0 && memcmp(hdr + 8, "WAVE", 4) == 0) {
        return fp;
    }
    fclose(fp);
    return NULL;
}

bool AudioEngine::PlayMusic(const char* filepath) {
    if (!filepath || filepath[0] == '\0') {
        StopMusic();
        return false;
    }

    if (sMusicPlaying && strcmp(sCurrentMusicPath, filepath) == 0) {
        return true;
    }

    StopMusic();

    FILE* f = TryOpenWavFile(filepath);
    if (!f) {
        char altPath[256];
        strncpy(altPath, filepath, sizeof(altPath) - 1);
        altPath[sizeof(altPath) - 1] = '\0';
        char* ext = strrchr(altPath, '.');
        if (ext && strcmp(ext, ".ogg") == 0) {
            strcpy(ext, ".wav");
            f = TryOpenWavFile(altPath);
        } else if (ext && strcmp(ext, ".wav") == 0) {
            strcpy(ext, ".ogg");
            f = TryOpenWavFile(altPath);
        }
    }
#if defined(__PSP__)
    if (!f) {
        char msPath[256];
        snprintf(msPath, sizeof(msPath), "ms0:/PSP/GAME/HCR/%s", filepath);
        f = TryOpenWavFile(msPath);
        if (!f) {
            char* ext = strrchr(msPath, '.');
            if (ext && strcmp(ext, ".ogg") == 0) {
                strcpy(ext, ".wav");
                f = TryOpenWavFile(msPath);
            }
        }
    }
#endif

    if (!f) {
#if !defined(__PSP__)
        int oggErr = 0;
        stb_vorbis* ogg = stb_vorbis_open_filename(filepath, &oggErr, NULL);
        if (!ogg) {
            char oggPath[256];
            snprintf(oggPath, sizeof(oggPath), "%s", filepath);
            char* ext = strrchr(oggPath, '.');
            if (ext && strcmp(ext, ".wav") == 0) {
                strcpy(ext, ".ogg");
                ogg = stb_vorbis_open_filename(oggPath, &oggErr, NULL);
            }
        }
        if (ogg) {
            stb_vorbis_info info = stb_vorbis_get_info(ogg);
            if (mAudioDevice > 0) SDL_LockAudioDevice(mAudioDevice);
            sOggMusicStream = ogg;
            snprintf(sCurrentMusicPath, sizeof(sCurrentMusicPath), "%s", filepath);
            sMusicSampleRate = (info.sample_rate > 0) ? (int)info.sample_rate : 22050;
            sMusicChannels = 1;
            sMusicRingHead = 0;
            sMusicRingTail = 0;
            sMusicPosFP = 0;
            sMusicUnderrunCount = 0;
            if (mAudioDevice > 0) SDL_UnlockAudioDevice(mAudioDevice);
            UpdateMusicStream(true);
            __sync_synchronize();
            sMusicPlaying = true;
            return true;
        }
#endif
        return false;
    }

    // Parse WAV header chunks (f is positioned at offset 12, after "RIFF....WAVE")
    int channels = 0;
    int sampleRate = 0;
    int bitsPerSample = 0;
    long dataStart = 0;
    long dataBytes = 0;

    while (!feof(f)) {
        char chunkId[4];
        if (fread(chunkId, 1, 4, f) != 4) break;
        unsigned int chunkSize = 0;
        if (fread(&chunkSize, 1, 4, f) != 4) break;

        long chunkDataPos = ftell(f);
        if (memcmp(chunkId, "fmt ", 4) == 0 && chunkSize >= 16) {
            unsigned char fmtBuf[16];
            if (fread(fmtBuf, 1, 16, f) != 16) {
                fclose(f);
                return false;
            }
            unsigned short format = ReadU16LE(fmtBuf + 0);
            if (format != 1) { // PCM only
                fclose(f);
                return false;
            }
            channels = ReadU16LE(fmtBuf + 2);
            sampleRate = (int)ReadU32LE(fmtBuf + 4);
            bitsPerSample = ReadU16LE(fmtBuf + 14);
        } else if (memcmp(chunkId, "data", 4) == 0) {
            dataStart = chunkDataPos;
            dataBytes = chunkSize;
            break;
        }
        long paddedSize = ((long)chunkSize + 1L) & ~1L;
        fseek(f, chunkDataPos + paddedSize, SEEK_SET);
    }

    if (channels != 1 || sampleRate <= 0 || bitsPerSample != 16 || dataBytes <= 0) {
        fclose(f);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long totalFileBytes = ftell(f);
    if (dataStart + dataBytes > totalFileBytes) {
        dataBytes = totalFileBytes - dataStart;
    }
    fseek(f, dataStart, SEEK_SET);

#if !defined(__PSP__)
    if (mAudioDevice > 0) SDL_LockAudioDevice(mAudioDevice);
#endif
    sMusicFile = f;
    strncpy(sCurrentMusicPath, filepath, sizeof(sCurrentMusicPath) - 1);
    sCurrentMusicPath[sizeof(sCurrentMusicPath) - 1] = '\0';
    sMusicDataStart = dataStart;
    sMusicDataBytes = dataBytes;
    sMusicFileBytesRead = 0;
    sMusicSampleRate = sampleRate;
    sMusicChannels = channels;
    sMusicRingHead = 0;
    sMusicRingTail = 0;
    sMusicPosFP = 0;
    sMusicUnderrunCount = 0;
    sMusicPlaying = false;
#if !defined(__PSP__)
    if (mAudioDevice > 0) SDL_UnlockAudioDevice(mAudioDevice);
#endif

    // Pre-fill ring buffer before enabling audio callback playback
    UpdateMusicStream(true);
    __sync_synchronize();
    sMusicPlaying = true;
    return true;
}

void AudioEngine::StopMusic() {
#if !defined(__PSP__)
    if (mAudioDevice > 0) SDL_LockAudioDevice(mAudioDevice);
    if (sOggMusicStream) {
        stb_vorbis_close(sOggMusicStream);
        sOggMusicStream = NULL;
    }
#endif
    sMusicPlaying = false;
    __sync_synchronize();
#if defined(__PSP__)
    for (int wait = 0; wait < 40 && sAudioInMusicMix; ++wait) {
        sceKernelDelayThread(500);
    }
#endif
    if (sMusicFile) {
        fclose(sMusicFile);
        sMusicFile = NULL;
    }
    sCurrentMusicPath[0] = '\0';
    sMusicRingHead = 0;
    sMusicRingTail = 0;
    sMusicPosFP = 0;
#if !defined(__PSP__)
    if (mAudioDevice > 0) SDL_UnlockAudioDevice(mAudioDevice);
#endif
}

bool AudioEngine::IsMusicPlaying() {
    return sMusicPlaying;
}

int AudioEngine::GetMusicUnderrunCount() {
    return sMusicUnderrunCount;
}

int AudioEngine::GetMusicRingSampleCount() {
    return (sMusicRingHead - sMusicRingTail + MUSIC_RING_BUF_SAMPLES) % MUSIC_RING_BUF_SAMPLES;
}

int AudioEngine::GetMusicRingCapacity() {
    return MUSIC_RING_BUF_SAMPLES - 1;
}
