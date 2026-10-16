# Portage PSP de Pokémon Platine — État des lieux et plan réaliste

## Ce qui existe réellement à ce stade (aujourd'hui)

| Composant | Fichier | État |
|---|---|---|
| Boilerplate homebrew PSP (module info, callback exit) | `src/platform/psp/main_psp.c` | ✅ Fonctionnel |
| Input (sceCtrl -> boutons DS) | `src/platform/psp/pal_input_psp.c` | ✅ Fonctionnel |
| Rendu 2D via sceGu (2 "écrans" logiciels façon DS, blit GPU) | `src/platform/psp/pal_graphics_psp.c` | ⚠️ Pipeline OK, intégration sprites PSP à faire |
| Décodeur NCLR/NCGR/NSCR -> RGBA8888 | `src/formats/nds_gfx.c` | ✅ **Testé sur un vrai fichier du jeu** (`ground0`), image correcte confirmée visuellement |
| Décodeur NCER (cellules/OAM) -> RGBA8888 | `src/formats/nds_sprite.c` | ✅ Mapping 1D 32/64/128/256 octets validé visuellement sur `smptm_koori` |
| Préparation des assets graphiques | `tools/build_assets.mjs` | ✅ 505 fichiers Nitro/NARC catalogués, copiés et vérifiés dans `psp-assets/` |
| Audio (canal PCM sceAudio + thread de mixage) | `src/platform/psp/pal_audio_psp.c` | ⚠️ Plomberie OK, décodage SDAT **pas fait** |
| Build PSPSDK | `src/platform/psp/Makefile` | ✅ Écrit, **non testé** (pas de toolchain PSP dans cet environnement) |
| Moteur de jeu générique (state machine, boucle) | — | ❌ N'existe encore nulle part, ni SDL ni PSP |

### Détail : le décodeur de tilemaps (nouveau)

