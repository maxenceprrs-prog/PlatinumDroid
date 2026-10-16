/**
 * @file nds_gfx.c
 * @brief Décodeur des formats Nitro NCLR/NCGR/NSCR vers RGBA8888.
 *
 * Tous les offsets utilisés ici ont été vérifiés par lecture directe de
 * tools/nitrogfx/gfx.c dans CE MÊME dépôt (ReadNtrPalette et
 * ReadNtrImage) plutôt que reconstruits de mémoire — c'est l'outil que
 * le projet de décompilation utilise lui-même pour fabriquer la ROM, il
 * fait donc foi. Le format NSCR n'a qu'une fonction d'écriture dans cet
 * outil (WriteNtrScreen) ; le décodeur ci-dessous en est l'inverse exact.
 */

#include "nds_gfx.h"
#include <stdlib.h>
#include <string.h>

static uint32_t ReadU32LE(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t ReadU16LE(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

/* --- NCLR : palette --- */
bool NdsGfx_ParsePalette(const void* data, size_t size, NdsGfx_Palette* out) {
    const uint8_t* buf = (const uint8_t*)data;
    if (!buf || size < 0x28) return false;

    /* En-tête générique Nitro : magic "RLCN" (NCLR) ou "RPCN" (NCPR) */
    if (memcmp(buf, "RLCN", 4) != 0 && memcmp(buf, "RPCN", 4) != 0) {
        return false;
    }

    const uint8_t* paletteHeader = buf + 0x10;
    if (memcmp(paletteHeader, "TTLP", 4) != 0) {
        return false; /* pas de section palette (PLTT) après l'en-tête */
    }

    /* [0x08] : 3 = 4bpp (16 couleurs), autre valeur = 8bpp (256 couleurs) */
    out->is_8bpp = (paletteHeader[0x8] != 3);

    /* [0x10] (u32 LE) : taille en octets des données de couleur qui suivent */
    uint32_t paletteDataSize = ReadU32LE(paletteHeader + 0x10);
    int numColors = (int)(paletteDataSize / 2); /* chaque couleur = 2 octets (u16 RGB555) */
    if (numColors > 256) numColors = 256;
    if (numColors < 0) numColors = 0;

    const uint8_t* paletteData = paletteHeader + 0x18;
    size_t available = size - (size_t)(paletteData - buf);
    if ((size_t)numColors * 2 > available) {
        numColors = (int)(available / 2);
    }

    memset(out->colors, 0, sizeof(out->colors));
    for (int i = 0; i < numColors; i++) {
        uint16_t entry = ReadU16LE(paletteData + i * 2);
        /* RGB555 standard NDS/GBA : bits 0-4=R, 5-9=G, 10-14=B (identique
         * à PAL_ColorFromRGB555 déjà présent dans pal_graphics.h) */
        uint8_t r5 = (entry >> 0) & 0x1F;
        uint8_t g5 = (entry >> 5) & 0x1F;
        uint8_t b5 = (entry >> 10) & 0x1F;
        out->colors[i].r = (uint8_t)((r5 * 255) / 31);
        out->colors[i].g = (uint8_t)((g5 * 255) / 31);
        out->colors[i].b = (uint8_t)((b5 * 255) / 31);
        out->colors[i].a = 255;
    }
    out->num_colors = numColors;
    return true;
}

/* --- NCGR : tuiles --- */
bool NdsGfx_ParseTileset(const void* data, size_t size, NdsGfx_Tileset* out) {
    const uint8_t* buf = (const uint8_t*)data;
    if (!buf || size < 0x20) return false;

    if (memcmp(buf, "RGCN", 4) != 0) {
        return false;
    }

    const uint8_t* charHeader = buf + 0x10;
    if (memcmp(charHeader, "RAHC", 4) != 0) {
        return false;
    }

    /* [0xC] : 3 = 4bpp, autre valeur = 8bpp — même convention que NCLR */
    bool is_8bpp = (charHeader[0xC] != 3);
    int bitDepth = is_8bpp ? 8 : 4;
    int tileSizeBytes = bitDepth * 8; /* 4bpp -> 32 octets/tuile, 8bpp -> 64 octets/tuile */

    /* [0x4] (u32 LE) = taille du chunk RAHC entier (header 0x20 inclus).
     * On en déduit la taille des données de tuiles brutes. */
    uint32_t chunkSize = ReadU32LE(charHeader + 0x4);
    if (chunkSize < 0x20) return false;
    uint32_t tileDataSize = chunkSize - 0x20;

    const uint8_t* imageData = charHeader + 0x20;
    size_t available = size - (size_t)(imageData - buf);
    if (tileDataSize > available) {
        tileDataSize = (uint32_t)available; /* fichier tronqué : on prend ce qu'il y a */
    }

    int numTiles = (int)(tileDataSize / tileSizeBytes);
    if (numTiles <= 0) {
        out->tile_indices = NULL;
        out->num_tiles = 0;
        out->is_8bpp = is_8bpp;
        return true;
    }

    uint8_t* indices = (uint8_t*)malloc((size_t)numTiles * 64); /* toujours 64 pixels/tuile (8x8) */
    if (!indices) return false;

    for (int t = 0; t < numTiles; t++) {
        const uint8_t* src = imageData + (size_t)t * tileSizeBytes;
        uint8_t* dst = indices + (size_t)t * 64;

        if (!is_8bpp) {
            /* 4bpp : chaque octet source contient 2 pixels (nibble bas puis haut) */
            for (int b = 0; b < 32; b++) {
                dst[b * 2 + 0] = src[b] & 0x0F;
                dst[b * 2 + 1] = (src[b] >> 4) & 0x0F;
            }
        } else {
            /* 8bpp : un octet = un pixel, copie directe */
            memcpy(dst, src, 64);
        }
    }

    out->tile_indices = indices;
    out->num_tiles = numTiles;
    out->is_8bpp = is_8bpp;
    return true;
}

void NdsGfx_FreeTileset(NdsGfx_Tileset* tileset) {
    if (tileset && tileset->tile_indices) {
        free(tileset->tile_indices);
        tileset->tile_indices = NULL;
        tileset->num_tiles = 0;
    }
}

/* --- NSCR : plan de tuiles --- */
bool NdsGfx_ParseScreen(const void* data, size_t size, NdsGfx_Screen* out) {
    const uint8_t* buf = (const uint8_t*)data;
    if (!buf || size < 0x24) return false;

    if (memcmp(buf, "RCSN", 4) != 0) {
        return false;
    }

    const uint8_t* scrnHeader = buf + 0x10;
    /* Le tag stocké est "NRCS" (RCSN à l'envers, comme TTLP/RAHC ci-dessus) */
    if (memcmp(scrnHeader, "NRCS", 4) != 0) {
        return false;
    }

    /* [0x08] u16 LE = largeur en PIXELS, [0x0A] u16 LE = hauteur en PIXELS
     * (vérifié dans WriteNtrScreen : NSCRHeader[0x8]=width*8 etc.) */
    int width_px = ReadU16LE(scrnHeader + 0x8);
    int height_px = ReadU16LE(scrnHeader + 0xA);
    int tiles_wide = width_px / 8;
    int tiles_high = height_px / 8;
    if (tiles_wide <= 0 || tiles_high <= 0) return false;

    /* [0x10] u32 LE = taille des données d'entrées qui suivent (chunk_size - 0x14) */
    uint32_t dataSize = ReadU32LE(scrnHeader + 0x10);
    int numEntries = (int)(dataSize / 2);
    int expectedEntries = tiles_wide * tiles_high;
    if (numEntries > expectedEntries) numEntries = expectedEntries;

    const uint8_t* entryData = scrnHeader + 0x14;
    size_t available = size - (size_t)(entryData - buf);
    if ((size_t)numEntries * 2 > available) {
        numEntries = (int)(available / 2);
    }

    uint16_t* entries = (uint16_t*)calloc((size_t)tiles_wide * tiles_high, sizeof(uint16_t));
    if (!entries) return false;

    for (int i = 0; i < numEntries; i++) {
        entries[i] = ReadU16LE(entryData + i * 2);
    }

    out->entries = entries;
    out->tiles_wide = tiles_wide;
    out->tiles_high = tiles_high;
    return true;
}

void NdsGfx_FreeScreen(NdsGfx_Screen* screen) {
    if (screen && screen->entries) {
        free(screen->entries);
        screen->entries = NULL;
        screen->tiles_wide = 0;
        screen->tiles_high = 0;
    }
}

/* --- Composition finale : screen + tileset + palette -> RGBA8888 --- */
bool NdsGfx_RenderScreenToRGBA(const NdsGfx_Screen* screen,
                                const NdsGfx_Tileset* tileset,
                                const NdsGfx_Palette* palette,
                                uint32_t* out_rgba,
                                int out_stride_px) {
    if (!screen || !tileset || !palette || !out_rgba) return false;
    if (!screen->entries || !tileset->tile_indices) return false;

    for (int ty = 0; ty < screen->tiles_high; ty++) {
        for (int tx = 0; tx < screen->tiles_wide; tx++) {
            uint16_t entry = screen->entries[ty * screen->tiles_wide + tx];

            /* Format d'entrée NDS standard (identique GBA + 4 bits de palette) :
             *   bits 0-9  : numéro de tuile
             *   bit  10   : miroir horizontal
             *   bit  11   : miroir vertical
             *   bits 12-15: numéro de palette (pertinent seulement en 4bpp,
             *               où chaque tuile choisit sa sous-palette de 16
             *               couleurs parmi les 16 sous-palettes possibles) */
            int tile_num = entry & 0x03FF;
            bool hflip = (entry & 0x0400) != 0;
            bool vflip = (entry & 0x0800) != 0;
            int pal_num = (entry >> 12) & 0x0F;

            if (tile_num >= tileset->num_tiles) continue; /* tuile hors limites : on saute plutôt que planter */

            const uint8_t* tile_pixels = tileset->tile_indices + (size_t)tile_num * 64;
            int pal_base = tileset->is_8bpp ? 0 : (pal_num * 16); /* décalage dans la palette pour le 4bpp */

            for (int py = 0; py < 8; py++) {
                int src_y = vflip ? (7 - py) : py;
                for (int px = 0; px < 8; px++) {
                    int src_x = hflip ? (7 - px) : px;
                    uint8_t color_index = tile_pixels[src_y * 8 + src_x];

                    int dst_x = tx * 8 + px;
                    int dst_y = ty * 8 + py;
                    uint32_t* dst_pixel = &out_rgba[dst_y * out_stride_px + dst_x];

                    /* Index 0 = transparent, convention standard NDS pour
                     * le 4bpp comme le 8bpp (couleur 0 de chaque sous-palette
                     * n'est jamais dessinée). */
                    if (color_index == 0) {
                        *dst_pixel = 0x00000000;
                        continue;
                    }

                    int final_index = pal_base + color_index;
                    if (final_index >= palette->num_colors) {
                        *dst_pixel = 0x00000000;
                        continue;
                    }

                    NdsGfx_Color c = palette->colors[final_index];
                    /* RGBA8888 empaqueté en petit-boutiste comme attendu par
                     * sceGu (GU_PSM_8888) dans pal_graphics_psp.c */
                    *dst_pixel = ((uint32_t)c.a << 24) | ((uint32_t)c.b << 16) |
                                 ((uint32_t)c.g << 8) | (uint32_t)c.r;
                }
            }
        }
    }

    return true;
}
