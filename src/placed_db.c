#include "placed_db.h"
#include <string.h>

static DbResult
alloc_u32(Arena *a, uint32_t **out, uint32_t count)
{
	uint32_t *p = (uint32_t *)arena_alloc(a, sizeof(uint32_t) * (size_t)count, _Alignof(uint32_t));
	if (!p) {
		return DB_ERR_OOM;
	}
	*out = p;
	return DB_OK;
}

static DbResult
alloc_i32(Arena *a, int32_t **out, uint32_t count)
{
	int32_t *p = (int32_t *)arena_alloc(a, sizeof(int32_t) * (size_t)count, _Alignof(int32_t));
	if (!p) {
		return DB_ERR_OOM;
	}
	*out = p;
	return DB_OK;
}

static DbResult
alloc_i64(Arena *a, int64_t **out, uint32_t count)
{
	int64_t *p = (int64_t *)arena_alloc(a, sizeof(int64_t) * (size_t)count, _Alignof(int64_t));
	if (!p) {
		return DB_ERR_OOM;
	}
	*out = p;
	return DB_OK;
}

static DbResult
alloc_u8(Arena *a, uint8_t **out, uint32_t count)
{
	uint8_t *p = (uint8_t *)arena_alloc(a, sizeof(uint8_t) * (size_t)count, _Alignof(uint8_t));
	if (!p) {
		return DB_ERR_OOM;
	}
	*out = p;
	return DB_OK;
}

static DbResult
alloc_cptr(Arena *a, const char ***out, uint32_t count)
{
	const char **p = (const char **)arena_alloc(a, sizeof(const char *) * (size_t)count, _Alignof(const char *));
	if (!p) {
		return DB_ERR_OOM;
	}
	*out = p;
	return DB_OK;
}

void
placed_db_init(PlacedDb *db, Arena *arena)
{
	memset(db, 0, sizeof(*db));
	db->arena = arena;
	db->net_mark_gen = 1;
}

DbResult
placed_db_alloc(PlacedDb *db,
	uint32_t inst_cap,
	uint32_t pin_cap,
	uint32_t net_cap,
	uint32_t netpin_cap)
{
	Arena *a = db->arena;
	DbResult r;

	db->inst_cap = inst_cap;
	db->pin_cap = pin_cap;
	db->net_cap = net_cap;
	db->netpin_cap = netpin_cap;

	r = alloc_cptr(a, &db->inst_name, inst_cap); if (r) return r;
	r = alloc_i32(a, &db->inst_x, inst_cap); if (r) return r;
	r = alloc_i32(a, &db->inst_y, inst_cap); if (r) return r;
	r = alloc_u8(a, &db->inst_orient, inst_cap); if (r) return r;
	r = alloc_u32(a, &db->inst_pin_offset, inst_cap); if (r) return r;
	r = alloc_u32(a, &db->inst_pin_count, inst_cap); if (r) return r;

	r = alloc_cptr(a, &db->pin_name, pin_cap); if (r) return r;
	db->pin_id = (PinId *)arena_alloc(a, sizeof(PinId) * (size_t)pin_cap, _Alignof(PinId));
	if (!db->pin_id) return DB_ERR_OOM;
	db->pin_inst = (InstId *)arena_alloc(a, sizeof(InstId) * (size_t)pin_cap, _Alignof(InstId));
	if (!db->pin_inst) return DB_ERR_OOM;
	db->pin_net = (NetId *)arena_alloc(a, sizeof(NetId) * (size_t)pin_cap, _Alignof(NetId));
	if (!db->pin_net) return DB_ERR_OOM;
	r = alloc_i32(a, &db->pin_dx, pin_cap); if (r) return r;
	r = alloc_i32(a, &db->pin_dy, pin_cap); if (r) return r;

	r = alloc_cptr(a, &db->net_name, net_cap); if (r) return r;
	r = alloc_u32(a, &db->net_pin_offset, net_cap); if (r) return r;
	r = alloc_u32(a, &db->net_pin_count, net_cap); if (r) return r;

	db->net_pin_ids = (PinId *)arena_alloc(a, sizeof(PinId) * (size_t)netpin_cap, _Alignof(PinId));
	if (!db->net_pin_ids) return DB_ERR_OOM;

	r = alloc_i32(a, &db->net_min_x, net_cap); if (r) return r;
	r = alloc_i32(a, &db->net_max_x, net_cap); if (r) return r;
	r = alloc_i32(a, &db->net_min_y, net_cap); if (r) return r;
	r = alloc_i32(a, &db->net_max_y, net_cap); if (r) return r;
	r = alloc_i64(a, &db->net_hpwl, net_cap); if (r) return r;

	r = alloc_u32(a, &db->net_mark, net_cap); if (r) return r;
	memset(db->net_mark, 0, sizeof(uint32_t) * (size_t)net_cap);

	placed_db_reset(db);
	return DB_OK;
}

