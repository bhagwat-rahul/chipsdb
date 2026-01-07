#include "bench_case.h"

#include <stdio.h>
#include <string.h>

static const char *
arena_strdup(Arena *a, const char *s, uint32_t len)
{
	char *p = (char *)arena_alloc(a, (size_t)len + 1, _Alignof(char));
	uint32_t i;
	if (!p) return 0;
	for (i = 0; i < len; i++) p[i] = s[i];
	p[len] = 0;
	return p;
}

static const char *
arena_u32_name(Arena *a, const char *prefix, uint32_t v)
{
	char tmp[32];
	int n = snprintf(tmp, sizeof(tmp), "%s%u", prefix, v);
	if (n <= 0) return 0;
	if ((size_t)n >= sizeof(tmp)) n = (int)(sizeof(tmp) - 1);
	return arena_strdup(a, tmp, (uint32_t)n);
}

void
bench_rng_seed(BenchRng *rng, uint32_t seed)
{
	if (seed == 0) seed = 1;
	rng->state = seed;
}

uint32_t
bench_rng_u32(BenchRng *rng)
{
	/* xorshift32 */
	uint32_t x = rng->state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	rng->state = x;
	return x;
}

uint32_t
bench_rng_range(BenchRng *rng, uint32_t lo, uint32_t hi)
{
	uint32_t span;
	uint32_t x;
	if (hi <= lo) return lo;
	span = hi - lo;
	x = bench_rng_u32(rng);
	return lo + (x % span);
}

static void
shuffle_u32(BenchRng *rng, uint32_t *a, uint32_t n)
{
	uint32_t i;
	if (n <= 1) return;
	for (i = n - 1; i > 0; i--) {
		uint32_t j = bench_rng_range(rng, 0, i + 1);
		uint32_t t = a[i];
		a[i] = a[j];
		a[j] = t;
	}
}

DbResult
bench_case_build_placed_db(Arena *arena, PlacedDb *db, const BenchCaseParams *p)
{
	uint32_t N = p->inst_count;
	uint32_t M = p->nets_count;
	uint32_t P = p->pins_per_inst;
	uint32_t K = p->pins_per_net;
	uint32_t pin_cap;
	uint32_t netpin_cap;
	BenchRng rng;
	InstId inst_base;
	NetId net_base;
	uint32_t *pin_perm;
	uint32_t pin_i;
	uint32_t net_i;

	if (N == 0 || M == 0 || P == 0 || K == 0) return DB_ERR_INVALID_INPUT;

	pin_cap = N * P;
	netpin_cap = M * K;
	if (pin_cap < netpin_cap) return DB_ERR_INVALID_INPUT;

	placed_db_init(db, arena);
	if (placed_db_alloc(db, N, pin_cap, M, netpin_cap) != DB_OK) {
		return DB_ERR_OOM;
	}

	bench_rng_seed(&rng, p->seed);

	inst_base = placed_db_reserve_insts(db, N);
	if (inst_base == (InstId)DB_INVALID_ID) return DB_ERR_CAPACITY;

	for (pin_i = 0; pin_i < N; pin_i++) {
		InstId inst = inst_base + pin_i;
		int32_t x = (int32_t)bench_rng_range(&rng, 0, (uint32_t)p->coord_span);
		int32_t y = (int32_t)bench_rng_range(&rng, 0, (uint32_t)p->coord_span);

		db->inst_name[inst] = arena_u32_name(arena, "U", pin_i);
		db->inst_x[inst] = x;
		db->inst_y[inst] = y;
		db->inst_orient[inst] = 0;

		(void)placed_db_reserve_inst_pins(db, inst, P);
		{
			uint32_t off = db->inst_pin_offset[inst];
			uint32_t j;
			for (j = 0; j < P; j++) {
				uint32_t pin = off + j;
				db->pin_name[pin] = arena_u32_name(arena, "P", j);
				db->pin_dx[pin] = (int32_t)j;
				db->pin_dy[pin] = 0;
			}
		}
	}

	net_base = placed_db_reserve_nets(db, M);
	if (net_base == (NetId)DB_INVALID_ID) return DB_ERR_CAPACITY;
	for (net_i = 0; net_i < M; net_i++) {
		NetId net = net_base + net_i;
		db->net_name[net] = arena_u32_name(arena, "N", net_i);
	}

	pin_perm = (uint32_t *)arena_alloc(arena, sizeof(uint32_t) * (size_t)pin_cap, _Alignof(uint32_t));
	if (!pin_perm) return DB_ERR_OOM;
	for (pin_i = 0; pin_i < pin_cap; pin_i++) pin_perm[pin_i] = pin_i;
	shuffle_u32(&rng, pin_perm, pin_cap);

	for (net_i = 0; net_i < M; net_i++) {
		PinId pins_stack[64];
		PinId *pins = pins_stack;
		uint32_t j;
		NetId net = net_base + net_i;

		if (K > 64) {
			pins = (PinId *)arena_alloc(arena, sizeof(PinId) * (size_t)K, _Alignof(PinId));
			if (!pins) return DB_ERR_OOM;
		}

		for (j = 0; j < K; j++) {
			uint32_t pick = pin_perm[net_i * K + j];
			pins[j] = (PinId)pick;
		}

		if (placed_db_define_net_pins(db, net, pins, K) != DB_OK) {
			return DB_ERR_INVALID_INPUT;
		}
	}

	if (placed_db_validate(db) != DB_OK) return DB_ERR_INVALID_INPUT;
	return DB_OK;
}
