#!/bin/sh
# build_psp.sh — Compile le portage PSP dans WSL Ubuntu-PSP
set -e

PSPDEV=/usr/local/pspdev
export PSPDEV
export PATH=/usr/local/pspdev/bin:/usr/bin:/bin

SRC="/mnt/c/Users/philippe/Music/Pokemon psp"
BUILD="$SRC/build_psp"

psp-gcc --version
PSPSDK=$(psp-config --pspsdk-path)
echo "PSPSDK = $PSPSDK"
mkdir -p "$BUILD"
cd "$BUILD"

compile() {
    echo "  CC  $1"
    psp-gcc -O2 -G0 -Wall -Wno-unused-function \
        -I"$PSPSDK/include" \
        -I"$SRC/include" \
        -I"$SRC" \
        -c "$SRC/$1" -o "$2"
}

compile nds_gfx.c          nds_gfx.o
compile nds_sprite.c       nds_sprite.o
compile pal_graphics_psp.c pal_graphics_psp.o
compile pal_input_psp.c    pal_input_psp.o
compile pal_audio_psp.c    pal_audio_psp.o
compile psp_asset_loader.c psp_asset_loader.o
compile psp_asset_renderer.c psp_asset_renderer.o
compile scene_title.c      scene_title.o
compile main_psp.c         main_psp.o

echo "  LINK"
OBJS="main_psp.o pal_graphics_psp.o pal_input_psp.o pal_audio_psp.o nds_gfx.o nds_sprite.o psp_asset_loader.o psp_asset_renderer.o scene_title.o"
LIBS="-lpspgu -lpspgum -lpspaudio -lpspctrl -lpspdisplay -lpspge -lm"

psp-gcc -specs="$PSPSDK/lib/prxspecs" \
    -Wl,-q,-T"$PSPSDK/lib/linkfile.prx" \
    $OBJS $LIBS -L"$PSPSDK/lib" -lpspsdk -lc \
    -o pokeplatinum_psp.elf

psp-prxgen pokeplatinum_psp.elf pokeplatinum_psp.prx 2>/dev/null || true

echo "  MKSFO + PACK-PBP"
mksfo "Pokemon Platine (PSP)" PARAM.SFO
if [ -f pokeplatinum_psp.prx ]; then
    pack-pbp EBOOT.PBP PARAM.SFO NULL NULL NULL NULL NULL pokeplatinum_psp.prx NULL
else
    pack-pbp EBOOT.PBP PARAM.SFO NULL NULL NULL NULL NULL pokeplatinum_psp.elf NULL
fi

echo ""
echo "===== BUILD OK ====="
ls -lh "$BUILD/EBOOT.PBP"
