#ifndef PAL_AUDIO_H
#define PAL_AUDIO_H

#include "pal_common.h"

typedef void* PAL_AudioHandle;

/* Initialisation du système audio (fréquence Hz, canaux 1=mono 2=stéréo) */
BOOL PAL_Audio_Init(int frequency, int channels);
void PAL_Audio_Shutdown(void);

/* Chargement et lecture d'un fichier WAV depuis le disque */
PAL_AudioHandle PAL_Audio_LoadMusic(const char *path);
void            PAL_Audio_PlayMusic(PAL_AudioHandle music, BOOL loop);
void            PAL_Audio_StopMusic(PAL_AudioHandle music);
void            PAL_Audio_FreeMusic(PAL_AudioHandle music);

/* Volume : 0.0f (silence) à 1.0f (max) */
void            PAL_Audio_SetVolume(float volume);

#endif /* PAL_AUDIO_H */

