/**
 * @file pal_graphics_android.c
 * @brief Rendu graphique software pur et sans dépendance SDL pour l'application Android
 */

#include "platform/pal_graphics.h"
#include "formats/nds_gfx.h"
#include "formats/nds_sprite.h"
#include <stdlib.h>
#include <string.h>

#define DS_SCREEN_WIDTH  256
#define DS_SCREEN_HEIGHT 192

struct PAL_Surface {
    uint32_t *pixels;
    int logical_w;
    int logical_h;
    void *gu_texture;
    int dirty;
};

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

void* PAL_Graphics_AllocVRAM(size_t size) { return malloc(size); }
void  PAL_Graphics_FreeVRAM(void* ptr) { free(ptr); }
void  PAL_Graphics_TransferVRAM(void* src, void* dst, size_t size) { if (src && dst) memcpy(dst, src, size); }
