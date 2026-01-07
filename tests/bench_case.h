#ifndef CHIPSDB_BENCH_CASE_H
#define CHIPSDB_BENCH_CASE_H

#include <stdint.h>

#include "arena.h"
#include "placed_db.h"

typedef struct BenchCaseParams {
	uint32_t seed;
	uint32_t inst_count;       /* N */
	uint32_t nets_count;       /* M */
	uint32_t pins_per_inst;    /* P */
	uint32_t pins_per_net;     /* K */
	int32_t coord_span;        /* coordinate range in dbu */
} BenchCaseParams;

typedef struct BenchRng {
	uint32_t state;
} BenchRng;

void bench_rng_seed(BenchRng *rng, uint32_t seed);
uint32_t bench_rng_u32(BenchRng *rng);
uint32_t bench_rng_range(BenchRng *rng, uint32_t lo, uint32_t hi);

DbResult bench_case_build_placed_db(
	Arena *arena,
	PlacedDb *db,
	const BenchCaseParams *p);

#endif
