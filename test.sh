#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
CC="${CC:-cc}"
CFLAGS=(-std=c11 -Wall -Wextra -Wpedantic -O2)

BUILD_DIR="$ROOT/build"
TEST_BUILD_DIR="$BUILD_DIR/tests"

mkdir -p "$TEST_BUILD_DIR"

tests=(placed_db lef_parse tech_lef_parse site_parse)
benches=(bench_hpwl gen_bench_lef_def)

pick="${1:-}"
if [[ -z "$pick" ]]; then
  echo "Select target:"
  echo "  all"
  for t in "${tests[@]}"; do
    echo "  $t"
  done
  for b in "${benches[@]}"; do
    echo "  $b"
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

build_and_run_bench() {
  local name="$1"
  shift
  local bench_src="$ROOT/tests/${name}.c"
  local out="$TEST_BUILD_DIR/${name}"

  local srcs=()
  for f in "$ROOT/src/"*.c; do
    [[ "$f" == "$ROOT/src/main.c" ]] && continue
    srcs+=("$f")
  done

  echo "+ $CC ${CFLAGS[*]} $bench_src $ROOT/tests/bench_case.c ${srcs[*]} -I$ROOT/src -I$ROOT/tests -o $out"
  $CC "${CFLAGS[@]}" \
    "$bench_src" \
    "$ROOT/tests/bench_case.c" \
    "${srcs[@]}" \
    -I"$ROOT/src" -I"$ROOT/tests" \
    -o "$out"

  echo "+ $out $*"
  "$out" "$@"
}

case "$pick" in
  all)
    for t in "${tests[@]}"; do
      build_and_run "$t"
    done
    ;;
  placed_db|lef_parse|tech_lef_parse|site_parse)
    build_and_run "$pick"
    ;;
  bench_hpwl)
    build_and_run_bench bench_hpwl "${@:2}"
    ;;
  gen_bench_lef_def)
    build_and_run_bench gen_bench_lef_def "${@:2}"
    ;;
  *)
    echo "unknown test: $pick" 1>&2
    exit 2
    ;;
esac


