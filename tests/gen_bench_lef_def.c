#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <sys/stat.h>

#include "arena.h"
#include "bench_case.h"
#include "placed_db.h"

static uint32_t
getenv_u32(const char *key, uint32_t def)
{
	const char *s = getenv(key);
	if (!s || !s[0]) return def;
	return (uint32_t)strtoul(s, 0, 10);
}

static int
ensure_dir(const char *path)
{
	if (mkdir(path, 0755) == 0) return 0;
	if (errno == EEXIST) return 0;
	return -1;
}

static void
write_lef(FILE *f, uint32_t pins_per_inst)
{
	uint32_t i;

	fprintf(f, "VERSION 5.8 ;\n");
	fprintf(f, "UNITS\n");
	fprintf(f, "  DATABASE MICRONS 1000 ;\n");
	fprintf(f, "END UNITS\n\n");
	fprintf(f, "MACRO CELL\n");
	fprintf(f, "  CLASS CORE ;\n");
	fprintf(f, "  ORIGIN 0 0 ;\n");
	fprintf(f, "  SIZE 1.0 BY 1.0 ;\n");
	for (i = 0; i < pins_per_inst; i++) {
		double x1 = 0.1 + 0.05 * (double)i;
		double y1 = 0.1;
		double x2 = x1 + 0.02;
		double y2 = y1 + 0.02;
		fprintf(f, "  PIN P%u\n", i);
		fprintf(f, "    DIRECTION INOUT ;\n");
		fprintf(f, "    PORT\n");
		fprintf(f, "      LAYER M1 ;\n");
		fprintf(f, "        RECT %.6f %.6f %.6f %.6f ;\n", x1, y1, x2, y2);
		fprintf(f, "    END\n");
		fprintf(f, "  END P%u\n", i);
	}
	fprintf(f, "END CELL\n\n");
	fprintf(f, "END LIBRARY\n");
}

static void
write_def(FILE *f, const PlacedDb *db)
{
	uint32_t i;

	fprintf(f, "VERSION 5.8 ;\n");
	fprintf(f, "DIVIDERCHAR \"/\" ;\n");
	fprintf(f, "BUSBITCHARS \"[]\" ;\n");
	fprintf(f, "DESIGN bench ;\n");
	fprintf(f, "UNITS DISTANCE MICRONS 1000 ;\n\n");

	fprintf(f, "COMPONENTS %u ;\n", db->inst_count);
	for (i = 0; i < db->inst_count; i++) {
		const char *name = db->inst_name[i] ? db->inst_name[i] : "U";
		fprintf(f, "  - %s CELL + PLACED ( %d %d ) N ;\n", name, db->inst_x[i], db->inst_y[i]);
	}
	fprintf(f, "END COMPONENTS\n\n");

	fprintf(f, "NETS %u ;\n", db->net_count);
	for (i = 0; i < db->net_count; i++) {
		const PinId *pins;
		uint32_t n;
		uint32_t j;
		const char *nname = db->net_name[i] ? db->net_name[i] : "N";
		if (placed_db_net_pins(db, (NetId)i, &pins, &n) != DB_OK) {
			fprintf(stderr, "net pins query failed\n");
			exit(1);
		}
		fprintf(f, "  - %s\n", nname);
		for (j = 0; j < n; j++) {
			PinId pin = pins[j];
			InstId inst = db->pin_inst[pin];
			const char *iname = db->inst_name[inst] ? db->inst_name[inst] : "U";
			const char *pname = db->pin_name[pin] ? db->pin_name[pin] : "P";
			fprintf(f, "    ( %s %s )\n", iname, pname);
		}
		fprintf(f, "  ;\n");
	}
	fprintf(f, "END NETS\n\n");

	fprintf(f, "END DESIGN\n");
}

int main(int argc, char **argv)
{
	BenchCaseParams p;
	Arena arena;
	void *mem;
	PlacedDb db;

	FILE *lef;
	FILE *def;

	(void)argc;
	(void)argv;

	p.seed = getenv_u32("SEED", 1);
	p.inst_count = getenv_u32("N", 20000);
	p.nets_count = getenv_u32("M", 20000);
	p.pins_per_inst = getenv_u32("P", 4);
	p.pins_per_net = getenv_u32("K", 4);
	p.coord_span = (int32_t)getenv_u32("SPAN", 100000);

	mem = malloc(256ull * 1024ull * 1024ull);
	if (!mem) {
		fprintf(stderr, "oom\n");
		return 1;
	}
	arena_init(&arena, mem, 256ull * 1024ull * 1024ull);

	if (bench_case_build_placed_db(&arena, &db, &p) != DB_OK) {
		fprintf(stderr, "case build failed\n");
		free(mem);
		return 1;
	}
	placed_db_recompute_all_nets(&db);

	if (ensure_dir("build") != 0) {
		fprintf(stderr, "mkdir build failed\n");
		free(mem);
		return 1;
	}
	if (ensure_dir("build/bench") != 0) {
		fprintf(stderr, "mkdir build/bench failed\n");
		free(mem);
		return 1;
	}

	lef = fopen("build/bench/bench.lef", "wb");
	if (!lef) {
		fprintf(stderr, "open bench.lef failed\n");
		free(mem);
		return 1;
	}
	write_lef(lef, p.pins_per_inst);
	fclose(lef);

	def = fopen("build/bench/bench.def", "wb");
	if (!def) {
		fprintf(stderr, "open bench.def failed\n");
		free(mem);
		return 1;
	}
	write_def(def, &db);
	fclose(def);

	printf("wrote build/bench/bench.lef and build/bench/bench.def\n");

	free(mem);
	return 0;
}
