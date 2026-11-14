#if defined(__PSP__)
#include <pspkernel.h>
#include <pspdebug.h>
#include <unistd.h>
#include "core/fpu.h"

PSP_MODULE_INFO("HillClimbRacing", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024); // Allocate all available user partition memory minus 1 MB headroom
#endif

#include <string.h>
#include <stdio.h>
#include "core/System.h"
#include "core/Game.h"
#include "core/Config.h"
#include "graphics/Renderer2D.h"

int main(int argc, char* argv[]) {
#if defined(__PSP__)
    fpu_no_exceptions();
    // Change working directory to EBOOT's directory so relative asset paths work
    if (argc > 0 && argv[0]) {
        char dir[256];
        strncpy(dir, argv[0], sizeof(dir));
        dir[sizeof(dir) - 1] = '\0';

        char* slash = strrchr(dir, '/');
        if (!slash) slash = strrchr(dir, '\\');
        if (slash) {
            *slash = '\0';
            chdir(dir);
        }
    }
#endif

    // Load external data-driven configurations
    Config::LoadAll();

    // Check for game assets (Bring Your Own APK model)
#if defined(__PSP__)
    FILE* testFile = fopen("assets/Car.tex", "rb");
#else
    FILE* testFile = fopen("assets/Car.png", "rb");
    if (!testFile) testFile = fopen("assets/Car.tex", "rb");
#endif
    if (!testFile) {
#if defined(__PSP__)
        pspDebugScreenInit();
        pspDebugScreenPrintf("==================================================\n");
        pspDebugScreenPrintf("          HILL CLIMB RACING PSP (BETA)\n");
        pspDebugScreenPrintf("==================================================\n\n");
        pspDebugScreenPrintf("ERROR: Game assets not found in 'assets/'!\n\n");
        pspDebugScreenPrintf("This is a clean-room reimplementation.\n");
        pspDebugScreenPrintf("Original game assets are NOT distributed with it.\n\n");
        pspDebugScreenPrintf("Please:\n");
        pspDebugScreenPrintf("1. Provide your legally owned HCR APK\n");
        pspDebugScreenPrintf("2. Run 'python3 tools/convert_assets.py --apk <apk_path>'\n");
        pspDebugScreenPrintf("3. Copy 'assets' folder alongside EBOOT.PBP\n\n");
        pspDebugScreenPrintf("Exiting to XMB in 10 seconds...\n");
        sceKernelDelayThread(10 * 1000 * 1000);
        sceKernelExitGame();
#else
        printf("==================================================\n");
        printf("          HILL CLIMB RACING PC (BETA)\n");
        printf("==================================================\n\n");
        printf("ERROR: Game assets not found in 'assets/'!\n");
        printf("Original game assets are NOT distributed with this repository.\n\n");
        printf("Please run:\n");
        printf("  python3 tools/convert_assets.py --apk <path/to/hcr.apk>\n");
        printf("and place assets in the 'assets/' folder.\n\n");
#endif
        return -1;
    }
    fclose(testFile);

    if (!System::Init()) {
        return -1;
    }

    Renderer2D::Init();

    Game game;
    if (game.Init()) {
        game.Run();
    }

    System::Shutdown();

#if defined(__PSP__)
    sceKernelExitGame();
#endif
    return 0;
}
