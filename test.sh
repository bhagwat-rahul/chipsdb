#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
CC="${CC:-cc}"
CFLAGS=(-std=c11 -Wall -Wextra -Wpedantic -O2)

BUILD_DIR="$ROOT/build"
TEST_BUILD_DIR="$BUILD_DIR/tests"

mkdir -p "$TEST_BUILD_DIR"

tests=(placed_db lef_parse tech_lef_parse site_parse)
benches=(bench_hpwl gen_bench_lef_def bench_compare_hpwl)

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

bench_compare_hpwl() {
  local N="${N:-20000}"
  local M="${M:-20000}"
  local K="${K:-4}"
  local P="${P:-4}"
  local SEED="${SEED:-1}"
  local SPAN="${SPAN:-100000}"

  echo "bench_compare_hpwl: N=$N M=$M K=$K P=$P SEED=$SEED SPAN=$SPAN"

  N="$N" M="$M" K="$K" P="$P" SEED="$SEED" SPAN="$SPAN" \
    build_and_run_bench gen_bench_lef_def

  local chips_full
  local chips_step
  chips_full="$(build_and_run_bench bench_hpwl full --inst "$N" --nets "$M" --k "$K" --p "$P" --iters 10 --seed "$SEED" | tail -n 1)"
  chips_step="$(build_and_run_bench bench_hpwl step --inst "$N" --nets "$M" --k "$K" --p "$P" --steps 1000 --moves 16 --seed "$SEED" | tail -n 1)"

  local chips_ns_per_net
  local chips_nets_per_s
  local chips_ns_per_step
  local chips_steps_per_s
  chips_ns_per_net="$(echo "$chips_full" | sed -n 's/.*ns_per_net=\([^ ]*\).*/\1/p')"
  chips_nets_per_s="$(echo "$chips_full" | sed -n 's/.*nets_per_s=\([^ ]*\).*/\1/p')"
  chips_ns_per_step="$(echo "$chips_step" | sed -n 's/.*ns_per_step=\([^ ]*\).*/\1/p')"
  chips_steps_per_s="$(echo "$chips_step" | sed -n 's/.*steps_per_s=\([^ ]*\).*/\1/p')"

  local have_openroad="0"
  if command -v openroad >/dev/null 2>&1; then
    have_openroad="1"
  fi

  local iters="100"
  if [[ "$M" -ge 10000 ]]; then
    iters="10"
  fi

  local or_line
  if [[ "$have_openroad" == "1" ]]; then
    or_line="$(ITERS="$iters" \
      TECH_LEF="$ROOT/data/openroad_tech.lef" \
      LEF="$ROOT/build/bench/bench.lef" \
      DEF="$ROOT/build/bench/bench.def" \
      openroad -no_init -exit "$ROOT/tests/openroad_hpwl.tcl" 2>/dev/null | tail -n 1)"
  else
    or_line=""
  fi

  local or_ns_per_iter
  local or_nets
  or_ns_per_iter="$(echo "$or_line" | sed -n 's/.*ns_per_iter=\([^ ]*\).*/\1/p')"
  or_nets="$(echo "$or_line" | sed -n 's/.*nets=\([0-9][0-9]*\).*/\1/p')"

  echo ""
  echo "| tool | mode | N(inst) | M(nets) | K(pins/net) | P(pins/inst) | ns_per_net | nets_per_s | ns_per_step | steps_per_s | notes |"
  echo "|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---|"
  echo "| chipsdb | full_hpwl | $N | $M | $K | $P | $chips_ns_per_net | $chips_nets_per_s |  |  | in-memory |"
  echo "| chipsdb | step_hpwl | $N | $M | $K | $P |  |  | $chips_ns_per_step | $chips_steps_per_s | moves=16, steps=1000 |"

  if [[ "$have_openroad" != "1" ]]; then
    echo "| openroad | full_hpwl | $N | $M | $K | $P |  |  |  |  | openroad_not_found |"
    return 2
  fi

  if [[ -z "$or_ns_per_iter" || -z "$or_nets" ]]; then
    echo "| openroad | full_hpwl | $N | $M | $K | $P |  |  |  |  | openroad_parse_failed |"
    echo ""
    echo "openroad_raw: $or_line"
    return 2
  fi

  awk -v N="$N" -v M="$M" -v K="$K" -v P="$P" \
      -v c_np="$chips_ns_per_net" -v c_nps="$chips_nets_per_s" \
      -v o_nsi="$or_ns_per_iter" -v o_nets="$or_nets" -v iters="$iters" \
      'BEGIN {
        o_np = o_nsi / o_nets;
        o_nps = 1e9 / o_np;
        speedup = o_np / c_np;
        printf("| openroad | full_hpwl | %u | %u | %u | %u | %.3f | %.3f |  |  | iters=%s,nets=%u (odb_tcl) |\n",
               N, M, K, P, o_np, o_nps, iters, o_nets);
        printf("| compare | full_hpwl | %u | %u | %u | %u |  |  |  |  | speedup(openroad/chipsdb)=%.2fx |\n",
               N, M, K, P, speedup);
      }'
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
  bench_compare_hpwl)
    bench_compare_hpwl "${@:2}"
    ;;
  *)
    echo "unknown test: $pick" 1>&2
    exit 2
    ;;
esac


