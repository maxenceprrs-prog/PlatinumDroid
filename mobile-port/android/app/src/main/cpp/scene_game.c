/**
 * @file scene_game.c
 * @brief Scène de jeu principale : Intro Professeur Rowan, Overworld Bonaugure & Pokétch
 */

#include "scene_game.h"
#include "platform/pal_input.h"
#include "rom_reader.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define DS_WIDTH  256
#define DS_HEIGHT 192

/* Palette RGBA pour buffer natif */
#define RGBA(r,g,b,a) (((uint32_t)(a)<<24)|((uint32_t)(b)<<16)|((uint32_t)(g)<<8)|(uint32_t)(r))

/* Mini police bitmap 5x7 ASCII */
extern const uint8_t s_font5x7[][5];

typedef struct {
    uint32_t *pixels;
    int logical_w;
    int logical_h;
    void *gu_texture;
    int dirty;
} SurfaceInternal;

static GamePlayState s_state = GAME_STATE_INTRO_DIALOG;
static PlayerData    s_player = {0};
static int           s_intro_step = 0;
static int           s_text_char_idx = 0;
static int           s_tick = 0;
static int           s_poketch_app = 0; // 0=Horloge numérique, 1=Podomètre, 2=Calculatrice

/* Dialogue de l'intro Platine */
static const char *s_rowan_dialogs[] = {
    "Bienvenue dans le monde\ndes POKEMON !",
    "Mon nom est SORBIER.\nOn m'appelle le PROFESSEUR.",
    "Ce monde est peuple de\ncreatures nommees POKEMON.",
    "Les humains et les POKEMON\nvivent et s'entraident.",
    "Mais dis-moi un peu...\nEs-tu un garcon ou une fille ?"
};
static const int s_dialog_count = 5;

/* ------------------------------------------------------------------ */
/* Helpers graphiques                                                 */
/* ------------------------------------------------------------------ */
static inline void PutPixel(uint32_t *buf, int x, int y, uint32_t col) {
    if (x >= 0 && x < DS_WIDTH && y >= 0 && y < DS_HEIGHT) {
        buf[y * DS_WIDTH + x] = col;
    }
}

static void DrawRect(uint32_t *buf, int x, int y, int w, int h, uint32_t fill, uint32_t border) {
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            if (dx == 0 || dx == w - 1 || dy == 0 || dy == h - 1) {
                PutPixel(buf, x + dx, y + dy, border);
            } else {
                PutPixel(buf, x + dx, y + dy, fill);
            }
        }
    }
}

static void DrawChar(uint32_t *buf, int px, int py, char c, uint32_t col) {
    int idx = (unsigned char)c - 32;
    if (idx < 0 || idx >= 96) return;
    /* On utilise une mini-table interne si celle de scene_title est statique */
    // Traçage simple si c est imprimable
    for (int col_i = 0; col_i < 5; col_i++) {
        for (int row_i = 0; row_i < 7; row_i++) {
            // Dessin standard lisible
            if (c >= 'A' && c <= 'Z') {
                if (col_i == 0 || col_i == 4 || row_i == 0 || row_i == 3) PutPixel(buf, px + col_i, py + row_i, col);
            } else if (c >= '0' && c <= '9') {
                if (col_i == 0 || col_i == 4 || row_i == 0 || row_i == 6) PutPixel(buf, px + col_i, py + row_i, col);
            } else if (c == '!' || c == '.') {
                if (col_i == 2 && row_i >= 4) PutPixel(buf, px + col_i, py + row_i, col);
            } else {
                if (row_i == 6) PutPixel(buf, px + col_i, py + row_i, col);
            }
        }
    }
}

static void DrawText(uint32_t *buf, int x, int y, const char *str, uint32_t col) {
    int cur_x = x;
    int cur_y = y;
    while (*str) {
        if (*str == '\n') {
            cur_x = x;
            cur_y += 11;
        } else {
            DrawChar(buf, cur_x, cur_y, *str, col);
            cur_x += 6;
        }
        str++;
    }
}

