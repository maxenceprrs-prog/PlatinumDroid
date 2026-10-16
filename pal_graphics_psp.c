/**
 * @file pal_graphics_psp.c
 * @brief Implémentation PSP de la couche graphique (PAL_Graphics) via sceGu.
 *
 * PROBLÈME DE FOND À COMPRENDRE AVANT DE LIRE CE FICHIER :
 * La PSP a UN SEUL écran (480x272). La DS en a DEUX (256x192 chacun,
 * empilés). Pokémon Platine utilise activement les deux écrans en
 * permanence (carte du bas, Pokétch, menus tactiles...). Il n'y a pas
 * de solution "évidente" — j'ai choisi une approche pragmatique :
 *
 *   - Écran MAIN (jeu, combats, overworld) : mis à l'échelle pour
 *     remplir tout l'écran PSP (256x192 -> zoom vers 480x272,
 *     donc légère déformation d'aspect ratio, ou letterbox si on
 *     préfère préserver les proportions — voir SCALE_MODE ci-dessous).
 *   - Écran SUB (carte, Pokétch) : rendu en overlay redimensionné,
 *     activé/désactivé avec le bouton SELECT (voir pal_input_psp.c).
 *
 * C'est un choix de design, pas une vérité gravée dans le marbre —
 * à ajuster une fois que quelque chose s'affiche réellement à l'écran.
 *
 * Approche technique : chaque "écran" DS est un buffer logiciel
 * RGBA8888 en RAM CPU (comme le ferait un vrai framebuffer DS-like).
 * On le blitte/transforme en mémoire, puis on l'upload dans une
 * texture GU et on l'affiche via un simple quad texturé en mode 2D
 * orthographique. C'est l'approche la plus simple qui permette de
 * réutiliser telles quelles les routines de rendu logicielles
 * (tilemap, sprites) qui viennent du moteur, sans réécrire tout le
 * pipeline de rendu en primitives GU natives dès le premier jet.
 */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <string.h>
#include <malloc.h>
#include <stddef.h>

#include "platform/pal_graphics.h"
#include "platform/psp/pal_graphics_psp_ext.h"
#include "formats/nds_gfx.h"
#include "formats/nds_sprite.h"

#define PSP_SCREEN_WIDTH  480
#define PSP_SCREEN_HEIGHT 272
#define PSP_BUF_WIDTH     512   /* Les textures/framebuffers GU doivent être des puissances de 2 en stride */

#define DS_SCREEN_WIDTH   256
#define DS_SCREEN_HEIGHT  192

/* Chaque texture GU doit avoir des dimensions puissance de 2 */
#define TEX_WIDTH  256
#define TEX_HEIGHT 256

typedef struct {
    float u, v;
    unsigned int color;
    float x, y, z;
} Vertex;

struct PAL_Surface {
    u32* pixels;        /* Buffer logiciel RGBA8888, taille TEX_WIDTH*TEX_HEIGHT */
    int  logical_w;     /* 256 (DS_SCREEN_WIDTH) */
    int  logical_h;     /* 192 (DS_SCREEN_HEIGHT) */
    void* gu_texture;   /* Zone mémoire alignée utilisée comme texture GU */
    BOOL dirty;         /* TRUE si le buffer logiciel a changé depuis le dernier upload GPU */
};

/* Display lists GU (doivent être alignées et dans une zone non cachée pour le DMA) */
static unsigned int __attribute__((aligned(16))) s_display_list[262144 / sizeof(unsigned int)];

/* Framebuffers GU (VRAM PSP, pas VRAM DS !) */
static void* s_fb0 = (void*)0;
static void* s_fb1 = (void*)0x88000;   /* offset VRAM standard pour double buffer 512*272*4 */
static void* s_zbuf = (void*)0x110000;

static PAL_Surface s_screens[PAL_SCREEN_MAX];
static PAL_Screen s_active_screen = PAL_SCREEN_MAIN;
static BOOL s_show_sub_overlay = FALSE; /* togglé par SELECT, voir pal_input_psp.c */

