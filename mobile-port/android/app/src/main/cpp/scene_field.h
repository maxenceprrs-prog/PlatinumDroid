#ifndef SCENE_FIELD_H
#define SCENE_FIELD_H

#include "platform/pal_graphics.h"
#include "platform/pal_input.h"
#include "psp_asset_loader.h"

typedef enum {
    SCENE_FIELD_RUNNING = 0,
    SCENE_FIELD_BACK_TO_TITLE
} SceneFieldResult;

BOOL SceneField_Init(PspAssetStore *store, PAL_Surface *main_screen, PAL_Surface *sub_screen);
SceneFieldResult SceneField_Update(PAL_Surface *main_screen, PAL_Surface *sub_screen,
                                   u32 held, u32 pressed);
void SceneField_Shutdown(void);

#endif /* SCENE_FIELD_H */
