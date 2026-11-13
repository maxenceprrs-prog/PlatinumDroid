/**
 * @file audio_sdat.c
 * @brief Parseur SDAT Nintendo DS — lecture SWAV (PCM8, PCM16, IMA-ADPCM)
 *
 * Implémente le décodage des trois formats audio présents dans les SDAT DS :
 *   - Format 0 : PCM 8-bit  unsigned → converti s16
 *   - Format 1 : PCM 16-bit signed   → copie directe
 *   - Format 2 : IMA ADPCM 4-bit     → décodeur complet
 *
 * Référence : GBATEK NDS Audio / nocash specs
 */

#include "audio_sdat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Macros lecture little-endian                                        */
/* ------------------------------------------------------------------ */
#define RL16(p) ((uint16_t)((uint8_t)(p)[0] | ((uint8_t)(p)[1] << 8)))
#define RL32(p) ((uint32_t)((uint8_t)(p)[0] | ((uint8_t)(p)[1]<<8) | ((uint8_t)(p)[2]<<16) | ((uint8_t)(p)[3]<<24)))

/* ------------------------------------------------------------------ */
/* Structure interne                                                    */
/* ------------------------------------------------------------------ */
typedef struct {
    uint32_t offset; /* offset depuis début SDAT */
    uint32_t size;
    char     name[32];
} SwavEntry;

struct SdatIndex {
    uint8_t  *data;      /* contenu complet du SDAT */
    size_t    data_size;
    uint32_t  fat_offset;
    uint32_t  file_offset;
    SwavEntry *swav_entries;
    int        swav_count;
};

/* ------------------------------------------------------------------ */
/* IMA-ADPCM tables                                                    */
/* ------------------------------------------------------------------ */
static const int s_step_table[89] = {
    7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,
    50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,
    307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,
    1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,
    5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,
    20350,22385,24623,27086,29794,32767
};
static const int s_index_table[8] = { -1,-1,-1,-1, 2,4,6,8 };

static int ima_clamp(int v) {
    return v < -32768 ? -32768 : (v > 32767 ? 32767 : v);
}
static int ima_step_clamp(int idx) {
    return idx < 0 ? 0 : (idx > 88 ? 88 : idx);
}

/* ------------------------------------------------------------------ */
/* Sdat_Open                                                           */
/* ------------------------------------------------------------------ */
SdatIndex *Sdat_Open(const char *path) {
    if (!path) return NULL;

    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize < 64) { fclose(f); return NULL; }

    uint8_t *data = (uint8_t *)malloc((size_t)fsize);
    if (!data) { fclose(f); return NULL; }
    if (fread(data, 1, (size_t)fsize, f) != (size_t)fsize) {
        free(data); fclose(f); return NULL;
    }
    fclose(f);

    /* Validation magic "SDAT" */
    if (memcmp(data, "SDAT", 4) != 0) { free(data); return NULL; }

    SdatIndex *idx = (SdatIndex *)calloc(1, sizeof(SdatIndex));
    if (!idx) { free(data); return NULL; }
    idx->data      = data;
    idx->data_size = (size_t)fsize;

    /* Header SDAT : offset 0x10 = nombre de blocs, puis les offsets */
    /* Structure : SDAT header (0x40 bytes) → SYMB? → INFO? → FAT → FILE */
    /* On cherche les blocs par leur magic tag */
    uint32_t pos = 0x10; /* début des block records dans le header */
    uint32_t num_blocks = RL32(data + 0x0C); /* word count in header = includes blocks */
    /* En pratique on cherche FAT et FILE */

    /* Méthode robuste : scan des chunks par magic */
    uint32_t scan = 0x40; /* après le header SDAT fixe */
    uint32_t fat_off = 0, file_off = 0;
    uint32_t symb_off = 0;

    while (scan + 8 <= (uint32_t)fsize) {
        if (memcmp(data + scan, "SYMB", 4) == 0) symb_off = scan;
        if (memcmp(data + scan, "FAT ", 4) == 0) fat_off  = scan;
        if (memcmp(data + scan, "FILE", 4) == 0) { file_off = scan; break; }
        uint32_t bsize = RL32(data + scan + 4);
        if (bsize < 8) break;
        scan += bsize;
    }
    (void)symb_off; /* utilisé plus bas pour les noms */

    if (!fat_off || !file_off) { free(data); free(idx); return NULL; }

    idx->fat_offset  = fat_off;
    idx->file_offset = file_off;

    /* FAT block : offset+8 = nombre d'entrées, puis 16 octets par entrée */
    uint32_t fat_count = RL32(data + fat_off + 8);
    if (fat_count == 0 || fat_off + 12 + fat_count * 16 > (uint32_t)fsize) {
        free(data); free(idx); return NULL;
    }

    /* Compter les SWAV via INFO block (si présent) */
    /* INFO block structure : offset+8 = 8 records (SEQ,SEQARC,BANK,WAVE=3,PLAYER,GROUP,PLAYER2,STRM) */
    /* Pour simplifier : on considère toutes les entrées FAT comme potentiellement des SWAV */
    /* Vraie approche : chercher INFO et extraire le sous-bloc WAVE (index 3) */
    int swav_count = 0;
    uint32_t swav_base_fat = 0;

    /* Chercher bloc INFO */
    uint32_t info_off = 0;
    scan = 0x40;
    while (scan + 8 <= (uint32_t)fsize) {
        if (memcmp(data + scan, "INFO", 4) == 0) { info_off = scan; break; }
        uint32_t bsize = RL32(data + scan + 4);
        if (bsize < 8) break;
        scan += bsize;
    }

    if (info_off) {
        /* INFO header : +8 = 8 uint32 offsets relatifs à info_off pour chaque record type */
        /* Record index 3 = WAVE ARCHIVE (SWAR) → mais SWAV est dans les fichiers FILE */
        /* En pratique, le nombre de SWAV = fat_count (approche simplifiée) */
        /* On utilise fat_count directement */
        swav_count     = (int)fat_count;
        swav_base_fat  = 0;
    } else {
        swav_count    = (int)fat_count;
        swav_base_fat = 0;
    }

    idx->swav_count   = swav_count;
    idx->swav_entries = (SwavEntry *)calloc((size_t)swav_count, sizeof(SwavEntry));
    if (!idx->swav_entries) { free(data); free(idx); return NULL; }

    /* Remplir les entrées depuis la FAT */
    for (int i = 0; i < swav_count; i++) {
        uint32_t entry_off = fat_off + 12 + (uint32_t)(swav_base_fat + i) * 16;
        if (entry_off + 8 > (uint32_t)fsize) break;
        idx->swav_entries[i].offset = RL32(data + entry_off);
        idx->swav_entries[i].size   = RL32(data + entry_off + 4);
        snprintf(idx->swav_entries[i].name, sizeof(idx->swav_entries[i].name), "swav_%04d", i);
    }

    /* Noms depuis SYMB si disponible (optionnel) */
    /* (omis pour la première version) */

    return idx;
}

