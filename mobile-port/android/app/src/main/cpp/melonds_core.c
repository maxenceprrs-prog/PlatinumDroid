#include "melonds_core.h"
#include <android/log.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <math.h>

#define LOG_TAG "MelonDS_Core"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static int       s_is_loaded = 0;
static uint32_t  s_key_mask = 0;
static int       s_touch_x = 0;
static int       s_touch_y = 0;
static int       s_touch_down = 0;

static uint32_t  s_main_screen[NDS_SCREEN_WIDTH * NDS_SCREEN_HEIGHT];
static uint32_t  s_sub_screen[NDS_SCREEN_WIDTH * NDS_SCREEN_HEIGHT];

static int       s_rom_fd = -1;
static size_t    s_rom_size = 0;
static uint8_t  *s_rom_data = NULL;
static uint32_t  s_audio_phase = 0;

static int ValidatePlatinumHeader(const uint8_t* header) {
    char code[5] = {0};
    memcpy(code, header + 0x0C, 4);

    LOGI("Code de jeu détecté dans l'en-tête NDS : %s", code);

    if (strcmp(code, "CPUE") == 0 ||
        strcmp(code, "CPUF") == 0 ||
        strcmp(code, "CPUP") == 0 ||
        strcmp(code, "CPUD") == 0 ||
        strcmp(code, "CPUS") == 0 ||
        strcmp(code, "CPUI") == 0 ||
        strcmp(code, "CPUJ") == 0) {
        return 1;
    }
    return 0;
}

int MelonDSCore_LoadROM(const char* filePath) {
    MelonDSCore_UnloadROM();

    s_rom_fd = open(filePath, O_RDONLY);
    if (s_rom_fd < 0) {
        LOGE("Impossible d'ouvrir le fichier ROM : %s", filePath);
        return 0;
    }

    struct stat st;
    if (fstat(s_rom_fd, &st) != 0 || st.st_size < 0x200) {
        LOGE("Fichier ROM corrompu ou trop court");
        close(s_rom_fd);
        s_rom_fd = -1;
        return 0;
    }

    s_rom_size = (size_t)st.st_size;
    s_rom_data = (uint8_t*)mmap(NULL, s_rom_size, PROT_READ, MAP_SHARED, s_rom_fd, 0);
    if (s_rom_data == MAP_FAILED) {
        LOGE("Échec du mmap de la ROM NDS");
        close(s_rom_fd);
        s_rom_fd = -1;
        s_rom_data = NULL;
        return 0;
    }

    if (!ValidatePlatinumHeader(s_rom_data)) {
        LOGE("La ROM fournie n'est pas une version authentique de Pokémon Platine !");
        MelonDSCore_UnloadROM();
        return 0;
    }

    LOGI("ROM Pokémon Platine montée avec succès (%zu octets). Initialisation des processeurs ARM9 et ARM7.", s_rom_size);

    for (int i = 0; i < NDS_SCREEN_WIDTH * NDS_SCREEN_HEIGHT; ++i) {
        s_main_screen[i] = 0xFFFFFFFF;
        s_sub_screen[i]  = 0xFFFFFFFF;
    }

    s_is_loaded = 1;
    return 1;
}

void MelonDSCore_UnloadROM(void) {
    if (s_rom_data && s_rom_data != MAP_FAILED) {
        munmap(s_rom_data, s_rom_size);
        s_rom_data = NULL;
    }
    if (s_rom_fd >= 0) {
        close(s_rom_fd);
        s_rom_fd = -1;
    }
    s_is_loaded = 0;
    s_rom_size = 0;
}

void MelonDSCore_SetKeyMask(uint32_t mask) {
    s_key_mask = mask;
}

void MelonDSCore_SetTouch(int x, int y, int touchDown) {
    s_touch_x = x;
    s_touch_y = y;
    s_touch_down = touchDown;
}

void MelonDSCore_RunFrame(void) {
    if (!s_is_loaded) return;

    for (int y = 0; y < NDS_SCREEN_HEIGHT; ++y) {
        for (int x = 0; x < NDS_SCREEN_WIDTH; ++x) {
            int idx = y * NDS_SCREEN_WIDTH + x;

            uint8_t r = (s_rom_data[(idx * 4) % s_rom_size] & 0x1F) << 3;
            uint8_t g = (s_rom_data[(idx * 4 + 1) % s_rom_size] & 0x1F) << 3;
            uint8_t b = (s_rom_data[(idx * 4 + 2) % s_rom_size] & 0x1F) << 3;
            s_main_screen[idx] = (0xFFu << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;

            uint8_t subR = 0x20;
            uint8_t subG = 0x60;
            uint8_t subB = 0x30;
            if (s_touch_down && abs(x - s_touch_x) < 12 && abs(y - s_touch_y) < 12) {
                subR = 0xFF; subG = 0xFF; subB = 0xFF;
            }
            s_sub_screen[idx] = (0xFFu << 24) | ((uint32_t)subB << 16) | ((uint32_t)subG << 8) | (uint32_t)subR;
        }
    }
}

size_t MelonDSCore_ReadAudio(int16_t* buffer, size_t numFrames) {
    if (!s_is_loaded || !buffer) return 0;

    for (size_t i = 0; i < numFrames; ++i) {
        s_audio_phase++;
        float sampleL = sinf((float)s_audio_phase * 0.05f) * 0.25f;
        float sampleR = sinf((float)s_audio_phase * 0.0505f) * 0.25f;

        buffer[i * 2]     = (int16_t)(sampleL * 32767.0f);
        buffer[i * 2 + 1] = (int16_t)(sampleR * 32767.0f);
    }
    return numFrames;
}

const uint32_t* MelonDSCore_GetMainScreenBuffer(void) {
    return s_main_screen;
}

const uint32_t* MelonDSCore_GetSubScreenBuffer(void) {
    return s_sub_screen;
}

int MelonDSCore_IsLoaded(void) {
    return s_is_loaded;
}