void
placed_db_reset(PlacedDb *db)
{
	db->inst_count = 0;
	db->pin_count = 0;
	db->net_count = 0;
	db->netpin_count = 0;
	db->net_mark_gen = 1;
}

InstId
placed_db_reserve_insts(PlacedDb *db, uint32_t n)
{
	uint32_t base = db->inst_count;
	if (base + n > db->inst_cap) {
		return (InstId)DB_INVALID_ID;
	}
	db->inst_count = base + n;
	return (InstId)base;
}

PinId
placed_db_reserve_pins(PlacedDb *db, uint32_t n)
{
	uint32_t base = db->pin_count;
	uint32_t i;
	if (base + n > db->pin_cap) {
		return (PinId)DB_INVALID_ID;
	}
	db->pin_count = base + n;
	for (i = 0; i < n; i++) {
		db->pin_id[base + i] = (PinId)(base + i);
	}
	return (PinId)base;
}

PinId
placed_db_reserve_inst_pins(PlacedDb *db, InstId inst, uint32_t n)
{
	PinId base;
	uint32_t i;
	uint32_t p;

	if ((uint32_t)inst >= db->inst_count) {
		return (PinId)DB_INVALID_ID;
	}
	if (db->inst_pin_count[inst] != 0) {
		return (PinId)DB_INVALID_ID;
	}

	base = placed_db_reserve_pins(db, n);
	if (base == (PinId)DB_INVALID_ID) {
		return (PinId)DB_INVALID_ID;
	}

	db->inst_pin_offset[inst] = (uint32_t)base;
	db->inst_pin_count[inst] = n;

	p = (uint32_t)base;
	for (i = 0; i < n; i++) {
		uint32_t pin = p + i;
		db->pin_inst[pin] = inst;
		db->pin_net[pin] = (NetId)DB_INVALID_ID;
		db->pin_dx[pin] = 0;
		db->pin_dy[pin] = 0;
		db->pin_name[pin] = 0;
	}

	return base;
}

NetId
placed_db_reserve_nets(PlacedDb *db, uint32_t n)
{
	uint32_t base = db->net_count;
	if (base + n > db->net_cap) {
		return (NetId)DB_INVALID_ID;
	}
	db->net_count = base + n;
	return (NetId)base;
}

uint32_t
placed_db_reserve_netpins(PlacedDb *db, uint32_t n)
{
	uint32_t base = db->netpin_count;
	if (base + n > db->netpin_cap) {
		return DB_INVALID_ID;
	}
	db->netpin_count = base + n;
	return base;
}

DbResult
placed_db_define_net_pins(PlacedDb *db, NetId net, const PinId *pins, uint32_t pin_count)
{
	uint32_t off;
	uint32_t i;

	if ((uint32_t)net >= db->net_count) {
		return DB_ERR_INVALID_INPUT;
	}
	if (!pins && pin_count) {
		return DB_ERR_INVALID_INPUT;
	}
	if (db->net_pin_count[net] != 0) {
		return DB_ERR_INVALID_INPUT;
	}

	off = placed_db_reserve_netpins(db, pin_count);
	if (off == DB_INVALID_ID) {
		return DB_ERR_CAPACITY;
	}

	db->net_pin_offset[net] = off;
	db->net_pin_count[net] = pin_count;

	for (i = 0; i < pin_count; i++) {
		PinId pin = pins[i];
		if ((uint32_t)pin >= db->pin_count) {
			return DB_ERR_INVALID_INPUT;
		}
		if (db->pin_net[pin] != (NetId)DB_INVALID_ID) {
			return DB_ERR_INVALID_INPUT;
		}
		db->net_pin_ids[off + i] = pin;
		db->pin_net[pin] = net;
	}

	return DB_OK;
}

