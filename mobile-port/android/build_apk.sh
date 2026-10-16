#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR"
APP_DIR="$PROJECT_DIR/app/src/main"
BUILD_DIR="$PROJECT_DIR/build"

ANDROID_SDK="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-/opt/android-sdk}}"
ANDROID_API="${ANDROID_API:-34}"
BUILD_TOOLS_VERSION="${BUILD_TOOLS_VERSION:-34.0.0}"
DEFAULT_NDK_VERSION="${ANDROID_NDK_VERSION:-26.1.10909125}"

if [[ -n "${ANDROID_NDK_ROOT:-}" ]]; then
  NDK="$ANDROID_NDK_ROOT"
else
  NDK="$ANDROID_SDK/ndk/$DEFAULT_NDK_VERSION"
fi

BUILD_TOOLS="$ANDROID_SDK/build-tools/$BUILD_TOOLS_VERSION"
PLATFORM="$ANDROID_SDK/platforms/android-$ANDROID_API/android.jar"

if [[ ! -d "$ANDROID_SDK" ]]; then
  echo "[ERREUR] Android SDK introuvable: $ANDROID_SDK"
  echo "Définis ANDROID_SDK_ROOT (ou ANDROID_HOME)."
  exit 1
fi
if [[ ! -d "$NDK" ]]; then
  echo "[ERREUR] Android NDK introuvable: $NDK"
  echo "Définis ANDROID_NDK_ROOT ou ANDROID_NDK_VERSION."
  exit 1
fi
if [[ ! -d "$BUILD_TOOLS" ]]; then
  echo "[ERREUR] Build-tools introuvables: $BUILD_TOOLS"
  echo "Définis BUILD_TOOLS_VERSION."
  exit 1
fi
if [[ ! -f "$PLATFORM" ]]; then
  echo "[ERREUR] Plateforme Android introuvable: $PLATFORM"
  echo "Définis ANDROID_API."
  exit 1
fi

if [[ ! -f "$APP_DIR/AndroidManifest.xml" ]]; then
  echo "[ERREUR] AndroidManifest introuvable dans $APP_DIR"
  exit 1
fi

for tool in javac keytool; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "[ERREUR] Outil manquant: $tool"
    exit 1
  fi
done

for tool in aapt d8 zipalign apksigner; do
  if [[ ! -x "$BUILD_TOOLS/$tool" ]]; then
    echo "[ERREUR] Outil build-tools manquant: $BUILD_TOOLS/$tool"
    exit 1
  fi
done

echo "==> 1. Nettoyage et préparation des dossiers de build..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR/gen" "$BUILD_DIR/obj" "$BUILD_DIR/apk/lib/arm64-v8a" "$BUILD_DIR/apk/lib/x86_64" "$BUILD_DIR/apk/assets"

echo "==> 2. Compilation des bibliothèques C natives..."
TARGET_ARM64="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android${ANDROID_API}-clang"
TARGET_X86_64="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/x86_64-linux-android${ANDROID_API}-clang"

if [[ ! -x "$TARGET_ARM64" || ! -x "$TARGET_X86_64" ]]; then
  echo "[ERREUR] Compilateurs NDK introuvables."
  exit 1
fi

NATIVE_SOURCES=(
  "$APP_DIR/cpp/jni_bridge.c"
  "$APP_DIR/cpp/scene_title.c"
  "$APP_DIR/cpp/nds_gfx.c"
  "$APP_DIR/cpp/nds_sprite.c"
  "$APP_DIR/cpp/psp_asset_loader.c"
  "$APP_DIR/cpp/psp_asset_renderer.c"
  "$APP_DIR/cpp/pal_graphics_android.c"
)

"$TARGET_ARM64" -O3 -fPIC -shared \
  -I"$APP_DIR/cpp/include" \
  -I"$APP_DIR/cpp" \
  "${NATIVE_SOURCES[@]}" \
  -llog -landroid -lm \
  -o "$BUILD_DIR/apk/lib/arm64-v8a/libpokeplatinum.so"

echo "    libpokeplatinum.so (arm64-v8a) OK !"

"$TARGET_X86_64" -O3 -fPIC -shared \
  -I"$APP_DIR/cpp/include" \
  -I"$APP_DIR/cpp" \
  "${NATIVE_SOURCES[@]}" \
  -llog -landroid -lm \
  -o "$BUILD_DIR/apk/lib/x86_64/libpokeplatinum.so"

echo "    libpokeplatinum.so (x86_64) OK !"

echo "==> 4. Génération de R.java via aapt..."
$BUILD_TOOLS/aapt package -m \
    -J "$BUILD_DIR/gen" \
    -M "$APP_DIR/AndroidManifest.xml" \
    -S "$APP_DIR/res" \
    -I "$PLATFORM"

echo "==> 5. Compilation Java..."
javac -d "$BUILD_DIR/obj" \
    -classpath "$PLATFORM" \
    -source 1.8 -target 1.8 \
    "$BUILD_DIR/gen/com/game/pokeplatinum/R.java" \
    "$APP_DIR/java/com/game/pokeplatinum/MainActivity.java"

echo "==> 6. Création de classes.dex (d8)..."
$BUILD_TOOLS/d8 "$BUILD_DIR/obj/com/game/pokeplatinum"/*.class \
    --lib "$PLATFORM" \
    --output "$BUILD_DIR/apk"

echo "==> 7. Copie des assets..."
if [[ -d "$APP_DIR/assets" ]]; then
  cp -r "$APP_DIR/assets/." "$BUILD_DIR/apk/assets/"
else
  echo "[WARN] Dossier assets absent: $APP_DIR/assets (APK créé sans assets supplémentaires)."
fi

echo "==> 8. Packaging APK initial (aapt)..."
$BUILD_TOOLS/aapt package -f \
    -M "$APP_DIR/AndroidManifest.xml" \
    -S "$APP_DIR/res" \
    -I "$PLATFORM" \
    -F "$BUILD_DIR/unaligned.apk" \
    "$BUILD_DIR/apk"

echo "==> 9. Alignement de l'APK (zipalign)..."
$BUILD_TOOLS/zipalign -f -p 4 "$BUILD_DIR/unaligned.apk" "$BUILD_DIR/PokemonPlatinum.apk"

echo "==> 10. Signature debug de l'APK (apksigner)..."
if [ ! -f "$PROJECT_DIR/debug.keystore" ]; then
    keytool -genkey -v -keystore "$PROJECT_DIR/debug.keystore" \
        -alias androiddebugkey -storepass android -keypass android \
        -keyalg RSA -keysize 2048 -validity 10000 \
        -dname "CN=Android Debug,O=Android,C=US"
fi

$BUILD_TOOLS/apksigner sign \
    --ks "$PROJECT_DIR/debug.keystore" \
    --ks-pass pass:android \
    --key-pass pass:android \
    --ks-key-alias androiddebugkey \
    --out "$PROJECT_DIR/PokemonPlatinum_Release.apk" \
    "$BUILD_DIR/PokemonPlatinum.apk"

echo ""
echo "=========================================================="
echo " [SUCCÈS] APK Android autonome généré avec succès !"
echo " Emplacement : $PROJECT_DIR/PokemonPlatinum_Release.apk"
echo "=========================================================="
ls -lh "$PROJECT_DIR/PokemonPlatinum_Release.apk"
