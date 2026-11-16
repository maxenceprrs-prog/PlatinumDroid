/**
 * @file scene_title.c
 * @brief Menu principal PSP : "CONTINUER / NOUVELLE PARTIE / OPTIONS"
 *
 * Pipeline complet :
 *   1. Fond : tilemap ground0 (NSCR+NCGR+NCLR) décodée via PspAssets_DrawTilemap.
 *   2. Cadre semi-transparent (rectangle RGBA) dessiné dans le buffer CPU.
 *   3. Texte des options rendu pixel par pixel via une mini-police bitmap 5×7.
 *   4. Curseur animé (clignotement 30 Hz) sur l'option active.
 *   5. Navigation : UP/DOWN déplace le curseur, A/CROIX sélectionne,
 *      START quitte proprement.
 *
 * Aucune dépendance aux NARC manquants — fonctionne avec les seuls
 * assets prebuilt/data/ validés lors des tests précédents.
 */

#include "scene_title.h"
#include "psp_asset_renderer.h"
#include "pal_graphics_psp_ext.h"
#include "platform/pal_input.h"

#include <string.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Mini police bitmap 5×7, ASCII 32-127                               */
/* Chaque caractère = 5 octets (1 bit = 1 pixel, bit7 = colonne 0)   */
/* ------------------------------------------------------------------ */
static const uint8_t s_font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* ' ' */
    {0x00,0x00,0x5F,0x00,0x00}, /* '!' */
    {0x00,0x07,0x00,0x07,0x00}, /* '"' */
    {0x14,0x7F,0x14,0x7F,0x14}, /* '#' */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* '$' */
    {0x23,0x13,0x08,0x64,0x62}, /* '%' */
    {0x36,0x49,0x55,0x22,0x50}, /* '&' */
    {0x00,0x05,0x03,0x00,0x00}, /* ''' */
    {0x00,0x1C,0x22,0x41,0x00}, /* '(' */
    {0x00,0x41,0x22,0x1C,0x00}, /* ')' */
    {0x08,0x2A,0x1C,0x2A,0x08}, /* '*' */
    {0x08,0x08,0x3E,0x08,0x08}, /* '+' */
    {0x00,0x50,0x30,0x00,0x00}, /* ',' */
    {0x08,0x08,0x08,0x08,0x08}, /* '-' */
    {0x00,0x60,0x60,0x00,0x00}, /* '.' */
    {0x20,0x10,0x08,0x04,0x02}, /* '/' */
    {0x3E,0x51,0x49,0x45,0x3E}, /* '0' */
    {0x00,0x42,0x7F,0x40,0x00}, /* '1' */
    {0x42,0x61,0x51,0x49,0x46}, /* '2' */
    {0x21,0x41,0x45,0x4B,0x31}, /* '3' */
    {0x18,0x14,0x12,0x7F,0x10}, /* '4' */
    {0x27,0x45,0x45,0x45,0x39}, /* '5' */
    {0x3C,0x4A,0x49,0x49,0x30}, /* '6' */
    {0x01,0x71,0x09,0x05,0x03}, /* '7' */
    {0x36,0x49,0x49,0x49,0x36}, /* '8' */
    {0x06,0x49,0x49,0x29,0x1E}, /* '9' */
    {0x00,0x36,0x36,0x00,0x00}, /* ':' */
    {0x00,0x56,0x36,0x00,0x00}, /* ';' */
    {0x00,0x08,0x14,0x22,0x41}, /* '<' */
    {0x14,0x14,0x14,0x14,0x14}, /* '=' */
    {0x41,0x22,0x14,0x08,0x00}, /* '>' */
    {0x02,0x01,0x51,0x09,0x06}, /* '?' */
    {0x32,0x49,0x79,0x41,0x3E}, /* '@' */
    {0x7E,0x11,0x11,0x11,0x7E}, /* 'A' */
    {0x7F,0x49,0x49,0x49,0x36}, /* 'B' */
    {0x3E,0x41,0x41,0x41,0x22}, /* 'C' */
    {0x7F,0x41,0x41,0x22,0x1C}, /* 'D' */
    {0x7F,0x49,0x49,0x49,0x41}, /* 'E' */
    {0x7F,0x09,0x09,0x09,0x01}, /* 'F' */
    {0x3E,0x41,0x49,0x49,0x7A}, /* 'G' */
    {0x7F,0x08,0x08,0x08,0x7F}, /* 'H' */
    {0x00,0x41,0x7F,0x41,0x00}, /* 'I' */
    {0x20,0x40,0x41,0x3F,0x01}, /* 'J' */
    {0x7F,0x08,0x14,0x22,0x41}, /* 'K' */
    {0x7F,0x40,0x40,0x40,0x40}, /* 'L' */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* 'M' */
    {0x7F,0x04,0x08,0x10,0x7F}, /* 'N' */
    {0x3E,0x41,0x41,0x41,0x3E}, /* 'O' */
    {0x7F,0x09,0x09,0x09,0x06}, /* 'P' */
    {0x3E,0x41,0x51,0x21,0x5E}, /* 'Q' */
    {0x7F,0x09,0x19,0x29,0x46}, /* 'R' */
    {0x46,0x49,0x49,0x49,0x31}, /* 'S' */
    {0x01,0x01,0x7F,0x01,0x01}, /* 'T' */
    {0x3F,0x40,0x40,0x40,0x3F}, /* 'U' */
    {0x1F,0x20,0x40,0x20,0x1F}, /* 'V' */
    {0x3F,0x40,0x38,0x40,0x3F}, /* 'W' */
    {0x63,0x14,0x08,0x14,0x63}, /* 'X' */
    {0x07,0x08,0x70,0x08,0x07}, /* 'Y' */
    {0x61,0x51,0x49,0x45,0x43}, /* 'Z' */
    {0x00,0x7F,0x41,0x41,0x00}, /* '[' */
    {0x02,0x04,0x08,0x10,0x20}, /* '\' */
    {0x00,0x41,0x41,0x7F,0x00}, /* ']' */
    {0x04,0x02,0x01,0x02,0x04}, /* '^' */
    {0x40,0x40,0x40,0x40,0x40}, /* '_' */
    {0x00,0x01,0x02,0x04,0x00}, /* '`' */
    {0x20,0x54,0x54,0x54,0x78}, /* 'a' */
    {0x7F,0x48,0x44,0x44,0x38}, /* 'b' */
    {0x38,0x44,0x44,0x44,0x20}, /* 'c' */
    {0x38,0x44,0x44,0x48,0x7F}, /* 'd' */
    {0x38,0x54,0x54,0x54,0x18}, /* 'e' */
    {0x08,0x7E,0x09,0x01,0x02}, /* 'f' */
    {0x0C,0x52,0x52,0x52,0x3E}, /* 'g' */
    {0x7F,0x08,0x04,0x04,0x78}, /* 'h' */
    {0x00,0x44,0x7D,0x40,0x00}, /* 'i' */
    {0x20,0x40,0x44,0x3D,0x00}, /* 'j' */
    {0x7F,0x10,0x28,0x44,0x00}, /* 'k' */
    {0x00,0x41,0x7F,0x40,0x00}, /* 'l' */
    {0x7C,0x04,0x18,0x04,0x78}, /* 'm' */
    {0x7C,0x08,0x04,0x04,0x78}, /* 'n' */
    {0x38,0x44,0x44,0x44,0x38}, /* 'o' */
    {0x7C,0x14,0x14,0x14,0x08}, /* 'p' */
    {0x08,0x14,0x14,0x18,0x7C}, /* 'q' */
    {0x7C,0x08,0x04,0x04,0x08}, /* 'r' */
    {0x48,0x54,0x54,0x54,0x20}, /* 's' */
    {0x04,0x3F,0x44,0x40,0x20}, /* 't' */
    {0x3C,0x40,0x40,0x20,0x7C}, /* 'u' */
    {0x1C,0x20,0x40,0x20,0x1C}, /* 'v' */
    {0x3C,0x40,0x30,0x40,0x3C}, /* 'w' */
    {0x44,0x28,0x10,0x28,0x44}, /* 'x' */
    {0x0C,0x50,0x50,0x50,0x3C}, /* 'y' */
    {0x44,0x64,0x54,0x4C,0x44}, /* 'z' */
    {0x00,0x08,0x36,0x41,0x00}, /* '{' */
    {0x00,0x00,0x7F,0x00,0x00}, /* '|' */
    {0x00,0x41,0x36,0x08,0x00}, /* '}' */
    {0x10,0x08,0x08,0x10,0x08}, /* '~' */
};

