#ifndef PSP_ASSET_RENDERER_H
#define PSP_ASSET_RENDERER_H

#include "psp_asset_loader.h"
#include "platform/pal_graphics.h"

BOOL PspAssets_DrawTilemap(PspAssetStore *store, PAL_Surface *surface,
                           const char *nscr_path, const char *ncgr_path, const char *nclr_path,
                           int dst_x, int dst_y);
BOOL PspAssets_DrawCell(PspAssetStore *store, PAL_Surface *surface,
                        const char *ncer_path, int cell_index,
                        const char *ncgr_path, const char *nclr_path,
                        int dst_x, int dst_y, int width, int height);

#endif
