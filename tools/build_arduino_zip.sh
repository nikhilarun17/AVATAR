#!/usr/bin/env bash
# Build an Arduino-IDE-importable .zip from the self-contained library folder.
#
# Arduino IDE's "Sketch > Include Library > Add .ZIP Library" imports whichever
# top-level folder is inside the zip. We zip ONLY arduino/<LIB_NAME>, so the
# Python side (a sibling folder) is never included -- no ignore file needed.
#
# Usage:  ./tools/build_arduino_zip.sh
# Output: dist/<LIB_NAME>.zip

set -euo pipefail

LIB_NAME="AVATAR" 

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/arduino/$LIB_NAME"
OUT_DIR="$ROOT/dist"
OUT_ZIP="$OUT_DIR/$LIB_NAME.zip"

if [ ! -d "$SRC" ]; then
    echo "error: $SRC not found (is LIB_NAME correct?)" >&2
    exit 1
fi

mkdir -p "$OUT_DIR"
rm -f "$OUT_ZIP"

# Zip from arduino/ so the archive's top-level folder is exactly <LIB_NAME>.
( cd "$ROOT/arduino" && zip -r -q "$OUT_ZIP" "$LIB_NAME" -x '*/.DS_Store' )

echo "built: $OUT_ZIP"
echo "import it via Arduino IDE > Sketch > Include Library > Add .ZIP Library"