/* ------------------------------------------------------------------ */
/* Constantes de layout PSP (buffer logiciel 256×192 = DS main screen) */
/* ------------------------------------------------------------------ */
#define BUF_W   256
#define BUF_H   192
#define FONT_W  6   /* 5px glyph + 1px kern */
#define FONT_H  9   /* 7px glyph + 2px leading */

/* Options du menu */
static const char *s_menu_labels[] = {
    "CONTINUER",
    "NOUVELLE PARTIE",
    "OPTIONS"
};
static const int s_menu_count = 3;

/* État interne */
static int  s_cursor      = 0;   /* option active */
static int  s_blink_tick  = 0;   /* pour l'animation du curseur */
static int  s_bg_loaded   = FALSE;
static int  s_frame       = 0;   /* compteur global de frames */

/* ------------------------------------------------------------------ */
/* Helpers dessin dans le buffer logiciel u32 RGBA8888                 */
/* ------------------------------------------------------------------ */
/* PSP stocke la couleur comme ABGR dans le buffer logiciel de PAL    */
#define RGBA(r,g,b,a) (((uint32_t)(a)<<24)|((uint32_t)(b)<<16)|((uint32_t)(g)<<8)|(uint32_t)(r))

static uint32_t *s_pixels = NULL; /* pointeur vers PAL_Surface->pixels */

