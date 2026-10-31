#!/usr/bin/env bash
# Checks a built AppImage: extracts it, runs ldd on the game binary, and prints
# the newest GLIBC_ / GLIBCXX_ / CXXABI_ symbol versions it needs across the
# binary and the bundled libs. Compare against the Deck: `ldd --version` there
# (or in the matching fedora:<N> distrobox).
#
# Usage: packaging/appimage/check-glibc.sh [path/to/App.AppImage]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
APPIMAGE="$(realpath "${1:-$ROOT/dist/SuperMarioWar-8P-x86_64.AppImage}")"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

cd "$WORK"
"$APPIMAGE" --appimage-extract >/dev/null
GAMEDIR="$WORK/squashfs-root/usr/lib/smw"

echo "== Bundled libs next to smw"
ls -1 "$GAMEDIR" | grep '\.so' || echo "(none)"

echo
echo "== ldd smw"
ldd "$GAMEDIR/smw"
if ldd "$GAMEDIR/smw" | grep -q 'not found'; then
    echo "FAIL: unresolved libraries" >&2
    exit 1
fi
if ldd "$GAMEDIR/smw" | awk '/libSDL3/ { print $3 }' | grep -qv "^$GAMEDIR/"; then
    echo "FAIL: SDL3 libs resolve outside the AppImage" >&2
    exit 1
fi

echo
echo "== Newest required symbol versions (binary + bundled libs)"
for prefix in GLIBC_ GLIBCXX_ CXXABI_; do
    newest="$(objdump -T "$GAMEDIR/smw" "$GAMEDIR"/*.so* 2>/dev/null \
        | grep -o "${prefix}[0-9][0-9.]*" | sort -uV | tail -1)"
    printf '%-9s %s\n' "$prefix" "${newest:-none}"
done
