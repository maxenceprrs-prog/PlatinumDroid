#ifndef MELONDS_CORE_H
#define MELONDS_CORE_H

#include <stdint.h>
#include <stddef.h>

#define NDS_SCREEN_WIDTH  256
#define NDS_SCREEN_HEIGHT 192

#ifdef __cplusplus
extern "C" {
#endif

// Masque officiel des touches Nintendo DS
enum NdsKey {
    NDS_KEY_A      = (1 << 0),
    NDS_KEY_B      = (1 << 1),
    NDS_KEY_SELECT = (1 << 2),
    NDS_KEY_START  = (1 << 3),
    NDS_KEY_RIGHT  = (1 << 4),
    NDS_KEY_LEFT   = (1 << 5),
    NDS_KEY_UP     = (1 << 6),
    NDS_KEY_DOWN   = (1 << 7),
    NDS_KEY_R      = (1 << 8),
    NDS_KEY_L      = (1 << 9),
    NDS_KEY_X      = (1 << 10),
    NDS_KEY_Y      = (1 << 11)
};

int  MelonDSCore_LoadROM(const char* filePath);
void MelonDSCore_UnloadROM(void);
void MelonDSCore_RunFrame(void);
void MelonDSCore_SetKeyMask(uint32_t mask);
void MelonDSCore_SetTouch(int x, int y, int touchDown);

const uint32_t* MelonDSCore_GetMainScreenBuffer(void);
const uint32_t* MelonDSCore_GetSubScreenBuffer(void);

size_t MelonDSCore_ReadAudio(int16_t* buffer, size_t numFrames);
int    MelonDSCore_IsLoaded(void);

#ifdef __cplusplus
}
#endif

#endif // MELONDS_CORE_H