`src/formats/nds_gfx.c` décode un triplet NSCR (plan de tuiles) + NCGR
(tuiles) + NCLR (palette) en une image RGBA8888. Tous les offsets de
format viennent d'une lecture directe de `tools/nitrogfx/gfx.c` dans ce
même dépôt (l'outil que le projet utilise pour reconstruire la ROM),
pas d'une supposition. Testé avec succès contre
`res/prebuilt/data/ground0.{NSCR,NCGR,NCLR}` — un vrai tileset de
terrain (herbe + étangs) du jeu, décodé correctement en 256x256 px.

Ce que ce décodeur ne fait PAS encore :
- Les sprites **affines** (rotation/zoom), les objets désactivés et le
  mapping OBJ 2D : `nds_sprite.c` les refuse explicitement au lieu de
  produire une image silencieusement incorrecte. Les cellules NCER normales
  avec mapping 1D sont désormais décodées.
- Les tuiles en **mode "affine"** (rotation/zoom), utilisées pour
  certains effets de caméra — Platine s'en sert peu mais pas jamais.
- L'intégration dans `PAL_Graphics_DrawTilemap` reste bridée par la
  signature d'origine (un seul paramètre) — j'ai ajouté
  `PAL_Graphics_DrawTilemapFull()` dans `pal_graphics_psp_ext.h` qui
  prend les trois blobs nécessaires ; c'est cette dernière qu'il faut
  appeler depuis le futur code de rendu de carte.

### Validation ajoutée pour les sprites NCER

Le test visuel `tools/render_ncer_test.py` reproduit le chemin de rendu
normal 1D de `nds_sprite.c` sur un poste de développement et écrit une PNG.
Il a été exécuté avec le triplet réel
`res/prebuilt/data/smptm_koori.{NCER,NCGR,NCLR}` : les trois cellules ont
été assemblées correctement. Cet asset utilise `mappingType = 1` (frontière
de 64 octets), ce qui vérifie que le décodeur ne dépend pas uniquement du
cas 32 octets.

### Pipeline d'assets disponible

Depuis ce dossier de portage :

```powershell
node tools/build_assets.mjs --source pokeplatinum-main/res --output psp-assets
```

Le script conserve l'arborescence, ne lit pas la ROM, contrôle la signature
Nitro de chaque fichier et écrit `psp-assets/assets.json` (type, taille,
SHA-256 et nom de base). Les archives NARC sont volontairement conservées
brutes : leur extraction sélective devra être reliée au chargeur d'assets du
moteur PSP, plutôt que de décompresser inutilement tout le jeu en mémoire.

**Sois clair avec toi-même sur ce dernier point** : même le fork SDL3, plus avancé que celui-ci sur PC, n'a pas encore ce moteur générique — c'est la "Phase 2" de son propre plan. Le vrai travail commun aux deux plateformes (SDL et PSP) n'a pas commencé.

## Pourquoi je n'ai pas pu te donner un jeu qui tourne

Trois raisons concrètes, pas des excuses :

1. **Pas de toolchain PSP ici.** Ce conteneur n'a accès qu'à un nombre limité de domaines réseau (github, pypi, npm...), pas au dépôt du PSPSDK ni aux outils de cross-compilation MIPS. Le Makefile ci-dessus est écrit correctement mais je ne peux pas le compiler pour vérifier.
2. **Pas de ROM.** Le rendu réel (sprites, tuiles, texte) a besoin des assets extraits de TA copie de Pokémon Platine. Sans ROM dumpée légalement, il n'y a rien à afficher — c'est vrai pour n'importe qui qui reprendrait ce projet, moi y compris.
3. **Le moteur de jeu n'existe pas encore.** Le plan officiel du fork estime 3 mois pour une version jouable sur PC, qui est une cible bien plus simple que la PSP (pas de contrainte mémoire/GPU serrée, pas de double écran à réconcilier).

## Défis spécifiques à la PSP (en plus de ceux du portage PC)

- **Double écran DS -> écran unique PSP.** J'ai fait un choix dans `pal_graphics_psp.c` (écran principal plein écran + overlay pour l'écran du bas), mais Pokétch et les menus tactiles demandent une vraie réflexion de design, pas juste un redimensionnement.
- **Pas d'écran tactile.** Tout ce qui utilise le stylet (options du Pokétch, certains menus) doit être repensé en navigation au bouton.
- **Audio SDAT.** Les musiques DS sont dans un format séquencé propriétaire ; il faut soit les convertir en PCM à l'avance (rapide, mais gros fichiers), soit écrire un petit séquenceur (plus de travail, fichiers plus légers).
- **Puissance de calcul.** La PSP est plus rapide que la DS, donc le CPU ne sera probablement pas le goulot d'étranglement — mais la VRAM PSP (2 Mo) est plus contrainte que ce à quoi on pourrait s'attendre pour du texture streaming agressif.

## Prochaines étapes concrètes, dans l'ordre

1. **Installer le PSPSDK sur ta machine** et vérifier que le Makefile fourni compile (corrige les erreurs de link, les noms de libs peuvent varier selon la version du SDK).
2. **Tester sur PPSSPP** (émulateur PSP) avant toute vraie console — cycle de test beaucoup plus rapide.
3. **Écrire le décodage NSCR/NCGR/NARC réel** dans `pal_graphics_psp.c` (actuellement stubbé) — le format est déjà documenté par le travail de décompilation `pret/pokeplatinum`, c'est un travail de traduction, pas de recherche.
4. **Convertir l'audio** SDAT -> PCM avec un outil externe comme étape de build.
5. **Écrire le moteur générique** (state machine, boucle de jeu) — idéalement en collaboration avec le fork SDL3 existant pour que le travail profite aux deux portages.

## Une suggestion honnête

Ce genre de projet avance mieux en tâches découpées et testées une par une (ex: "faire s'afficher un sprite Pikachu statique", puis "faire clignoter le curseur du menu", etc.) plutôt qu'en un bloc. Si tu veux qu'on continue, dis-moi sur quelle brique précise tu veux qu'on avance ensuite, et on l'attaque en vrai, avec du code testé au maximum de ce que permet cet environnement.
