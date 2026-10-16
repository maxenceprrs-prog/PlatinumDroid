#include "psp_asset_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool IsSafeRelativePath(const char *path) {
    if (!path || !path[0] || path[0] == '/' || strchr(path, '\\') || strchr(path, ':')) return false;
    /* The build pipeline only emits normal relative paths.  Refuse traversal
     * rather than normalising it, so a game config cannot escape asset root. */
    for (const char *part = path; *part;) {
        const char *end = strchr(part, '/');
        size_t length = end ? (size_t)(end - part) : strlen(part);
        if (!length || (length == 1 && part[0] == '.') ||
            (length == 2 && part[0] == '.' && part[1] == '.')) return false;
        part = end ? end + 1 : part + length;
    }
    return true;
}

static void FreeSlot(PspAssetCacheSlot *slot) {
    free(slot->data);
    memset(slot, 0, sizeof(*slot));
}

bool PspAssetStore_Init(PspAssetStore *store, const char *root) {
    size_t length;
    if (!store || !root) return false;
    length = strlen(root);
    if (!length || length >= sizeof(store->root)) return false;
    memset(store, 0, sizeof(*store));
    memcpy(store->root, root, length + 1);
    /* A trailing slash is accepted but not required. */
    while (length > 0 && store->root[length - 1] == '/') store->root[--length] = '\0';
    return length != 0;
}

void PspAssetStore_Shutdown(PspAssetStore *store) {
    if (!store) return;
    for (int i = 0; i < PSP_ASSET_CACHE_SLOTS; ++i) FreeSlot(&store->slots[i]);
    memset(store, 0, sizeof(*store));
}

static PspAssetCacheSlot *ChooseSlot(PspAssetStore *store) {
    PspAssetCacheSlot *candidate = NULL;
    for (int i = 0; i < PSP_ASSET_CACHE_SLOTS; ++i) {
        PspAssetCacheSlot *slot = &store->slots[i];
        if (!slot->data) return slot;
        if (!slot->references && (!candidate || slot->last_used < candidate->last_used)) candidate = slot;
    }
    return candidate;
}

bool PspAssetStore_Load(PspAssetStore *store, const char *relative_path, PspAssetBlob *out) {
    char full_path[PSP_ASSET_ROOT_MAX + PSP_ASSET_PATH_MAX + 2];
    FILE *file;
    long length;
    PspAssetCacheSlot *slot;
    if (!store || !out || !IsSafeRelativePath(relative_path) || strlen(relative_path) >= PSP_ASSET_PATH_MAX) return false;
    out->data = NULL; out->size = 0;
    for (int i = 0; i < PSP_ASSET_CACHE_SLOTS; ++i) {
        slot = &store->slots[i];
        if (slot->data && strcmp(slot->path, relative_path) == 0) {
            slot->references++; slot->last_used = ++store->clock;
            out->data = slot->data; out->size = slot->size;
            return true;
        }
    }
    slot = ChooseSlot(store);
    if (!slot) return false; /* every cache entry is currently in use */
    int written = snprintf(full_path, sizeof(full_path), "%s/%s", store->root, relative_path);
    if (written < 0 || written >= (int)sizeof(full_path)) return false;
    file = fopen(full_path, "rb");
    if (!file) return false;
    if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) <= 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file); return false;
    }
    uint8_t *data = (uint8_t *)malloc((size_t)length);
    if (!data || fread(data, 1, (size_t)length, file) != (size_t)length) {
        free(data); fclose(file); return false;
    }
    fclose(file);
    FreeSlot(slot);
    memcpy(slot->path, relative_path, strlen(relative_path) + 1);
    slot->data = data; slot->size = (size_t)length;
    slot->references = 1; slot->last_used = ++store->clock;
    out->data = data; out->size = slot->size;
    return true;
}

void PspAssetStore_Release(PspAssetStore *store, PspAssetBlob blob) {
    if (!store || !blob.data) return;
    for (int i = 0; i < PSP_ASSET_CACHE_SLOTS; ++i) {
        PspAssetCacheSlot *slot = &store->slots[i];
        if (slot->data == blob.data) {
            if (slot->references) slot->references--;
            return;
        }
    }
}
