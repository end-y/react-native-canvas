#!/usr/bin/env bash
#
# Build our own Skia static libs for react-native-canvas. Produces libskia.a
# for: iOS device arm64, iOS sim arm64, Android arm64-v8a, Android x86_64,
# then copies headers (matched to the libs) + libs into third_party/skia.
#
# Skia milestone: chrome/m148. GPU via Ganesh: GL on Android, Metal on Apple
# (Vulkan stays off).
# v1 launch additions: image DECODE codecs on (png/jpeg/webp; encode stays off
# until toDataURL), fonts on (FreeType on Android, CoreText on Apple).
# Still off: icu/harfbuzz/skparagraph/skshaper (v1 text = simple single-line
# shaping via SkFont; complex-script shaping is a v2 rebuild), svg, vulkan.
#
# One-time source setup:
#   git clone https://skia.googlesource.com/skia.git "$SKIA_SRC"
#   cd "$SKIA_SRC" && git checkout -b m148 origin/chrome/m148
#   python3 tools/git-sync-deps && python3 bin/fetch-gn && python3 bin/fetch-ninja
#
set -euo pipefail

SKIA_SRC="${SKIA_SRC:-$HOME/skia-build/skia}"
NDK="${ANDROID_NDK:-$HOME/Library/Android/sdk/ndk/27.1.12297006}"
DEST="$(cd "$(dirname "$0")/.." && pwd)/third_party/skia"

# Ganesh on; per-target backend (GL/Metal) added below. Vulkan off.
# Decode codecs ON (drawImage), encode OFF (no toDataURL yet). Bundled (not
# system) third-party libs — we cross-compile for ios/android.
COMMON='is_official_build=true skia_enable_ganesh=true skia_use_vulkan=false skia_use_icu=false skia_use_harfbuzz=false skia_enable_skparagraph=false skia_enable_skshaper=false skia_enable_svg=false skia_use_expat=false skia_use_libjpeg_turbo_decode=true skia_use_libjpeg_turbo_encode=false skia_use_libpng_decode=true skia_use_libpng_encode=false skia_use_libwebp_decode=true skia_use_libwebp_encode=false skia_use_system_libjpeg_turbo=false skia_use_system_libpng=false skia_use_system_libwebp=false skia_use_system_zlib=false'

cd "$SKIA_SRC"

build() { # <outdir> <extra gn args>
  echo ">>> building $1"
  bin/gn gen "out/$1" --args="$COMMON ${*:2}"
  third_party/ninja/ninja -C "out/$1" skia
}

build ios-sim-arm64 'target_os="ios" target_cpu="arm64" ios_use_simulator=true  ios_min_target="15.0" skia_use_metal=true'
build ios-arm64     'target_os="ios" target_cpu="arm64" ios_use_simulator=false ios_min_target="15.0" skia_ios_use_signing=false skia_use_metal=true'
# Android text needs FreeType (Apple uses the built-in CoreText fonthost).
build android-arm64 "target_os=\"android\" target_cpu=\"arm64\" ndk=\"$NDK\" ndk_api=24 skia_use_gl=true skia_use_freetype=true skia_use_system_freetype2=false"
build android-x64   "target_os=\"android\" target_cpu=\"x64\"   ndk=\"$NDK\" ndk_api=24 skia_use_gl=true skia_use_freetype=true skia_use_system_freetype2=false"

echo ">>> copying headers + libs into $DEST"
rm -rf "$DEST/include" "$DEST/modules"
cp -R include "$DEST/"
mkdir -p "$DEST/modules"; cp -R modules/skcms "$DEST/modules/"
cp LICENSE "$DEST/LICENSE_SKIA"
# Drop the large unused Vulkan headers (built with vulkan disabled).
rm -rf "$DEST/include/third_party/vulkan"

mkdir -p "$DEST/libs/apple/ios-arm64" "$DEST/libs/apple/ios-sim-arm64" \
         "$DEST/libs/android/arm64-v8a" "$DEST/libs/android/x86_64"
cp out/ios-arm64/libskia.a     "$DEST/libs/apple/ios-arm64/"
cp out/ios-sim-arm64/libskia.a "$DEST/libs/apple/ios-sim-arm64/"
cp out/android-arm64/libskia.a "$DEST/libs/android/arm64-v8a/"
cp out/android-x64/libskia.a   "$DEST/libs/android/x86_64/"

echo "Done. Skia headers + libs updated in $DEST"