static inline void SetPixel(int x, int y, uint32_t col) {
    if (x < 0 || x >= BUF_W || y < 0 || y >= BUF_H) return;
    /* PAL_Surface stride = 256 (TEX_WIDTH) */
    s_pixels[y * 256 + x] = col;
}

static void FillRect(int x, int y, int w, int h, uint32_t col) {
    for (int dy = 0; dy < h; dy++)
        for (int dx = 0; dx < w; dx++)
            SetPixel(x+dx, y+dy, col);
}

/* Rectangle avec bordure de 1px */
static void DrawRect(int x, int y, int w, int h, uint32_t fill, uint32_t border) {
    FillRect(x, y, w, h, fill);
    /* bords */
    for (int dx = 0; dx < w; dx++) { SetPixel(x+dx, y, border); SetPixel(x+dx, y+h-1, border); }
    for (int dy = 0; dy < h; dy++) { SetPixel(x, y+dy, border); SetPixel(x+w-1, y+dy, border); }
}

/* Dessin d'un caractère ASCII (police 5×7) à (px, py) */
static void DrawChar(int px, int py, char c, uint32_t col) {
    int idx = (unsigned char)c - 32;
    if (idx < 0 || idx >= (int)(sizeof(s_font5x7)/sizeof(s_font5x7[0]))) return;
    const uint8_t *glyph = s_font5x7[idx];
    for (int col_i = 0; col_i < 5; col_i++) {
        uint8_t column = glyph[col_i];
        for (int row_i = 0; row_i < 7; row_i++) {
            if (column & (1 << row_i))
                SetPixel(px + col_i, py + row_i, col);
        }
    }
}

/* Dessin d'une chaîne centrée horizontalement à y donné */
static void DrawStringCentered(int y, const char *str, uint32_t col) {
    int len = 0;
    while (str[len]) len++;
    int total_w = len * FONT_W;
    int x = (BUF_W - total_w) / 2;
    for (int i = 0; str[i]; i++)
        DrawChar(x + i * FONT_W, y, str[i], col);
}

static void DrawString(int x, int y, const char *str, uint32_t col) {
    for (int i = 0; str[i]; i++)
        DrawChar(x + i * FONT_W, y, str[i], col);
}

/* ------------------------------------------------------------------ */
/* Accès aux pixels du surface PAL (opaque struct)                     */
/* ------------------------------------------------------------------ */
/* La structure PAL_Surface est définie dans pal_graphics_psp.c ;
   on la récupère via le pointeur pixels exposé indirectement :
   la surface retournée par PAL_Graphics_GetScreen est un PAL_Surface*,
   mais on ne connaît pas sa définition ici. On passe par un pointeur
   opaque et on accède aux pixels via l'extension PSP qui prend
   PAL_Surface*. Pour le dessin software direct, on fait un accès
   "friend" : on cast le premier membre (pixels est le 1er champ). */
typedef struct {
    uint32_t *pixels;
    int logical_w;
    int logical_h;
    void *gu_texture;
    int dirty;
} PAL_SurfaceInternal;

