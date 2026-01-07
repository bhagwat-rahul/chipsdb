#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
CC="${CC:-cc}"
CFLAGS=(-std=c11 -Wall -Wextra -Wpedantic -O2)

BUILD_DIR="$ROOT/build"
TEST_BUILD_DIR="$BUILD_DIR/tests"

mkdir -p "$TEST_BUILD_DIR"

tests=(placed_db lef_parse tech_lef_parse)

pick="${1:-}"
if [[ -z "$pick" ]]; then
  echo "Select test:"
  echo "  all"
  for t in "${tests[@]}"; do
    echo "  $t"
  done
  read -r -p "> " pick
fi

build_and_run() {
  local name="$1"
  local test_src="$ROOT/tests/test_${name}.c"
  local out="$TEST_BUILD_DIR/test_${name}"

  local srcs=()
  for f in "$ROOT/src/"*.c; do
    [[ "$f" == "$ROOT/src/main.c" ]] && continue
    srcs+=("$f")
  done

  echo "+ $CC ${CFLAGS[*]} $test_src ${srcs[*]} -I$ROOT/src -o $out"
  $CC "${CFLAGS[@]}" "$test_src" "${srcs[@]}" -I"$ROOT/src" -o "$out"

  echo "+ $out"
  "$out"
}

case "$pick" in
  all)
    for t in "${tests[@]}"; do
      build_and_run "$t"
    done
    ;;
  placed_db|lef_parse|tech_lef_parse)
    build_and_run "$pick"
    ;;
  *)
    echo "unknown test: $pick" 1>&2
    exit 2
    ;;
esac


