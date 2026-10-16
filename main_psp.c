/**
 * @file main_psp.c
 * @brief Point d'entrée homebrew PSP — v2 avec scène titre.
 *
 * Pipeline :
 *   1. Initialise les trois PAL (graphics / input / audio).
 *   2. Charge le store d'assets depuis la Memory Stick.
 *   3. Lance SceneTitle (fond ground0 + menu CONTINUER/NOUVELLE PARTIE/OPTIONS).
 *   4. Réagit au résultat : placeholder pour les autres scènes.
 *   5. Quitte proprement via sceKernelExitGame.
 */

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>

#include "platform/pal_graphics.h"
#include "platform/pal_input.h"
#include "platform/pal_audio.h"
#include "psp_asset_loader.h"
#include "psp_asset_renderer.h"
#include "scene_title.h"

/* Racine des assets sur la Memory Stick (ou mapping PPSSPP) */
#define PSP_ASSET_ROOT "ms0:/PSP/GAME/POKEPLATINUM/psp-assets"

PSP_MODULE_INFO("POKEPLATINUM_PSP", 0, 2, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1);

/* ------------------------------------------------------------------ */
/* Callback de sortie                                                  */
/* ------------------------------------------------------------------ */
static int exit_callback(int arg1, int arg2, void *common) {
    (void)arg1; (void)arg2; (void)common;
    sceKernelExitGame();
    return 0;
}

static int callback_thread(SceSize args, void *argp) {
    (void)args; (void)argp;
    int cbid = sceKernelCreateCallback("ExitCallback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static void SetupExitCallback(void) {
    int thid = sceKernelCreateThread("cb_thread", callback_thread, 0x11, 0xFA0, 0, NULL);
    if (thid >= 0) sceKernelStartThread(thid, 0, NULL);
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */
int main(void) {
    SetupExitCallback();

    /* --- Init PAL --- */
    if (!PAL_Graphics_Init(480, 272)) return 1;
    PAL_Input_Init();
    PAL_Audio_Init(44100, 2);

    /* --- Assets --- */
    PspAssetStore assets;
    if (!PspAssetStore_Init(&assets, PSP_ASSET_ROOT)) {
        PAL_Audio_Shutdown();
        PAL_Input_Shutdown();
        PAL_Graphics_Shutdown();
        return 1;
    }

    PAL_Surface *main_screen = PAL_Graphics_GetScreen(PAL_SCREEN_MAIN);

    /* --- Scène titre --- */
    SceneTitle_Init(&assets, main_screen);

    BOOL running = TRUE;
    while (running) {
        PAL_Input_Update();
        u32 pressed = PAL_Input_GetPressed();

        SceneTitleResult result = SceneTitle_Update(main_screen, pressed);

        switch (result) {
            case SCENE_TITLE_QUIT:
                running = FALSE;
                break;

            case SCENE_TITLE_NEW_GAME:
                /* TODO Phase 3 : lancer l'intro (scène Professeur Sorbier) */
                /* Pour l'instant : retour au menu (confirme que A fonctionne) */
                SceneTitle_Init(&assets, main_screen);
                break;

            case SCENE_TITLE_CONTINUE:
                /* TODO Phase 3 : charger la sauvegarde */
                SceneTitle_Init(&assets, main_screen);
                break;

            case SCENE_TITLE_OPTIONS:
                /* TODO Phase 3 : scène options */
                SceneTitle_Init(&assets, main_screen);
                break;

            case SCENE_TITLE_RUNNING:
            default:
                break;
        }

        PAL_Graphics_BeginFrame();
        /* Les pixels ont été écrits directement dans le buffer par SceneTitle_Update.
         * BeginFrame/EndFrame s'occupent du blit GPU -> framebuffer PSP. */
        PAL_Graphics_EndFrame();
    }

    /* --- Cleanup --- */
    SceneTitle_Shutdown();
    PspAssetStore_Shutdown(&assets);
    PAL_Audio_Shutdown();
    PAL_Input_Shutdown();
    PAL_Graphics_Shutdown();

    sceKernelExitGame();
    return 0;
}
