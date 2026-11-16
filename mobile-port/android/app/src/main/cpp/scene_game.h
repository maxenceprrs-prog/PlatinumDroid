/**
 * @file scene_game.h
 * @brief Scène de jeu principale Pokémon Platine :
 *        - Introduction Professeur Rowan (Sorbier)
 *        - Overworld Bonaugure (Twinleaf Town)
 *        - Pokétch sur l'écran tactile inférieur
 *        - Moteur de dialogue et gestion des déplacements joueur
 */

#ifndef SCENE_GAME_H
#define SCENE_GAME_H

#include "platform/pal_common.h"
#include "platform/pal_graphics.h"
#include "psp_asset_loader.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GAME_STATE_INTRO_DIALOG,    /* Intro Professeur Rowan */
    GAME_STATE_INTRO_GENDER,    /* Choix Fille / Garçon */
    GAME_STATE_INTRO_NAME,      /* Choix du Nom */
    GAME_STATE_OVERWORLD,       /* Exploration Bonaugure / Twinleaf */
    GAME_STATE_DIALOG,          /* Dialogue PNJ en cours */
    GAME_STATE_MENU             /* Menu Sac / Équipe / Sauvegarde */
} GamePlayState;

typedef struct {
    int player_x;           /* Position sur la grille (tiles 16x16) */
    int player_y;
    int pixel_x;            /* Position interpolée en pixels pour scrolling */
    int pixel_y;
    int facing;             /* 0=BAS, 1=HAUT, 2=GAUCHE, 3=DROITE */
    int moving;             /* En déplacement ? */
    int walk_frame;         /* Frame d'animation marche (0, 1, 2) */
    int step_counter;       /* Podomètre pour la Pokétch */
    char player_name[16];   /* Nom du joueur (ex: "LOUKA", "AURORE") */
    int is_female;          /* 0=Garçon (Louka), 1=Fille (Aurore) */
} PlayerData;

/**
 * Initialise le moteur de jeu complet
 */
int SceneGame_Init(PspAssetStore *store, PAL_Surface *top_screen, PAL_Surface *sub_screen, int is_new_game);

/**
 * Met à jour et rend la frame de jeu
 */
void SceneGame_Update(PAL_Surface *top_screen, PAL_Surface *sub_screen, u32 pressed, u32 held, int touch_x, int touch_y, BOOL touch_down);

/**
 * Libère les ressources du jeu
 */
void SceneGame_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* SCENE_GAME_H */
