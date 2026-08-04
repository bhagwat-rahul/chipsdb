#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>

#include "arena.h"
#include "bench_case.h"
#include "placed_db.h"

static uint64_t
now_ns(void)
{
	struct timespec ts;
	/*
	 * C11 fallback that works without POSIX feature macros.
	 * This is wall-clock (not monotonic), but good enough for throughput
	 * measurements inside a single run.
	 */
	timespec_get(&ts, TIME_UTC);
	return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static int
streq(const char *a, const char *b)
{
	return strcmp(a, b) == 0;
}

static void
usage(void)
{
	fprintf(stderr, "usage:\n");
	fprintf(stderr, "  bench_hpwl full [--inst N] [--nets M] [--k K] [--p P] [--iters I] [--seed S]\n");
	fprintf(stderr, "  bench_hpwl step [--inst N] [--nets M] [--k K] [--p P] [--steps T] [--moves P] [--seed S]\n");
}

static uint32_t
arg_u32(int *i, int argc, char **argv, uint32_t def)
{
	if (*i + 1 >= argc) return def;
	(*i)++;
	return (uint32_t)strtoul(argv[*i], 0, 10);
}

int main(int argc, char **argv)
{
	const char *mode;

	BenchCaseParams p;
	uint32_t iters = 10;
	uint32_t steps = 1000;
	uint32_t moves_per_step = 16;

	Arena arena;
	void *mem;
	PlacedDb db;

	uint64_t t0, t1;

	if (argc < 2) {
		usage();
		return 2;
	}

	memset(&p, 0, sizeof(p));
	p.seed = 1;
	p.inst_count = 20000;
	p.nets_count = 20000;
	p.pins_per_inst = 4;
	p.pins_per_net = 4;
	p.coord_span = 100000;

	mode = argv[1];
	{
		int i;
		for (i = 2; i < argc; i++) {
			if (streq(argv[i], "--inst")) p.inst_count = arg_u32(&i, argc, argv, p.inst_count);
			else if (streq(argv[i], "--nets")) p.nets_count = arg_u32(&i, argc, argv, p.nets_count);
			else if (streq(argv[i], "--k")) p.pins_per_net = arg_u32(&i, argc, argv, p.pins_per_net);
			else if (streq(argv[i], "--p")) p.pins_per_inst = arg_u32(&i, argc, argv, p.pins_per_inst);
			else if (streq(argv[i], "--iters")) iters = arg_u32(&i, argc, argv, iters);
			else if (streq(argv[i], "--steps")) steps = arg_u32(&i, argc, argv, steps);
			else if (streq(argv[i], "--moves")) moves_per_step = arg_u32(&i, argc, argv, moves_per_step);
			else if (streq(argv[i], "--seed")) p.seed = arg_u32(&i, argc, argv, p.seed);
			else {
				fprintf(stderr, "unknown arg: %s\n", argv[i]);
				return 2;
			}
		}
	}

	mem = malloc(512ull * 1024ull * 1024ull);
	if (!mem) {
		fprintf(stderr, "oom\n");
		return 1;
	}
	arena_init(&arena, mem, 512ull * 1024ull * 1024ull);

	if (bench_case_build_placed_db(&arena, &db, &p) != DB_OK) {
		fprintf(stderr, "case build failed\n");
		free(mem);
		return 1;
	}

	/* Warmup */
	placed_db_recompute_all_nets(&db);

	if (streq(mode, "full")) {
		double nets = (double)db.net_count * (double)iters;
		t0 = now_ns();
		{
			uint32_t i;
			for (i = 0; i < iters; i++) {
				placed_db_recompute_all_nets(&db);
			}
		}
		t1 = now_ns();

		{
			double dt = (double)(t1 - t0);
			double ns_per_net = dt / nets;
			double nets_per_s = nets * (1e9 / dt);
			printf("hpwl_full ns_per_net=%.2f nets_per_s=%.2f N=%u M=%u K=%u P=%u iters=%u\n",
				ns_per_net,
				nets_per_s,
				p.inst_count,
				p.nets_count,
				p.pins_per_net,
				p.pins_per_inst,
				iters);
		}
	} else if (streq(mode, "step")) {
		BenchRng rng;
		double steps_d = (double)steps;
		bench_rng_seed(&rng, p.seed ^ 0x9e3779b9u);

		t0 = now_ns();
		{
			uint32_t s;
			for (s = 0; s < steps; s++) {
				DbBatch b = placed_db_begin_batch(&db, &arena, moves_per_step, 0);
				uint32_t m;
				for (m = 0; m < moves_per_step; m++) {
					uint32_t inst = bench_rng_range(&rng, 0, p.inst_count);
					int32_t nx = (int32_t)bench_rng_range(&rng, 0, (uint32_t)p.coord_span);
					int32_t ny = (int32_t)bench_rng_range(&rng, 0, (uint32_t)p.coord_span);
					(void)placed_db_batch_move_inst(&b, (InstId)inst, nx, ny);
				}
				(void)placed_db_end_batch(&b);
			}
		}
		t1 = now_ns();

		{
			double dt = (double)(t1 - t0);
			double ns_per_step = dt / steps_d;
			double steps_per_s = steps_d * (1e9 / dt);
			printf("hpwl_step ns_per_step=%.2f steps_per_s=%.2f N=%u M=%u K=%u P=%u steps=%u moves=%u\n",
				ns_per_step,
				steps_per_s,
				p.inst_count,
				p.nets_count,
				p.pins_per_net,
				p.pins_per_inst,
				steps,
				moves_per_step);
		}
	} else {
		usage();
		free(mem);
		return 2;
	}

	free(mem);
	return 0;
}
