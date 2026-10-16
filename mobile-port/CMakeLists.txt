cmake_minimum_required(VERSION 3.16)
project(PokemonPlatinumMobile C)

set(CMAKE_C_STANDARD 11)

find_package(SDL2 REQUIRED)

include_directories(
    include
    src
    ${SDL2_INCLUDE_DIRS}
)

set(SOURCES
    src/main_mobile.c
    src/pal_graphics_sdl2.c
    src/pal_input_sdl2.c
    src/scene_title.c
    src/nds_gfx.c
    src/nds_sprite.c
    src/psp_asset_loader.c
    src/psp_asset_renderer.c
)

add_executable(pokemon_platinum_mobile ${SOURCES})

target_link_libraries(pokemon_platinum_mobile
    ${SDL2_LIBRARIES}
    m
)
