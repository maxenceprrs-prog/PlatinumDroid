#ifndef PAL_GRAPHICS_H
#define PAL_GRAPHICS_H

#include "pal_common.h"

typedef struct PAL_Surface PAL_Surface;

typedef enum {
    PAL_SCREEN_MAIN = 0,
    PAL_SCREEN_SUB  = 1,
    PAL_SCREEN_MAX  = 2
} PAL_Screen;

BOOL PAL_Graphics_Init(int window_width, int window_height);
void PAL_Graphics_Shutdown(void);

PAL_Surface* PAL_Graphics_GetScreen(PAL_Screen screen);
void PAL_Graphics_SetActiveScreen(PAL_Screen screen);

void PAL_Graphics_BeginFrame(void);
void PAL_Graphics_EndFrame(void);

void PAL_Graphics_DrawSprite(PAL_Surface* surf, const void* sprite_data,
                             int x, int y, int width, int height);

void PAL_Graphics_DrawTilemap(PAL_Surface* surf, const void* tilemap_data);

void PAL_Graphics_BlitSurface(PAL_Surface* src, PAL_Surface* dst,
                              int sx, int sy, int w, int h, int dx, int dy);

void* PAL_Graphics_AllocVRAM(size_t size);
void  PAL_Graphics_FreeVRAM(void* ptr);
void  PAL_Graphics_TransferVRAM(void* src, void* dst, size_t size);

#endif /* PAL_GRAPHICS_H */