DbResult
placed_db_net_pins(const PlacedDb *db, NetId net, const PinId **pins, uint32_t *count)
{
	uint32_t off;
	uint32_t n;
	if ((uint32_t)net >= db->net_count) {
		return DB_ERR_INVALID_INPUT;
	}
	off = db->net_pin_offset[net];
	n = db->net_pin_count[net];
	if (off + n > db->netpin_count) {
		return DB_ERR_INVALID_INPUT;
	}
	*pins = db->net_pin_ids + off;
	*count = n;
	return DB_OK;
}

DbResult
placed_db_inst_pins(const PlacedDb *db, InstId inst, const PinId **pins, uint32_t *count)
{
	uint32_t off;
	uint32_t n;
	if ((uint32_t)inst >= db->inst_count) {
		return DB_ERR_INVALID_INPUT;
	}
	off = db->inst_pin_offset[inst];
	n = db->inst_pin_count[inst];
	if (off + n > db->pin_count) {
		return DB_ERR_INVALID_INPUT;
	}
	*pins = db->pin_id + off;
	*count = n;
	return DB_OK;
}

DbResult
placed_db_net_bbox(const PlacedDb *db, NetId net, BBoxI32 *out_bbox)
{
	if ((uint32_t)net >= db->net_count) {
		return DB_ERR_INVALID_INPUT;
	}
	out_bbox->min_x = db->net_min_x[net];
	out_bbox->max_x = db->net_max_x[net];
	out_bbox->min_y = db->net_min_y[net];
	out_bbox->max_y = db->net_max_y[net];
	return DB_OK;
}

DbResult
placed_db_net_hpwl(const PlacedDb *db, NetId net, int64_t *out_hpwl)
{
	if ((uint32_t)net >= db->net_count) {
		return DB_ERR_INVALID_INPUT;
	}
	*out_hpwl = db->net_hpwl[net];
	return DB_OK;
}

DbResult
placed_db_validate(const PlacedDb *db)
{
	uint32_t i;

	if (!db) {
		return DB_ERR_INVALID_INPUT;
	}

	if (db->inst_count > db->inst_cap) return DB_ERR_INVALID_INPUT;
	if (db->pin_count > db->pin_cap) return DB_ERR_INVALID_INPUT;
	if (db->net_count > db->net_cap) return DB_ERR_INVALID_INPUT;
	if (db->netpin_count > db->netpin_cap) return DB_ERR_INVALID_INPUT;

	/* Inst -> Pins: contiguous slice and correct pin ownership */
	for (i = 0; i < db->inst_count; i++) {
		uint32_t off = db->inst_pin_offset[i];
		uint32_t n = db->inst_pin_count[i];
		uint32_t j;
		if (off + n > db->pin_count) return DB_ERR_INVALID_INPUT;
		for (j = 0; j < n; j++) {
			uint32_t pin = off + j;
			if ((uint32_t)db->pin_id[pin] != pin) return DB_ERR_INVALID_INPUT;
			if ((uint32_t)db->pin_inst[pin] != i) return DB_ERR_INVALID_INPUT;
			if (db->pin_net[pin] != (NetId)DB_INVALID_ID &&
			    (uint32_t)db->pin_net[pin] >= db->net_count) {
				return DB_ERR_INVALID_INPUT;
			}
		}
	}

	/* Net -> Pins: range checks and bidirectional consistency (pin_net == net) */
	for (i = 0; i < db->net_count; i++) {
		uint32_t off = db->net_pin_offset[i];
		uint32_t n = db->net_pin_count[i];
		uint32_t j;
		if (off + n > db->netpin_count) return DB_ERR_INVALID_INPUT;
		for (j = 0; j < n; j++) {
			PinId pin = db->net_pin_ids[off + j];
			if ((uint32_t)pin >= db->pin_count) return DB_ERR_INVALID_INPUT;
			if ((uint32_t)db->pin_net[pin] != i) return DB_ERR_INVALID_INPUT;
		}
	}

	return DB_OK;
}

