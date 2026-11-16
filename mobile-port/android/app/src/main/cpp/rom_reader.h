/**
 * @file rom_reader.h
 * @brief Lecteur natif NitroFS pour ROM Nintendo DS (.nds) de Pokemon Platine
 */

#ifndef ROM_READER_H
#define ROM_READER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char     game_title[13];     /* "POKEMON PL\0" */
    char     game_code[5];       /* "CPUE\0" */
    uint32_t rom_size;           /* 128 MB = 134217728 */
    uint32_t fat_offset;
    uint32_t fat_size;
    uint32_t fnt_offset;
    uint32_t fnt_size;
    int      is_loaded;
} NdsRomInfo;

/**
 * Initialise le lecteur de ROM.
 * Tente d'ouvrir la ROM depuis le chemin fourni, ou scanne les chemins par défaut.
 * @param preferred_path Chemin optionnel vers la ROM, ou NULL.
 * @return 1 si la ROM Nintendo DS valide a été trouvée et ouverte, 0 sinon.
 */
int RomReader_Init(const char *preferred_path);

/**
 * Retourne les informations sur la ROM actuellement chargée.
 */
const NdsRomInfo *RomReader_GetInfo(void);

/**
 * Vérifie si la ROM est chargée avec succès.
 */
int RomReader_IsLoaded(void);

/**
 * Lit un fichier brut depuis la ROM par son File ID FAT (0 .. num_files-1).
 * Alloue le buffer de sortie avec malloc(). L'appelant doit free().
 */
int RomReader_ReadFile(uint32_t file_id, void **out_data, size_t *out_size);

/**
 * Lit un bloc brut à un offset absolu dans la ROM.
 */
int RomReader_ReadRaw(uint32_t offset, void *dest, size_t size);

/**
 * Ferme la ROM et libère la table FAT/FNT en mémoire.
 */
void RomReader_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* ROM_READER_H */
