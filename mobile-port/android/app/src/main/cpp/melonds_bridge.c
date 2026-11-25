/**
 * @file melonds_bridge.c
 * @brief Pont JNI ultra-haute performance pour melonDS natif (libretro.so) ARM64
 * 
 * Optimisations majeures :
 * 1. Activation du JIT ARM64 (Recompilateur dynamique) : gain x5 à x10 par rapport à l'interpréteur C++.
 * 2. Rendu vidéo direct via AndroidBitmap_lockPixels (zéro copie JNI, zéro allocation d'arrays Java).
 * 3. melonDS Top/Bottom 256x384 ARGB8888 écrit directement dans le Bitmap GPU.
 * 4. Multi-threading melonDS Soft_Threaded activé.
 * 5. Audio ring buffer haute performance synchronisé.
 */

#include <jni.h>
#include <android/log.h>
#include <android/bitmap.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "include/libretro.h"

#define TAG "MelonDS_JNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

#define DS_WIDTH  256
#define DS_HEIGHT 192
#define TOTAL_FRAMEBUFFER_HEIGHT (DS_HEIGHT * 2) /* 384 */

/* Audio Ring Buffer (stereo 16-bit PCM @ 32768Hz) */
#define AUDIO_BUFFER_SIZE (32768 * 2) /* ~1 seconde */
static int16_t s_audio_ring[AUDIO_BUFFER_SIZE];
static volatile int s_audio_read_pos = 0;
static volatile int s_audio_write_pos = 0;

/* Pointeur direct vers le Bitmap cible pour la frame en cours */
static uint32_t *s_direct_bitmap_pixels = NULL;
static bool s_has_new_frame = false;

/* État des entrées */
static uint32_t s_input_mask = 0;
static int s_touch_x = 0;
static int s_touch_y = 0;
static bool s_is_touching = false;

/* Répertoires & Mémoire ROM */
static char s_save_dir[1024] = {0};
static void *s_rom_buffer = NULL;
static size_t s_rom_size = 0;
static bool s_core_initialized = false;
static bool s_game_loaded = false;

/* ------------------------------------------------------------------------- */
/* Callbacks Libretro requis par melonDS                                     */
/* ------------------------------------------------------------------------- */

static void cb_log(enum retro_log_level level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int android_level = ANDROID_LOG_INFO;
    if (level == RETRO_LOG_WARN) {
        android_level = ANDROID_LOG_WARN;
    } else if (level == RETRO_LOG_ERROR) {
        android_level = ANDROID_LOG_ERROR;
    }
    __android_log_vprint(android_level, "MelonDS_Core", fmt, args);
    va_end(args);
}

