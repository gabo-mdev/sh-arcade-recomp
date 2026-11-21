#!/usr/bin/env bash
# Pack a Linux x64 release tarball (no ROMs).
set -euo pipefail

VERSION="${1:-0.1.0}"
EXE="${2:-build/splatterhouse}"
OUT_DIR="${3:-dist}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if [[ ! -f "$EXE" ]]; then
  echo "Missing $EXE — build first." >&2
  exit 1
fi

STAGE_NAME="sh-arcade-recomp-v${VERSION}-linux-x64"
STAGE="${OUT_DIR}/${STAGE_NAME}"
TAR="${OUT_DIR}/${STAGE_NAME}.tar.gz"

rm -rf "$STAGE"
mkdir -p "$STAGE"
cp "$EXE" "$STAGE/splatterhouse"
chmod +x "$STAGE/splatterhouse"
cp README.md LICENSE "$STAGE/"

mkdir -p "$OUT_DIR"
rm -f "$TAR"
tar -C "$OUT_DIR" -czf "$TAR" "$STAGE_NAME"

echo "OK -> $TAR"
echo "Contains: splatterhouse, README.md, LICENSE"
