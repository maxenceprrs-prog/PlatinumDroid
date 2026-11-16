/**
 * @file bgm_synth.h
 * @brief Synthétiseur PCM de musiques de fond Pokemon Platine
 */

#ifndef BGM_SYNTH_H
#define BGM_SYNTH_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BGM_TRACK_NONE = 0,
    BGM_TRACK_TITLE = 1,
    BGM_TRACK_TWINLEAF = 2,
    BGM_TRACK_BATTLE = 3
} BgmTrack;

typedef struct {
    int16_t *samples;
    size_t   num_samples;
    int      sample_rate;
    int      channels;
} BgmBuffer;

/**
 * Génère le flux audio PCM complet d'une piste musicale
 */
BgmBuffer BgmSynth_Generate(BgmTrack track);

/**
 * Libère la mémoire d'un buffer BGM
 */
void BgmSynth_Free(BgmBuffer *buf);

/**
 * Joue une musique synthétisée en continu (boucle)
 */
void BgmSynth_PlayTrack(BgmTrack track);

/**
 * Arrête la musique en cours
 */
void BgmSynth_Stop(void);

#ifdef __cplusplus
}
#endif

#endif /* BGM_SYNTH_H */
