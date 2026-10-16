#include "scene_field.h"
#include "psp_asset_renderer.h"

#include <stdint.h>
#include <string.h>

#define DS_WIDTH  256
#define DS_HEIGHT 192

#define RGBA(r,g,b,a) (((uint32_t)(a)<<24)|((uint32_t)(b)<<16)|((uint32_t)(g)<<8)|(uint32_t)(r))

typedef struct {
    uint32_t *pixels;
    int logical_w;
    int logical_h;
    void *gu_texture;
    int dirty;
} SurfaceInternal;

static PspAssetStore *s_store = NULL;
static int s_player_x = 128;
static int s_player_y = 96;
static int s_has_map = FALSE;
static int s_has_npc_cell = FALSE;

static void FillRect(uint32_t *pixels, int x, int y, int w, int h, uint32_t color) {
    if (!pixels || w <= 0 || h <= 0) return;
    int x0 = (x < 0) ? 0 : x;
    int y0 = (y < 0) ? 0 : y;
    int x1 = (x + w > DS_WIDTH) ? DS_WIDTH : x + w;
    int y1 = (y + h > DS_HEIGHT) ? DS_HEIGHT : y + h;
    for (int py = y0; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) {
            pixels[py * DS_WIDTH + px] = color;
        }
    }
}

static void DrawFallbackMap(PAL_Surface *main_screen) {
    SurfaceInternal *s = (SurfaceInternal *)main_screen;
    if (!s || !s->pixels) return;

    for (int y = 0; y < DS_HEIGHT; ++y) {
        uint32_t c = RGBA(20, (uint8_t)(100 + y / 3), 35, 255);
        for (int x = 0; x < DS_WIDTH; ++x) s->pixels[y * DS_WIDTH + x] = c;
    }
    for (int y = 0; y < DS_HEIGHT; y += 16) FillRect(s->pixels, 0, y, DS_WIDTH, 1, RGBA(60, 140, 70, 255));
    for (int x = 0; x < DS_WIDTH; x += 16) FillRect(s->pixels, x, 0, 1, DS_HEIGHT, RGBA(60, 140, 70, 255));
}

static void DrawHud(PAL_Surface *sub_screen) {
    SurfaceInternal *s = (SurfaceInternal *)sub_screen;
    if (!s || !s->pixels) return;
    memset(s->pixels, 0, DS_WIDTH * DS_HEIGHT * sizeof(uint32_t));
    FillRect(s->pixels, 10, 10, DS_WIDTH - 20, 40, RGBA(35, 45, 78, 255));
    FillRect(s->pixels, 10, 58, DS_WIDTH - 20, 34, RGBA(35, 45, 78, 255));
    FillRect(s->pixels, 10, 100, DS_WIDTH - 20, 82, RGBA(20, 25, 45, 255));
    FillRect(s->pixels, 20, 112, 56, 56, RGBA(64, 100, 166, 255));
    FillRect(s->pixels, 180, 112, 56, 56, RGBA(150, 80, 90, 255));
    s->dirty = TRUE;
}

static void DrawPlayer(PAL_Surface *main_screen) {
    SurfaceInternal *s = (SurfaceInternal *)main_screen;
    if (!s || !s->pixels) return;

    FillRect(s->pixels, s_player_x - 4, s_player_y - 4, 9, 9, RGBA(255, 245, 130, 255));
    FillRect(s->pixels, s_player_x - 2, s_player_y - 2, 5, 5, RGBA(255, 108, 64, 255));
    s->dirty = TRUE;
}

BOOL SceneField_Init(PspAssetStore *store, PAL_Surface *main_screen, PAL_Surface *sub_screen) {
    s_store = store;
    s_player_x = DS_WIDTH / 2;
    s_player_y = DS_HEIGHT / 2;

    s_has_map = PspAssets_DrawTilemap(store, main_screen,
                                      "prebuilt/data/ground0.NSCR",
                                      "prebuilt/data/ground0.NCGR",
                                      "prebuilt/data/ground0.NCLR",
                                      0, 0);
    if (!s_has_map) {
        DrawFallbackMap(main_screen);
    }

    s_has_npc_cell = PspAssets_DrawCell(store, main_screen,
                                        "prebuilt/data/smptm_koori.NCER", 0,
                                        "prebuilt/data/smptm_koori.NCGR",
                                        "prebuilt/data/smptm_koori.NCLR",
                                        190, 72, 48, 64);
    DrawHud(sub_screen);
    DrawPlayer(main_screen);
    return TRUE;
}

SceneFieldResult SceneField_Update(PAL_Surface *main_screen, PAL_Surface *sub_screen,
                                   u32 held, u32 pressed) {
    if ((pressed & PAL_BUTTON_START) || (pressed & PAL_BUTTON_B)) {
        return SCENE_FIELD_BACK_TO_TITLE;
    }

    const int speed = 2;
    if (held & PAL_BUTTON_LEFT)  s_player_x -= speed;
    if (held & PAL_BUTTON_RIGHT) s_player_x += speed;
    if (held & PAL_BUTTON_UP)    s_player_y -= speed;
    if (held & PAL_BUTTON_DOWN)  s_player_y += speed;

    if (s_player_x < 4) s_player_x = 4;
    if (s_player_y < 4) s_player_y = 4;
    if (s_player_x > DS_WIDTH - 5) s_player_x = DS_WIDTH - 5;
    if (s_player_y > DS_HEIGHT - 5) s_player_y = DS_HEIGHT - 5;

    if (s_has_map) {
        PspAssets_DrawTilemap(s_store, main_screen,
                              "prebuilt/data/ground0.NSCR",
                              "prebuilt/data/ground0.NCGR",
                              "prebuilt/data/ground0.NCLR",
                              0, 0);
    } else {
        DrawFallbackMap(main_screen);
    }

    if (s_has_npc_cell) {
        PspAssets_DrawCell(s_store, main_screen,
                           "prebuilt/data/smptm_koori.NCER", 0,
                           "prebuilt/data/smptm_koori.NCGR",
                           "prebuilt/data/smptm_koori.NCLR",
                           190, 72, 48, 64);
    }

    DrawPlayer(main_screen);
    DrawHud(sub_screen);
    return SCENE_FIELD_RUNNING;
}

void SceneField_Shutdown(void) {
    s_store = NULL;
}