/* ------------------------------------------------------------------ */
/* Sdat_Close                                                          */
/* ------------------------------------------------------------------ */
void Sdat_Close(SdatIndex *idx) {
    if (!idx) return;
    free(idx->swav_entries);
    free(idx->data);
    free(idx);
}

/* ------------------------------------------------------------------ */
/* Sdat_SwavCount / Sdat_SwavName                                     */
/* ------------------------------------------------------------------ */
int Sdat_SwavCount(const SdatIndex *idx) {
    return idx ? idx->swav_count : 0;
}

const char *Sdat_SwavName(const SdatIndex *idx, int swav_id) {
    if (!idx || swav_id < 0 || swav_id >= idx->swav_count) return NULL;
    return idx->swav_entries[swav_id].name;
}

/* ------------------------------------------------------------------ */
/* Sdat_DecodeSwav                                                     */
/* ------------------------------------------------------------------ */
SdatSwav Sdat_DecodeSwav(const SdatIndex *idx, int swav_id) {
    SdatSwav result = {NULL, 0, 0, 1, 0, 0};
    if (!idx || swav_id < 0 || swav_id >= idx->swav_count) return result;

    uint32_t off  = idx->swav_entries[swav_id].offset;
    uint32_t size = idx->swav_entries[swav_id].size;

    if (off == 0 || size < 16 || off + size > (uint32_t)idx->data_size) return result;

    const uint8_t *swav = idx->data + off;

    /* SWAV header (si le fichier commence par magic "SWAV") */
    int header_skip = 0;
    if (memcmp(swav, "SWAV", 4) == 0) {
        /* SWAV file : header 0x10 bytes + DATA block 0x08 bytes + WAVE header 0x10 bytes */
        header_skip = 0x18; /* après SWAV + DATA chunk header */
    }

    /* WAVE info header (10 bytes dans SWAV) */
    if (off + header_skip + 10 > (uint32_t)idx->data_size) return result;
    const uint8_t *wh = swav + header_skip;

    uint8_t  wave_type   = wh[0];     /* 0=PCM8, 1=PCM16, 2=ADPCM */
    uint8_t  loop_flag   = wh[1];
    uint16_t sample_rate = RL16(wh + 2);
    uint16_t time        = RL16(wh + 4); /* timer = 16756991 / sample_rate */
    uint32_t loop_start  = RL32(wh + 6); /* en words (16-bit) */
    uint32_t non_loop_len= 0;
    if (header_skip + 10 + 4 <= size) non_loop_len = RL32(wh + 10); /* en words */

    const uint8_t *pcm_raw = wh + 12;
    size_t         raw_size = size - (size_t)(pcm_raw - swav);

    if (raw_size == 0) return result;

    /* Calculer le nombre d'échantillons de sortie */
    size_t out_samples = 0;
    int16_t *out = NULL;

    switch (wave_type) {
    case 0: /* PCM 8-bit unsigned */
        out_samples = raw_size;
        out = (int16_t *)malloc(out_samples * sizeof(int16_t));
        if (!out) return result;
        for (size_t i = 0; i < out_samples; i++)
            out[i] = (int16_t)(((int)pcm_raw[i] - 128) * 256);
        break;

    case 1: /* PCM 16-bit signed */
        out_samples = raw_size / 2;
        out = (int16_t *)malloc(out_samples * sizeof(int16_t));
        if (!out) return result;
        memcpy(out, pcm_raw, out_samples * sizeof(int16_t));
        break;

    case 2: { /* IMA-ADPCM 4-bit */
        /* Header 4 bytes : initial predictor (s16) + initial step index (u8) + padding */
        if (raw_size < 4) return result;
        int predictor = (int)(int16_t)RL16(pcm_raw);
        int step_idx  = (int)pcm_raw[2];
        step_idx = ima_step_clamp(step_idx);

        const uint8_t *adpcm = pcm_raw + 4;
        size_t adpcm_size    = raw_size - 4;
        out_samples = adpcm_size * 2; /* 2 nibbles par octet */

        out = (int16_t *)malloc(out_samples * sizeof(int16_t));
        if (!out) return result;

        size_t out_pos = 0;
        for (size_t i = 0; i < adpcm_size && out_pos < out_samples; i++) {
            for (int nibble = 0; nibble < 2 && out_pos < out_samples; nibble++) {
                int n = (nibble == 0) ? (adpcm[i] & 0x0F) : ((adpcm[i] >> 4) & 0x0F);
                int step = s_step_table[step_idx];
                int diff  = step >> 3;
                if (n & 1) diff += step >> 2;
                if (n & 2) diff += step >> 1;
                if (n & 4) diff += step;
                if (n & 8) diff = -diff;
                predictor = ima_clamp(predictor + diff);
                out[out_pos++] = (int16_t)predictor;
                step_idx = ima_step_clamp(step_idx + s_index_table[n & 7]);
            }
        }
        break;
    }

    default:
        return result; /* format inconnu */
    }

    result.pcm         = out;
    result.num_samples = out_samples;
    result.sample_rate = (sample_rate > 0) ? sample_rate : 22050;
    result.channels    = 1;  /* SWAV DS = toujours mono */
    result.loop        = loop_flag ? 1 : 0;
    result.loop_start  = loop_start * 2; /* words → samples */
    return result;
}

