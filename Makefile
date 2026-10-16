# Makefile PSPSDK pour le squelette de portage.
#
# PRÉREQUIS (à faire sur TA machine, pas dans ce conteneur qui n'a pas
# le toolchain PSP) :
#   1. Installer le PSPSDK : https://github.com/pspdev/pspdev
#      (fournit psp-gcc, psp-g++, PSPSDK, pspdev en variable d'env)
#   2. export PSPDEV=/chemin/vers/pspdev ; export PATH=$PATH:$PSPDEV/bin
#
# Build :
#   cd src/platform/psp && make
#
# Ça produit un EBOOT.PBP à copier dans PSP/GAME/POKEPLATINUM/ sur ta
# Memory Stick (ou son émulation dans PPSSPP).

TARGET = pokeplatinum_psp
OBJS = main_psp.o pal_graphics_psp.o pal_input_psp.o pal_audio_psp.o nds_gfx.o nds_sprite.o psp_asset_loader.o psp_asset_renderer.o

INCDIR = ../../../include
CFLAGS = -O2 -G0 -Wall -I$(INCDIR)
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBS = -lpspgu -lpspgum -lpspaudio -lpspctrl -lpspdisplay -lm

BUILD_PRX = 1
PSP_FW_VERSION = 660

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Pokemon Platinum (Homebrew Port - WIP)

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
