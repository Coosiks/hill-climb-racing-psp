#ifndef HCR_SYSTEM_H
#define HCR_SYSTEM_H

#if defined(__PSP__)
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspctrl.h>
#include <psprtc.h>
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
// Provide PSP controller bit constants on PC for uniform 1:1 cross-platform input mapping
#define PSP_CTRL_SELECT     0x000001
#define PSP_CTRL_START      0x000008
#define PSP_CTRL_UP         0x000010
#define PSP_CTRL_RIGHT      0x000020
#define PSP_CTRL_DOWN       0x000040
#define PSP_CTRL_LEFT       0x000080
#define PSP_CTRL_LTRIGGER   0x000100
#define PSP_CTRL_RTRIGGER   0x000200
#define PSP_CTRL_TRIANGLE   0x001000
#define PSP_CTRL_CIRCLE     0x002000
#define PSP_CTRL_CROSS      0x004000
#define PSP_CTRL_SQUARE     0x008000
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

class System {
public:
    static bool Init();
    static void Shutdown();
    static void BeginFrame();
    static void EndFrame();
    static bool IsRunning();
    static void PollInput();

    // Precision timer
    static double GetTimeSeconds();
    static float GetDeltaTime();

    // Input state
    static unsigned int ButtonsPressed; // Single-frame press
    static unsigned int ButtonsHeld;

#if defined(__PSP__)
    static SceCtrlData Pad;
    static void* GetVramBuffer(unsigned int width, unsigned int height, unsigned int psm);
#else
    static SDL_Window* Window;
    static SDL_GLContext GLContext;
#endif

private:
    static bool mRunning;
    static unsigned int mLastButtons;

#if defined(__PSP__)
    static int mExitCallbackThreadId;
    static int ExitCallback(int arg1, int arg2, void *common);
    static int CallbackThread(SceSize args, void *argp);
    static int SetupCallbacks();
#endif
};

#endif // HCR_SYSTEM_H
