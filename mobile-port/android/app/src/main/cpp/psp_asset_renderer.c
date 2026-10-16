#include "psp_asset_renderer.h"
#include "pal_graphics_psp_ext.h"

BOOL PspAssets_DrawTilemap(PspAssetStore *store, PAL_Surface *surface,
                           const char *nscr_path, const char *ncgr_path, const char *nclr_path,
                           int dst_x, int dst_y) {
    PspAssetBlob nscr = {0}, ncgr = {0}, nclr = {0};
    BOOL ok = FALSE;
    if (PspAssetStore_Load(store, nscr_path, &nscr) &&
        PspAssetStore_Load(store, ncgr_path, &ncgr) &&
        PspAssetStore_Load(store, nclr_path, &nclr)) {
        ok = PAL_Graphics_DrawTilemapFull(surface, nscr.data, nscr.size, ncgr.data, ncgr.size,
                                          nclr.data, nclr.size, dst_x, dst_y);
    }
    PspAssetStore_Release(store, nclr);
    PspAssetStore_Release(store, ncgr);
    PspAssetStore_Release(store, nscr);
    return ok;
}

BOOL PspAssets_DrawCell(PspAssetStore *store, PAL_Surface *surface,
                        const char *ncer_path, int cell_index,
                        const char *ncgr_path, const char *nclr_path,
                        int dst_x, int dst_y, int width, int height) {
    PspAssetBlob ncer = {0}, ncgr = {0}, nclr = {0};
    BOOL ok = FALSE;
    if (PspAssetStore_Load(store, ncer_path, &ncer) &&
        PspAssetStore_Load(store, ncgr_path, &ncgr) &&
        PspAssetStore_Load(store, nclr_path, &nclr)) {
        ok = PAL_Graphics_DrawNcerCell(surface, ncer.data, ncer.size, cell_index,
                                       ncgr.data, ncgr.size, nclr.data, nclr.size,
                                       dst_x, dst_y, width, height);
    }
    PspAssetStore_Release(store, nclr);
    PspAssetStore_Release(store, ncgr);
    PspAssetStore_Release(store, ncer);
    return ok;
}