BOOL PAL_Graphics_Init(int window_width, int window_height) {
    (void)window_width;
    (void)window_height;

    sceGuInit();
    sceGuStart(GU_DIRECT, s_display_list);

    sceGuDrawBuffer(GU_PSM_8888, s_fb0, PSP_BUF_WIDTH);
    sceGuDispBuffer(PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, s_fb1, PSP_BUF_WIDTH);
    sceGuDepthBuffer(s_zbuf, PSP_BUF_WIDTH);

    sceGuOffset(2048 - (PSP_SCREEN_WIDTH / 2), 2048 - (PSP_SCREEN_HEIGHT / 2));
    sceGuViewport(2048, 2048, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);
    sceGuDepthRange(0xc350, 0x2710);

    sceGuScissor(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);

    /* Rendu 2D pur : pas de test de profondeur, pas de culling.
     * Platine n'a besoin de vraie 3D que pour l'écran-titre (Giratina) et
     * quelques effets de caméra en overworld — ça viendra dans une passe
     * ultérieure une fois le rendu 2D de base validé. */
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST); /* NEAREST = fidèle au pixel-art DS, pas de flou */
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    /* Allocation des deux "écrans" logiques façon DS */
    for (int i = 0; i < PAL_SCREEN_MAX; i++) {
        s_screens[i].pixels = (u32*)memalign(16, TEX_WIDTH * TEX_HEIGHT * sizeof(u32));
        memset(s_screens[i].pixels, 0, TEX_WIDTH * TEX_HEIGHT * sizeof(u32));
        s_screens[i].logical_w = DS_SCREEN_WIDTH;
        s_screens[i].logical_h = DS_SCREEN_HEIGHT;
        s_screens[i].gu_texture = s_screens[i].pixels; /* upload direct depuis ce buffer */
        s_screens[i].dirty = TRUE;
    }

    return TRUE;
}

void PAL_Graphics_Shutdown(void) {
    for (int i = 0; i < PAL_SCREEN_MAX; i++) {
        if (s_screens[i].pixels) {
            free(s_screens[i].pixels);
            s_screens[i].pixels = NULL;
        }
    }
    sceGuTerm();
}

PAL_Surface* PAL_Graphics_GetScreen(PAL_Screen screen) {
    if (screen < 0 || screen >= PAL_SCREEN_MAX) return NULL;
    return &s_screens[screen];
}

void PAL_Graphics_SetActiveScreen(PAL_Screen screen) {
    s_active_screen = screen;
}

void PAL_Graphics_BeginFrame(void) {
    sceGuStart(GU_DIRECT, s_display_list);
    sceGuClearColor(0xff000000);
    sceGuClear(GU_COLOR_BUFFER_BIT);
}

/* Dessine une des deux surfaces logiques DS comme un quad texturé,
 * à la position/taille données en coordonnées écran PSP. */
static void BlitScreenToDisplay(PAL_Surface* surf, int dst_x, int dst_y, int dst_w, int dst_h) {
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexImage(0, TEX_WIDTH, TEX_HEIGHT, TEX_WIDTH, surf->gu_texture);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);

    Vertex* verts = (Vertex*)sceGuGetMemory(2 * sizeof(Vertex));
    verts[0].u = 0.0f;
    verts[0].v = 0.0f;
    verts[0].color = 0xffffffff;
    verts[0].x = (float)dst_x;
    verts[0].y = (float)dst_y;
    verts[0].z = 0.0f;

    verts[1].u = (float)surf->logical_w;
    verts[1].v = (float)surf->logical_h;
    verts[1].color = 0xffffffff;
    verts[1].x = (float)(dst_x + dst_w);
    verts[1].y = (float)(dst_y + dst_h);
    verts[1].z = 0.0f;

    sceGuDrawArray(GU_SPRITES,
                   GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                   2, 0, verts);
}

