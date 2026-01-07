#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

CC="${CC:-cc}"
CFLAGS=(
  -std=c11
  -Wall
  -Wextra
  -Wpedantic
  -O2
)

BUILD_DIR="$ROOT/build"
OUT="$BUILD_DIR/chipsdb"

mkdir -p "$BUILD_DIR"

echo "+ $CC ${CFLAGS[*]} $ROOT/src/*.c -o $OUT"
$CC "${CFLAGS[@]}" "$ROOT/src/"*.c -o "$OUT"

echo "+ $OUT $*"
exec "$OUT" "$@"
