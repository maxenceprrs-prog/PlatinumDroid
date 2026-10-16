#ifndef PAL_GRAPHICS_PSP_EXT_H
#define PAL_GRAPHICS_PSP_EXT_H

/**
 * @file pal_graphics_psp_ext.h
 * @brief Extension PSP de PAL_Graphics : dessine un tilemap DS complet.
 *
 * L'interface PAL_Graphics_DrawTilemap() héritée de pal_graphics.h ne
 * prend qu'un seul blob (le plan de tuiles NSCR), ce qui ne suffit pas
 * à produire une image — il faut aussi le jeu de tuiles (NCGR) et la
 * palette (NCLR). Cette fonction complète comble ce manque pour le
 * portage PSP ; voir nds_gfx.h pour le décodeur sous-jacent.
 */

#include "platform/pal_graphics.h"
#include <stddef.h>

/**
 * Décode et dessine un tilemap DS complet (NSCR + NCGR + NCLR) dans une
 * surface PAL, à la position (dst_x, dst_y) en pixels.
 * @return TRUE si le décodage et le dessin ont réussi, FALSE sinon
 *         (fichier invalide/tronqué — dans ce cas rien n'est dessiné).
 */
BOOL PAL_Graphics_DrawTilemapFull(PAL_Surface* surf,
                                   const void* nscr_data, size_t nscr_size,
                                   const void* ncgr_data, size_t ncgr_size,
                                   const void* nclr_data, size_t nclr_size,
                                   int dst_x, int dst_y);

/** Decode one standard NCER cell plus its NCGR/NCLR data and composite it.
 * The NCER must use normal, 1D OAM mapping; unsupported affine/2D cells fail
 * cleanly and leave the surface unchanged. */
BOOL PAL_Graphics_DrawNcerCell(PAL_Surface* surf,
                                const void* ncer_data, size_t ncer_size, int cell_index,
                                const void* ncgr_data, size_t ncgr_size,
                                const void* nclr_data, size_t nclr_size,
                                int dst_x, int dst_y, int cell_width, int cell_height);

#endif /* PAL_GRAPHICS_PSP_EXT_H */