void PAL_Graphics_EndFrame(void) {
    /* Écran principal : plein écran PSP (voir note de design en tête de fichier) */
    BlitScreenToDisplay(&s_screens[PAL_SCREEN_MAIN], 0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);

    /* Écran du bas (carte/Pokétch) en overlay réduit, coin inférieur droit,
     * activable via SELECT. Provisoire : à remplacer par un vrai écran
     * dédié accessible via un menu, une fois le reste plus avancé. */
    if (s_show_sub_overlay) {
        int overlay_w = 160, overlay_h = 120;
        BlitScreenToDisplay(&s_screens[PAL_SCREEN_SUB],
                             PSP_SCREEN_WIDTH - overlay_w - 4,
                             PSP_SCREEN_HEIGHT - overlay_h - 4,
                             overlay_w, overlay_h);
    }

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

void PAL_Graphics_DrawSprite(PAL_Surface* surf, const void* sprite_data,
                              int x, int y, int width, int height) {
    /* TODO (Phase suivante) : le format des sprites DS décompilés est du
     * paletté 4/8bpp (OAM), pas du RGBA direct. Cette fonction doit
     * dépalettiser avant d'écrire dans surf->pixels — même logique que
     * le PAL SDL (voir pal_sprite.h), on ne fait que changer la cible
     * d'écriture finale (RGBA8888 logiciel au lieu d'une texture SDL).
     * Non implémenté ici pour rester honnête sur l'état d'avancement. */
    (void)surf; (void)sprite_data; (void)x; (void)y; (void)width; (void)height;
    surf->dirty = TRUE;
}

void PAL_Graphics_DrawTilemap(PAL_Surface* surf, const void* tilemap_data) {
    /* La signature héritée de pal_graphics.h ne prend que le plan de
     * tuiles (NSCR), pas les tuiles (NCGR) ni la palette (NCLR) — or on
     * a besoin des trois pour produire une image. C'est une limitation
     * de l'interface commune, pas de ce fichier : impossible de dessiner
     * quoi que ce soit de correct avec un seul des trois blobs.
     * Utiliser PAL_Graphics_DrawTilemapFull() ci-dessous, qui prend les
     * trois et fait le vrai travail via le décodeur nds_gfx. */
    (void)surf; (void)tilemap_data;
}

/**
 * Version complète (extension PSP, pas dans l'interface PAL d'origine) :
 * décode un plan de tuiles NSCR + son jeu de tuiles NCGR + sa palette
 * NCLR, et écrit le résultat en RGBA8888 dans le buffer logiciel de la
 * surface, à la position (dst_x, dst_y). C'est ce qu'il faut appeler
 * concrètement depuis le code de rendu de fond/overworld.
 *
 * Retourne FALSE si un des trois blobs est invalide (mauvais magic,
 * fichier tronqué, etc.) — dans ce cas rien n'est dessiné, plutôt que
 * de risquer d'écrire n'importe quoi à l'écran.
 */
BOOL PAL_Graphics_DrawTilemapFull(PAL_Surface* surf,
                                   const void* nscr_data, size_t nscr_size,
                                   const void* ncgr_data, size_t ncgr_size,
                                   const void* nclr_data, size_t nclr_size,
                                   int dst_x, int dst_y) {
    if (!surf) return FALSE;

    NdsGfx_Screen screen = {0};
    NdsGfx_Tileset tileset = {0};
    NdsGfx_Palette palette = {0};
    BOOL ok = FALSE;

    if (NdsGfx_ParseScreen(nscr_data, nscr_size, &screen) &&
        NdsGfx_ParseTileset(ncgr_data, ncgr_size, &tileset) &&
        NdsGfx_ParsePalette(nclr_data, nclr_size, &palette)) {

        /* On rend dans un buffer temporaire à la taille exacte du plan de
         * tuiles, puis on le blitte dans la surface cible à (dst_x,dst_y) —
         * évite de dépasser du buffer TEX_WIDTH*TEX_HEIGHT si le plan de
         * tuiles est plus grand qu'un écran DS (cas des grandes cartes). */
        int px_w = screen.tiles_wide * 8;
        int px_h = screen.tiles_high * 8;
        uint32_t* temp = (uint32_t*)malloc((size_t)px_w * px_h * sizeof(uint32_t));

        if (temp && NdsGfx_RenderScreenToRGBA(&screen, &tileset, &palette, temp, px_w)) {
            for (int row = 0; row < px_h; row++) {
                int out_y = dst_y + row;
                if (out_y < 0 || out_y >= TEX_HEIGHT) continue;
                int copy_w = px_w;
                if (dst_x + copy_w > TEX_WIDTH) copy_w = TEX_WIDTH - dst_x;
                if (copy_w > 0 && dst_x >= 0) {
                    memcpy(&surf->pixels[out_y * TEX_WIDTH + dst_x],
                           &temp[row * px_w],
                           (size_t)copy_w * sizeof(uint32_t));
                }
            }
            surf->dirty = TRUE;
            ok = TRUE;
        }
        free(temp);
    }

    NdsGfx_FreeScreen(&screen);
    NdsGfx_FreeTileset(&tileset);
    return ok;
}

BOOL PAL_Graphics_DrawNcerCell(PAL_Surface* surf,
                                const void* ncer_data, size_t ncer_size, int cell_index,
                                const void* ncgr_data, size_t ncgr_size,
                                const void* nclr_data, size_t nclr_size,
                                int dst_x, int dst_y, int cell_width, int cell_height) {
    NdsGfx_CellBank bank = {0};
    NdsGfx_Tileset tileset = {0};
    NdsGfx_Palette palette = {0};
    uint32_t* temp = NULL;
    BOOL ok = FALSE;
    if (!surf || cell_width <= 0 || cell_height <= 0 ||
        cell_width > TEX_WIDTH || cell_height > TEX_HEIGHT) goto done;
    if (!NdsGfx_ParseCellBank(ncer_data, ncer_size, &bank) ||
        !NdsGfx_ParseTileset(ncgr_data, ncgr_size, &tileset) ||
        !NdsGfx_ParsePalette(nclr_data, nclr_size, &palette)) goto done;
    temp = (uint32_t*)calloc((size_t)cell_width * cell_height, sizeof(uint32_t));
    if (!temp || !NdsGfx_RenderCellToRGBA(&bank, cell_index, &tileset, &palette,
                                           temp, cell_width, cell_height, cell_width)) goto done;
    for (int y = 0; y < cell_height; ++y) for (int x = 0; x < cell_width; ++x) {
        int dx = dst_x + x, dy = dst_y + y;
        uint32_t pixel = temp[y * cell_width + x];
        if ((pixel >> 24) && dx >= 0 && dx < TEX_WIDTH && dy >= 0 && dy < TEX_HEIGHT)
            surf->pixels[dy * TEX_WIDTH + dx] = pixel;
    }
    surf->dirty = TRUE;
    ok = TRUE;
done:
    free(temp);
    NdsGfx_FreeCellBank(&bank);
    NdsGfx_FreeTileset(&tileset);
    return ok;
}

void PAL_Graphics_BlitSurface(PAL_Surface* src, PAL_Surface* dst,
                               int sx, int sy, int w, int h, int dx, int dy) {
    if (!src || !dst) return;
    for (int row = 0; row < h; row++) {
        int src_y = sy + row;
        int dst_y = dy + row;
        if (src_y < 0 || src_y >= TEX_HEIGHT) continue;
        if (dst_y < 0 || dst_y >= TEX_HEIGHT) continue;
        memcpy(&dst->pixels[dst_y * TEX_WIDTH + dx],
               &src->pixels[src_y * TEX_WIDTH + sx],
               w * sizeof(u32));
    }
    dst->dirty = TRUE;
}

void* PAL_Graphics_AllocVRAM(size_t size) {
    /* Sur PSP, pas de VRAM séparée adressable comme sur DS : on alloue
     * simplement de la RAM alignée, la texture GU sera uploadée depuis
     * là au moment du rendu. */
    return memalign(16, size);
}

void PAL_Graphics_FreeVRAM(void* ptr) {
    free(ptr);
}

void PAL_Graphics_TransferVRAM(void* src, void* dst, size_t size) {
    memcpy(dst, src, size);
}
