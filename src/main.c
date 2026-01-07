#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include "arena.h"
#include "placed_db.h"

int main(void)
{
	Arena arena;
	void *mem;

	PlacedDb db;
	InstId inst_base;
	NetId net0;
	PinId pin0;
	PinId pin1;
	uint32_t netpins_off;
	int64_t hpwl_before;
	int64_t hpwl_after;
	DbBatch batch;

	mem = malloc(1024 * 1024);
	if (!mem) {
		fprintf(stderr, "oom\n");
		return 1;
	}
	arena_init(&arena, mem, 1024 * 1024);

	placed_db_init(&db, &arena);
	if (placed_db_alloc(&db, 8, 16, 8, 32) != DB_OK) {
		fprintf(stderr, "db alloc failed\n");
		return 1;
	}

	inst_base = placed_db_reserve_insts(&db, 2);
	if (inst_base == (InstId)DB_INVALID_ID) {
		fprintf(stderr, "inst reserve failed\n");
		return 1;
	}

	db.inst_name[inst_base + 0] = "U1";
	db.inst_x[inst_base + 0] = 0;
	db.inst_y[inst_base + 0] = 0;
	db.inst_orient[inst_base + 0] = 0;

	db.inst_name[inst_base + 1] = "U2";
	db.inst_x[inst_base + 1] = 100;
	db.inst_y[inst_base + 1] = 0;
	db.inst_orient[inst_base + 1] = 0;

	net0 = placed_db_reserve_nets(&db, 1);
	if (net0 == (NetId)DB_INVALID_ID) {
		fprintf(stderr, "net reserve failed\n");
		return 1;
	}
	db.net_name[net0] = "N1";

	pin0 = placed_db_reserve_inst_pins(&db, inst_base + 0, 1);
	pin1 = placed_db_reserve_inst_pins(&db, inst_base + 1, 1);
	if (pin0 == (PinId)DB_INVALID_ID || pin1 == (PinId)DB_INVALID_ID) {
		fprintf(stderr, "pin reserve failed\n");
		return 1;
	}

	db.pin_name[pin0] = "A";
	db.pin_dx[pin0] = 0;
	db.pin_dy[pin0] = 0;

	db.pin_name[pin1] = "A";
	db.pin_dx[pin1] = 0;
	db.pin_dy[pin1] = 0;

	{
		PinId pins[2];
		pins[0] = pin0;
		pins[1] = pin1;
		if (placed_db_define_net_pins(&db, net0, pins, 2) != DB_OK) {
			fprintf(stderr, "define net pins failed\n");
			return 1;
		}
	}

	netpins_off = db.net_pin_offset[net0];
	if (netpins_off == DB_INVALID_ID) {
		fprintf(stderr, "net pin offset invalid\n");
		return 1;
	}

	if (placed_db_validate(&db) != DB_OK) {
		fprintf(stderr, "db validate failed (post-build)\n");
		return 1;
	}

	placed_db_recompute_all_nets(&db);
	placed_db_net_hpwl(&db, net0, &hpwl_before);
	printf("hpwl before: %" PRId64 "\n", hpwl_before);

	batch = placed_db_begin_batch(&db, &arena, 1);
	if (placed_db_batch_move_inst(&batch, inst_base + 1, 10, 0) != DB_OK) {
		fprintf(stderr, "batch move failed\n");
		return 1;
	}
	placed_db_end_batch(&batch);

	if (placed_db_validate(&db) != DB_OK) {
		fprintf(stderr, "db validate failed (post-batch)\n");
		return 1;
	}

	placed_db_net_hpwl(&db, net0, &hpwl_after);
	printf("hpwl after: %" PRId64 "\n", hpwl_after);

	free(mem);
	return 0;
}
