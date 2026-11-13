#include "System.h"

#if defined(__PSP__)
#include "fpu.h"
#include <psppower.h>

// Align display list to 16 bytes for GE
static unsigned int __attribute__((aligned(16))) gDisplayList[262144];

#define VRAM_BUF_WIDTH 512
#define SCR_WIDTH 480
#define SCR_HEIGHT 272

bool System::mRunning = true;
int System::mExitCallbackThreadId = 0;
SceCtrlData System::Pad;
unsigned int System::ButtonsPressed = 0;
unsigned int System::ButtonsHeld = 0;
unsigned int System::mLastButtons = 0;

int System::ExitCallback(int arg1, int arg2, void *common) {
    mRunning = false;
    sceKernelExitGame();
    return 0;
}

int System::CallbackThread(SceSize args, void *argp) {
    fpu_no_exceptions();
    int cbid = sceKernelCreateCallback("Exit Callback", ExitCallback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

int System::SetupCallbacks() {
    int thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0) {
        sceKernelStartThread(thid, 0, 0);
    }
    return thid;
}

bool System::Init() {
    // Unlock 333 MHz max Allegrex CPU clock and 166 MHz bus
    scePowerSetClockFrequency(333, 333, 166);

    SetupCallbacks();

    // Setup controller
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    // Initialize GU
    void* fbp0 = guGetStaticVramBuffer(VRAM_BUF_WIDTH, SCR_HEIGHT, GU_PSM_8888);
    void* fbp1 = guGetStaticVramBuffer(VRAM_BUF_WIDTH, SCR_HEIGHT, GU_PSM_8888);
    void* zbp  = guGetStaticVramBuffer(VRAM_BUF_WIDTH, SCR_HEIGHT, GU_PSM_4444);

    sceGuInit();
    sceGuStart(GU_DIRECT, gDisplayList);

    sceGuDrawBuffer(GU_PSM_8888, fbp0, VRAM_BUF_WIDTH);
    sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, fbp1, VRAM_BUF_WIDTH);
    sceGuDepthBuffer(zbp, VRAM_BUF_WIDTH);

    sceGuOffset(2048 - (SCR_WIDTH / 2), 2048 - (SCR_HEIGHT / 2));
    sceGuViewport(2048, 2048, SCR_WIDTH, SCR_HEIGHT);
    sceGuDepthRange(65535, 0);

    sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);

    sceGuDepthFunc(GU_GEQUAL);
    sceGuDisable(GU_DEPTH_TEST);

    sceGuFrontFace(GU_CW);
    sceGuShadeModel(GU_SMOOTH);
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_CLIP_PLANES);

    // Alpha blending
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);

    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    return true;
}

void System::Shutdown() {
    sceGuTerm();
}

void System::BeginFrame() {
    sceGuStart(GU_DIRECT, gDisplayList);

    // Clear screen (Sky blue default)
    sceGuClearColor(0xFFEA8A3B); // ABGR
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
}

void System::EndFrame() {
    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

bool System::IsRunning() {
    return mRunning;
}

void System::PollInput() {
    sceCtrlReadBufferPositive(&Pad, 1);
    ButtonsHeld = Pad.Buttons;
    ButtonsPressed = ButtonsHeld & (~mLastButtons);
    mLastButtons = ButtonsHeld;
}

#else // PC Platform Implementation via SDL2 and OpenGL

bool System::mRunning = true;
unsigned int System::ButtonsPressed = 0;
unsigned int System::ButtonsHeld = 0;
unsigned int System::mLastButtons = 0;
SDL_Window* System::Window = NULL;
SDL_GLContext System::GLContext = NULL;

bool System::Init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

    // Create 960x544 window (exact 2x scale of PSP 480x272)
    Window = SDL_CreateWindow("Hill Climb Racing (PC Development Build)",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              960, 544,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!Window) {
        printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        return false;
    }

    GLContext = SDL_GL_CreateContext(Window);
    if (!GLContext) {
        printf("SDL_GL_CreateContext Error: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_SetSwapInterval(1); // 60 FPS V-Sync

    glViewport(0, 0, 960, 544);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);

    mRunning = true;
    return true;
}

void System::Shutdown() {
    if (GLContext) {
        SDL_GL_DeleteContext(GLContext);
        GLContext = NULL;
    }
    if (Window) {
        SDL_DestroyWindow(Window);
        Window = NULL;
    }
    SDL_Quit();
}

void System::BeginFrame() {
    glClearColor(0.23f, 0.54f, 0.92f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void System::EndFrame() {
    if (Window) {
        SDL_GL_SwapWindow(Window);
    }
}

bool System::IsRunning() {
    return mRunning;
}

void System::PollInput() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            mRunning = false;
        }
    }

    const Uint8* state = SDL_GetKeyboardState(NULL);
    ButtonsHeld = 0;

    // Controls mapping
    if (state[SDL_SCANCODE_RIGHT] || state[SDL_SCANCODE_D]) ButtonsHeld |= (PSP_CTRL_RIGHT | PSP_CTRL_RTRIGGER);
    if (state[SDL_SCANCODE_LEFT]  || state[SDL_SCANCODE_A]) ButtonsHeld |= (PSP_CTRL_LEFT  | PSP_CTRL_LTRIGGER);
    if (state[SDL_SCANCODE_UP]    || state[SDL_SCANCODE_W]) ButtonsHeld |= PSP_CTRL_UP;
    if (state[SDL_SCANCODE_DOWN]  || state[SDL_SCANCODE_S]) ButtonsHeld |= PSP_CTRL_DOWN;

    if (state[SDL_SCANCODE_SPACE] || state[SDL_SCANCODE_RETURN]) ButtonsHeld |= PSP_CTRL_CROSS;
    if (state[SDL_SCANCODE_ESCAPE] || state[SDL_SCANCODE_BACKSPACE]) ButtonsHeld |= PSP_CTRL_CIRCLE;
    if (state[SDL_SCANCODE_P] || state[SDL_SCANCODE_PAUSE]) ButtonsHeld |= PSP_CTRL_START;
    if (state[SDL_SCANCODE_TAB]) ButtonsHeld |= PSP_CTRL_SELECT;

    // Upgrade hotkeys
    if (state[SDL_SCANCODE_1] || state[SDL_SCANCODE_I]) ButtonsHeld |= PSP_CTRL_TRIANGLE; // Engine
    if (state[SDL_SCANCODE_2] || state[SDL_SCANCODE_J]) ButtonsHeld |= PSP_CTRL_SQUARE;   // Suspension
    if (state[SDL_SCANCODE_3] || state[SDL_SCANCODE_K]) ButtonsHeld |= PSP_CTRL_CIRCLE;   // Tires
    if (state[SDL_SCANCODE_4] || state[SDL_SCANCODE_L]) ButtonsHeld |= PSP_CTRL_SELECT;   // 4WD

    ButtonsPressed = ButtonsHeld & (~mLastButtons);
    mLastButtons = ButtonsHeld;
}

#endif
