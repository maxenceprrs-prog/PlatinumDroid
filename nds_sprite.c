#include "nds_sprite.h"
#include <stdlib.h>
#include <string.h>

static uint16_t ReadU16LE(const uint8_t *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t ReadU32LE(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool ObjSize(uint16_t a0, uint16_t a1, int *w, int *h) {
    static const uint8_t sizes[3][4][2] = {
        {{8,8},{16,16},{32,32},{64,64}},
        {{16,8},{32,8},{32,16},{64,32}},
        {{8,16},{8,32},{16,32},{32,64}}
    };
    int shape = (a0 >> 14) & 3, size = (a1 >> 14) & 3;
    if (shape == 3) return false;
    *w = sizes[shape][size][0]; *h = sizes[shape][size][1];
    return true;
}

bool NdsGfx_ParseCellBank(const void *data, size_t size, NdsGfx_CellBank *out) {
    const uint8_t *buf = (const uint8_t *)data;
    const uint8_t *chunk;
    uint16_t count;
    if (!out || !buf || size < 0x30 || memcmp(buf, "RECN", 4) != 0) return false;
    memset(out, 0, sizeof(*out));
    chunk = buf + 0x10;
    if (memcmp(chunk, "KBEC", 4) != 0 || ReadU32LE(chunk + 4) < 0x20) return false;
    count = ReadU16LE(chunk + 8);
    if (!count || count > 4096) return false;
    bool extended = chunk[0x0a] == 1;
    size_t cell_size = extended ? 0x10 : 0x08;
    uint32_t chunk_size = ReadU32LE(chunk + 4);
    uint32_t mapping = ReadU32LE(chunk + 0x10);
    if (chunk_size > size - 0x10 || 0x20u + (size_t)count * cell_size > chunk_size) return false;
    out->cells = (NdsGfx_Cell *)calloc(count, sizeof(*out->cells));
    if (!out->cells) return false;
    out->num_cells = count; out->mapping_type = mapping;

    const uint8_t *oam = chunk + 0x20 + (size_t)count * cell_size;
    const uint8_t *chunk_end = chunk + chunk_size;
    for (int c = 0; c < count; ++c) {
        const uint8_t *cell = chunk + 0x20 + (size_t)c * cell_size;
        uint16_t n = ReadU16LE(cell);
        if (n > 128 || (size_t)(chunk_end - oam) < (size_t)n * 6) goto fail;
        out->cells[c].objects = (NdsGfx_Obj *)calloc(n, sizeof(NdsGfx_Obj));
        if (n && !out->cells[c].objects) goto fail;
        out->cells[c].num_objects = n;
        for (int i = 0; i < n; ++i) {
            const uint8_t *o = oam + i * 6;
            uint16_t a0 = ReadU16LE(o), a1 = ReadU16LE(o + 2), a2 = ReadU16LE(o + 4);
            NdsGfx_Obj *dst = &out->cells[c].objects[i];
            /* Affine/double-size/disabled objects are not silently guessed. */
            if ((a0 & 0x0300) != 0 || !ObjSize(a0, a1, &dst->width, &dst->height)) goto fail;
            dst->x = a1 & 0x1ff; if (dst->x >= 256) dst->x -= 512;
            dst->y = a0 & 0xff;  if (dst->y >= 128) dst->y -= 256;
            dst->tile_number = a2 & 0x3ff;
            dst->palette_number = (a2 >> 12) & 0x0f;
            dst->hflip = (a1 & 0x1000) != 0; dst->vflip = (a1 & 0x2000) != 0;
        }
        oam += (size_t)n * 6;
    }
    return true;
fail:
    NdsGfx_FreeCellBank(out); return false;
}

void NdsGfx_FreeCellBank(NdsGfx_CellBank *bank) {
    if (!bank) return;
    for (int i = 0; i < bank->num_cells; ++i) free(bank->cells[i].objects);
    free(bank->cells); memset(bank, 0, sizeof(*bank));
}

bool NdsGfx_RenderCellToRGBA(const NdsGfx_CellBank *bank, int cell_index,
                             const NdsGfx_Tileset *tileset, const NdsGfx_Palette *palette,
                             uint32_t *out, int out_w, int out_h, int stride) {
    if (!bank || !tileset || !palette || !out || cell_index < 0 || cell_index >= bank->num_cells ||
        bank->mapping_type > 3 || out_w <= 0 || out_h <= 0 || stride < out_w) return false;
    const NdsGfx_Cell *cell = &bank->cells[cell_index];
    for (int i = 0; i < cell->num_objects; ++i) {
        const NdsGfx_Obj *o = &cell->objects[i];
        int tiles_x = o->width / 8;
        int bytes_per_tile = tileset->is_8bpp ? 64 : 32;
        /* NCER's CharName is measured in 32-byte OBJ units.  mapping_type
         * controls the row boundary, not a separate "2D" storage mode. */
        int boundary_units = 1 << bank->mapping_type;
        int raw_units_per_row = (tiles_x * bytes_per_tile) / 32;
        int row_units = ((raw_units_per_row + boundary_units - 1) / boundary_units) * boundary_units;
        int base_tile = (o->tile_number * boundary_units * 32) / bytes_per_tile;
        int row_tiles = (row_units * 32) / bytes_per_tile;
        for (int py = 0; py < o->height; ++py) for (int px = 0; px < o->width; ++px) {
            int sx = o->hflip ? o->width - 1 - px : px, sy = o->vflip ? o->height - 1 - py : py;
            int tile = base_tile + (sy / 8) * row_tiles + sx / 8;
            int dx = o->x + px, dy = o->y + py;
            if (tile < 0 || tile >= tileset->num_tiles || dx < 0 || dy < 0 || dx >= out_w || dy >= out_h) continue;
            uint8_t ci = tileset->tile_indices[tile * 64 + (sy % 8) * 8 + (sx % 8)];
            int pi = tileset->is_8bpp ? ci : o->palette_number * 16 + ci;
            if (!ci || pi >= palette->num_colors) continue;
            NdsGfx_Color col = palette->colors[pi];
            out[dy * stride + dx] = ((uint32_t)col.a << 24) | ((uint32_t)col.b << 16) |
                                  ((uint32_t)col.g << 8) | col.r;
        }
    }
    return true;
}