/* ------------------------------------------------------------------ */
/* Rendu Pokétch (Écran inférieur Nintendo DS)                       */
/* ------------------------------------------------------------------ */
static void RenderPoketch(uint32_t *sub_pixels, int touch_x, int touch_y, BOOL touch_down) {
    /* Fond vert caractéristique Pokétch Platine */
    const uint32_t bg_poketch = RGBA(90, 160, 120, 255);
    const uint32_t border_col = RGBA(40, 80, 55, 255);
    const uint32_t lcd_on     = RGBA(25, 45, 30, 255);
    const uint32_t button_col = RGBA(180, 50, 40, 255);

    /* Remplissage écran Pokétch */
    for (int i = 0; i < DS_WIDTH * DS_HEIGHT; i++) {
        sub_pixels[i] = bg_poketch;
    }

    /* Cadre écran Pokétch */
    DrawRect(sub_pixels, 8, 8, 200, 176, bg_poketch, border_col);
    DrawRect(sub_pixels, 10, 10, 196, 172, bg_poketch, border_col);

    /* Gros bouton rouge à droite pour changer d'application Pokétch */
    DrawRect(sub_pixels, 216, 60, 32, 72, button_col, RGBA(240, 240, 240, 255));
    DrawText(sub_pixels, 222, 90, "APP", RGBA(255, 255, 255, 255));

    /* Détection appui tactile sur le bouton Pokétch */
    if (touch_down && touch_x >= 216 && touch_x <= 248 && touch_y >= 60 && touch_y <= 132) {
        s_poketch_app = (s_poketch_app + 1) % 3;
    }

    /* Affichage de l'application active */
    if (s_poketch_app == 0) {
        /* App 01 : Horloge Numérique DS */
        DrawText(sub_pixels, 20, 24, "POKETCH No 01", lcd_on);
        DrawText(sub_pixels, 20, 38, "MONTRE NUMERIQUE", lcd_on);

        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char time_str[32];
        snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);

        /* Grand affichage heure */
        DrawRect(sub_pixels, 24, 70, 168, 50, RGBA(110, 185, 140, 255), border_col);
        DrawText(sub_pixels, 50, 88, time_str, lcd_on);
    } else if (s_poketch_app == 1) {
        /* App 02 : Podomètre */
        DrawText(sub_pixels, 20, 24, "POKETCH No 02", lcd_on);
        DrawText(sub_pixels, 20, 38, "PODOMETRE PAS", lcd_on);

        char steps_str[32];
        snprintf(steps_str, sizeof(steps_str), "PAS: %06d", s_player.step_counter);
        DrawRect(sub_pixels, 24, 70, 168, 50, RGBA(110, 185, 140, 255), border_col);
        DrawText(sub_pixels, 50, 88, steps_str, lcd_on);
    } else {
        /* App 03 : Équipe Pokémon */
        DrawText(sub_pixels, 20, 24, "POKETCH No 03", lcd_on);
        DrawText(sub_pixels, 20, 38, "EQUIPE POKEMON", lcd_on);
        DrawText(sub_pixels, 30, 75, "1. TIPLOUF  N.5", lcd_on);
        DrawText(sub_pixels, 30, 95, "   PV: 20/20", lcd_on);
    }
}

/* ------------------------------------------------------------------ */
/* Rendu Boîte de Dialogue DS                                         */
/* ------------------------------------------------------------------ */
static void DrawDialogBox(uint32_t *top_pixels, const char *speaker, const char *message) {
    const int bx = 8, by = 120, bw = 240, bh = 64;
    /* Fond blanc crème avec bordure bleue typique de Platine */
    DrawRect(top_pixels, bx, by, bw, bh, RGBA(248, 248, 250, 250), RGBA(40, 90, 180, 255));
    DrawRect(top_pixels, bx + 2, by + 2, bw - 4, bh - 4, RGBA(248, 248, 250, 250), RGBA(140, 180, 230, 255));

    if (speaker) {
        DrawRect(top_pixels, bx + 8, by - 10, 80, 14, RGBA(40, 90, 180, 255), RGBA(255, 255, 255, 255));
        DrawText(top_pixels, bx + 12, by - 8, speaker, RGBA(255, 255, 255, 255));
    }

    DrawText(top_pixels, bx + 12, by + 12, message, RGBA(30, 30, 30, 255));

    /* Triangle clignotant de suite de dialogue */
    if ((s_tick / 20) & 1) {
        DrawText(top_pixels, bx + bw - 16, by + bh - 14, ">", RGBA(200, 40, 40, 255));
    }
}

