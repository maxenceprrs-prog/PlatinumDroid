#!/usr/bin/env bash
# ===========================================================================
# build_apk.sh — Packaging APK Pokémon Platine autonome avec cœur melonDS NDK
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
MELONDS_LIBS="/opt/melonds_core/libs/arm64-v8a"

# ---------------------------------------------------------------------------
echo "==> 1. Nettoyage et préparation des dossiers de build..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR/gen" "$BUILD_DIR/obj" \
         "$BUILD_DIR/apk/lib/arm64-v8a" \
         "$BUILD_DIR/apk/assets"

# ---------------------------------------------------------------------------
echo "==> 2. Intégration du cœur natif melonDS (libretro.so ARM64)..."
if [ ! -f "$MELONDS_LIBS/libretro.so" ]; then
    echo "ERREUR: $MELONDS_LIBS/libretro.so introuvable !"
    exit 1
fi
cp "$MELONDS_LIBS/libretro.so" "$BUILD_DIR/apk/lib/arm64-v8a/libretro.so"
echo "    libretro.so copié dans l'APK !"

# ---------------------------------------------------------------------------
echo "==> 3. Compilation du pont JNI melonDS (libpokeplatinum.so ARM64)..."
"$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android34-clang" \
    -O3 -DNDEBUG -march=armv8-a -fomit-frame-pointer -fPIC -shared \
    -I"$CPP/include" \
    -I/opt/melonds_core/src/libretro/libretro-common/include \
    "$CPP/melonds_bridge.c" \
    -L"$MELONDS_LIBS" -lretro \
    -ljnigraphics -llog -landroid -lm \
    -o "$BUILD_DIR/apk/lib/arm64-v8a/libpokeplatinum.so"
echo "    libpokeplatinum.so compilé avec succès !"

# ---------------------------------------------------------------------------
echo "==> 4. Génération de R.java via aapt..."
$BUILD_TOOLS/aapt package -m \
    --min-sdk-version 24 \
    --target-sdk-version 34 \
    -J "$BUILD_DIR/gen" \
    -M "$APP_DIR/AndroidManifest.xml" \
    -S "$APP_DIR/res" \
    -I "$PLATFORM"

# ---------------------------------------------------------------------------
echo "==> 5. Compilation Java (MainActivity.java)..."
javac -d "$BUILD_DIR/obj" \
    -classpath "$PLATFORM" \
    -source 1.8 -target 1.8 \
    "$BUILD_DIR/gen/com/game/pokeplatinum/R.java" \
    "$APP_DIR/java/com/game/pokeplatinum/MainActivity.java"
echo "    Classes Java compilées avec succès !"

# ---------------------------------------------------------------------------
echo "==> 6. Création de classes.dex (d8)..."
$BUILD_TOOLS/d8 "$BUILD_DIR/obj/com/game/pokeplatinum/"*.class \
    --lib "$PLATFORM" \
    --output "$BUILD_DIR/apk"
echo "    classes.dex généré avec succès !"

# ---------------------------------------------------------------------------
echo "==> 7. Copie des assets..."
if [ -d "$APP_DIR/assets" ]; then
    cp -r "$APP_DIR/assets/"* "$BUILD_DIR/apk/assets/" 2>/dev/null || true
fi

# ---------------------------------------------------------------------------
echo "==> 8. Packaging APK initial (aapt)..."
$BUILD_TOOLS/aapt package -f \
    --min-sdk-version 24 \
    --target-sdk-version 34 \
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
echo " [SUCCES] APK Android avec vrai melonDS généré avec succès !"
echo " Emplacement : $PROJECT_DIR/PokemonPlatinum_Release.apk"
echo "=========================================================="
ls -lh "$PROJECT_DIR/PokemonPlatinum_Release.apk"
