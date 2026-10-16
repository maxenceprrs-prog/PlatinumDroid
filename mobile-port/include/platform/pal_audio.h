#ifndef PAL_AUDIO_H
#define PAL_AUDIO_H

#include "pal_common.h"

typedef void* PAL_AudioHandle;

BOOL PAL_Audio_Init(int frequency, int channels);
void PAL_Audio_Shutdown(void);

PAL_AudioHandle PAL_Audio_LoadMusic(const char* path);
void            PAL_Audio_PlayMusic(PAL_AudioHandle music, BOOL loop);

#endif /* PAL_AUDIO_H */
