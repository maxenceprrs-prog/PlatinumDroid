#!/usr/bin/env bash
# ===========================================================================
# build_apk.sh — Génération de l'APK PokemonPlatinum pour Android
# ===========================================================================
set -e

ANDROID_SDK=/opt/android-sdk
NDK=$ANDROID_SDK/ndk/26.1.10909125
BUILD_TOOLS=$ANDROID_SDK/build-tools/34.0.0
PLATFORM=$ANDROID_SDK/platforms/android-34/android.jar

PROJECT_DIR="/mnt/c/Users/philippe/Music/Pokemon psp/mobile-port/android"
APP_DIR="$PROJECT_DIR/app/src/main"
CPP="$APP_DIR/cpp"
BUILD_DIR="$PROJECT_DIR/build"

# ---------------------------------------------------------------------------
echo "==> 1. Nettoyage et préparation des dossiers de build..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR/gen" "$BUILD_DIR/obj" \
         "$BUILD_DIR/apk/lib/arm64-v8a" \
         "$BUILD_DIR/apk/lib/x86_64" \
         "$BUILD_DIR/apk/assets"

# ---------------------------------------------------------------------------
echo "==> 2. Compilation des bibliothèques C natives (ARM64-v8a)..."
"$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android34-clang" \
    -O2 -fPIC -shared \
    -I"$CPP/include" \
    -I"$CPP" \
    "$CPP/jni_bridge.c" \
    "$CPP/scene_title.c" \
    "$CPP/nds_gfx.c" \
    "$CPP/nds_sprite.c" \
    "$CPP/psp_asset_loader.c" \
    "$CPP/psp_asset_renderer.c" \
    "$CPP/pal_graphics_android.c" \
    "$CPP/audio_assets.c" \
    "$CPP/audio_sdat.c" \
    "$CPP/pal_audio_android.c" \
    -llog -landroid -lm \
    -o "$BUILD_DIR/apk/lib/arm64-v8a/libpokeplatinum.so"
echo "    libpokeplatinum.so (ARM64) OK !"

# ---------------------------------------------------------------------------
echo "==> 3. Compilation des bibliothèques C natives (x86_64)..."
"$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/x86_64-linux-android34-clang" \
    -O2 -fPIC -shared \
    -I"$CPP/include" \
    -I"$CPP" \
    "$CPP/jni_bridge.c" \
    "$CPP/scene_title.c" \
    "$CPP/nds_gfx.c" \
    "$CPP/nds_sprite.c" \
    "$CPP/psp_asset_loader.c" \
    "$CPP/psp_asset_renderer.c" \
    "$CPP/pal_graphics_android.c" \
    "$CPP/audio_assets.c" \
    "$CPP/audio_sdat.c" \
    "$CPP/pal_audio_android.c" \
    -llog -landroid -lm \
    -o "$BUILD_DIR/apk/lib/x86_64/libpokeplatinum.so"
echo "    libpokeplatinum.so (x86_64) OK !"

# ---------------------------------------------------------------------------
echo "==> 4. Génération de R.java via aapt..."
$BUILD_TOOLS/aapt package -m \
    -J "$BUILD_DIR/gen" \
    -M "$APP_DIR/AndroidManifest.xml" \
    -S "$APP_DIR/res" \
    -I "$PLATFORM"

# ---------------------------------------------------------------------------
echo "==> 5. Compilation Java..."
javac -d "$BUILD_DIR/obj" \
    -classpath "$PLATFORM" \
    -source 1.8 -target 1.8 \
    "$BUILD_DIR/gen/com/game/pokeplatinum/R.java" \
    "$APP_DIR/java/com/game/pokeplatinum/MainActivity.java"

# ---------------------------------------------------------------------------
echo "==> 6. Création de classes.dex (d8)..."
$BUILD_TOOLS/d8 "$BUILD_DIR/obj/com/game/pokeplatinum/"*.class \
    --lib "$PLATFORM" \
    --output "$BUILD_DIR/apk"

# ---------------------------------------------------------------------------
echo "==> 7. Copie des assets..."
cp -r "$APP_DIR/assets/"* "$BUILD_DIR/apk/assets/"

# ---------------------------------------------------------------------------
echo "==> 8. Packaging APK initial (aapt)..."
$BUILD_TOOLS/aapt package -f \
    -M "$APP_DIR/AndroidManifest.xml" \
    -S "$APP_DIR/res" \
    -I "$PLATFORM" \
    -F "$BUILD_DIR/unaligned.apk" \
    "$BUILD_DIR/apk"

# ---------------------------------------------------------------------------
echo "==> 9. Alignement de l'APK (zipalign)..."
$BUILD_TOOLS/zipalign -f -p 4 \
    "$BUILD_DIR/unaligned.apk" \
    "$BUILD_DIR/PokemonPlatinum.apk"

# ---------------------------------------------------------------------------
echo "==> 10. Signature debug de l'APK (apksigner)..."
if [ ! -f "$PROJECT_DIR/debug.keystore" ]; then
    keytool -genkey -v \
        -keystore "$PROJECT_DIR/debug.keystore" \
        -alias androiddebugkey \
        -storepass android -keypass android \
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
echo " [SUCCES] APK Android genere avec succes !"
echo " Emplacement : $PROJECT_DIR/PokemonPlatinum_Release.apk"
echo "=========================================================="
ls -lh "$PROJECT_DIR/PokemonPlatinum_Release.apk"
