/**
 * @file jni_bridge.c
 * @brief Pont JNI pour Android reliant l'activité Java à notre moteur C portable
 */

#include <jni.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "platform/pal_graphics.h"
#include "platform/pal_input.h"
#include "scene_title.h"
#include "psp_asset_loader.h"

#define DS_WIDTH  256
#define DS_HEIGHT 192

static PspAssetStore s_asset_store;
static PAL_Surface*  s_main_surf = NULL;
static PAL_Surface*  s_sub_surf = NULL;
static u32           s_current_input_held = 0;
static u32           s_prev_input_held = 0;

typedef enum {
    GAME_STATE_TITLE = 0,
    GAME_STATE_FIELD_PLACEHOLDER = 1
} GameState;

static GameState s_game_state = GAME_STATE_TITLE;
static int s_player_x = DS_WIDTH / 2;
static int s_player_y = DS_HEIGHT / 2;
static int s_frame_counter = 0;

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

#define RGBA(r,g,b,a) (((uint32_t)(a)<<24)|((uint32_t)(b)<<16)|((uint32_t)(g)<<8)|(uint32_t)(r))

static void FillSurface(uint32_t *pixels, uint32_t color) {
    if (!pixels) return;
    for (int i = 0; i < DS_WIDTH * DS_HEIGHT; ++i) {
        pixels[i] = color;
    }
}

static void FillRect(uint32_t *pixels, int x, int y, int w, int h, uint32_t color) {
    if (!pixels || w <= 0 || h <= 0) return;
    int x0 = (x < 0) ? 0 : x;
    int y0 = (y < 0) ? 0 : y;
    int x1 = (x + w > DS_WIDTH) ? DS_WIDTH : x + w;
    int y1 = (y + h > DS_HEIGHT) ? DS_HEIGHT : y + h;
    for (int py = y0; py < y1; ++py) {
        uint32_t *row = &pixels[py * DS_WIDTH];
        for (int px = x0; px < x1; ++px) {
            row[px] = color;
        }
    }
}

static void RenderFieldPlaceholder(void) {
    s_frame_counter++;

    for (int y = 0; y < DS_HEIGHT; ++y) {
        uint8_t r = (uint8_t)(14 + (y * 20 / DS_HEIGHT));
        uint8_t g = (uint8_t)(90 + (y * 70 / DS_HEIGHT));
        uint8_t b = (uint8_t)(40 + (y * 20 / DS_HEIGHT));
        uint32_t color = RGBA(r, g, b, 255);
        for (int x = 0; x < DS_WIDTH; ++x) {
            s_main_pixels[y * DS_WIDTH + x] = color;
        }
    }

    for (int y = 0; y < DS_HEIGHT; y += 16) {
        FillRect(s_main_pixels, 0, y, DS_WIDTH, 1, RGBA(35, 130, 60, 255));
    }
    for (int x = 0; x < DS_WIDTH; x += 16) {
        FillRect(s_main_pixels, x, 0, 1, DS_HEIGHT, RGBA(35, 130, 60, 255));
    }

    FillRect(s_main_pixels, s_player_x - 4, s_player_y - 4, 9, 9, RGBA(255, 240, 80, 255));
    FillRect(s_main_pixels, s_player_x - 2, s_player_y - 2, 5, 5, RGBA(255, 90, 40, 255));

    FillSurface(s_sub_pixels, RGBA(20, 25, 45, 255));
    FillRect(s_sub_pixels, 16, 16, DS_WIDTH - 32, 32, RGBA(45, 55, 90, 255));
    FillRect(s_sub_pixels, 16, 60, DS_WIDTH - 32, 32, RGBA(45, 55, 90, 255));
    FillRect(s_sub_pixels, 16, 104, DS_WIDTH - 32, 72, RGBA(30, 36, 62, 255));
    FillRect(s_sub_pixels, 24, 116, 48, 48, RGBA(70, 90, 160, 255));
    FillRect(s_sub_pixels, 184, 116, 48, 48, RGBA(150, 70, 90, 255));
}

static void EnterTitleState(void) {
    s_game_state = GAME_STATE_TITLE;
    s_prev_input_held = 0;
    SceneTitle_Init(&s_asset_store, s_main_surf);
    memset(s_sub_pixels, 0, sizeof(s_sub_pixels));
}

static void EnterFieldState(void) {
    s_game_state = GAME_STATE_FIELD_PLACEHOLDER;
    s_player_x = DS_WIDTH / 2;
    s_player_y = DS_HEIGHT / 2;
    s_frame_counter = 0;
}

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

    s_current_input_held = 0;
    s_prev_input_held = 0;
    EnterTitleState();
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

    s_current_input_held = (u32)input_flags;
    u32 pressed = s_current_input_held & ~s_prev_input_held;
    s_prev_input_held = s_current_input_held;

    if (s_game_state == GAME_STATE_TITLE) {
        SceneTitleResult result = SceneTitle_Update(s_main_surf, pressed);
        if (result == SCENE_TITLE_NEW_GAME || result == SCENE_TITLE_CONTINUE || result == SCENE_TITLE_OPTIONS) {
            EnterFieldState();
        }
        return;
    }

    const int speed = 2;
    if (s_current_input_held & PAL_BUTTON_LEFT)  s_player_x -= speed;
    if (s_current_input_held & PAL_BUTTON_RIGHT) s_player_x += speed;
    if (s_current_input_held & PAL_BUTTON_UP)    s_player_y -= speed;
    if (s_current_input_held & PAL_BUTTON_DOWN)  s_player_y += speed;

    if (s_player_x < 4) s_player_x = 4;
    if (s_player_y < 4) s_player_y = 4;
    if (s_player_x > DS_WIDTH - 5) s_player_x = DS_WIDTH - 5;
    if (s_player_y > DS_HEIGHT - 5) s_player_y = DS_HEIGHT - 5;

    if ((pressed & PAL_BUTTON_START) || (pressed & PAL_BUTTON_B)) {
        EnterTitleState();
        return;
    }

    RenderFieldPlaceholder();
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
