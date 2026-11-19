cmake_minimum_required(VERSION 3.22.1)
project(pokeplatinum LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(pokeplatinum SHARED
    jni_bridge.c
    scene_title.c
    scene_game.c
    nds_gfx.c
    nds_sprite.c
    psp_asset_loader.c
    psp_asset_renderer.c
    pal_graphics_android.c
    audio_assets.c
    audio_sdat.c
    bgm_synth.c
    rom_reader.c
    melonds_core.c
    pal_audio_android.c
)

find_library(log-lib log)
find_library(android-lib android)

target_link_libraries(pokeplatinum
    ${log-lib}
    ${android-lib}
    m
)
