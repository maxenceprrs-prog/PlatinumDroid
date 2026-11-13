/**
 * @file audio_sdat.h
 * @brief Parseur SDAT Nintendo DS minimal — extrait les flux SWAV (PCM)
 *
 * Structure SDAT :
 *   SDAT → SYMB (noms) → INFO (meta) → FAT (offsets fichiers) → FILE (données)
 *
 * Ce module expose uniquement ce dont on a besoin pour la lecture audio :
 *   - Indexer les entrées SWAV (samples PCM 8-bit ADPCM ou PCM-8/16)
 *   - Décoder un SWAV en PCM 16-bit utilisable par AudioTrack
 *
 * Référence format : https://gbatek.no-ip.info/  (NDS Audio)
 */

#ifndef AUDIO_SDAT_H
#define AUDIO_SDAT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Types publics                                                        */
/* ------------------------------------------------------------------ */

/** Un sample SWAV décodé en PCM 16-bit stéréo (ou mono) */
typedef struct {
    int16_t *pcm;           /* Buffer PCM 16-bit, alloué par malloc */
    size_t   num_samples;   /* Nombre TOTAL d'échantillons (frames * ch) */
    int      sample_rate;   /* Hz (typiquement 8000–32728 pour DS) */
    int      channels;      /* 1 ou 2 */
    int      loop;          /* 1 si le sample boucle */
    uint32_t loop_start;    /* Frame de début de boucle */
} SdatSwav;

/** Index SDAT chargé en mémoire */
typedef struct SdatIndex SdatIndex;

/* ------------------------------------------------------------------ */
/* API                                                                  */
/* ------------------------------------------------------------------ */

/**
 * Charge et indexe un fichier .sdat depuis le disque.
 * @return  Pointeur opaque, ou NULL en cas d'erreur. Libérer avec Sdat_Close().
 */
SdatIndex *Sdat_Open(const char *path);

/**
 * Libère toutes les ressources d'un SdatIndex.
 */
void Sdat_Close(SdatIndex *idx);

/**
 * Nombre de SWAV dans le SDAT.
 */
int Sdat_SwavCount(const SdatIndex *idx);

/**
 * Nom symbolique d'un SWAV (peut être NULL si SYMB absent).
 */
const char *Sdat_SwavName(const SdatIndex *idx, int swav_id);

/**
 * Décode le SWAV n°swav_id en PCM 16-bit.
 * @return  SdatSwav avec .pcm alloué, ou {NULL,...} en cas d'erreur.
 *          Libérer avec Sdat_FreeSwav().
 */
SdatSwav Sdat_DecodeSwav(const SdatIndex *idx, int swav_id);

/**
 * Libère le buffer PCM d'un SdatSwav.
 */
void Sdat_FreeSwav(SdatSwav *s);

/**
 * Exporte un SWAV décodé en fichier WAV PCM sur le disque (pour debug).
 */
int Sdat_ExportWav(const SdatSwav *s, const char *out_path);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_SDAT_H */