static PAL_SurfaceInternal *s_surf_internal = NULL;

/* ------------------------------------------------------------------ */
/* SceneTitle_Init                                                     */
/* ------------------------------------------------------------------ */
int SceneTitle_Init(PspAssetStore *store, PAL_Surface *screen) {
    s_cursor     = 0;
    s_blink_tick = 0;
    s_frame      = 0;
    s_bg_loaded  = FALSE;

    s_surf_internal = (PAL_SurfaceInternal *)screen;
    s_pixels        = s_surf_internal->pixels;

    /* Fond : tilemap ground0 (validée visuellement lors des tests) */
    s_bg_loaded = PspAssets_DrawTilemap(store, screen,
                      "prebuilt/data/ground0.NSCR",
                      "prebuilt/data/ground0.NCGR",
                      "prebuilt/data/ground0.NCLR",
                      0, 0);
    return TRUE; /* on continue même si ground0 échoue : menu blanc */
}

/* ------------------------------------------------------------------ */
/* DrawMenu : dessine le cadre + texte du menu                        */
/* ------------------------------------------------------------------ */
static void DrawMenu(void) {
    const int PANEL_X   = 30;
    const int PANEL_Y   = 80;
    const int PANEL_W   = 196;
    const int PANEL_H   = 14 + s_menu_count * (FONT_H + 2);

    const uint32_t COL_BG_DARK    = RGBA(10,  20,  50, 220);
    const uint32_t COL_BORDER     = RGBA(150, 200, 255, 255);
    const uint32_t COL_TEXT       = RGBA(255, 255, 255, 255);
    const uint32_t COL_SELECTED   = RGBA(255, 240, 60,  255);
    const uint32_t COL_CURSOR     = RGBA(255, 200, 0,   255);
    const uint32_t COL_TITLE      = RGBA(180, 220, 255, 255);

    /* Fond semi-opaque du panneau */
    DrawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COL_BG_DARK, COL_BORDER);

    /* Titre "POKEMON PLATINE" */
    DrawStringCentered(PANEL_Y + 3, "POKEMON PLATINE", COL_TITLE);

    /* Séparateur horizontal */
    for (int dx = 2; dx < PANEL_W - 2; dx++)
        SetPixel(PANEL_X + dx, PANEL_Y + 12, COL_BORDER);

    /* Options */
    int blink_on = (s_blink_tick / 15) & 1; /* clignotement ~2Hz à 30fps */

    for (int i = 0; i < s_menu_count; i++) {
        int oy = PANEL_Y + 14 + i * (FONT_H + 2);
        int ox = PANEL_X + 16;

        uint32_t col = (i == s_cursor) ? COL_SELECTED : COL_TEXT;
        DrawString(ox, oy, s_menu_labels[i], col);

        /* Curseur clignotant "▶" simulé par un triangle */
        if (i == s_cursor && blink_on) {
            SetPixel(PANEL_X + 6, oy + 3, COL_CURSOR);
            SetPixel(PANEL_X + 7, oy + 3, COL_CURSOR);
            SetPixel(PANEL_X + 7, oy + 4, COL_CURSOR);
            SetPixel(PANEL_X + 8, oy + 4, COL_CURSOR);
            SetPixel(PANEL_X + 7, oy + 5, COL_CURSOR);
            SetPixel(PANEL_X + 6, oy + 5, COL_CURSOR);
        }
    }

    /* Aide-mémoire touches en bas du panneau */
    const int HINT_Y = PANEL_Y + PANEL_H + 3;
    if (HINT_Y + FONT_H < BUF_H) {
        const uint32_t COL_HINT = RGBA(150, 180, 220, 200);
        DrawString(PANEL_X, HINT_Y, "[X]=OK [O]=Quitter [START]=Off", COL_HINT);
    }
}

