/**
 * @file pal_input_psp.c
 * @brief Implémentation PSP de la couche d'abstraction d'entrée (PAL_Input).
 *
 * Mappe les boutons physiques de la PSP sur les constantes PAL_BUTTON_*
 * définies dans pal_input.h, pour que le code moteur/jeu (state.c, etc.)
 * n'ait jamais besoin de connaître la plateforme sous-jacente.
 *
 * Mapping choisi (aucune touche X/Y physique sur PSP, donc on utilise
 * les gâchettes comme repli, à ajuster selon les retours de test) :
 *
 *   PSP Croix     -> PAL_BUTTON_A   (action / valider, convention PSP standard)
 *   PSP Rond      -> PAL_BUTTON_B   (annuler / retour)
 *   PSP Triangle  -> PAL_BUTTON_X
 *   PSP Carré     -> PAL_BUTTON_Y
 *   PSP L / R     -> PAL_BUTTON_L / PAL_BUTTON_R
 *   PSP Select    -> PAL_BUTTON_SELECT
 *   PSP Start     -> PAL_BUTTON_START
 *   Croix directionnelle + stick analogique -> PAL_BUTTON_UP/DOWN/LEFT/RIGHT
 */

#include <pspctrl.h>
#include <string.h>

#include "platform/pal_input.h"

/* Seuil de tolérance du stick analogique pour le convertir en direction
 * numérique (le stick PSP va de 0 à 255, le centre est à 128). */
#define PSP_ANALOG_DEADZONE 40
#define PSP_ANALOG_CENTER   128

static u32 s_held_prev = 0;
static u32 s_held_curr = 0;
static u32 s_pressed = 0;
static u32 s_released = 0;

BOOL PAL_Input_Init(void) {
    /* Mode d'échantillonnage analogique + numérique, non bloquant.
     * sceCtrlSetSamplingCycle(0) = cadence liée au vsync (recommandé). */
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    s_held_prev = 0;
    s_held_curr = 0;
    s_pressed = 0;
    s_released = 0;
    return TRUE;
}

void PAL_Input_Shutdown(void) {
    /* Rien à libérer côté PSP : sceCtrl ne s'alloue pas dynamiquement. */
}

static u32 MapPspButtonsToPal(const SceCtrlData* pad) {
    u32 held = 0;

    if (pad->Buttons & PSP_CTRL_CROSS)    held |= PAL_BUTTON_A;
    if (pad->Buttons & PSP_CTRL_CIRCLE)   held |= PAL_BUTTON_B;
    if (pad->Buttons & PSP_CTRL_TRIANGLE) held |= PAL_BUTTON_X;
    if (pad->Buttons & PSP_CTRL_SQUARE)   held |= PAL_BUTTON_Y;
    if (pad->Buttons & PSP_CTRL_LTRIGGER) held |= PAL_BUTTON_L;
    if (pad->Buttons & PSP_CTRL_RTRIGGER) held |= PAL_BUTTON_R;
    if (pad->Buttons & PSP_CTRL_SELECT)   held |= PAL_BUTTON_SELECT;
    if (pad->Buttons & PSP_CTRL_START)    held |= PAL_BUTTON_START;

    /* Croix directionnelle numérique */
    if (pad->Buttons & PSP_CTRL_UP)    held |= PAL_BUTTON_UP;
    if (pad->Buttons & PSP_CTRL_DOWN)  held |= PAL_BUTTON_DOWN;
    if (pad->Buttons & PSP_CTRL_LEFT)  held |= PAL_BUTTON_LEFT;
    if (pad->Buttons & PSP_CTRL_RIGHT) held |= PAL_BUTTON_RIGHT;

    /* Stick analogique -> direction numérique (beaucoup de joueurs PSP
     * préfèrent le stick pour ce type de jeu ; on l'OR avec la croix). */
    int dx = (int)pad->Lx - PSP_ANALOG_CENTER;
    int dy = (int)pad->Ly - PSP_ANALOG_CENTER;

    if (dx < -PSP_ANALOG_DEADZONE) held |= PAL_BUTTON_LEFT;
    if (dx >  PSP_ANALOG_DEADZONE) held |= PAL_BUTTON_RIGHT;
    if (dy < -PSP_ANALOG_DEADZONE) held |= PAL_BUTTON_UP;
    if (dy >  PSP_ANALOG_DEADZONE) held |= PAL_BUTTON_DOWN;

    return held;
}

void PAL_Input_Update(void) {
    SceCtrlData pad;
    memset(&pad, 0, sizeof(pad));

    /* Lecture bloquante synchronisée au vblank : simple et suffisant
     * pour un jeu au tour par tour / déplacement en grille comme Platine. */
    sceCtrlReadBufferPositive(&pad, 1);

    s_held_prev = s_held_curr;
    s_held_curr = MapPspButtonsToPal(&pad);

    s_pressed  = s_held_curr & ~s_held_prev;
    s_released = s_held_prev & ~s_held_curr;
}

u32 PAL_Input_GetHeld(void) {
    return s_held_curr;
}

u32 PAL_Input_GetPressed(void) {
    return s_pressed;
}

u32 PAL_Input_GetReleased(void) {
    return s_released;
}

/* La PSP n'a pas d'écran tactile : l'écran du bas de la DS (menus tactiles,
 * clavier, Pokétch) doit être réémulé avec des boutons/menus classiques.
 * C'est un vrai chantier de design, pas juste un détail technique — voir
 * PORTING_PLAN_PSP.md section "Écran tactile -> PSP". */
PAL_TouchState PAL_Input_GetTouch(void) {
    PAL_TouchState touch;
    touch.x = 0;
    touch.y = 0;
    touch.pressed = FALSE;
    touch.held = FALSE;
    touch.released = FALSE;
    return touch;
}

BOOL PAL_Input_IsTouchPressed(void) {
    return FALSE;
}
