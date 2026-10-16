#ifndef PSP_ASSET_LOADER_H
#define PSP_ASSET_LOADER_H

/* Runtime loader for the tree produced by tools/build_assets.mjs.
 * Files are intentionally retained in their Nitro form: NdsGfx performs the
 * decoding only when a screen or cell needs to be drawn. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PSP_ASSET_CACHE_SLOTS 8
#define PSP_ASSET_ROOT_MAX 240
#define PSP_ASSET_PATH_MAX 192

typedef struct {
    const uint8_t *data;
    size_t size;
} PspAssetBlob;

typedef struct {
    char path[PSP_ASSET_PATH_MAX];
    uint8_t *data;
    size_t size;
    unsigned int references;
    unsigned int last_used;
} PspAssetCacheSlot;

typedef struct {
    char root[PSP_ASSET_ROOT_MAX];
    unsigned int clock;
    PspAssetCacheSlot slots[PSP_ASSET_CACHE_SLOTS];
} PspAssetStore;

/* root example: "ms0:/PSP/GAME/POKEPLATINUM/psp-assets". */
bool PspAssetStore_Init(PspAssetStore *store, const char *root);
void PspAssetStore_Shutdown(PspAssetStore *store);

/* relative_path must use forward slashes and be relative to root.  Absolute
 * paths, drive prefixes, backslashes and traversal segments are rejected. */
bool PspAssetStore_Load(PspAssetStore *store, const char *relative_path, PspAssetBlob *out);
void PspAssetStore_Release(PspAssetStore *store, PspAssetBlob blob);

#endif
