#ifndef CHIPSDB_TECH_DB_H
#define CHIPSDB_TECH_DB_H

#include <stdint.h>

#include "arena.h"
#include "db_types.h"

typedef uint32_t LayerId;

typedef enum TrackDir {
	TRACK_DIR_UNKNOWN = 0,
	TRACK_DIR_X,
	TRACK_DIR_Y,
} TrackDir;

typedef enum LayerDir {
	LAYER_DIR_UNKNOWN = 0,
	LAYER_DIR_HORIZONTAL,
	LAYER_DIR_VERTICAL,
} LayerDir;

typedef struct TechDb {
	Arena *arena;

	/* LEF units: database units per micron (e.g. 1000) */
	int32_t dbu_per_micron;

	uint32_t layer_count;
	uint32_t track_count;

	uint32_t layer_cap;
	uint32_t track_cap;

	/* Routing layer table (SoA) */
	const char **layer_name;
	uint8_t *layer_dir;      /* LayerDir */
	int32_t *layer_pitch;    /* dbu */
	int32_t *layer_width;    /* dbu */
	int32_t *layer_spacing;  /* dbu */

	/* Tracks (SoA) */
	uint8_t *track_dir;      /* TrackDir */
	int32_t *track_start;    /* dbu */
	int32_t *track_pitch;    /* dbu */
	uint32_t *track_count_n; /* number of tracks */
	const char **track_layer;
} TechDb;

void tech_db_init(TechDb *db, Arena *arena);
DbResult tech_db_alloc(TechDb *db, uint32_t layer_cap, uint32_t track_cap);
void tech_db_reset(TechDb *db);

DbResult tech_db_parse_tech_lef_file(TechDb *db, const char *path);

#endif