/* ------------------------------------------------------------------ */
/* Sdat_FreeSwav                                                       */
/* ------------------------------------------------------------------ */
void Sdat_FreeSwav(SdatSwav *s) {
    if (!s) return;
    free(s->pcm);
    s->pcm         = NULL;
    s->num_samples = 0;
}

/* ------------------------------------------------------------------ */
/* Sdat_ExportWav                                                      */
/* ------------------------------------------------------------------ */
int Sdat_ExportWav(const SdatSwav *s, const char *out_path) {
    if (!s || !s->pcm || !out_path) return 0;

    FILE *f = fopen(out_path, "wb");
    if (!f) return 0;

    uint32_t data_size  = (uint32_t)(s->num_samples * 2);
    uint32_t riff_size  = 36 + data_size;
    int      byte_rate  = s->sample_rate * s->channels * 2;
    int      block_align= s->channels * 2;

    uint8_t hdr[44];
    memcpy(hdr, "RIFF", 4);
    hdr[4]=(riff_size)&0xFF; hdr[5]=(riff_size>>8)&0xFF; hdr[6]=(riff_size>>16)&0xFF; hdr[7]=(riff_size>>24)&0xFF;
    memcpy(hdr+8,"WAVE",4);
    memcpy(hdr+12,"fmt ",4);
    hdr[16]=16; hdr[17]=0; hdr[18]=0; hdr[19]=0;
    hdr[20]=1; hdr[21]=0;
    hdr[22]=(uint8_t)s->channels; hdr[23]=0;
    hdr[24]=s->sample_rate&0xFF; hdr[25]=(s->sample_rate>>8)&0xFF;
    hdr[26]=(s->sample_rate>>16)&0xFF; hdr[27]=(s->sample_rate>>24)&0xFF;
    hdr[28]=byte_rate&0xFF; hdr[29]=(byte_rate>>8)&0xFF;
    hdr[30]=(byte_rate>>16)&0xFF; hdr[31]=(byte_rate>>24)&0xFF;
    hdr[32]=(uint8_t)block_align; hdr[33]=0;
    hdr[34]=16; hdr[35]=0;
    memcpy(hdr+36,"data",4);
    hdr[40]=data_size&0xFF; hdr[41]=(data_size>>8)&0xFF;
    hdr[42]=(data_size>>16)&0xFF; hdr[43]=(data_size>>24)&0xFF;

    fwrite(hdr, 1, 44, f);
    fwrite(s->pcm, sizeof(int16_t), s->num_samples, f);
    fclose(f);
    return 1;
}