static void
recompute_net(PlacedDb *db, NetId net)
{
	uint32_t off = db->net_pin_offset[net];
	uint32_t n = db->net_pin_count[net];
	int32_t min_x = 0;
	int32_t max_x = 0;
	int32_t min_y = 0;
	int32_t max_y = 0;
	uint32_t i;

	if (n == 0) {
		db->net_min_x[net] = 0;
		db->net_max_x[net] = 0;
		db->net_min_y[net] = 0;
		db->net_max_y[net] = 0;
		db->net_hpwl[net] = 0;
		return;
	}

	for (i = 0; i < n; i++) {
		PinId pin = db->net_pin_ids[off + i];
		InstId inst = db->pin_inst[pin];
		int32_t x = db->inst_x[inst] + db->pin_dx[pin];
		int32_t y = db->inst_y[inst] + db->pin_dy[pin];
		if (i == 0) {
			min_x = max_x = x;
			min_y = max_y = y;
		} else {
			if (x < min_x) min_x = x;
			if (x > max_x) max_x = x;
			if (y < min_y) min_y = y;
			if (y > max_y) max_y = y;
		}
	}

	db->net_min_x[net] = min_x;
	db->net_max_x[net] = max_x;
	db->net_min_y[net] = min_y;
	db->net_max_y[net] = max_y;
	db->net_hpwl[net] = (int64_t)(max_x - min_x) + (int64_t)(max_y - min_y);
}

DbResult
placed_db_recompute_all_nets(PlacedDb *db)
{
	uint32_t i;
	for (i = 0; i < db->net_count; i++) {
		recompute_net(db, (NetId)i);
	}
	return DB_OK;
}

DbBatch
placed_db_begin_batch(PlacedDb *db, Arena *scratch_arena, uint32_t touched_inst_cap, uint32_t touched_net_cap)
{
	DbBatch b;
	memset(&b, 0, sizeof(b));
	b.db = db;
	b.scratch_frame = arena_begin_frame(scratch_arena);
	b.touched_cap = touched_inst_cap;
	if (touched_inst_cap) {
		b.touched_insts = (InstId *)arena_alloc(
			scratch_arena,
			sizeof(InstId) * (size_t)touched_inst_cap,
			_Alignof(InstId));
		if (!b.touched_insts) {
			b.touched_cap = 0;
		}
	}

	b.touched_net_cap = touched_net_cap;
	if (touched_net_cap) {
		b.touched_nets = (NetId *)arena_alloc(
			scratch_arena,
			sizeof(NetId) * (size_t)touched_net_cap,
			_Alignof(NetId));
		if (!b.touched_nets) {
			b.touched_net_cap = 0;
		}
	}
	return b;
}

static DbResult
batch_touch_net(DbBatch *batch, NetId net)
{
	uint32_t i;

	for (i = 0; i < batch->touched_net_count; i++) {
		if (batch->touched_nets[i] == net) {
			return DB_OK;
		}
	}
	if (batch->touched_net_count >= batch->touched_net_cap) {
		return DB_ERR_CAPACITY;
	}
	batch->touched_nets[batch->touched_net_count++] = net;
	return DB_OK;
}

DbResult
placed_db_batch_move_inst(DbBatch *batch, InstId inst, int32_t new_x, int32_t new_y)
{
	PlacedDb *db = batch->db;
	if ((uint32_t)inst >= db->inst_count) {
		return DB_ERR_INVALID_INPUT;
	}
	if (batch->touched_count >= batch->touched_cap) {
		return DB_ERR_CAPACITY;
	}
	db->inst_x[inst] = new_x;
	db->inst_y[inst] = new_y;
	batch->touched_insts[batch->touched_count++] = inst;
	return DB_OK;
}

