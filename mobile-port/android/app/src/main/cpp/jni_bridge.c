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
#include "scene_game.h"
#include "psp_asset_loader.h"
#include "bgm_synth.h"

#define DS_WIDTH  256
#define DS_HEIGHT 192

enum {
    SCENE_MODE_TITLE = 0,
    SCENE_MODE_GAME  = 1
};

static int           s_active_scene = SCENE_MODE_TITLE;
static PspAssetStore s_asset_store;
static PAL_Surface*  s_main_surf = NULL;
static PAL_Surface*  s_sub_surf = NULL;
static u32           s_current_input = 0;
static u32           s_last_input = 0;
static int           s_repeat_counter = 0;

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

    s_last_input = 0;
    s_repeat_counter = 0;

    PspAssetStore_Init(&s_asset_store, path);
    (*env)->ReleaseStringUTFChars(env, asset_root, path);

    SceneTitle_Init(&s_asset_store, s_main_surf);
    BgmSynth_PlayTrack(BGM_TRACK_TITLE);
}

JNIEXPORT void JNICALL
Java_com_game_pokeplatinum_MainActivity_nativeShutdown(JNIEnv *env, jclass clazz) {
    (void)env; (void)clazz;
    BgmSynth_Stop();
    SceneTitle_Shutdown();
    SceneGame_Shutdown();
    PspAssetStore_Shutdown(&s_asset_store);
}

JNIEXPORT void JNICALL
Java_com_game_pokeplatinum_MainActivity_nativeUpdate(JNIEnv *env, jclass clazz,
                                                     jint input_flags, jint touch_x, jint touch_y, jboolean touch_down) {
    (void)env; (void)clazz;

    u32 held = (u32)input_flags;
    u32 just_pressed = held & ~s_last_input;
    u32 dir_mask = PAL_BUTTON_UP | PAL_BUTTON_DOWN | PAL_BUTTON_LEFT | PAL_BUTTON_RIGHT;

    u32 to_send = just_pressed;

    /* Auto-repeat pour la navigation fluide dans les menus */
    if (held & dir_mask) {
        if ((s_last_input & dir_mask) == (held & dir_mask)) {
            s_repeat_counter++;
            if (s_repeat_counter > 25 && (s_repeat_counter % 8) == 0) {
                to_send |= (held & dir_mask);
            }
        } else {
            s_repeat_counter = 0;
        }
    } else {
        s_repeat_counter = 0;
    }

    s_last_input = held;
    s_current_input = to_send;

    if (s_active_scene == SCENE_MODE_TITLE) {
        SceneTitleResult res = SceneTitle_Update(s_main_surf, s_current_input, touch_x, touch_y, touch_down);
        if (res == SCENE_TITLE_NEW_GAME || res == SCENE_TITLE_CONTINUE) {
            s_active_scene = SCENE_MODE_GAME;
            SceneGame_Init(&s_asset_store, s_main_surf, s_sub_surf, (res == SCENE_TITLE_NEW_GAME));
            BgmSynth_PlayTrack(BGM_TRACK_TWINLEAF);
        }
    } else if (s_active_scene == SCENE_MODE_GAME) {
        SceneGame_Update(s_main_surf, s_sub_surf, to_send, held, touch_x, touch_y, touch_down);
    }
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
