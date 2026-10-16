/**
 * @file pal_audio_psp.c
 * @brief Implémentation PSP de l'audio (PAL_Audio) via la lib sceAudio bas niveau.
 *
 * ÉTAT RÉEL : la plomberie de lecture PCM (ouverture de canal, thread de
 * lecture, mixage basique) est fonctionnelle. Ce qui NE l'est PAS : le
 * décodage des musiques d'origine, qui sont au format SDAT (format
 * propriétaire Nintendo, séquencé façon MIDI + banques d'instruments).
 * Il faut soit :
 *   (a) convertir les musiques en PCM/OGG à l'avance avec un outil externe
 *       (ex: sdat2wav) comme étape de build, la piste la plus rapide, ou
 *   (b) réimplémenter un séquenceur SDAT minimal en C (bien plus de travail).
 * Le plan de portage recommande (a) pour rester dans un calendrier réaliste.
 */

#include <pspkernel.h>
#include <pspaudio.h>
#include <pspthreadman.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "platform/pal_audio.h"

#define AUDIO_CHANNEL_BGM   0
#define AUDIO_SAMPLES_COUNT 2048  /* doit rester un multiple de 64, contrainte sceAudio */

typedef struct {
    s16* pcm_data;      /* Données PCM 16 bits déjà décodées en mémoire */
    size_t pcm_len;      /* Nombre de frames stéréo */
    size_t pcm_pos;
    BOOL loop;
    BOOL playing;
} PspAudioTrack;

static int s_bgm_channel = -1;
static PspAudioTrack s_bgm_track;
static SceUID s_audio_thread = -1;
static volatile BOOL s_audio_thread_running = FALSE;

static int AudioThreadEntry(SceSize args, void* argp) {
    (void)args; (void)argp;
    s16 mix_buffer[AUDIO_SAMPLES_COUNT * 2]; /* stéréo entrelacé */

    while (s_audio_thread_running) {
        if (s_bgm_track.playing && s_bgm_track.pcm_data) {
            for (int i = 0; i < AUDIO_SAMPLES_COUNT; i++) {
                if (s_bgm_track.pcm_pos >= s_bgm_track.pcm_len) {
                    if (s_bgm_track.loop) {
                        s_bgm_track.pcm_pos = 0;
                    } else {
                        s_bgm_track.playing = FALSE;
                        mix_buffer[i * 2 + 0] = 0;
                        mix_buffer[i * 2 + 1] = 0;
                        continue;
                    }
                }
                mix_buffer[i * 2 + 0] = s_bgm_track.pcm_data[s_bgm_track.pcm_pos * 2 + 0];
                mix_buffer[i * 2 + 1] = s_bgm_track.pcm_data[s_bgm_track.pcm_pos * 2 + 1];
                s_bgm_track.pcm_pos++;
            }
        } else {
            memset(mix_buffer, 0, sizeof(mix_buffer));
        }

        sceAudioOutputBlocking(s_bgm_channel, PSP_AUDIO_VOLUME_MAX, mix_buffer);
    }
    return 0;
}

BOOL PAL_Audio_Init(int frequency, int channels) {
    (void)channels; /* on force la sortie stéréo, standard PSP */

    s_bgm_channel = sceAudioChReserve(AUDIO_CHANNEL_BGM, AUDIO_SAMPLES_COUNT, PSP_AUDIO_FORMAT_STEREO);
    if (s_bgm_channel < 0) return FALSE;

    memset(&s_bgm_track, 0, sizeof(s_bgm_track));

    s_audio_thread_running = TRUE;
    s_audio_thread = sceKernelCreateThread("pal_audio_thread", AudioThreadEntry, 0x12, 0x10000, 0, NULL);
    if (s_audio_thread >= 0) {
        sceKernelStartThread(s_audio_thread, 0, NULL);
    }

    /* La fréquence réelle de sortie sceAudio est fixée par le matériel
     * (44.1kHz) ; si les assets sources sont à une autre fréquence, il
     * faut les rééchantillonner à la conversion (étape de build), pas ici. */
    (void)frequency;

    return TRUE;
}

void PAL_Audio_Shutdown(void) {
    s_audio_thread_running = FALSE;
    if (s_audio_thread >= 0) {
        sceKernelWaitThreadEnd(s_audio_thread, NULL);
        sceKernelDeleteThread(s_audio_thread);
        s_audio_thread = -1;
    }
    if (s_bgm_channel >= 0) {
        sceAudioChRelease(s_bgm_channel);
        s_bgm_channel = -1;
    }
    free(s_bgm_track.pcm_data);
}

PAL_AudioHandle PAL_Audio_LoadMusic(const char* path) {
    /* Charge un fichier PCM brut stéréo 16 bits déjà converti au moment
     * du build (voir tools/convert_audio dans le plan de portage) —
     * PAS un fichier SDAT original, qui nécessiterait un décodeur dédié. */
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    PspAudioTrack* track = (PspAudioTrack*)calloc(1, sizeof(PspAudioTrack));
    track->pcm_data = (s16*)malloc(size);
    fread(track->pcm_data, 1, size, f);
    fclose(f);

    track->pcm_len = size / (2 * sizeof(s16)); /* stéréo 16 bits */
    track->pcm_pos = 0;
    track->loop = FALSE;
    track->playing = FALSE;

    return (PAL_AudioHandle)track;
}

void PAL_Audio_PlayMusic(PAL_AudioHandle music, BOOL loop) {
    if (!music) return;
    PspAudioTrack* track = (PspAudioTrack*)music;

    free(s_bgm_track.pcm_data);
    s_bgm_track = *track;
    s_bgm_track.loop = loop;
    s_bgm_track.pcm_pos = 0;
    s_bgm_track.playing = TRUE;
}
