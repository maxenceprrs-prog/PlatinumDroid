#ifndef NDS_SPRITE_H
#define NDS_SPRITE_H

/* Nitro NCER (cell/OAM) decoder.  NCER supplies placement information;
 * pixels and colors remain in the NCGR/NCLR objects declared in nds_gfx.h. */
#include "nds_gfx.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int x, y;
    int width, height;
    uint16_t tile_number;
    uint8_t palette_number;
    bool hflip, vflip;
} NdsGfx_Obj;

typedef struct {
    NdsGfx_Obj *objects;
    int num_objects;
} NdsGfx_Cell;

typedef struct {
    NdsGfx_Cell *cells;
    int num_cells;
    /* NCER 1D OBJ mapping boundary: 0, 1, 2 and 3 mean 32, 64, 128 and
     * 256-byte row boundaries respectively. */
    uint32_t mapping_type;
} NdsGfx_CellBank;

bool NdsGfx_ParseCellBank(const void *data, size_t size, NdsGfx_CellBank *out);
void NdsGfx_FreeCellBank(NdsGfx_CellBank *bank);

/* Renders one normal (non-affine) cell into RGBA8888.  Coordinates are local
 * to the cell; transparent palette index 0 is preserved as alpha 0.
 * All four 1D NCER mapping boundaries are supported. */
bool NdsGfx_RenderCellToRGBA(const NdsGfx_CellBank *bank, int cell_index,
                             const NdsGfx_Tileset *tileset,
                             const NdsGfx_Palette *palette,
                             uint32_t *out_rgba, int out_width, int out_height,
                             int out_stride_px);

#endif
