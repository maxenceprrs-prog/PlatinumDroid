# PlatinumDroid — État du projet au 23/10/2026

## ✅ Ce qui est fait

### APK Android fonctionnel
- **`PokemonPlatinum_Release.apk`** — 14 Mo, signé, installable
- Compilé pour **ARM64-v8a** et **x86_64**
- Double écran DS en portrait (256×192 × 2)
- Boutons virtuels tactiles (D-pad + A/B + START/SELECT)
- Scène titre animée avec menu CONTINUER / NOUVELLE PARTIE / OPTIONS

### Couche audio complète (nouveauté)
| Fichier | Rôle |
|---|---|
| `audio_assets.c/.h` | Chargeur WAV PCM 8/16-bit |
| `audio_sdat.c/.h` | Parseur SDAT DS (PCM8, PCM16, IMA-ADPCM 4-bit) |
| `pal_audio_android.c` | Backend AudioTrack via JNI + thread de mixing |

### Architecture C native (sans SDL)
```
MainActivity.java
    └─ JNI ──► jni_bridge.c
                  ├─ scene_title.c       (menu titre animé)
                  ├─ pal_graphics_android.c  (rendu double-écran)
                  ├─ nds_gfx.c / nds_sprite.c  (décodeurs DS)
                  ├─ psp_asset_loader.c / psp_asset_renderer.c
                  ├─ audio_assets.c      (WAV loader)
                  ├─ audio_sdat.c        (SDAT parser + IMA-ADPCM)
                  └─ pal_audio_android.c (AudioTrack thread)
```

---

## 🚧 Ce qui reste à faire

### Priorité 1 — Tester l'APK sur appareil réel
```powershell
# Installer via adb (téléphone branché en USB)
adb install "mobile-port\android\PokemonPlatinum_Release.apk"
```
Ou transférer directement le fichier APK sur le téléphone.

### Priorité 2 — Audio : brancher une musique SDAT réelle
Le pipeline audio est prêt, mais il faut :
1. Extraire un SWAV depuis le SDAT de la ROM avec `tools/inspect_sdat.mjs`
2. L'exporter en WAV via `Sdat_ExportWav()`
3. Appeler depuis Java : `nativeAudioSetSource("/path/to/music.wav", true)`

### Priorité 3 — Overworld et gameplay
Le plus gros chantier : intégrer le code C décompilé de `pokeplatinum-main` (1023 fichiers sources).  
**Voie recommandée :** utiliser `pspoke/build.sh platinum --rom <rom.nds>` pour compiler le moteur complet, puis adapter le JNI bridge.

### Priorité 4 — Amélioration UX
- [ ] Vibration haptique sur les boutons virtuels
- [ ] Sauvegarde persistante (SharedPreferences / fichier)
- [ ] Mode paysage (2 écrans côte à côte)
- [ ] Son SSEQ (séquenceur DS complet — très complexe)

---

## Structure des fichiers clés

```
mobile-port/
├── android/
│   ├── PokemonPlatinum_Release.apk   ← APK final (14 Mo)
│   ├── build_apk.sh                  ← Script de build WSL
│   └── app/src/main/
│       ├── java/com/game/pokeplatinum/
│       │   └── MainActivity.java     ← UI + AudioTrack + GameView
│       └── cpp/
│           ├── jni_bridge.c          ← Pont JNI principal
│           ├── scene_title.c         ← Menu titre
│           ├── pal_graphics_android.c
│           ├── audio_assets.c        ← WAV loader ← NOUVEAU
│           ├── audio_sdat.c          ← SDAT parser ← NOUVEAU
│           └── pal_audio_android.c   ← AudioTrack ← NOUVEAU
└── PORTING_MOBILE_PLAN.md
```

---

## Commande de build (WSL Ubuntu-PSP)
```bash
cd '/mnt/c/Users/philippe/Music/Pokemon psp/mobile-port/android'
sed -i 's/\r//' build_apk.sh
bash build_apk.sh
```
