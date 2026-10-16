/* Test natif (gcc classique) du décodeur nds_gfx.c contre de vrais
 * fichiers du dépôt. Produit un PPM (format trivial, pas besoin de
 * libpng) pour inspection visuelle. Ne fait PAS partie du build PSP. */
#include <stdio.h>
#include <stdlib.h>
#include "nds_gfx.h"

static void* ReadWholeFile(const char* path, size_t* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Impossible d'ouvrir %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    void* buf = malloc(size);
    fread(buf, 1, size, f);
    fclose(f);
    *out_size = (size_t)size;
    return buf;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s fichier.NSCR fichier.NCGR fichier.NCLR sortie.ppm\n", argv[0]);
        return 1;
    }

    size_t nscr_size, ncgr_size, nclr_size;
    void* nscr_data = ReadWholeFile(argv[1], &nscr_size);
    void* ncgr_data = ReadWholeFile(argv[2], &ncgr_size);
    void* nclr_data = ReadWholeFile(argv[3], &nclr_size);
    if (!nscr_data || !ncgr_data || !nclr_data) return 1;

    NdsGfx_Screen screen = {0};
    NdsGfx_Tileset tileset = {0};
    NdsGfx_Palette palette = {0};

    if (!NdsGfx_ParseScreen(nscr_data, nscr_size, &screen)) {
        fprintf(stderr, "ECHEC parsing NSCR\n"); return 1;
    }
    printf("NSCR: %d x %d tuiles (%d x %d px)\n", screen.tiles_wide, screen.tiles_high,
           screen.tiles_wide * 8, screen.tiles_high * 8);

    if (!NdsGfx_ParseTileset(ncgr_data, ncgr_size, &tileset)) {
        fprintf(stderr, "ECHEC parsing NCGR\n"); return 1;
    }
    printf("NCGR: %d tuiles, %s\n", tileset.num_tiles, tileset.is_8bpp ? "8bpp" : "4bpp");

    if (!NdsGfx_ParsePalette(nclr_data, nclr_size, &palette)) {
        fprintf(stderr, "ECHEC parsing NCLR\n"); return 1;
    }
    printf("NCLR: %d couleurs, %s\n", palette.num_colors, palette.is_8bpp ? "8bpp" : "4bpp");

    int w = screen.tiles_wide * 8;
    int h = screen.tiles_high * 8;
    uint32_t* rgba = (uint32_t*)malloc((size_t)w * h * sizeof(uint32_t));

    if (!NdsGfx_RenderScreenToRGBA(&screen, &tileset, &palette, rgba, w)) {
        fprintf(stderr, "ECHEC composition finale\n"); return 1;
    }

    FILE* out = fopen(argv[4], "wb");
    fprintf(out, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) {
        uint8_t r = rgba[i] & 0xFF;
        uint8_t g = (rgba[i] >> 8) & 0xFF;
        uint8_t b = (rgba[i] >> 16) & 0xFF;
        fputc(r, out); fputc(g, out); fputc(b, out);
    }
    fclose(out);

    printf("OK -> %s\n", argv[4]);
    return 0;
}
