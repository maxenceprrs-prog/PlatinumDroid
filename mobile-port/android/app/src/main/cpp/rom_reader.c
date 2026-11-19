/**
 * @file rom_reader.c
 * @brief Implémentation du lecteur de ROM Nintendo DS NitroFS pour Pokemon Platine
 */

#include "rom_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <android/log.h>

#define LOG_TAG "NdsRomReader"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static FILE       *s_rom_file = NULL;
static NdsRomInfo  s_info = {0};
static uint8_t    *s_fat_table = NULL;
static uint32_t    s_num_fat_entries = 0;

static const char *s_default_rom_paths[] = {
    "/sdcard/Android/data/com.game.pokeplatinum/files/PokemonPlatinum.nds",
    "/storage/emulated/0/Android/data/com.game.pokeplatinum/files/PokemonPlatinum.nds",
    "/sdcard/Download/PokemonPlatinum.nds",
    "/sdcard/Download/Pokemon - Platinum Version (USA) (Rev 1).nds",
    "/sdcard/Download/Pokemon - Platinum Version.nds",
    "/storage/emulated/0/Download/PokemonPlatinum.nds",
    "/storage/emulated/0/Download/Pokemon - Platinum Version (USA) (Rev 1).nds",
    "/sdcard/PokemonPlatinum.nds",
    "PokemonPlatinum.nds"
};
static const int s_default_path_count = sizeof(s_default_rom_paths) / sizeof(s_default_rom_paths[0]);

static uint32_t read_u32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int RomReader_Init(const char *preferred_path) {
    RomReader_Shutdown();

    FILE *f = NULL;
    const char *opened_path = NULL;

    /* 1. Essayer le chemin préféré si spécifié */
    if (preferred_path && strlen(preferred_path) > 0) {
        f = fopen(preferred_path, "rb");
        if (f) opened_path = preferred_path;
    }

    /* 2. Essayer les emplacements standards */
    if (!f) {
        for (int i = 0; i < s_default_path_count; i++) {
            f = fopen(s_default_rom_paths[i], "rb");
            if (f) {
                opened_path = s_default_rom_paths[i];
                break;
            }
        }
    }

    if (!f) {
        LOGE("RomReader: Impossible de trouver la ROM Pokemon Platinum.");
        return 0;
    }

    /* 3. Vérification de la taille */
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz < 512) {
        LOGE("RomReader: Fichier ROM trop petit (%ld octets)", sz);
        fclose(f);
        return 0;
    }

    /* 4. Lecture du header NDS (512 octets) */
    uint8_t header[512];
    if (fread(header, 1, 512, f) != 512) {
        LOGE("RomReader: Erreur lecture header NDS");
        fclose(f);
        return 0;
    }

    memcpy(s_info.game_title, header, 12);
    s_info.game_title[12] = '\0';

    memcpy(s_info.game_code, header + 12, 4);
    s_info.game_code[4] = '\0';

    s_info.rom_size   = (uint32_t)sz;
    s_info.fnt_offset = read_u32le(header + 0x40);
    s_info.fnt_size   = read_u32le(header + 0x44);
    s_info.fat_offset = read_u32le(header + 0x48);
    s_info.fat_size   = read_u32le(header + 0x4C);

    LOGI("RomReader: ROM ouverte '%s' | Titre: '%s' | Code: '%s' | Taille: %u bytes",
         opened_path, s_info.game_title, s_info.game_code, s_info.rom_size);

    /* 5. Chargement de la table FAT en RAM */
    if (s_info.fat_size > 0 && s_info.fat_size < 16 * 1024 * 1024 &&
        s_info.fat_offset + s_info.fat_size <= s_info.rom_size) {
        s_fat_table = (uint8_t *)malloc(s_info.fat_size);
        if (s_fat_table) {
            fseek(f, s_info.fat_offset, SEEK_SET);
            if (fread(s_fat_table, 1, s_info.fat_size, f) == s_info.fat_size) {
                s_num_fat_entries = s_info.fat_size / 8;
                LOGI("RomReader: Table FAT chargee avec %u entrees", s_num_fat_entries);
            } else {
                free(s_fat_table);
                s_fat_table = NULL;
            }
        }
    }

    s_rom_file = f;
    s_info.is_loaded = 1;
    return 1;
}

const NdsRomInfo *RomReader_GetInfo(void) {
    return &s_info;
}

int RomReader_IsLoaded(void) {
    return s_info.is_loaded && s_rom_file != NULL;
}

int RomReader_ReadFile(uint32_t file_id, void **out_data, size_t *out_size) {
    if (!RomReader_IsLoaded() || !s_fat_table || file_id >= s_num_fat_entries) {
        return 0;
    }

    const uint8_t *entry = s_fat_table + (file_id * 8);
    uint32_t top    = read_u32le(entry);
    uint32_t bottom = read_u32le(entry + 4);

    if (bottom <= top || bottom > s_info.rom_size) {
        return 0;
    }

    size_t len = (size_t)(bottom - top);
    void *buf = malloc(len);
    if (!buf) return 0;

    fseek(s_rom_file, top, SEEK_SET);
    if (fread(buf, 1, len, s_rom_file) != len) {
        free(buf);
        return 0;
    }

    *out_data = buf;
    *out_size = len;
    return 1;
}

int RomReader_ReadRaw(uint32_t offset, void *dest, size_t size) {
    if (!s_rom_file || offset + size > s_info.rom_size) return 0;
    fseek(s_rom_file, offset, SEEK_SET);
    return (fread(dest, 1, size, s_rom_file) == size);
}

void RomReader_Shutdown(void) {
    if (s_rom_file) {
        fclose(s_rom_file);
        s_rom_file = NULL;
    }
    if (s_fat_table) {
        free(s_fat_table);
        s_fat_table = NULL;
    }
    s_num_fat_entries = 0;
    memset(&s_info, 0, sizeof(s_info));
}
