#ifndef CHIPSDB_LIB_DB_H
#define CHIPSDB_LIB_DB_H

#include <stdint.h>

#include "arena.h"
#include "db_types.h"

typedef uint32_t MasterId;

typedef enum PinDir {
	PIN_DIR_UNKNOWN = 0,
	PIN_DIR_INPUT,
	PIN_DIR_OUTPUT,
	PIN_DIR_INOUT,
} PinDir;

typedef struct LibDb {
	Arena *arena;

	/* LEF units: database units per micron (e.g. 1000) */
	int32_t dbu_per_micron;

	uint32_t master_count;
	uint32_t pin_count;

	uint32_t master_cap;
	uint32_t pin_cap;

	/* Master table (SoA) */
	const char **master_name;
	int32_t *master_w;
	int32_t *master_h;
	uint32_t *master_pin_offset; /* into pin table, contiguous pins per master */
	uint32_t *master_pin_count;

	/* Pin table (SoA) */
	const char **pin_name;
	MasterId *pin_master;
	uint8_t *pin_dir;
	int32_t *pin_cx; /* pin center (dbu) */
	int32_t *pin_cy;
} LibDb;

void lib_db_init(LibDb *db, Arena *arena);
DbResult lib_db_alloc(LibDb *db, uint32_t master_cap, uint32_t pin_cap);
void lib_db_reset(LibDb *db);

DbResult lib_db_parse_lef_file(LibDb *db, const char *path);

#endif
