#ifndef NDS_GFX_H
#define NDS_GFX_H

/**
 * @file nds_gfx.h
 * @brief Décodeur des formats graphiques Nitro (DS) vers pixels RGBA8888.
 *
 * Couvre les trois formats dont a besoin PAL_Graphics_DrawTilemap pour
 * afficher quoi que ce soit de réel à l'écran :
 *
 *   - NCLR ("RLCN") : une ou plusieurs palettes de couleurs RGB555.
 *   - NCGR ("RGCN") : les tuiles brutes 8x8 pixels, en indices de
 *                     couleur (4bpp = 16 couleurs/tuile, ou 8bpp = 256).
 *   - NSCR ("RCSN") : le plan de tuiles (screen map) — pour chaque
 *                     case, quel numéro de tuile utiliser, avec quelle
 *                     palette, et si elle est retournée horizontalement
 *                     / verticalement.
 *
 * Ces formats sont documentés publiquement depuis longtemps par la scène
 * homebrew DS (NitroPaint, Tinke, et d'autres outils open source les
 * implémentent tous). Ce fichier ne fait qu'une chose : transformer ces
 * trois blobs en un tableau de pixels RGBA8888, indépendamment de toute
 * plateforme cible (utilisable aussi bien depuis pal_graphics_psp.c que
 * depuis la version SDL3).
 *
 * IMPORTANT : ce décodeur attend les fichiers NCLR/NCGR/NSCR tels
 * qu'extraits de TA ROM légalement dumpée. Il ne contient et ne
 * distribue aucune donnée de jeu — seulement le code qui sait les lire.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint8_t r, g, b, a;
} NdsGfx_Color;

/* --- NCLR : palette(s) de couleurs --- */
typedef struct {
    NdsGfx_Color colors[256]; /* jusqu'à 16 palettes de 16 couleurs (4bpp)
                                  ou 1 palette de 256 couleurs (8bpp) */
    int num_colors;           /* nombre de couleurs réellement présentes */
    bool is_8bpp;
} NdsGfx_Palette;

/* --- NCGR : tuiles décodées en indices de couleur (pas encore en RGBA,
       car un même jeu de tuiles peut être affiché avec des palettes
       différentes selon le contexte) --- */
typedef struct {
    uint8_t* tile_indices; /* num_tiles * 64 octets, 1 octet = 1 pixel (index 0-255) */
    int num_tiles;
    bool is_8bpp;
} NdsGfx_Tileset;

/* --- NSCR : plan de tuiles (screen map) --- */
typedef struct {
    uint16_t* entries;   /* tiles_wide * tiles_high entrées brutes NSCR */
    int tiles_wide;
    int tiles_high;
} NdsGfx_Screen;

/**
 * Parse un fichier NCLR complet (avec en-tête Nitro) en palette utilisable.
 * @return true si le parsing a réussi.
 */
bool NdsGfx_ParsePalette(const void* data, size_t size, NdsGfx_Palette* out);

/**
 * Parse un fichier NCGR complet en jeu de tuiles (indices de couleur).
 * Le buffer alloué dans out->tile_indices doit être libéré avec
 * NdsGfx_FreeTileset().
 */
bool NdsGfx_ParseTileset(const void* data, size_t size, NdsGfx_Tileset* out);
void NdsGfx_FreeTileset(NdsGfx_Tileset* tileset);

/**
 * Parse un fichier NSCR complet en plan de tuiles.
 * Le buffer alloué dans out->entries doit être libéré avec NdsGfx_FreeScreen().
 */
bool NdsGfx_ParseScreen(const void* data, size_t size, NdsGfx_Screen* out);
void NdsGfx_FreeScreen(NdsGfx_Screen* screen);

/**
 * Compose l'image finale : combine le plan de tuiles + le jeu de tuiles +
 * la palette pour produire une image RGBA8888 complète.
 *
 * @param screen        Plan de tuiles parsé (NdsGfx_ParseScreen)
 * @param tileset       Tuiles parsées (NdsGfx_ParseTileset)
 * @param palette       Palette parsée (NdsGfx_ParsePalette)
 * @param out_rgba      Buffer de sortie, doit contenir au moins
 *                      (screen->tiles_wide*8) * (screen->tiles_high*8) pixels
 * @param out_stride_px Largeur du buffer de sortie en pixels (peut être
 *                      plus grand que la largeur réelle, ex: texture GU 256px)
 * @return true si la composition a réussi
 */
bool NdsGfx_RenderScreenToRGBA(const NdsGfx_Screen* screen,
                                const NdsGfx_Tileset* tileset,
                                const NdsGfx_Palette* palette,
                                uint32_t* out_rgba,
                                int out_stride_px);

#endif /* NDS_GFX_H */
