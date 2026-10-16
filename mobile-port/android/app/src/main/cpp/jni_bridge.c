/**
 * @file jni_bridge.c
 * @brief Pont JNI pour Android reliant l'activité Java à notre moteur C portable
 */

#include <jni.h>
#include <string.h>
#include <stdlib.h>
#include "platform/pal_graphics.h"
#include "platform/pal_input.h"
#include "scene_title.h"
#include "psp_asset_loader.h"

#define DS_WIDTH  256
#define DS_HEIGHT 192

static PspAssetStore s_asset_store;
static PAL_Surface*  s_main_surf = NULL;
static PAL_Surface*  s_sub_surf = NULL;
static u32           s_current_input = 0;

/* Structure interne PAL_Surface identique à scene_title.c */
typedef struct {
    uint32_t *pixels;
    int logical_w;
    int logical_h;
    void *gu_texture;
    int dirty;
} SurfaceInternal;

static uint32_t s_main_pixels[DS_WIDTH * DS_HEIGHT];
static uint32_t s_sub_pixels[DS_WIDTH * DS_HEIGHT];
static SurfaceInternal s_main_internal;
static SurfaceInternal s_sub_internal;

JNIEXPORT void JNICALL
Java_com_game_pokeplatinum_MainActivity_nativeInit(JNIEnv *env, jclass clazz, jstring asset_root) {
    (void)clazz;
    const char *path = (*env)->GetStringUTFChars(env, asset_root, NULL);

    memset(s_main_pixels, 0, sizeof(s_main_pixels));
    memset(s_sub_pixels, 0, sizeof(s_sub_pixels));

    s_main_internal.pixels = s_main_pixels;
    s_main_internal.logical_w = DS_WIDTH;
    s_main_internal.logical_h = DS_HEIGHT;
    s_main_internal.dirty = 1;

    s_sub_internal.pixels = s_sub_pixels;
    s_sub_internal.logical_w = DS_WIDTH;
    s_sub_internal.logical_h = DS_HEIGHT;
    s_sub_internal.dirty = 1;

    s_main_surf = (PAL_Surface*)&s_main_internal;
    s_sub_surf = (PAL_Surface*)&s_sub_internal;

    PspAssetStore_Init(&s_asset_store, path);
    (*env)->ReleaseStringUTFChars(env, asset_root, path);

    SceneTitle_Init(&s_asset_store, s_main_surf);
}

JNIEXPORT void JNICALL
Java_com_game_pokeplatinum_MainActivity_nativeShutdown(JNIEnv *env, jclass clazz) {
    (void)env; (void)clazz;
    SceneTitle_Shutdown();
    PspAssetStore_Shutdown(&s_asset_store);
}

JNIEXPORT void JNICALL
Java_com_game_pokeplatinum_MainActivity_nativeUpdate(JNIEnv *env, jclass clazz,
                                                     jint input_flags, jint touch_x, jint touch_y, jboolean touch_down) {
    (void)env; (void)clazz; (void)touch_x; (void)touch_y; (void)touch_down;

    s_current_input = (u32)input_flags;
    SceneTitle_Update(s_main_surf, s_current_input);
}

JNIEXPORT void JNICALL
Java_com_game_pokeplatinum_MainActivity_nativeRender(JNIEnv *env, jclass clazz,
                                                     jintArray main_pixels_arr, jintArray sub_pixels_arr) {
    (void)clazz;

    jint *main_dest = (*env)->GetIntArrayElements(env, main_pixels_arr, NULL);
    jint *sub_dest  = (*env)->GetIntArrayElements(env, sub_pixels_arr, NULL);

    if (main_dest && sub_dest) {
        /* Convertit ABGR / RGBA natif pour Android Bitmap ARGB_8888 */
        for (int i = 0; i < DS_WIDTH * DS_HEIGHT; i++) {
            uint32_t c = s_main_pixels[i];
            /* RGBA -> ARGB */
            uint8_t r = (c >> 0) & 0xFF;
            uint8_t g = (c >> 8) & 0xFF;
            uint8_t b = (c >> 16) & 0xFF;
            uint8_t a = (c >> 24) & 0xFF;
            if (a == 0) a = 0xFF;
            main_dest[i] = (jint)((a << 24) | (r << 16) | (g << 8) | b);
        }

        for (int i = 0; i < DS_WIDTH * DS_HEIGHT; i++) {
            uint32_t c = s_sub_pixels[i];
            uint8_t r = (c >> 0) & 0xFF;
            uint8_t g = (c >> 8) & 0xFF;
            uint8_t b = (c >> 16) & 0xFF;
            uint8_t a = (c >> 24) & 0xFF;
            if (a == 0) a = 0xFF;
            sub_dest[i] = (jint)((a << 24) | (r << 16) | (g << 8) | b);
        }

        (*env)->ReleaseIntArrayElements(env, main_pixels_arr, main_dest, 0);
        (*env)->ReleaseIntArrayElements(env, sub_pixels_arr, sub_dest, 0);
    }
}