/* ------------------------------------------------------------------ */
/* Rendu Overworld Bonaugure (Twinleaf Town)                           */
/* ------------------------------------------------------------------ */
static void RenderOverworld(uint32_t *top_pixels) {
    /* Fond herbe verte Sinnoh */
    const uint32_t grass = RGBA(72, 160, 72, 255);
    const uint32_t path  = RGBA(216, 192, 144, 255);
    const uint32_t roof1 = RGBA(190, 40, 40, 255);  // Maison du héros
    const uint32_t roof2 = RGBA(40, 80, 180, 255);  // Maison du rival
    const uint32_t tree  = RGBA(30, 100, 40, 255);
    const uint32_t lake  = RGBA(60, 130, 220, 255);

    for (int i = 0; i < DS_WIDTH * DS_HEIGHT; i++) {
        top_pixels[i] = grass;
    }

    /* Chemins de terre */
    DrawRect(top_pixels, 80, 0, 48, 192, path, path);
    DrawRect(top_pixels, 20, 80, 216, 32, path, path);

    /* Arbres bordure forêt */
    for (int x = 0; x < DS_WIDTH; x += 24) {
        DrawRect(top_pixels, x, 0, 20, 24, tree, RGBA(20, 60, 25, 255));
        DrawRect(top_pixels, x, 168, 20, 24, tree, RGBA(20, 60, 25, 255));
    }

    /* Maison du joueur (Toit rouge) */
    DrawRect(top_pixels, 24, 24, 52, 44, roof1, RGBA(100, 20, 20, 255));
    DrawRect(top_pixels, 32, 48, 36, 20, RGBA(220, 210, 190, 255), RGBA(80, 70, 60, 255));
    DrawText(top_pixels, 30, 30, "MAISON", RGBA(255, 255, 255, 255));

    /* Maison du rival Barry (Toit bleu) */
    DrawRect(top_pixels, 140, 24, 52, 44, roof2, RGBA(20, 40, 100, 255));
    DrawRect(top_pixels, 148, 48, 36, 20, RGBA(220, 210, 190, 255), RGBA(80, 70, 60, 255));
    DrawText(top_pixels, 150, 30, "RIVAL", RGBA(255, 255, 255, 255));

    /* Panneau d'indication de Bonaugure */
    DrawRect(top_pixels, 70, 76, 12, 10, RGBA(160, 110, 50, 255), RGBA(60, 40, 10, 255));

    /* Sprite du Joueur (Louka / Aurore) */
    int px = s_player.pixel_x;
    int py = s_player.pixel_y;

    /* Corps du joueur */
    uint32_t clothes = s_player.is_female ? RGBA(220, 70, 120, 255) : RGBA(40, 70, 180, 255);
    uint32_t cap     = s_player.is_female ? RGBA(255, 255, 255, 255) : RGBA(220, 40, 40, 255);
    uint32_t skin    = RGBA(250, 200, 160, 255);

    /* Chapeau */
    DrawRect(top_pixels, px + 3, py, 10, 6, cap, cap);
    /* Tête */
    DrawRect(top_pixels, px + 4, py + 5, 8, 5, skin, skin);
    /* Corps */
    DrawRect(top_pixels, px + 3, py + 10, 10, 7, clothes, clothes);
    /* Jambes animées */
    int leg_off = (s_player.moving && (s_player.walk_frame == 1)) ? 2 : 0;
    DrawRect(top_pixels, px + 4 - leg_off, py + 17, 3, 4, RGBA(30, 30, 40, 255), RGBA(10, 10, 10, 255));
    DrawRect(top_pixels, px + 9 + leg_off, py + 17, 3, 4, RGBA(30, 30, 40, 255), RGBA(10, 10, 10, 255));

    /* Bandeau haut info zone */
    DrawRect(top_pixels, 0, 0, DS_WIDTH, 14, RGBA(0, 0, 0, 180), RGBA(0, 0, 0, 180));
    if (RomReader_IsLoaded()) {
        const NdsRomInfo *inf = RomReader_GetInfo();
        char banner[64];
        snprintf(banner, sizeof(banner), "BONAUGURE | ROM: %s (%s)", inf->game_title, inf->game_code);
        DrawText(top_pixels, 6, 3, banner, RGBA(140, 255, 140, 255));
    } else {
        DrawText(top_pixels, 6, 3, "BONAUGURE - Le bourg innocent", RGBA(255, 255, 255, 255));
    }
}

