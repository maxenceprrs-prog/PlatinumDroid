/* Small host-side regression test; it contains no game data. */
#include "nds_sprite.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void put16(unsigned char *p, unsigned value) { p[0] = value; p[1] = value >> 8; }
static void put32(unsigned char *p, unsigned value) { put16(p, value); put16(p + 2, value >> 16); }

int main(void) {
    unsigned char ncer[0x3e] = {0};
    memcpy(ncer, "RECN", 4); memcpy(ncer + 0x10, "KBEC", 4);
    put32(ncer + 0x14, 0x2e); put16(ncer + 0x18, 1);
    put32(ncer + 0x20, 0);    /* standard 1D tile mapping */
    put16(ncer + 0x30, 1);    /* one normal 8-byte cell header */
    put16(ncer + 0x38, 0); put16(ncer + 0x3a, 0); put16(ncer + 0x3c, 0);

    NdsGfx_CellBank bank;
    assert(NdsGfx_ParseCellBank(ncer, sizeof(ncer), &bank));
    assert(bank.num_cells == 1 && bank.cells[0].num_objects == 1);
    uint8_t tile[64]; memset(tile, 1, sizeof(tile));
    NdsGfx_Tileset tiles = {tile, 1, false};
    NdsGfx_Palette palette = {0}; palette.num_colors = 16;
    palette.colors[1] = (NdsGfx_Color){255, 0, 0, 255};
    uint32_t output[64] = {0};
    assert(NdsGfx_RenderCellToRGBA(&bank, 0, &tiles, &palette, output, 8, 8, 8));
    assert(output[0] == 0xff0000ffu && output[63] == 0xff0000ffu);
    NdsGfx_FreeCellBank(&bank);
    return 0;
}
