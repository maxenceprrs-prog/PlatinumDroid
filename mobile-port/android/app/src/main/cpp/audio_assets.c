/**
 * @file audio_assets.c
 * @brief Chargeur WAV PCM 16-bit pour Android (sans SDL, sans dépendances tierces)
 *
 * Supporte :
 *   - WAV PCM 8-bit  (converti en s16)
 *   - WAV PCM 16-bit (natif)
 *   - Mono / Stéréo
 */

#include "audio_assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Helpers lecture little-endian depuis un buffer                      */
/* ------------------------------------------------------------------ */
static inline uint16_t read_u16le(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}
static inline uint32_t read_u32le(const uint8_t *p) {
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

/* ------------------------------------------------------------------ */
/* AudioAssets_LoadWav                                                 */
/* ------------------------------------------------------------------ */
WavBuffer AudioAssets_LoadWav(const char *path) {
    WavBuffer result = {NULL, 0, 0, 0, 0};
    if (!path) return result;

    FILE *f = fopen(path, "rb");
    if (!f) return result;

    /* Lire tout le fichier en mémoire */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size < 44) { fclose(f); return result; } /* trop petit */

    uint8_t *data = (uint8_t *)malloc((size_t)file_size);
    if (!data) { fclose(f); return result; }
    if (fread(data, 1, (size_t)file_size, f) != (size_t)file_size) {
        free(data); fclose(f); return result;
    }
    fclose(f);

    /* Vérification RIFF */
    if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0) {
        free(data); return result;
    }

    /* Parcourir les chunks */
    int       audio_format   = 0;
    int       channels       = 0;
    int       sample_rate    = 0;
    int       bits_per_sample = 0;
    uint8_t  *pcm_data       = NULL;
    size_t    pcm_size       = 0;

    size_t pos = 12; /* après "RIFF....WAVE" */
    while (pos + 8 <= (size_t)file_size) {
        char     chunk_id[5] = {0};
        uint32_t chunk_size;
        memcpy(chunk_id, data + pos, 4);
        chunk_size = read_u32le(data + pos + 4);
        pos += 8;

        if (strcmp(chunk_id, "fmt ") == 0 && chunk_size >= 16) {
            audio_format    = (int)read_u16le(data + pos);
            channels        = (int)read_u16le(data + pos + 2);
            sample_rate     = (int)read_u32le(data + pos + 4);
            bits_per_sample = (int)read_u16le(data + pos + 14);
        } else if (strcmp(chunk_id, "data") == 0) {
            pcm_data = data + pos;
            pcm_size = chunk_size;
            break;
        }

        /* Aligne sur 2 octets */
        pos += chunk_size + (chunk_size & 1);
    }

    /* Validation */
    if (audio_format != 1 || channels < 1 || channels > 2 ||
        sample_rate <= 0 || (bits_per_sample != 8 && bits_per_sample != 16) ||
        !pcm_data || pcm_size == 0) {
        free(data); return result;
    }

    /* Calcul du nombre total d'échantillons */
    size_t bytes_per_sample = (size_t)(bits_per_sample / 8);
    size_t total_samples    = pcm_size / bytes_per_sample;
    size_t total_frames     = total_samples / (size_t)channels;

    int16_t *out = (int16_t *)malloc(total_samples * sizeof(int16_t));
    if (!out) { free(data); return result; }

    if (bits_per_sample == 16) {
        /* Copie directe */
        memcpy(out, pcm_data, total_samples * sizeof(int16_t));
    } else {
        /* 8-bit unsigned → 16-bit signed */
        for (size_t i = 0; i < total_samples; i++) {
            out[i] = (int16_t)(((int)pcm_data[i] - 128) * 256);
        }
    }

    free(data);

    result.samples     = out;
    result.num_samples = total_samples;
    result.sample_rate = sample_rate;
    result.channels    = channels;
    result.frames      = (int)total_frames;
    return result;
}

/* ------------------------------------------------------------------ */
/* AudioAssets_Free                                                    */
/* ------------------------------------------------------------------ */
void AudioAssets_Free(WavBuffer *buf) {
    if (!buf) return;
    free(buf->samples);
    buf->samples     = NULL;
    buf->num_samples = 0;
    buf->frames      = 0;
}

/* ------------------------------------------------------------------ */
/* AudioAssets_CreateSilenceWav (utilitaire de test)                  */
/* ------------------------------------------------------------------ */
int AudioAssets_CreateSilenceWav(const char *path, int sample_rate, int channels, int seconds) {
    if (!path || sample_rate <= 0 || channels < 1 || channels > 2 || seconds <= 0)
        return 0;

    int total_samples = sample_rate * channels * seconds;
    int byte_rate     = sample_rate * channels * 2;
    int data_size     = total_samples * 2;

    FILE *f = fopen(path, "wb");
    if (!f) return 0;

    /* RIFF header */
    uint8_t hdr[44];
    memcpy(hdr,      "RIFF", 4);
    uint32_t riff_size = (uint32_t)(36 + data_size);
    hdr[4]  = riff_size & 0xFF; hdr[5]  = (riff_size >> 8) & 0xFF;
    hdr[6]  = (riff_size >> 16) & 0xFF; hdr[7]  = (riff_size >> 24) & 0xFF;
    memcpy(hdr + 8,  "WAVE", 4);
    memcpy(hdr + 12, "fmt ", 4);
    hdr[16] = 16; hdr[17] = 0; hdr[18] = 0; hdr[19] = 0;  /* chunk size */
    hdr[20] = 1;  hdr[21] = 0;  /* PCM */
    hdr[22] = (uint8_t)channels; hdr[23] = 0;
    hdr[24] = sample_rate & 0xFF; hdr[25] = (sample_rate >> 8) & 0xFF;
    hdr[26] = (sample_rate >> 16) & 0xFF; hdr[27] = (sample_rate >> 24) & 0xFF;
    hdr[28] = byte_rate & 0xFF; hdr[29] = (byte_rate >> 8) & 0xFF;
    hdr[30] = (byte_rate >> 16) & 0xFF; hdr[31] = (byte_rate >> 24) & 0xFF;
    hdr[32] = (uint8_t)(channels * 2); hdr[33] = 0; /* block align */
    hdr[34] = 16; hdr[35] = 0; /* bits per sample */
    memcpy(hdr + 36, "data", 4);
    hdr[40] = data_size & 0xFF; hdr[41] = (data_size >> 8) & 0xFF;
    hdr[42] = (data_size >> 16) & 0xFF; hdr[43] = (data_size >> 24) & 0xFF;

    fwrite(hdr, 1, 44, f);

    /* Silence : zéros */
    int16_t zero = 0;
    for (int i = 0; i < total_samples; i++)
        fwrite(&zero, sizeof(int16_t), 1, f);

    fclose(f);
    return 1;
}
