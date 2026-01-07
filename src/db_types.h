#ifndef CHIPSDB_DB_TYPES_H
#define CHIPSDB_DB_TYPES_H

#include <stdint.h>

typedef enum DbResult {
	DB_OK = 0,
	DB_ERR_OOM,
	DB_ERR_CAPACITY,
	DB_ERR_INVALID_INPUT,
} DbResult;

typedef uint32_t InstId;
typedef uint32_t PinId;
typedef uint32_t NetId;

static const uint32_t DB_INVALID_ID = UINT32_MAX;

typedef struct BBoxI32 {
	int32_t min_x;
	int32_t min_y;
	int32_t max_x;
	int32_t max_y;
} BBoxI32;

static inline BBoxI32 bbox_empty(void)
{
	BBoxI32 b;
	b.min_x = 0;
	b.min_y = 0;
	b.max_x = 0;
	b.max_y = 0;
	return b;
}

#endif
