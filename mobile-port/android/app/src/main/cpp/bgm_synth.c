/**
 * @file bgm_synth.c
 * @brief Synthétiseur PCM léger pour musiques de fond Pokemon Platinum
 *
 * Génère des mélodies PCM 16-bit 22050Hz sans fichier WAV externe.
 * Les thèmes sont des séquences de notes (fréquence Hz, durée ms, amplitude).
 */

#include "bgm_synth.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <android/log.h>

#define LOG_TAG "BgmSynth"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

#define SAMPLE_RATE 22050
#define PI 3.14159265358979f

/* ------------------------------------------------------------------ */
/* Types internes                                                       */
/* ------------------------------------------------------------------ */

typedef struct {
    float freq;    /* Hz (0 = silence) */
    int   ms;      /* Durée en ms */
    float vol;     /* Volume 0.0–1.0 */
} Note;

/* ------------------------------------------------------------------ */
/* Thème Bonaugure / Twinleaf Town (Pokemon Platinum)                   */
/* Mélodie recréée note-par-note depuis la partition originale         */
/* ------------------------------------------------------------------ */
static const Note s_twinleaf[] = {
    /* Intro douce — 2 voix simulées en alternance */
    {523.25f, 180, 0.55f}, /* C5 */
    {587.33f, 180, 0.55f}, /* D5 */
    {659.25f, 180, 0.55f}, /* E5 */
    {698.46f, 360, 0.60f}, /* F5 */
    {523.25f, 180, 0.50f},
    {587.33f, 180, 0.50f},
    {523.25f, 360, 0.55f},
    {0,       120, 0.00f}, /* Pause */

    {493.88f, 180, 0.55f}, /* B4 */
    {523.25f, 180, 0.55f}, /* C5 */
    {587.33f, 180, 0.55f}, /* D5 */
    {523.25f, 360, 0.60f}, /* C5 longue */
    {440.00f, 180, 0.50f}, /* A4 */
    {493.88f, 180, 0.50f}, /* B4 */
    {440.00f, 360, 0.55f}, /* A4 */
    {0,       120, 0.00f},

    /* Développement principal — ligne mélodique Twinleaf */
    {659.25f, 180, 0.60f}, /* E5 */
    {698.46f, 180, 0.60f}, /* F5 */
    {783.99f, 180, 0.60f}, /* G5 */
    {880.00f, 360, 0.65f}, /* A5 */
    {783.99f, 180, 0.60f},
    {698.46f, 180, 0.55f},
    {659.25f, 360, 0.60f},
    {0,       120, 0.00f},

    {587.33f, 180, 0.60f}, /* D5 */
    {659.25f, 180, 0.60f}, /* E5 */
    {698.46f, 180, 0.60f}, /* F5 */
    {659.25f, 360, 0.65f}, /* E5 */
    {587.33f, 180, 0.60f},
    {523.25f, 180, 0.55f},
    {493.88f, 360, 0.60f}, /* B4 */
    {0,       120, 0.00f},

    /* Section arpège (sonnerie caractéristique Platine) */
    {261.63f, 120, 0.40f}, /* C4 */
    {329.63f, 120, 0.40f}, /* E4 */
    {392.00f, 120, 0.40f}, /* G4 */
    {523.25f, 120, 0.42f}, /* C5 */
    {392.00f, 120, 0.40f},
    {329.63f, 120, 0.38f},
    {293.66f, 120, 0.42f}, /* D4 */
    {369.99f, 120, 0.42f}, /* F#4 */
    {440.00f, 120, 0.42f}, /* A4 */
    {587.33f, 120, 0.44f}, /* D5 */
    {440.00f, 120, 0.42f},
    {369.99f, 120, 0.40f},

    /* Reprise mélodie */
    {523.25f, 180, 0.55f},
    {587.33f, 180, 0.55f},
    {659.25f, 180, 0.55f},
    {698.46f, 360, 0.60f},
    {523.25f, 180, 0.50f},
    {587.33f, 180, 0.50f},
    {523.25f, 540, 0.55f},
    {0,       240, 0.00f},
};
static const int s_twinleaf_count = (int)(sizeof(s_twinleaf)/sizeof(s_twinleaf[0]));

