#ifndef CHIPSDB_PLACED_DB_H
#define CHIPSDB_PLACED_DB_H

#include <stdint.h>

#include "arena.h"
#include "db_types.h"

typedef struct PlacedDb {
	Arena *arena;

	uint32_t inst_count;
	uint32_t pin_count;
	uint32_t net_count;
	uint32_t netpin_count;

	uint32_t inst_cap;
	uint32_t pin_cap;
	uint32_t net_cap;
	uint32_t netpin_cap;

	/* InstTable (SoA) */
	const char **inst_name;
	int32_t *inst_x;
	int32_t *inst_y;
	uint8_t *inst_orient;
	uint32_t *inst_pin_offset; /* into PinTable, contiguous pins per inst */
	uint32_t *inst_pin_count;

	/* PinTable (SoA) */
	const char **pin_name;
	PinId *pin_id;
	InstId *pin_inst;
	NetId *pin_net;
	int32_t *pin_dx;
	int32_t *pin_dy;

	/* NetTable (SoA) */
	const char **net_name;
	uint32_t *net_pin_offset; /* into net_pin_ids[] */
	uint32_t *net_pin_count;

	/* NetPinIndex: adjacency (NetId -> PinId[]) */
	PinId *net_pin_ids;

	/* Derived net caches */
	int32_t *net_min_x;
	int32_t *net_max_x;
	int32_t *net_min_y;
	int32_t *net_max_y;
	int64_t *net_hpwl;

	/* Marks for batch dedupe (generation stamping) */
	uint32_t *net_mark;
	uint32_t net_mark_gen;
} PlacedDb;

typedef struct DbBatch {
	PlacedDb *db;
	ArenaFrame scratch_frame;

	InstId *touched_insts;
	uint32_t touched_count;
	uint32_t touched_cap;

	NetId *touched_nets;
	uint32_t touched_net_count;
	uint32_t touched_net_cap;
} DbBatch;

void placed_db_init(PlacedDb *db, Arena *arena);
DbResult placed_db_alloc(PlacedDb *db,
	uint32_t inst_cap,
	uint32_t pin_cap,
	uint32_t net_cap,
	uint32_t netpin_cap);
void placed_db_reset(PlacedDb *db);

InstId placed_db_reserve_insts(PlacedDb *db, uint32_t n);
PinId placed_db_reserve_pins(PlacedDb *db, uint32_t n);
PinId placed_db_reserve_inst_pins(PlacedDb *db, InstId inst, uint32_t n);
NetId placed_db_reserve_nets(PlacedDb *db, uint32_t n);
uint32_t placed_db_reserve_netpins(PlacedDb *db, uint32_t n);

DbResult placed_db_define_net_pins(PlacedDb *db, NetId net, const PinId *pins, uint32_t pin_count);

DbResult placed_db_net_pins(const PlacedDb *db, NetId net, const PinId **pins, uint32_t *count);
DbResult placed_db_inst_pins(const PlacedDb *db, InstId inst, const PinId **pins, uint32_t *count);

DbResult placed_db_net_bbox(const PlacedDb *db, NetId net, BBoxI32 *out_bbox);
DbResult placed_db_net_hpwl(const PlacedDb *db, NetId net, int64_t *out_hpwl);

DbResult placed_db_validate(const PlacedDb *db);

DbResult placed_db_recompute_all_nets(PlacedDb *db);

DbBatch placed_db_begin_batch(PlacedDb *db, Arena *scratch_arena, uint32_t touched_inst_cap, uint32_t touched_net_cap);
DbResult placed_db_batch_move_inst(DbBatch *batch, InstId inst, int32_t new_x, int32_t new_y);
DbResult placed_db_batch_redefine_net_pins(DbBatch *batch, NetId net, const PinId *pins, uint32_t pin_count);
DbResult placed_db_end_batch(DbBatch *batch);

#endif
