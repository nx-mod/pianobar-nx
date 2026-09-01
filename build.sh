#!/bin/bash
# Builds pianobar-nx (.nro) inside the devkitPro MSYS2 shell.
#   bash build.sh [target] [jobs]
#     target  all (default) | dist | clean

source /etc/profile.d/devkit-env.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR" || exit 1

# Point at the flat-cloned libnx fork, not devkitPro's stock system libnx --
# matches every other project's build.sh in this repo.
export LIBNX="$SCRIPT_DIR/../libnx/nx"

TARGET="${1:-all}"
JOBS="${2:-2}"

echo "DEVKITPRO=$DEVKITPRO"
echo "Target=$TARGET Jobs=$JOBS CWD=$(pwd)"

if [ "$TARGET" = "clean" ]; then
    make clean
    exit $?
fi

make -j"$JOBS" "$TARGET"
STATUS=$?

if [ $STATUS -eq 0 ] && [ "$TARGET" = "dist" ] && [ -f pianobar-nx.zip ]; then
    echo "== Build ok. Output in out/ =="
    find out -type f 2>/dev/null

    ZIPS_DIR="$SCRIPT_DIR/../_ZIPS_"
    SDCARD_DIR="$SCRIPT_DIR/../_SDCARD_"
    mkdir -p "$ZIPS_DIR"
    cp pianobar-nx.zip "$ZIPS_DIR/pianobar-nx-release.zip"
    mkdir -p "$SDCARD_DIR"
    cp -r out/* "$SDCARD_DIR/"
    echo "== Packaged: $ZIPS_DIR/pianobar-nx-release.zip =="
    echo "== Extracted onto: $SDCARD_DIR =="
elif [ $STATUS -eq 0 ]; then
    echo "== Build ok. Output: $(basename "$SCRIPT_DIR").nro =="
fi

exit $STATUS