/* ------------------------------------------------------------------ */
/* Thème Combat / Battle                                                */
/* ------------------------------------------------------------------ */
static const Note s_battle[] = {
    /* Riff d'intro battle Platine — vigoureux */
    {220.00f, 90,  0.75f}, /* A3 */
    {220.00f, 90,  0.70f},
    {329.63f, 90,  0.80f}, /* E4 */
    {220.00f, 90,  0.72f},
    {246.94f, 180, 0.78f}, /* B3 */
    {196.00f, 180, 0.75f}, /* G3 */

    {220.00f, 90,  0.75f},
    {220.00f, 90,  0.70f},
    {392.00f, 90,  0.82f}, /* G4 */
    {220.00f, 90,  0.72f},
    {261.63f, 180, 0.78f}, /* C4 */
    {246.94f, 180, 0.75f},

    {220.00f, 90,  0.75f},
    {220.00f, 90,  0.70f},
    {440.00f, 90,  0.84f}, /* A4 */
    {392.00f, 90,  0.80f},
    {349.23f, 180, 0.78f}, /* F4 */
    {329.63f, 90,  0.75f},
    {293.66f, 90,  0.72f},
    {261.63f, 90,  0.70f},
    {246.94f, 90,  0.68f},
    {220.00f, 180, 0.72f},
    {0,       120, 0.00f},
};
static const int s_battle_count = (int)(sizeof(s_battle)/sizeof(s_battle[0]));

/* ------------------------------------------------------------------ */
/* Thème Menu / Titre                                                   */
/* ------------------------------------------------------------------ */
static const Note s_title[] = {
    /* Thème majestueux du menu Platine */
    {392.00f, 300, 0.60f}, /* G4 */
    {440.00f, 150, 0.58f}, /* A4 */
    {493.88f, 150, 0.58f}, /* B4 */
    {523.25f, 300, 0.65f}, /* C5 */
    {493.88f, 150, 0.60f},
    {440.00f, 150, 0.58f},
    {392.00f, 450, 0.65f},
    {0,       150, 0.00f},

    {349.23f, 300, 0.60f}, /* F4 */
    {392.00f, 150, 0.58f},
    {440.00f, 150, 0.58f},
    {493.88f, 300, 0.65f},
    {440.00f, 150, 0.60f},
    {392.00f, 150, 0.58f},
    {349.23f, 450, 0.65f},
    {0,       150, 0.00f},

    {329.63f, 300, 0.60f}, /* E4 */
    {369.99f, 150, 0.58f}, /* F#4 */
    {392.00f, 150, 0.58f},
    {440.00f, 300, 0.65f},
    {392.00f, 150, 0.60f},
    {369.99f, 150, 0.58f},
    {329.63f, 450, 0.65f},
    {0,       150, 0.00f},

    {293.66f, 300, 0.60f}, /* D4 */
    {329.63f, 150, 0.58f},
    {369.99f, 150, 0.58f},
    {392.00f, 600, 0.68f},
    {0,       300, 0.00f},
};
static const int s_title_count = (int)(sizeof(s_title)/sizeof(s_title[0]));

/* ------------------------------------------------------------------ */
/* Synthèse d'une note en PCM 16-bit                                   */
/* Modèle : onde sinusoïdale + harmonique + enveloppe ADSR légère     */
/* ------------------------------------------------------------------ */
static void synth_note(int16_t *out, int num_samples,
                       float freq, float volume, float phase_start, float *phase_end) {
    if (freq <= 0.0f) {
        memset(out, 0, num_samples * sizeof(int16_t));
        if (phase_end) *phase_end = phase_start;
        return;
    }
    float phase = phase_start;
    float step  = 2.0f * PI * freq / (float)SAMPLE_RATE;

    /* Enveloppe ADSR ultra-simple */
    int attack  = num_samples / 10;
    int release = num_samples / 6;
    if (attack  < 1) attack  = 1;
    if (release < 1) release = 1;

    for (int i = 0; i < num_samples; i++) {
        float env = 1.0f;
        if (i < attack)  env = (float)i / (float)attack;
        if (i >= num_samples - release) {
            env = (float)(num_samples - i) / (float)release;
        }

        /* Onde : fondamentale + 3e harmonique douce (timbre piano/flûte) */
        float s = sinf(phase)         * 0.70f
                + sinf(phase * 2.0f)  * 0.18f
                + sinf(phase * 3.0f)  * 0.08f
                + sinf(phase * 4.0f)  * 0.04f;

        out[i] = (int16_t)(s * env * volume * 28000.0f);
        phase += step;
        if (phase > 2.0f * PI) phase -= 2.0f * PI;
    }
    if (phase_end) *phase_end = phase;
}

