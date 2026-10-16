/**
 * @file pal_graphics_sdl2.c
 * @brief Couche d'abstraction graphique portable basée sur SDL2
 * Compatible Linux, Windows, macOS, Android (via SDLActivity) et iOS.
 *
 * Gère le layout DS à deux écrans :
 *  - Soit mode portrait (Top screen au-dessus, Bottom touch screen en dessous)
 *  - Soit mode paysage (côte à côte ou centré avec mise à l'échelle)
 */

#include "platform/pal_graphics.h"
#include "formats/nds_gfx.h"
#include "formats/nds_sprite.h"
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define DS_SCREEN_WIDTH  256
#define DS_SCREEN_HEIGHT 192

struct PAL_Surface {
    uint32_t *pixels;
    int logical_w;
    int logical_h;
    SDL_Texture *texture;
    BOOL dirty;
};

static SDL_Window   *s_window   = NULL;
static SDL_Renderer *s_renderer = NULL;
static PAL_Surface  s_screens[PAL_SCREEN_MAX];
static PAL_Screen   s_active_screen = PAL_SCREEN_MAIN;
static int          s_window_w = 540;
static int          s_window_h = 960;

BOOL PAL_Graphics_Init(int window_width, int window_height) {
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_InitSubSystem(VIDEO) failed: %s\n", SDL_GetError());
        return FALSE;
    }

    s_window_w = (window_width > 0) ? window_width : 540;
    s_window_h = (window_height > 0) ? window_height : 960;

    s_window = SDL_CreateWindow(
        "Pokemon Platinum - Mobile Native Port",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        s_window_w, s_window_h,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (!s_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return FALSE;
    }

    s_renderer = SDL_CreateRenderer(s_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!s_renderer) {
        s_renderer = SDL_CreateRenderer(s_window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!s_renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(s_window);
        return FALSE;
    }

    for (int i = 0; i < PAL_SCREEN_MAX; i++) {
        s_screens[i].logical_w = DS_SCREEN_WIDTH;
        s_screens[i].logical_h = DS_SCREEN_HEIGHT;
        s_screens[i].pixels = (uint32_t *)calloc(DS_SCREEN_WIDTH * DS_SCREEN_HEIGHT, sizeof(uint32_t));
        s_screens[i].texture = SDL_CreateTexture(
            s_renderer,
            SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_STREAMING,
            DS_SCREEN_WIDTH, DS_SCREEN_HEIGHT
        );
        s_screens[i].dirty = TRUE;
    }

    return TRUE;
}

void PAL_Graphics_Shutdown(void) {
    for (int i = 0; i < PAL_SCREEN_MAX; i++) {
        if (s_screens[i].texture) {
            SDL_DestroyTexture(s_screens[i].texture);
            s_screens[i].texture = NULL;
        }
        if (s_screens[i].pixels) {
            free(s_screens[i].pixels);
            s_screens[i].pixels = NULL;
        }
    }

    if (s_renderer) {
        SDL_DestroyRenderer(s_renderer);
        s_renderer = NULL;
    }
    if (s_window) {
        SDL_DestroyWindow(s_window);
        s_window = NULL;
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

PAL_Surface* PAL_Graphics_GetScreen(PAL_Screen screen) {
    if (screen >= PAL_SCREEN_MAX) return NULL;
    return &s_screens[screen];
}

void PAL_Graphics_SetActiveScreen(PAL_Screen screen) {
    if (screen < PAL_SCREEN_MAX) {
        s_active_screen = screen;
    }
}

void PAL_Graphics_BeginFrame(void) {
    SDL_SetRenderDrawColor(s_renderer, 15, 15, 15, 255);
    SDL_RenderClear(s_renderer);
}

void PAL_Graphics_EndFrame(void) {
    int win_w = 0, win_h = 0;
    SDL_GetRendererOutputSize(s_renderer, &win_w, &win_h);

    for (int i = 0; i < PAL_SCREEN_MAX; i++) {
        if (s_screens[i].dirty && s_screens[i].texture && s_screens[i].pixels) {
            SDL_UpdateTexture(
                s_screens[i].texture,
                NULL,
                s_screens[i].pixels,
                DS_SCREEN_WIDTH * sizeof(uint32_t)
            );
            s_screens[i].dirty = FALSE;
        }
    }

    /* Layout Téléphone / Portrait par défaut : Écran haut au dessus, écran bas (tactile) en dessous */
    if (win_h >= win_w) {
        float scale = (float)win_w / (float)DS_SCREEN_WIDTH;
        int scaled_w = win_w;
        int scaled_h = (int)(DS_SCREEN_HEIGHT * scale);
        int gap = 8;
        int total_h = scaled_h * 2 + gap;
        int offset_y = (win_h - total_h) / 2;
        if (offset_y < 0) offset_y = 0;

        SDL_Rect top_rect = { 0, offset_y, scaled_w, scaled_h };
        SDL_Rect bot_rect = { 0, offset_y + scaled_h + gap, scaled_w, scaled_h };

        if (s_screens[PAL_SCREEN_MAIN].texture) {
            SDL_RenderCopy(s_renderer, s_screens[PAL_SCREEN_MAIN].texture, NULL, &top_rect);
        }
        if (s_screens[PAL_SCREEN_SUB].texture) {
            SDL_RenderCopy(s_renderer, s_screens[PAL_SCREEN_SUB].texture, NULL, &bot_rect);
        }
    } else {
        /* Layout Paysage : Écran haut à gauche, écran bas à droite */
        float scale_h = (float)win_h / (float)DS_SCREEN_HEIGHT;
        int scaled_h = win_h;
        int scaled_w = (int)(DS_SCREEN_WIDTH * scale_h);
        int gap = 16;
        int offset_x = (win_w - (scaled_w * 2 + gap)) / 2;

        SDL_Rect left_rect  = { offset_x, 0, scaled_w, scaled_h };
        SDL_Rect right_rect = { offset_x + scaled_w + gap, 0, scaled_w, scaled_h };

        if (s_screens[PAL_SCREEN_MAIN].texture) {
            SDL_RenderCopy(s_renderer, s_screens[PAL_SCREEN_MAIN].texture, NULL, &left_rect);
        }
        if (s_screens[PAL_SCREEN_SUB].texture) {
            SDL_RenderCopy(s_renderer, s_screens[PAL_SCREEN_SUB].texture, NULL, &right_rect);
        }
    }

    SDL_RenderPresent(s_renderer);
}

void PAL_Graphics_DrawSprite(PAL_Surface* surf, const void* sprite_data,
                             int x, int y, int width, int height) {
    (void)surf; (void)sprite_data; (void)x; (void)y; (void)width; (void)height;
}

void PAL_Graphics_DrawTilemap(PAL_Surface* surf, const void* tilemap_data) {
    (void)surf; (void)tilemap_data;
}

BOOL PAL_Graphics_DrawTilemapFull(PAL_Surface* surf,
                                   const void* nscr_data, size_t nscr_size,
                                   const void* ncgr_data, size_t ncgr_size,
                                   const void* nclr_data, size_t nclr_size,
                                   int dst_x, int dst_y) {
    if (!surf || !surf->pixels) return FALSE;

    NdsGfx_Screen screen = {0};
    NdsGfx_Tileset tileset = {0};
    NdsGfx_Palette palette = {0};
    BOOL ok = FALSE;

    if (NdsGfx_ParseScreen(nscr_data, nscr_size, &screen) &&
        NdsGfx_ParseTileset(ncgr_data, ncgr_size, &tileset) &&
        NdsGfx_ParsePalette(nclr_data, nclr_size, &palette)) {

        int px_w = screen.tiles_wide * 8;
        int px_h = screen.tiles_high * 8;
        uint32_t* temp = (uint32_t*)malloc((size_t)px_w * px_h * sizeof(uint32_t));

        if (temp && NdsGfx_RenderScreenToRGBA(&screen, &tileset, &palette, temp, px_w)) {
            for (int row = 0; row < px_h; row++) {
                int out_y = dst_y + row;
                if (out_y < 0 || out_y >= DS_SCREEN_HEIGHT) continue;
                int copy_w = px_w;
                if (dst_x + copy_w > DS_SCREEN_WIDTH) copy_w = DS_SCREEN_WIDTH - dst_x;
                if (copy_w > 0 && dst_x >= 0) {
                    memcpy(&surf->pixels[out_y * DS_SCREEN_WIDTH + dst_x],
                           &temp[row * px_w],
                           (size_t)copy_w * sizeof(uint32_t));
                }
            }
            surf->dirty = TRUE;
            ok = TRUE;
        }
        free(temp);
    }

    NdsGfx_FreeScreen(&screen);
    NdsGfx_FreeTileset(&tileset);
    return ok;
}

BOOL PAL_Graphics_DrawNcerCell(PAL_Surface* surf,
                                const void* ncer_data, size_t ncer_size, int cell_index,
                                const void* ncgr_data, size_t ncgr_size,
                                const void* nclr_data, size_t nclr_size,
                                int dst_x, int dst_y, int cell_width, int cell_height) {
    NdsGfx_CellBank bank = {0};
    NdsGfx_Tileset tileset = {0};
    NdsGfx_Palette palette = {0};
    uint32_t* temp = NULL;
    BOOL ok = FALSE;

    if (!surf || !surf->pixels || cell_width <= 0 || cell_height <= 0 ||
        cell_width > DS_SCREEN_WIDTH || cell_height > DS_SCREEN_HEIGHT) goto done;

    if (!NdsGfx_ParseCellBank(ncer_data, ncer_size, &bank) ||
        !NdsGfx_ParseTileset(ncgr_data, ncgr_size, &tileset) ||
        !NdsGfx_ParsePalette(nclr_data, nclr_size, &palette)) goto done;

    temp = (uint32_t*)calloc((size_t)cell_width * cell_height, sizeof(uint32_t));
    if (!temp || !NdsGfx_RenderCellToRGBA(&bank, cell_index, &tileset, &palette,
                                           temp, cell_width, cell_height, cell_width)) goto done;

    for (int y = 0; y < cell_height; ++y) {
        for (int x = 0; x < cell_width; ++x) {
            int dx = dst_x + x, dy = dst_y + y;
            uint32_t pixel = temp[y * cell_width + x];
            if ((pixel >> 24) && dx >= 0 && dx < DS_SCREEN_WIDTH && dy >= 0 && dy < DS_SCREEN_HEIGHT) {
                surf->pixels[dy * DS_SCREEN_WIDTH + dx] = pixel;
            }
        }
    }
    surf->dirty = TRUE;
    ok = TRUE;

done:
    free(temp);
    NdsGfx_FreeCellBank(&bank);
    NdsGfx_FreeTileset(&tileset);
    return ok;
}

void PAL_Graphics_BlitSurface(PAL_Surface* src, PAL_Surface* dst,
                              int sx, int sy, int w, int h, int dx, int dy) {
    if (!src || !dst || !src->pixels || !dst->pixels) return;

    for (int y = 0; y < h; y++) {
        int src_y = sy + y;
        int dst_y = dy + y;
        if (src_y < 0 || src_y >= src->logical_h) continue;
        if (dst_y < 0 || dst_y >= dst->logical_h) continue;

        for (int x = 0; x < w; x++) {
            int src_x = sx + x;
            int dst_x = dx + x;
            if (src_x < 0 || src_x >= src->logical_w) continue;
            if (dst_x < 0 || dst_x >= dst->logical_w) continue;

            dst->pixels[dst_y * dst->logical_w + dst_x] = src->pixels[src_y * src->logical_w + src_x];
        }
    }
    dst->dirty = TRUE;
}

void* PAL_Graphics_AllocVRAM(size_t size) {
    return malloc(size);
}

void PAL_Graphics_FreeVRAM(void* ptr) {
    free(ptr);
}

void PAL_Graphics_TransferVRAM(void* src, void* dst, size_t size) {
    if (src && dst) memcpy(dst, src, size);
}