/* ------------------------------------------------------------------ */
/* DrawTitle : titre animé en haut                                    */
/* ------------------------------------------------------------------ */
static void DrawTitle(void) {
    /* Bandeau noir en haut */
    FillRect(0, 0, BUF_W, 26, RGBA(0, 0, 0, 200));

    /* Texte "POKEMON PLATINE" en grand, animation simple */
    const uint32_t c1 = RGBA(255, 220, 0,  255);
    const uint32_t c2 = RGBA(255, 255, 255, 255);
    const char *title = "POKEMON PLATINE";

    /* Effet : décalage pixel sinusoïdal simple approché par compteur */
    int wave = (s_frame / 8) & 3;  /* 0..3 */
    int dy = (wave < 2) ? wave : (4 - wave); /* 0,1,2,1,0... */

    int len = 0; while (title[len]) len++;
    int tx = (BUF_W - len * 7) / 2; /* échelle ×1 police 6px + 1px */
    for (int i = 0; title[i]; i++) {
        uint32_t col = (i & 1) ? c1 : c2;
        DrawChar(tx + i * 7, 4 + dy, title[i], col);
    }

    /* Sous-titre */
    const uint32_t c3 = RGBA(200, 220, 255, 255);
    DrawStringCentered(16, "Homebrew PSP Port", c3);
}

/* ------------------------------------------------------------------ */
/* SceneTitle_Update                                                  */
/* ------------------------------------------------------------------ */
SceneTitleResult SceneTitle_Update(PAL_Surface *screen, u32 pressed,
                                  int touch_x, int touch_y, int touch_down) {
    s_frame++;
    s_blink_tick++;
    if (s_blink_tick >= 60) s_blink_tick = 0;

    /* Pointe sur les pixels de la surface */
    s_surf_internal = (PAL_SurfaceInternal *)screen;
    s_pixels        = s_surf_internal->pixels;

    /* --- Gestion Tactile direct sur le menu --- */
    if (touch_down && touch_x >= 20 && touch_x <= 236) {
        if (touch_y >= 90 && touch_y < 104) {
            s_cursor = 0;
            return SCENE_TITLE_CONTINUE;
        } else if (touch_y >= 104 && touch_y < 118) {
            s_cursor = 1;
            return SCENE_TITLE_NEW_GAME;
        } else if (touch_y >= 118 && touch_y < 134) {
            s_cursor = 2;
            return SCENE_TITLE_OPTIONS;
        }
    }

    /* --- Navigation manette / boutons --- */
    if (pressed & PAL_BUTTON_UP) {
        s_cursor--;
        if (s_cursor < 0) s_cursor = s_menu_count - 1;
        s_blink_tick = 0; /* repart pour la visibilité */
    }
    if (pressed & PAL_BUTTON_DOWN) {
        s_cursor++;
        if (s_cursor >= s_menu_count) s_cursor = 0;
        s_blink_tick = 0;
    }
    if (pressed & PAL_BUTTON_START) {
        /* START valide l'option sélectionnée pour plus de confort mobile */
        switch (s_cursor) {
            case 0: return SCENE_TITLE_CONTINUE;
            case 1: return SCENE_TITLE_NEW_GAME;
            case 2: return SCENE_TITLE_OPTIONS;
        }
    }
    if ((pressed & PAL_BUTTON_A) || (pressed & PAL_BUTTON_X)) {
        switch (s_cursor) {
            case 0: return SCENE_TITLE_CONTINUE;
            case 1: return SCENE_TITLE_NEW_GAME;
            case 2: return SCENE_TITLE_OPTIONS;
        }
    }
    /* CIRCLE/B = quitter (convention PSP) */
    if (pressed & PAL_BUTTON_B) {
        return SCENE_TITLE_QUIT;
    }

    /* --- Rendu ---
     * On redessine le fond chaque frame pour effacer les pixels de
     * frame précédente (le tilemap ground0 a déjà été rendu à l'init,
     * on le repeint ici si chargé, sinon remplissage de couleur). */
    if (!s_bg_loaded) {
        /* Dégradé de secours : ciel bleu -> vert herbe */
        for (int y = 0; y < BUF_H; y++) {
            uint8_t r = 30 + (uint8_t)(y * 40 / BUF_H);
            uint8_t g = 100 + (uint8_t)(y * 60 / BUF_H);
            uint8_t b = 200 - (uint8_t)(y * 100 / BUF_H);
            uint32_t col = RGBA(r, g, b, 255);
            for (int x = 0; x < BUF_W; x++)
                s_pixels[y * 256 + x] = col;
        }
    }

    DrawTitle();
    DrawMenu();

    /* Marquer la surface comme modifiée */
    s_surf_internal->dirty = TRUE;

    return SCENE_TITLE_RUNNING;
}

/* ------------------------------------------------------------------ */
void SceneTitle_Shutdown(void) {
    s_pixels = NULL;
    s_surf_internal = NULL;
}
