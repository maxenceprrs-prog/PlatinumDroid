#ifndef PAL_INPUT_H
#define PAL_INPUT_H

#include "pal_common.h"

#define PAL_BUTTON_A      (1 << 0)
#define PAL_BUTTON_B      (1 << 1)
#define PAL_BUTTON_SELECT (1 << 2)
#define PAL_BUTTON_START  (1 << 3)
#define PAL_BUTTON_RIGHT  (1 << 4)
#define PAL_BUTTON_LEFT   (1 << 5)
#define PAL_BUTTON_UP     (1 << 6)
#define PAL_BUTTON_DOWN   (1 << 7)
#define PAL_BUTTON_R      (1 << 8)
#define PAL_BUTTON_L      (1 << 9)
#define PAL_BUTTON_X      (1 << 10)
#define PAL_BUTTON_Y      (1 << 11)

typedef struct {
    int x;
    int y;
    BOOL pressed;
    BOOL held;
    BOOL released;
} PAL_TouchState;

BOOL PAL_Input_Init(void);
void PAL_Input_Shutdown(void);
void PAL_Input_Update(void);

u32  PAL_Input_GetHeld(void);
u32  PAL_Input_GetPressed(void);
u32  PAL_Input_GetReleased(void);

PAL_TouchState PAL_Input_GetTouch(void);
BOOL           PAL_Input_IsTouchPressed(void);

#endif /* PAL_INPUT_H */
