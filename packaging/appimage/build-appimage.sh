#!/usr/bin/env bash
# Builds dist/SuperMarioWar-8P-x86_64.AppImage from the "release" CMake preset.
# Run from anywhere inside the repo (inside your dev distrobox).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HERE="$ROOT/packaging/appimage"
BUILD="$ROOT/build/release"
APPDIR="$ROOT/build/AppDir"
GAMEDIR="$APPDIR/usr/lib/smw"
TOOLS="$ROOT/build/tools"
OUT="$ROOT/dist"
APPIMAGETOOL="$TOOLS/appimagetool-x86_64.AppImage"

cd "$ROOT"

for tool in cmake ninja ldd strip file; do
    if ! command -v "$tool" >/dev/null; then
        echo "Missing '$tool' (appimagetool also needs 'file') - install it in the build box" >&2
        exit 1
    fi
done

if [ -z "$(ls -A data 2>/dev/null)" ]; then
    echo "data/ is empty - run: git submodule update --init" >&2
    exit 1
fi

# 1. Build (all targets: cmake --install expects the editors to exist too)
cmake --preset release
cmake --build --preset release

# 2. Stage a portable install (smw binary + data/ side by side)
rm -rf "$APPDIR"
cmake --install "$BUILD" --prefix "$GAMEDIR" --strip
# Editors/server/headers/static libs aren't wanted in the game AppImage
rm -rf "$GAMEDIR/include" "$GAMEDIR/lib" "$GAMEDIR/share" \
       "$GAMEDIR/smw-leveledit" "$GAMEDIR/smw-worldedit" "$GAMEDIR/smw-server"

# 3. Bundle the SDL3 family next to the binary (rpath is $ORIGIN).
# The FetchContent'd SDL libs aren't part of `cmake --install`, so take them
# from the build tree: the un-installed binary's build rpath points into _deps/.
ldd "$BUILD/smw" | awk '/libSDL3/ && $3 ~ /^\// { print $3 }' | while read -r lib; do
    cp -L "$lib" "$GAMEDIR/$(basename "$lib")"
    strip --strip-unneeded "$GAMEDIR/$(basename "$lib")"
done
if ! ls "$GAMEDIR"/libSDL3.so.* >/dev/null 2>&1; then
    echo "No bundled libSDL3 next to smw - is the release build using system SDL?" >&2
    exit 1
fi
if ldd "$GAMEDIR/smw" | grep -q 'not found'; then
    echo "Unresolved libraries:" >&2
    ldd "$GAMEDIR/smw" | grep 'not found' >&2
    exit 1
fi

# 4. AppDir metadata
install -m755 "$HERE/AppRun" "$APPDIR/AppRun"
install -m644 "$HERE/supermariowar-8p.desktop" "$APPDIR/supermariowar-8p.desktop"
install -m644 "$ROOT/resources/icons/smw.png" "$APPDIR/smw.png"
ln -sf smw.png "$APPDIR/.DirIcon"

# 5. Pack
mkdir -p "$TOOLS" "$OUT"
if [ ! -x "$APPIMAGETOOL" ]; then
    URL=https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage
    # On hosts with AppArmor's curl profile (e.g. Tumbleweed), curl inside a
    # Fedora distrobox can't read /etc/pki/ca-trust and fails with error 77,
    # so fall back to wget.
    if ! curl -fL -o "$APPIMAGETOOL.part" "$URL"; then
        echo "curl failed, retrying with wget" >&2
        wget -O "$APPIMAGETOOL.part" "$URL"
    fi
    chmod +x "$APPIMAGETOOL.part"
    mv "$APPIMAGETOOL.part" "$APPIMAGETOOL"
fi
# Extract-and-run so this works without FUSE (e.g. inside distrobox/CI)
APPIMAGE_EXTRACT_AND_RUN=1 ARCH=x86_64 "$APPIMAGETOOL" --no-appstream \
    "$APPDIR" "$OUT/SuperMarioWar-8P-x86_64.AppImage"

echo
echo "Built: $OUT/SuperMarioWar-8P-x86_64.AppImage"