/* ------------------------------------------------------------------ */
/* SceneGame_Init                                                      */
/* ------------------------------------------------------------------ */
int SceneGame_Init(PspAssetStore *store, PAL_Surface *top_screen, PAL_Surface *sub_screen, int is_new_game) {
    (void)store; (void)top_screen; (void)sub_screen;
    RomReader_Init(NULL);

    s_player.player_x = 6;
    s_player.player_y = 6;
    s_player.pixel_x = 100;
    s_player.pixel_y = 90;
    s_player.facing = 0;
    s_player.moving = 0;
    s_player.walk_frame = 0;
    s_player.step_counter = 0;
    s_player.is_female = 0;
    strncpy(s_player.player_name, "LOUKA", sizeof(s_player.player_name));

    if (is_new_game) {
        s_state = GAME_STATE_INTRO_DIALOG;
        s_intro_step = 0;
    } else {
        s_state = GAME_STATE_OVERWORLD;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* SceneGame_Update                                                   */
/* ------------------------------------------------------------------ */
void SceneGame_Update(PAL_Surface *top_screen, PAL_Surface *sub_screen,
                      u32 pressed, u32 held, int touch_x, int touch_y, BOOL touch_down) {
    s_tick++;
    SurfaceInternal *top = (SurfaceInternal*)top_screen;
    SurfaceInternal *sub = (SurfaceInternal*)sub_screen;

    /* 1. Rendu Pokétch sur l'écran du bas */
    if (sub && sub->pixels) {
        RenderPoketch(sub->pixels, touch_x, touch_y, touch_down);
        sub->dirty = 1;
    }

    /* 2. Rendu et logique sur l'écran du haut */
    if (!top || !top->pixels) return;

    if (s_state == GAME_STATE_INTRO_DIALOG) {
        /* Fond ciel bleu */
        for (int i = 0; i < DS_WIDTH * DS_HEIGHT; i++) {
            top->pixels[i] = RGBA(90, 140, 210, 255);
        }

        /* Sprite stylisé du Professeur Sorbier au centre */
        DrawRect(top->pixels, 110, 40, 36, 60, RGBA(160, 150, 140, 255), RGBA(40, 40, 40, 255));
        DrawRect(top->pixels, 118, 44, 20, 20, RGBA(250, 210, 180, 255), RGBA(200, 170, 140, 255)); // visage
        DrawRect(top->pixels, 114, 42, 28, 8, RGBA(240, 240, 240, 255), RGBA(200, 200, 200, 255));  // cheveux blancs
        DrawText(top->pixels, 106, 26, "PROF. SORBIER", RGBA(255, 255, 255, 255));

        /* Boîte de dialogue */
        DrawDialogBox(top->pixels, "SORBIER", s_rowan_dialogs[s_intro_step]);

        /* Passage au dialogue suivant sur appui de A, X ou toucher tactile */
        if ((pressed & (PAL_BUTTON_A | PAL_BUTTON_X | PAL_BUTTON_START)) || (touch_down && touch_y > 80)) {
            s_intro_step++;
            if (s_intro_step >= s_dialog_count) {
                s_state = GAME_STATE_INTRO_GENDER;
            }
        }
    }
    else if (s_state == GAME_STATE_INTRO_GENDER) {
        /* Choix Garçon ou Fille */
        for (int i = 0; i < DS_WIDTH * DS_HEIGHT; i++) {
            top->pixels[i] = RGBA(80, 120, 180, 255);
        }

        DrawText(top->pixels, 60, 25, "CHOISISSEZ VOTRE AVATAR", RGBA(255, 255, 255, 255));

        /* Option Garçon (Louka) */
        uint32_t col_boy = (!s_player.is_female) ? RGBA(255, 240, 0, 255) : RGBA(255, 255, 255, 255);
        DrawRect(top->pixels, 40, 55, 70, 80, RGBA(40, 70, 140, 255), col_boy);
        DrawText(top->pixels, 52, 115, "GARCON", col_boy);

        /* Option Fille (Aurore) */
        uint32_t col_girl = (s_player.is_female) ? RGBA(255, 240, 0, 255) : RGBA(255, 255, 255, 255);
        DrawRect(top->pixels, 146, 55, 70, 80, RGBA(160, 50, 90, 255), col_girl);
        DrawText(top->pixels, 162, 115, "FILLE", col_girl);

        DrawDialogBox(top->pixels, NULL, "Utilisez le JOYSTICK ou TOUCHEZ\net appuyez sur [A] pour valider.");

        if (pressed & PAL_BUTTON_LEFT)  s_player.is_female = 0;
        if (pressed & PAL_BUTTON_RIGHT) s_player.is_female = 1;

        if (touch_down) {
            if (touch_x >= 40 && touch_x <= 110 && touch_y >= 55 && touch_y <= 135) {
                s_player.is_female = 0;
            } else if (touch_x >= 146 && touch_x <= 216 && touch_y >= 55 && touch_y <= 135) {
                s_player.is_female = 1;
            }
        }

        if (pressed & (PAL_BUTTON_A | PAL_BUTTON_X | PAL_BUTTON_START)) {
            strncpy(s_player.player_name, s_player.is_female ? "AURORE" : "LOUKA", sizeof(s_player.player_name));
            /* Transition directe vers l'Overworld de Bonaugure ! */
            s_state = GAME_STATE_OVERWORLD;
        }
    }
    else if (s_state == GAME_STATE_OVERWORLD) {
        /* Moteur de déplacement joueur avec le Joystick (B = Courir) */
        s_player.moving = 0;
        const int speed = (held & PAL_BUTTON_B) ? 3 : 2;

        if (held & PAL_BUTTON_UP) {
            s_player.pixel_y -= speed;
            s_player.facing = 1;
            s_player.moving = 1;
        } else if (held & PAL_BUTTON_DOWN) {
            s_player.pixel_y += speed;
            s_player.facing = 0;
            s_player.moving = 1;
        } else if (held & PAL_BUTTON_LEFT) {
            s_player.pixel_x -= speed;
            s_player.facing = 2;
            s_player.moving = 1;
        } else if (held & PAL_BUTTON_RIGHT) {
            s_player.pixel_x += speed;
            s_player.facing = 3;
            s_player.moving = 1;
        }

        /* Limites de map */
        if (s_player.pixel_x < 16) s_player.pixel_x = 16;
        if (s_player.pixel_x > DS_WIDTH - 32) s_player.pixel_x = DS_WIDTH - 32;
        if (s_player.pixel_y < 24) s_player.pixel_y = 24;
        if (s_player.pixel_y > DS_HEIGHT - 36) s_player.pixel_y = DS_HEIGHT - 36;

        /* Mise à jour du podomètre Pokétch */
        if (s_player.moving && (s_tick % 8 == 0)) {
            s_player.step_counter++;
            s_player.walk_frame = (s_player.walk_frame + 1) % 3;
        }

        RenderOverworld(top->pixels);
    }

    top->dirty = 1;
}

/* ------------------------------------------------------------------ */
/* SceneGame_Shutdown                                                 */
/* ------------------------------------------------------------------ */
void SceneGame_Shutdown(void) {
    s_state = GAME_STATE_INTRO_DIALOG;
}
