#!/usr/bin/env bash
# Build a distributable BPS patch for the Endless Trophy Race hack.
#
# The patch contains only this hack's changes -- players apply it to their own
# copy of the retail ROM. The ROM itself is never redistributed.
set -euo pipefail

REGION="${REGION:-us}"
VERSION="${VERSION:-v77}"
BASEROM="baseroms/baserom.${REGION}.${VERSION}.z64"
BUILT="build/dkr.${REGION}.${VERSION}.z64"
OUT="dist/EndlessTrophyRace.bps"
FLIPS="tools/Flips/flips"

cd "$(dirname "$0")"

if [ ! -f "$BASEROM" ]; then
    echo "error: missing $BASEROM (supply your own retail ROM)" >&2
    exit 1
fi

if [ ! -f "$BUILT" ]; then
    echo "error: missing $BUILT -- run 'make NON_MATCHING=1' first" >&2
    exit 1
fi

if [ ! -x "$FLIPS" ]; then
    echo "Building Flips..."
    git submodule update --init tools/Flips
    (cd tools/Flips && TARGET=cli make)
fi

mkdir -p dist
"$FLIPS" --create --bps "$BASEROM" "$BUILT" "$OUT"

# A patch that does not reproduce the build exactly is worse than no patch.
"$FLIPS" --apply "$OUT" "$BASEROM" dist/.verify.z64 >/dev/null
if [ "$(sha1sum < "$BUILT")" != "$(sha1sum < dist/.verify.z64)" ]; then
    rm -f dist/.verify.z64
    echo "error: patch does not reproduce $BUILT" >&2
    exit 1
fi
rm -f dist/.verify.z64

echo "Wrote $OUT ($(stat -c%s "$OUT") bytes), verified against $BUILT"
