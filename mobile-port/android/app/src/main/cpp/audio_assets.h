/**
 * @file audio_assets.h
 * @brief Chargeur de fichiers WAV PCM 16-bit depuis les assets Android
 *
 * Lit uniquement les WAV PCM (format 1) mono ou stéréo, 8 ou 16 bits.
 * Retourne un buffer PCM alloué par malloc() — libérer avec free().
 */

#ifndef AUDIO_ASSETS_H
#define AUDIO_ASSETS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int16_t  *samples;       /* Buffer PCM 16-bit (toujours converti en s16) */
    size_t    num_samples;   /* Nombre d'échantillons TOTAL (frames * channels) */
    int       sample_rate;   /* Fréquence d'échantillonnage (ex: 22050, 44100) */
    int       channels;      /* 1 = mono, 2 = stéréo */
    int       frames;        /* Nombre de frames audio = num_samples / channels */
} WavBuffer;

/**
 * Charge un fichier WAV PCM depuis le système de fichiers (après extraction des assets).
 * @param path  Chemin absolu vers le .wav
 * @return      WavBuffer alloué, ou {NULL,0,...} en cas d'erreur.
 *              Libérer avec AudioAssets_Free().
 */
WavBuffer AudioAssets_LoadWav(const char *path);

/**
 * Libère le buffer PCM d'un WavBuffer précédemment chargé.
 */
void AudioAssets_Free(WavBuffer *buf);

/**
 * Crée un fichier WAV PCM de silence pour les tests (path = chemin de sortie).
 * Durée : 1 seconde, 22050 Hz, mono 16-bit.
 */
int AudioAssets_CreateSilenceWav(const char *path, int sample_rate, int channels, int seconds);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_ASSETS_H */