static bool cb_environment(unsigned cmd, void *data) {
    switch (cmd) {
        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
            const enum retro_pixel_format *fmt = (const enum retro_pixel_format *)data;
            if (*fmt == RETRO_PIXEL_FORMAT_XRGB8888 || *fmt == RETRO_PIXEL_FORMAT_RGB565) {
                return true;
            }
            return false;
        }
        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: {
            *(const char **)data = s_save_dir;
            return true;
        }
        case RETRO_ENVIRONMENT_GET_CAN_DUPE: {
            *(bool *)data = true;
            return true;
        }
        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
            struct retro_log_callback *cb = (struct retro_log_callback *)data;
            if (cb) {
                cb->log = cb_log;
                return true;
            }
            return false;
        }
        case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
        case RETRO_ENVIRONMENT_SET_VARIABLES:
        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
        case RETRO_ENVIRONMENT_SET_MESSAGE:
            return true;

        case RETRO_ENVIRONMENT_GET_VARIABLE: {
            struct retro_variable *var = (struct retro_variable *)data;
            if (var && var->key) {
                /* Démarrage direct ROM sans BIOS officiel (FreeBIOS) */
                if (strcmp(var->key, "melonds_boot_directly") == 0 || strcmp(var->key, "melonds_boots_to_menu") == 0) {
                    var->value = "enabled";
                    return true;
                }
                /* Disposition écran : Top/Bottom (256x384) */
                if (strcmp(var->key, "melonds_screen_layout") == 0) {
                    var->value = "Top/Bottom";
                    return true;
                }
                /* ACTIVATION CRITIQUE DU JIT ARM64 */
                if (strcmp(var->key, "melonds_jit_enable") == 0) {
                    var->value = "enabled";
                    return true;
                }
                if (strcmp(var->key, "melonds_jit_block_size") == 0) {
                    var->value = "32";
                    return true;
                }
                if (strcmp(var->key, "melonds_jit_branch_optimisations") == 0) {
                    var->value = "enabled";
                    return true;
                }
                if (strcmp(var->key, "melonds_jit_literal_optimisations") == 0) {
                    var->value = "enabled";
                    return true;
                }
                if (strcmp(var->key, "melonds_jit_fast_memory") == 0) {
                    var->value = "disabled"; /* Sécurité mémoire sur Android 16 */
                    return true;
                }
                /* Rendu multi-thread logiciel */
                if (strcmp(var->key, "melonds_threaded_renderer") == 0) {
                    var->value = "enabled";
                    return true;
                }
                /* Gestion tactile */
                if (strcmp(var->key, "melonds_touch_mode") == 0) {
                    var->value = "Touch";
                    return true;
                }
                if (strcmp(var->key, "melonds_screen_gap") == 0) {
                    var->value = "0";
                    return true;
                }
                if (strcmp(var->key, "melonds_console_mode") == 0) {
                    var->value = "DS";
                    return true;
                }
                if (strcmp(var->key, "melonds_audio_interpolation") == 0) {
                    var->value = "none"; /* 'none' est plus rapide et authentique DS que 'linear' */
                    return true;
                }
                if (strcmp(var->key, "melonds_opengl_renderer") == 0) {
                    var->value = "disabled";
                    return true;
                }
            }
            return false;
        }
        default:
            return false;
    }
}

static void cb_video_refresh(const void *data, unsigned width, unsigned height, size_t pitch) {
    if (!data) return;

    /* Si un Bitmap direct est verrouillé, on copie les 256x384 pixels en 1 seul memcpy ultra-rapide */
    if (s_direct_bitmap_pixels) {
        memcpy(s_direct_bitmap_pixels, data, DS_WIDTH * TOTAL_FRAMEBUFFER_HEIGHT * sizeof(uint32_t));
    }
    s_has_new_frame = true;
}

static void cb_audio_sample(int16_t left, int16_t right) {
    int next_write = (s_audio_write_pos + 2) % AUDIO_BUFFER_SIZE;
    if (next_write != s_audio_read_pos) {
        s_audio_ring[s_audio_write_pos] = left;
        s_audio_ring[s_audio_write_pos + 1] = right;
        s_audio_write_pos = next_write;
    }
}

static size_t cb_audio_sample_batch(const int16_t *data, size_t frames) {
    if (!data || frames == 0) return 0;

    for (size_t i = 0; i < frames; i++) {
        int next_write = (s_audio_write_pos + 2) % AUDIO_BUFFER_SIZE;
        if (next_write == s_audio_read_pos) {
            /* Tampon plein : on avance read pour éviter le lag audio */
            s_audio_read_pos = (s_audio_read_pos + 2) % AUDIO_BUFFER_SIZE;
        }
        s_audio_ring[s_audio_write_pos] = data[i * 2];
        s_audio_ring[s_audio_write_pos + 1] = data[i * 2 + 1];
        s_audio_write_pos = next_write;
    }
    return frames;
}

static void cb_input_poll(void) {
}

