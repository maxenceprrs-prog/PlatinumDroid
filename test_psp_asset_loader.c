/* Host-side test.  Pass the generated psp-assets directory as argv[1]. */
#include "psp_asset_loader.h"
#include <assert.h>
#include <string.h>

int main(int argc, char **argv) {
    assert(argc == 2);
    PspAssetStore store;
    PspAssetBlob screen, tiles, palette;
    assert(PspAssetStore_Init(&store, argv[1]));
    assert(!PspAssetStore_Load(&store, "../not-allowed", &screen));
    assert(!PspAssetStore_Load(&store, "C:/not-allowed", &screen));
    assert(PspAssetStore_Load(&store, "graphics/bag/bag_ui_main.NSCR", &screen));
    assert(PspAssetStore_Load(&store, "prebuilt/data/smptm_koori.NCGR", &tiles));
    assert(PspAssetStore_Load(&store, "prebuilt/data/smptm_koori.NCLR", &palette));
    assert(screen.size > 4 && tiles.size > 4 && palette.size > 4);
    assert(memcmp(screen.data, "RCSN", 4) == 0);
    assert(memcmp(tiles.data, "RGCN", 4) == 0);
    assert(memcmp(palette.data, "RLCN", 4) == 0);
    PspAssetStore_Release(&store, palette);
    PspAssetStore_Release(&store, tiles);
    PspAssetStore_Release(&store, screen);
    PspAssetStore_Shutdown(&store);
    return 0;
}