DbResult
placed_db_batch_redefine_net_pins(DbBatch *batch, NetId net, const PinId *pins, uint32_t pin_count)
{
	PlacedDb *db = batch->db;
	uint32_t old_off;
	uint32_t old_n;
	uint32_t new_off;
	uint32_t i;
	DbResult r;

	if ((uint32_t)net >= db->net_count) {
		return DB_ERR_INVALID_INPUT;
	}
	if (!pins && pin_count) {
		return DB_ERR_INVALID_INPUT;
	}

	old_off = db->net_pin_offset[net];
	old_n = db->net_pin_count[net];
	if (old_off + old_n > db->netpin_count) {
		return DB_ERR_INVALID_INPUT;
	}

	for (i = 0; i < old_n; i++) {
		PinId pin = db->net_pin_ids[old_off + i];
		if ((uint32_t)pin >= db->pin_count) {
			return DB_ERR_INVALID_INPUT;
		}
		if (db->pin_net[pin] == net) {
			db->pin_net[pin] = (NetId)DB_INVALID_ID;
		}
	}

	new_off = placed_db_reserve_netpins(db, pin_count);
	if (new_off == DB_INVALID_ID) {
		return DB_ERR_CAPACITY;
	}
	db->net_pin_offset[net] = new_off;
	db->net_pin_count[net] = pin_count;

	for (i = 0; i < pin_count; i++) {
		PinId pin = pins[i];
		if ((uint32_t)pin >= db->pin_count) {
			return DB_ERR_INVALID_INPUT;
		}
		if (db->pin_net[pin] != (NetId)DB_INVALID_ID) {
			return DB_ERR_INVALID_INPUT;
		}
		db->net_pin_ids[new_off + i] = pin;
		db->pin_net[pin] = net;
	}

	r = batch_touch_net(batch, net);
	if (r) {
		return r;
	}
	return DB_OK;
}

DbResult
placed_db_end_batch(DbBatch *batch)
{
	PlacedDb *db = batch->db;
	Arena *scratch = batch->scratch_frame.arena;
	uint32_t gen;
	NetId *impacted;
	uint32_t impacted_count = 0;
	uint32_t impacted_cap = db->net_count;
	uint32_t i;

	if (db->net_count == 0) {
		arena_end_frame(&batch->scratch_frame);
		return DB_OK;
	}

	gen = db->net_mark_gen + 1;
	if (gen == 0) {
		memset(db->net_mark, 0, sizeof(uint32_t) * (size_t)db->net_cap);
		gen = 1;
	}
	db->net_mark_gen = gen;

	impacted = (NetId *)arena_alloc(
		scratch,
		sizeof(NetId) * (size_t)impacted_cap,
		_Alignof(NetId));
	if (!impacted) {
		arena_end_frame(&batch->scratch_frame);
		return DB_ERR_OOM;
	}

	for (i = 0; i < batch->touched_count; i++) {
		InstId inst = batch->touched_insts[i];
		uint32_t off = db->inst_pin_offset[inst];
		uint32_t n = db->inst_pin_count[inst];
		uint32_t j;

		for (j = 0; j < n; j++) {
			PinId pin = (PinId)(off + j);
			NetId net = db->pin_net[pin];
			if (net == (NetId)DB_INVALID_ID) {
				continue;
			}
			if (db->net_mark[net] == gen) {
				continue;
			}
			db->net_mark[net] = gen;
			impacted[impacted_count++] = net;
		}
	}

	for (i = 0; i < batch->touched_net_count; i++) {
		NetId net = batch->touched_nets[i];
		if ((uint32_t)net >= db->net_count) {
			continue;
		}
		if (db->net_mark[net] == gen) {
			continue;
		}
		db->net_mark[net] = gen;
		impacted[impacted_count++] = net;
	}

	for (i = 0; i < impacted_count; i++) {
		recompute_net(db, impacted[i]);
	}

	arena_end_frame(&batch->scratch_frame);
	return DB_OK;
}