static int16_t cb_input_state(unsigned port, unsigned device, unsigned index, unsigned id) {
    if (port != 0) return 0;

    if (device == RETRO_DEVICE_JOYPAD) {
        switch (id) {
            case RETRO_DEVICE_ID_JOYPAD_A:      return (s_input_mask & (1 << 0)) ? 1 : 0;
            case RETRO_DEVICE_ID_JOYPAD_B:      return (s_input_mask & (1 << 1)) ? 1 : 0;
            case RETRO_DEVICE_ID_JOYPAD_SELECT: return (s_input_mask & (1 << 2)) ? 1 : 0;
            case RETRO_DEVICE_ID_JOYPAD_START:  return (s_input_mask & (1 << 3)) ? 1 : 0;
            case RETRO_DEVICE_ID_JOYPAD_RIGHT:  return (s_input_mask & (1 << 4)) ? 1 : 0;
            case RETRO_DEVICE_ID_JOYPAD_LEFT:   return (s_input_mask & (1 << 5)) ? 1 : 0;
            case RETRO_DEVICE_ID_JOYPAD_UP:     return (s_input_mask & (1 << 6)) ? 1 : 0;
            case RETRO_DEVICE_ID_JOYPAD_DOWN:   return (s_input_mask & (1 << 7)) ? 1 : 0;
            case RETRO_DEVICE_ID_JOYPAD_R:      return (s_input_mask & (1 << 8)) ? 1 : 0;
            case RETRO_DEVICE_ID_JOYPAD_L:      return (s_input_mask & (1 << 9)) ? 1 : 0;
            case RETRO_DEVICE_ID_JOYPAD_X:      return (s_input_mask & (1 << 10)) ? 1 : 0;
            case RETRO_DEVICE_ID_JOYPAD_Y:      return (s_input_mask & (1 << 11)) ? 1 : 0;
            default: return 0;
        }
    }

    if (device == RETRO_DEVICE_POINTER) {
        if (id == RETRO_DEVICE_ID_POINTER_PRESSED) {
            return s_is_touching ? 1 : 0;
        }
        if (s_is_touching) {
            if (id == RETRO_DEVICE_ID_POINTER_X) {
                int x = s_touch_x;
                if (x < 0) x = 0;
                if (x > 255) x = 255;
                return (int16_t)(((int)x * 65536 / 256) - 32768);
            }
            if (id == RETRO_DEVICE_ID_POINTER_Y) {
                int y = 192 + s_touch_y;
                if (y < 192) y = 192;
                if (y > 383) y = 383;
                return (int16_t)(((int)y * 65536 / 384) - 32768);
            }
        }
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
/* Fonctions JNI exposées à Android                                          */
/* ------------------------------------------------------------------------- */

JNIEXPORT jboolean JNICALL
Java_com_game_pokeplatinum_MainActivity_nativeInitEmulator(JNIEnv *env, jclass clazz,
                                                           jstring romPathStr,
                                                           jstring saveDirStr) {
    (void)clazz;
    const char *rom_path = (*env)->GetStringUTFChars(env, romPathStr, NULL);
    const char *save_dir = (*env)->GetStringUTFChars(env, saveDirStr, NULL);

    strncpy(s_save_dir, save_dir, sizeof(s_save_dir) - 1);

    LOGI("==> Initialisation du cœur melonDS optimisé avec ROM : %s", rom_path);

    retro_set_environment(cb_environment);
    retro_set_video_refresh(cb_video_refresh);
    retro_set_audio_sample(cb_audio_sample);
    retro_set_audio_sample_batch(cb_audio_sample_batch);
    retro_set_input_poll(cb_input_poll);
    retro_set_input_state(cb_input_state);

    if (!s_core_initialized) {
        retro_init();
        s_core_initialized = true;
    }

    /* Lecture du fichier ROM NDS en mémoire */
    FILE *f = fopen(rom_path, "rb");
    if (!f) {
        LOGE("Impossible d'ouvrir le fichier ROM : %s", rom_path);
        (*env)->ReleaseStringUTFChars(env, romPathStr, rom_path);
        (*env)->ReleaseStringUTFChars(env, saveDirStr, save_dir);
        return JNI_FALSE;
    }

    fseek(f, 0, SEEK_END);
    s_rom_size = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);

    if (s_rom_buffer) free(s_rom_buffer);
    s_rom_buffer = malloc(s_rom_size);
    if (!s_rom_buffer) {
        LOGE("Mémoire insuffisante pour charger la ROM (%zu octets)", s_rom_size);
        fclose(f);
        (*env)->ReleaseStringUTFChars(env, romPathStr, rom_path);
        (*env)->ReleaseStringUTFChars(env, saveDirStr, save_dir);
        return JNI_FALSE;
    }

    size_t read_bytes = fread(s_rom_buffer, 1, s_rom_size, f);
    fclose(f);

    LOGI("ROM chargée en mémoire avec succès : %zu octets", read_bytes);

    struct retro_game_info game_info;
    memset(&game_info, 0, sizeof(game_info));
    game_info.path = rom_path;
    game_info.data = s_rom_buffer;
    game_info.size = s_rom_size;

    bool ok = retro_load_game(&game_info);

    (*env)->ReleaseStringUTFChars(env, romPathStr, rom_path);
    (*env)->ReleaseStringUTFChars(env, saveDirStr, save_dir);

    if (!ok) {
        LOGE("retro_load_game a échoué !");
        return JNI_FALSE;
    }

    s_game_loaded = true;
    LOGI("[SUCCÈS] Jeu Nintendo DS chargé avec JIT ARM64 activé !");
    return JNI_TRUE;
}

/**
 * @brief Exécution d'une frame avec écriture directe dans le Bitmap Android
 * Zéro copie intermédiaire JNI, zéro conversion de format.
 */
JNIEXPORT jboolean JNICALL
Java_com_game_pokeplatinum_MainActivity_nativeRunFrameDirect(JNIEnv *env, jclass clazz,
                                                             jobject targetBitmap,
                                                             jint inputMask,
                                                             jint touchX, jint touchY,
                                                             jboolean isTouching) {
    (void)clazz;
    if (!s_game_loaded) return JNI_FALSE;

    s_input_mask = (uint32_t)inputMask;
    s_touch_x = (int)touchX;
    s_touch_y = (int)touchY;
    s_is_touching = (bool)isTouching;

    void *pixels = NULL;
    if (targetBitmap) {
        if (AndroidBitmap_lockPixels(env, targetBitmap, &pixels) >= 0 && pixels) {
            s_direct_bitmap_pixels = (uint32_t *)pixels;
        }
    }

    /* Exécution d'une frame réelle via JIT ARM64 */
    retro_run();

    if (targetBitmap && pixels) {
        AndroidBitmap_unlockPixels(env, targetBitmap);
        s_direct_bitmap_pixels = NULL;
    }

    return JNI_TRUE;
}

JNIEXPORT jint JNICALL
Java_com_game_pokeplatinum_MainActivity_nativeGetAudioSamples(JNIEnv *env, jclass clazz,
                                                              jshortArray audioArray) {
    (void)clazz;
    if (!s_game_loaded) return 0;

    jsize max_samples = (*env)->GetArrayLength(env, audioArray);

    int read_pos = s_audio_read_pos;
    int write_pos = s_audio_write_pos;
    int available = 0;

    if (write_pos >= read_pos) {
        available = write_pos - read_pos;
    } else {
        available = (AUDIO_BUFFER_SIZE - read_pos) + write_pos;
    }

    if (available == 0) return 0;

    int to_read = available;
    if (to_read > (int)max_samples) {
        to_read = (int)max_samples;
    }
    to_read &= ~1; /* Alignement stéréo */

    if (to_read <= 0) return 0;

    jshort temp[2048];
    int chunk = to_read > 2048 ? 2048 : to_read;

    for (int i = 0; i < chunk; i++) {
        temp[i] = s_audio_ring[read_pos];
        read_pos = (read_pos + 1) % AUDIO_BUFFER_SIZE;
    }
    s_audio_read_pos = read_pos;

    (*env)->SetShortArrayRegion(env, audioArray, 0, chunk, temp);
    return (jint)chunk;
}

JNIEXPORT void JNICALL
Java_com_game_pokeplatinum_MainActivity_nativeShutdownEmulator(JNIEnv *env, jclass clazz) {
    (void)env; (void)clazz;
    LOGI("Arrêt de l'émulateur melonDS...");
    if (s_game_loaded) {
        retro_unload_game();
        s_game_loaded = false;
    }
    if (s_core_initialized) {
        retro_deinit();
        s_core_initialized = false;
    }
    if (s_rom_buffer) {
        free(s_rom_buffer);
        s_rom_buffer = NULL;
    }
}
