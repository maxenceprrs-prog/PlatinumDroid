/**
 * @file scene_title.h
 * @brief Scène du menu principal (Continuer / Nouvelle partie / Options).
 *
 * Implémentation autonome : fond ground0 (tilemap validée), menu texte
 * dessiné pixel par pixel dans le buffer logiciel — aucune dépendance
 * aux NARC manquants. Branchement minimal sur PAL_Graphics / PAL_Input.
 */

#ifndef SCENE_TITLE_H
#define SCENE_TITLE_H

#include "platform/pal_graphics.h"
#include "platform/pal_input.h"
#include "psp_asset_loader.h"

typedef enum {
    SCENE_TITLE_RUNNING = 0,   /* toujours en cours */
    SCENE_TITLE_NEW_GAME,      /* joueur a choisi Nouvelle Partie */
    SCENE_TITLE_CONTINUE,      /* joueur a choisi Continuer      */
    SCENE_TITLE_OPTIONS,       /* joueur a choisi Options         */
    SCENE_TITLE_QUIT           /* START -> sortie propre          */
} SceneTitleResult;

/* Initialise la scène : charge les assets ground0 depuis assets_root.
 * Retourne FALSE si l'asset store ne peut pas être initialisé. */
int  SceneTitle_Init(PspAssetStore *store, PAL_Surface *screen);

/* À appeler à chaque frame. Dessine + lit les entrées.
 * Retourne un SceneTitleResult. */
SceneTitleResult SceneTitle_Update(PAL_Surface *screen, u32 pressed,
                                  int touch_x, int touch_y, int touch_down);

/* Libère les ressources internes (rien d'alloué pour l'instant). */
void SceneTitle_Shutdown(void);

#endif /* SCENE_TITLE_H */
