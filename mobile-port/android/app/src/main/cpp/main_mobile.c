/**
 * @file main_mobile.c
 * @brief Point d'entrée pour le portage autonome Mobile / Multiplateforme
 */

#include "platform/pal_graphics.h"
#include "platform/pal_input.h"
#include "scene_title.h"
#include "psp_asset_loader.h"
#include <stdio.h>
#include <stdbool.h>

#define ASSET_ROOT "psp-assets"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    printf("[Mobile Port] Initialisation des sous-systèmes graphiques et tactiles...\n");

    /* Résolution téléphone type mode portrait (540x960 ou natif) */
    if (!PAL_Graphics_Init(540, 960)) {
        fprintf(stderr, "[Erreur] Impossible d'initialiser PAL_Graphics\n");
        return 1;
    }
    PAL_Input_Init();

    PspAssetStore assets;
    if (!PspAssetStore_Init(&assets, ASSET_ROOT)) {
        fprintf(stderr, "[Attention] AssetStore introuvable à '%s', utilisation des buffers par défaut.\n", ASSET_ROOT);
    }

    PAL_Surface *main_screen = PAL_Graphics_GetScreen(PAL_SCREEN_MAIN);

    SceneTitle_Init(&assets, main_screen);

    bool running = true;
    while (running) {
        PAL_Input_Update();
        u32 pressed = PAL_Input_GetPressed();

        SceneTitleResult result = SceneTitle_Update(main_screen, pressed);
        if (result == SCENE_TITLE_QUIT) {
            running = false;
        }

        PAL_Graphics_BeginFrame();
        PAL_Graphics_EndFrame();

        SDL_Delay(16); /* ~60 FPS */
    }

    SceneTitle_Shutdown();
    PspAssetStore_Shutdown(&assets);
    PAL_Input_Shutdown();
    PAL_Graphics_Shutdown();

    printf("[Mobile Port] Fermeture propre effectuée.\n");
    return 0;
}
