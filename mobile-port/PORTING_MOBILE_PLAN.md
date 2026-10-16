# Plan d'Architecture & Portage Mobile : Pokémon Platine

Ce document décrit l'architecture du portage natif de **Pokémon Platine** pour smartphones (Android / iOS) via la couche portable SDL2.

---

## 1. Architecture Générale

```
+-------------------------------------------------------+
|             Logique du Jeu (C Décompilé)              |
|        (Système de combat, Pokémon, Overworld)        |
+-------------------------------------------------------+
                           |
+-------------------------------------------------------+
|             PAL (Platform Abstraction Layer)          |
|   +---------------------+   +---------------------+   |
|   |  pal_graphics_sdl2  |   |   pal_input_sdl2    |   |
|   |  (Double-écran DS)  |   | (Touchscreen+Boutons|   |
|   +---------------------+   +---------------------+   |
+-------------------------------------------------------+
                           |
+-------------------------------------------------------+
|                     SDL2 Backend                      |
|           (OpenGL ES / Vulkan / Metal)                |
+-------------------------------------------------------+
                           |
+-------------------------------------------------------+
|       Système Hôte (Android APK / iOS / Desktop)       |
+-------------------------------------------------------+
```

---

## 2. Gestion du Double Écran DS sur Smartphone

Les smartphones ont un ratio d'écran allongé (18:9, 19:9 ou 20:9).
La couche `pal_graphics_sdl2` implémente deux modes d'affichage dynamiques :

1. **Mode Portrait (recommandé pour jouer à une main)** :
   * **Écran supérieur DS** : placé en haut (scène principale, combats, exploration).
   * **Écran inférieur DS** : placé en bas (Pokétch, menus tactiles, choix d'attaques).
   * Les événements tactiles sur l'écran du bas sont transmis directement comme des coordonnées stylet.

2. **Mode Paysage (horizontal)** :
   * Les deux écrans sont affichés côte à côte ou l'écran tactile est affiché en incrustation/switchable selon le choix du joueur.

---

## 3. Structure des Fichiers Créés

* [`mobile-port/CMakeLists.txt`](file:///c:/Users/philippe/Music/Pokemon%20psp/mobile-port/CMakeLists.txt) : Script de compilation multiplateforme (Linux/macOS/Windows/Android NDK).
* [`mobile-port/src/pal_graphics_sdl2.c`](file:///c:/Users/philippe/Music/Pokemon%20psp/mobile-port/src/pal_graphics_sdl2.c) : Moteur de rendu SDL2 double buffer (Texture Streaming RGBA8888).
* [`mobile-port/src/pal_input_sdl2.c`](file:///c:/Users/philippe/Music/Pokemon%20psp/mobile-port/src/pal_input_sdl2.c) : Gestion du TouchScreen mobile (`SDL_FINGERDOWN`, `SDL_FINGERUP`, `SDL_FINGERMOTION`) et des boutons.
* [`mobile-port/src/main_mobile.c`](file:///c:/Users/philippe/Music/Pokemon%20psp/mobile-port/src/main_mobile.c) : Boucle de jeu principale cadencée à 60 FPS.
* [`mobile-port/build/pokemon_platinum_mobile`](file:///c:/Users/philippe/Music/Pokemon%20psp/mobile-port/build/pokemon_platinum_mobile) : Exécutable binaire compilé avec succès.

---

## 4. Packaging pour Smartphone (Android)

Pour générer le fichier **APK** final installable sur téléphone :
1. SDL2 fournit le template officiel `build-scripts/androidbuild.sh` ou un projet standard **Android Studio / Gradle** avec `CMake`.
2. Le code C présent dans `mobile-port/src/` est linké comme bibliothèque native `libmain.so`.
3. Les assets dans `mobile-port/build/psp-assets/` sont placés dans le dossier `assets/` de l'APK.
