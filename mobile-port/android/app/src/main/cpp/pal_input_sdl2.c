/**
 * @file pal_input_sdl2.c
 * @brief Gestionnaire d'entrées tactile (Touchscreen) et manettes/touches pour mobile
 */

#include "platform/pal_input.h"
#include <SDL2/SDL.h>

static u32 s_buttons_held     = 0;
static u32 s_buttons_pressed  = 0;
static u32 s_buttons_released = 0;
static PAL_TouchState s_touch = {0};

#define DS_SCREEN_WIDTH  256
#define DS_SCREEN_HEIGHT 192

BOOL PAL_Input_Init(void) {
    s_buttons_held     = 0;
    s_buttons_pressed  = 0;
    s_buttons_released = 0;
    memset(&s_touch, 0, sizeof(s_touch));
    return TRUE;
}

void PAL_Input_Shutdown(void) {
}

void PAL_Input_Update(void) {
    u32 prev_held = s_buttons_held;
    s_buttons_pressed = 0;
    s_buttons_released = 0;

    BOOL prev_touch_held = s_touch.held;
    s_touch.pressed  = FALSE;
    s_touch.released = FALSE;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            s_buttons_pressed |= PAL_BUTTON_START;
        }

        /* Clavier & Manettes */
        if (event.type == SDL_KEYDOWN && !event.key.repeat) {
            switch (event.key.keysym.sym) {
                case SDLK_UP:     case SDLK_w: s_buttons_held |= PAL_BUTTON_UP; break;
                case SDLK_DOWN:   case SDLK_s: s_buttons_held |= PAL_BUTTON_DOWN; break;
                case SDLK_LEFT:   case SDLK_a: s_buttons_held |= PAL_BUTTON_LEFT; break;
                case SDLK_RIGHT:  case SDLK_d: s_buttons_held |= PAL_BUTTON_RIGHT; break;
                case SDLK_z:      case SDLK_j: s_buttons_held |= PAL_BUTTON_A; break;
                case SDLK_x:      case SDLK_k: s_buttons_held |= PAL_BUTTON_B; break;
                case SDLK_c:      case SDLK_u: s_buttons_held |= PAL_BUTTON_X; break;
                case SDLK_v:      case SDLK_i: s_buttons_held |= PAL_BUTTON_Y; break;
                case SDLK_RETURN:              s_buttons_held |= PAL_BUTTON_START; break;
                case SDLK_SPACE:               s_buttons_held |= PAL_BUTTON_SELECT; break;
                case SDLK_q:                   s_buttons_held |= PAL_BUTTON_L; break;
                case SDLK_e:                   s_buttons_held |= PAL_BUTTON_R; break;
                default: break;
            }
        }
        else if (event.type == SDL_KEYUP) {
            switch (event.key.keysym.sym) {
                case SDLK_UP:     case SDLK_w: s_buttons_held &= ~PAL_BUTTON_UP; break;
                case SDLK_DOWN:   case SDLK_s: s_buttons_held &= ~PAL_BUTTON_DOWN; break;
                case SDLK_LEFT:   case SDLK_a: s_buttons_held &= ~PAL_BUTTON_LEFT; break;
                case SDLK_RIGHT:  case SDLK_d: s_buttons_held &= ~PAL_BUTTON_RIGHT; break;
                case SDLK_z:      case SDLK_j: s_buttons_held &= ~PAL_BUTTON_A; break;
                case SDLK_x:      case SDLK_k: s_buttons_held &= ~PAL_BUTTON_B; break;
                case SDLK_c:      case SDLK_u: s_buttons_held &= ~PAL_BUTTON_X; break;
                case SDLK_v:      case SDLK_i: s_buttons_held &= ~PAL_BUTTON_Y; break;
                case SDLK_RETURN:              s_buttons_held &= ~PAL_BUTTON_START; break;
                case SDLK_SPACE:               s_buttons_held &= ~PAL_BUTTON_SELECT; break;
                case SDLK_q:                   s_buttons_held &= ~PAL_BUTTON_L; break;
                case SDLK_e:                   s_buttons_held &= ~PAL_BUTTON_R; break;
                default: break;
            }
        }

        /* Tactile Mobile (Touch / Doigt) & Souris */
        if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_FINGERDOWN) {
            s_touch.held = TRUE;
            s_touch.pressed = TRUE;
        }
        else if (event.type == SDL_MOUSEBUTTONUP || event.type == SDL_FINGERUP) {
            s_touch.held = FALSE;
            s_touch.released = TRUE;
        }

        if (event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONDOWN) {
            s_touch.x = event.button.x;
            s_touch.y = event.button.y;
        }
    }

    s_buttons_pressed  = s_buttons_held & ~prev_held;
    s_buttons_released = ~s_buttons_held & prev_held;
}

u32 PAL_Input_GetHeld(void) {
    return s_buttons_held;
}

u32 PAL_Input_GetPressed(void) {
    return s_buttons_pressed;
}

u32 PAL_Input_GetReleased(void) {
    return s_buttons_released;
}

PAL_TouchState PAL_Input_GetTouch(void) {
    return s_touch;
}

BOOL PAL_Input_IsTouchPressed(void) {
    return s_touch.pressed;
}