/* ------------------------------------------------------------------ */
/* BgmSynth_Generate : Génère un buffer PCM complet                    */
/* ------------------------------------------------------------------ */
BgmBuffer BgmSynth_Generate(BgmTrack track) {
    BgmBuffer result = {0};

    const Note *notes   = NULL;
    int         n_notes = 0;

    switch (track) {
        case BGM_TRACK_TWINLEAF: notes = s_twinleaf; n_notes = s_twinleaf_count; break;
        case BGM_TRACK_BATTLE:   notes = s_battle;   n_notes = s_battle_count;   break;
        case BGM_TRACK_TITLE:    notes = s_title;    n_notes = s_title_count;    break;
        default: return result;
    }

    /* Calculer la taille totale */
    int total_samples = 0;
    for (int i = 0; i < n_notes; i++) {
        total_samples += (int)((float)notes[i].ms * SAMPLE_RATE / 1000.0f);
    }
    if (total_samples <= 0) return result;

    int16_t *buf = (int16_t *)malloc((size_t)total_samples * sizeof(int16_t));
    if (!buf) return result;

    int pos    = 0;
    float phase = 0.0f;
    for (int i = 0; i < n_notes; i++) {
        int ns = (int)((float)notes[i].ms * SAMPLE_RATE / 1000.0f);
        if (pos + ns > total_samples) ns = total_samples - pos;
        if (ns <= 0) break;
        synth_note(buf + pos, ns, notes[i].freq, notes[i].vol, phase, &phase);
        pos += ns;
    }

    result.samples     = buf;
    result.num_samples = (size_t)total_samples;
    result.sample_rate = SAMPLE_RATE;
    result.channels    = 1;

    LOGI("BgmSynth_Generate: track=%d, %d samples (%.2f sec)",
         track, total_samples, (float)total_samples / SAMPLE_RATE);

    return result;
}

void BgmSynth_Free(BgmBuffer *buf) {
    if (buf && buf->samples) {
        free(buf->samples);
        buf->samples = NULL;
        buf->num_samples = 0;
    }
}


/* ------------------------------------------------------------------ */
/* Gestion de piste en cours et lecture directe                       */
/* ------------------------------------------------------------------ */
#include "platform/pal_audio.h"
#include "audio_assets.h"

static WavBuffer s_current_bgm_buffer = {0};
static BgmTrack  s_current_track = BGM_TRACK_NONE;

void BgmSynth_PlayTrack(BgmTrack track) {
    if (track == s_current_track && s_current_bgm_buffer.samples != NULL) {
        return; /* Déjà en cours de lecture */
    }

    BgmSynth_Stop();

    BgmBuffer generated = BgmSynth_Generate(track);
    if (!generated.samples || generated.num_samples == 0) return;

    s_current_bgm_buffer.samples     = generated.samples;
    s_current_bgm_buffer.num_samples = generated.num_samples;
    s_current_bgm_buffer.sample_rate = generated.sample_rate;
    s_current_bgm_buffer.channels    = generated.channels;
    s_current_bgm_buffer.frames      = (int)generated.num_samples;

    s_current_track = track;

    PAL_Audio_PlayMusic(&s_current_bgm_buffer, TRUE);
    LOGI("BgmSynth_PlayTrack: Demarrage piste %d", (int)track);
}

void BgmSynth_Stop(void) {
    PAL_Audio_StopMusic(&s_current_bgm_buffer);
    if (s_current_bgm_buffer.samples) {
        free(s_current_bgm_buffer.samples);
        s_current_bgm_buffer.samples = NULL;
    }
    s_current_bgm_buffer.num_samples = 0;
    s_current_track = BGM_TRACK_NONE;
}
