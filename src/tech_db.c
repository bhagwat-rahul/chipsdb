#include "tech_db.h"

#include <stdio.h>
#include <string.h>

typedef enum TokKind {
	TOK_EOF = 0,
	TOK_IDENT,
	TOK_NUMBER,
	TOK_SEMI, /* ; */
} TokKind;

typedef struct Tok {
	TokKind kind;
	const char *s;
	uint32_t len;
} Tok;

typedef struct LefLex {
	const char *at;
	const char *end;
} LefLex;

static int
is_space(char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int
is_alpha(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$';
}

static int
is_digit(char c)
{
	return c >= '0' && c <= '9';
}

static void
lex_skip_ws_and_comments(LefLex *lx)
{
	for (;;) {
		while (lx->at < lx->end && is_space(*lx->at)) {
			lx->at++;
		}
		if (lx->at < lx->end && *lx->at == '#') {
			while (lx->at < lx->end && *lx->at != '\n') {
				lx->at++;
			}
			continue;
		}
		break;
	}
}

static Tok
lex_next(LefLex *lx)
{
	Tok t;
	const char *s;

	lex_skip_ws_and_comments(lx);
	if (lx->at >= lx->end) {
		t.kind = TOK_EOF;
		t.s = lx->end;
		t.len = 0;
		return t;
	}

	if (*lx->at == ';') {
		t.kind = TOK_SEMI;
		t.s = lx->at;
		t.len = 1;
		lx->at++;
		return t;
	}

	s = lx->at;
	if (is_alpha(*lx->at)) {
		lx->at++;
		while (lx->at < lx->end && (is_alpha(*lx->at) || is_digit(*lx->at))) {
			lx->at++;
		}
		t.kind = TOK_IDENT;
		t.s = s;
		t.len = (uint32_t)(lx->at - s);
		return t;
	}

	if (*lx->at == '-' || *lx->at == '+' || is_digit(*lx->at) || *lx->at == '.') {
		int saw_digit = 0;
		if (*lx->at == '-' || *lx->at == '+') {
			lx->at++;
		}
		while (lx->at < lx->end && is_digit(*lx->at)) {
			saw_digit = 1;
			lx->at++;
		}
		if (lx->at < lx->end && *lx->at == '.') {
			lx->at++;
			while (lx->at < lx->end && is_digit(*lx->at)) {
				saw_digit = 1;
				lx->at++;
			}
		}
		if (saw_digit) {
			t.kind = TOK_NUMBER;
			t.s = s;
			t.len = (uint32_t)(lx->at - s);
			return t;
		}
	}

	/* Unknown char: skip it as a one-byte token-ish */
	lx->at++;
	t.kind = TOK_IDENT;
	t.s = s;
	t.len = 1;
	return t;
}

static int
tok_eq(Tok t, const char *lit)
{
	size_t n = strlen(lit);
	if (t.len != (uint32_t)n) return 0;
	return memcmp(t.s, lit, n) == 0;
}

static const char *
arena_strdup_range(Arena *a, const char *s, uint32_t len)
{
	char *p = (char *)arena_alloc(a, (size_t)len + 1, _Alignof(char));
	uint32_t i;
	if (!p) return 0;
	for (i = 0; i < len; i++) {
		p[i] = s[i];
	}
	p[len] = 0;
	return p;
}

static int32_t
parse_i32(const char *s, uint32_t len)
{
	int neg = 0;
	uint32_t i = 0;
	int32_t x = 0;
	if (i < len && (s[i] == '-' || s[i] == '+')) {
		neg = (s[i] == '-');
		i++;
	}
	for (; i < len; i++) {
		char c = s[i];
		if (!is_digit(c)) break;
		x = x * 10 + (int32_t)(c - '0');
	}
	return neg ? -x : x;
}

static int32_t
parse_dbu_from_number(int32_t dbu_per_micron, Tok num)
{
	/* Convert decimal microns into integer dbu (rounded). */
	int neg = 0;
	uint32_t i = 0;
	int64_t ip = 0;
	int64_t fp = 0;
	int64_t fscale = 1;

	if (i < num.len && (num.s[i] == '-' || num.s[i] == '+')) {
		neg = (num.s[i] == '-');
		i++;
	}

	for (; i < num.len; i++) {
		char c = num.s[i];
		if (!is_digit(c)) break;
		ip = ip * 10 + (int64_t)(c - '0');
	}

	if (i < num.len && num.s[i] == '.') {
		i++;
		for (; i < num.len; i++) {
			char c = num.s[i];
			if (!is_digit(c)) break;
			fp = fp * 10 + (int64_t)(c - '0');
			fscale *= 10;
			if (fscale > 1000000) break;
		}
	}

	{
		int64_t v = ip * (int64_t)dbu_per_micron;
		if (fscale != 1) {
			int64_t fv = fp * (int64_t)dbu_per_micron;
			v += (fv + (fscale / 2)) / fscale;
		}
		if (neg) v = -v;
		if (v > INT32_MAX) v = INT32_MAX;
		if (v < INT32_MIN) v = INT32_MIN;
		return (int32_t)v;
	}
}

static DbResult
alloc_u32(Arena *a, uint32_t **out, uint32_t count)
{
	uint32_t *p = (uint32_t *)arena_alloc(a, sizeof(uint32_t) * (size_t)count, _Alignof(uint32_t));
	if (!p) return DB_ERR_OOM;
	*out = p;
	return DB_OK;
}

static DbResult
alloc_i32(Arena *a, int32_t **out, uint32_t count)
{
	int32_t *p = (int32_t *)arena_alloc(a, sizeof(int32_t) * (size_t)count, _Alignof(int32_t));
	if (!p) return DB_ERR_OOM;
	*out = p;
	return DB_OK;
}

static DbResult
alloc_u8(Arena *a, uint8_t **out, uint32_t count)
{
	uint8_t *p = (uint8_t *)arena_alloc(a, sizeof(uint8_t) * (size_t)count, _Alignof(uint8_t));
	if (!p) return DB_ERR_OOM;
	*out = p;
	return DB_OK;
}

static DbResult
alloc_cptr(Arena *a, const char ***out, uint32_t count)
{
	const char **p = (const char **)arena_alloc(a, sizeof(const char *) * (size_t)count, _Alignof(const char *));
	if (!p) return DB_ERR_OOM;
	*out = p;
	return DB_OK;
}

void
tech_db_init(TechDb *db, Arena *arena)
{
	memset(db, 0, sizeof(*db));
	db->arena = arena;
	db->dbu_per_micron = 1000;
}

DbResult
tech_db_alloc(TechDb *db, uint32_t layer_cap, uint32_t track_cap)
{
	Arena *a = db->arena;
	DbResult r;

	db->layer_cap = layer_cap;
	db->track_cap = track_cap;

	r = alloc_cptr(a, &db->layer_name, layer_cap); if (r) return r;
	r = alloc_u8(a, &db->layer_dir, layer_cap); if (r) return r;
	r = alloc_i32(a, &db->layer_pitch, layer_cap); if (r) return r;
	r = alloc_i32(a, &db->layer_width, layer_cap); if (r) return r;
	r = alloc_i32(a, &db->layer_spacing, layer_cap); if (r) return r;

	r = alloc_u8(a, &db->track_dir, track_cap); if (r) return r;
	r = alloc_i32(a, &db->track_start, track_cap); if (r) return r;
	r = alloc_i32(a, &db->track_pitch, track_cap); if (r) return r;
	r = alloc_u32(a, &db->track_count_n, track_cap); if (r) return r;
	r = alloc_cptr(a, &db->track_layer, track_cap); if (r) return r;

	tech_db_reset(db);
	return DB_OK;
}

void
tech_db_reset(TechDb *db)
{
	db->layer_count = 0;
	db->track_count = 0;
	db->dbu_per_micron = 1000;
}

static LayerId
tech_db_reserve_layers(TechDb *db, uint32_t n)
{
	uint32_t base = db->layer_count;
	if (base + n > db->layer_cap) return (LayerId)DB_INVALID_ID;
	db->layer_count = base + n;
	return (LayerId)base;
}

static uint32_t
tech_db_reserve_tracks(TechDb *db, uint32_t n)
{
	uint32_t base = db->track_count;
	if (base + n > db->track_cap) return DB_INVALID_ID;
	db->track_count = base + n;
	return base;
}

static LayerDir
layer_dir_from_tok(Tok t)
{
	if (tok_eq(t, "HORIZONTAL")) return LAYER_DIR_HORIZONTAL;
	if (tok_eq(t, "VERTICAL")) return LAYER_DIR_VERTICAL;
	return LAYER_DIR_UNKNOWN;
}

static TrackDir
track_dir_from_tok(Tok t)
{
	if (tok_eq(t, "X")) return TRACK_DIR_X;
	if (tok_eq(t, "Y")) return TRACK_DIR_Y;
	return TRACK_DIR_UNKNOWN;
}

DbResult
tech_db_parse_tech_lef_file(TechDb *db, const char *path)
{
	FILE *f;
	long fsz;
	char *buf;
	size_t rd;
	LefLex lx;
	Tok t;

	LayerId cur_layer = (LayerId)DB_INVALID_ID;
	const char *cur_layer_name = 0;

	f = fopen(path, "rb");
	if (!f) return DB_ERR_INVALID_INPUT;
	if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return DB_ERR_INVALID_INPUT; }
	fsz = ftell(f);
	if (fsz < 0) { fclose(f); return DB_ERR_INVALID_INPUT; }
	if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return DB_ERR_INVALID_INPUT; }

	buf = (char *)arena_alloc(db->arena, (size_t)fsz + 1, _Alignof(char));
	if (!buf) { fclose(f); return DB_ERR_OOM; }
	rd = fread(buf, 1, (size_t)fsz, f);
	fclose(f);
	if (rd != (size_t)fsz) return DB_ERR_INVALID_INPUT;
	buf[fsz] = 0;

	lx.at = buf;
	lx.end = buf + (size_t)fsz;

	for (;;) {
		t = lex_next(&lx);
		if (t.kind == TOK_EOF) break;

		if (t.kind == TOK_IDENT && tok_eq(t, "UNITS")) {
			/* UNITS ... MICRONS <int> ; */
			Tok u;
			for (;;) {
				u = lex_next(&lx);
				if (u.kind == TOK_EOF) break;
				if (u.kind == TOK_IDENT && tok_eq(u, "MICRONS")) {
					Tok n = lex_next(&lx);
					if (n.kind == TOK_NUMBER) {
						int32_t v = parse_i32(n.s, n.len);
						if (v > 0) db->dbu_per_micron = v;
					}
				}
				if (u.kind == TOK_SEMI) break;
			}
			continue;
		}

		if (t.kind == TOK_IDENT && tok_eq(t, "LAYER")) {
			Tok name = lex_next(&lx);
			LayerId lid = tech_db_reserve_layers(db, 1);
			if (lid == (LayerId)DB_INVALID_ID) return DB_ERR_CAPACITY;

			cur_layer = lid;
			cur_layer_name = arena_strdup_range(db->arena, name.s, name.len);

			db->layer_name[lid] = cur_layer_name;
			db->layer_dir[lid] = (uint8_t)LAYER_DIR_UNKNOWN;
			db->layer_pitch[lid] = 0;
			db->layer_width[lid] = 0;
			db->layer_spacing[lid] = 0;
			continue;
		}

		if (cur_layer != (LayerId)DB_INVALID_ID) {
			if (t.kind == TOK_IDENT && tok_eq(t, "TYPE")) {
				Tok v = lex_next(&lx);
				/* Only keep routing layers for now */
				if (!tok_eq(v, "ROUTING")) {
					cur_layer = (LayerId)DB_INVALID_ID;
					cur_layer_name = 0;
				}
				for (;;) {
					Tok s = lex_next(&lx);
					if (s.kind == TOK_EOF || s.kind == TOK_SEMI) break;
				}
				continue;
			}

			if (t.kind == TOK_IDENT && tok_eq(t, "DIRECTION")) {
				Tok v = lex_next(&lx);
				db->layer_dir[cur_layer] = (uint8_t)layer_dir_from_tok(v);
				for (;;) {
					Tok s = lex_next(&lx);
					if (s.kind == TOK_EOF || s.kind == TOK_SEMI) break;
				}
				continue;
			}

			if (t.kind == TOK_IDENT && tok_eq(t, "PITCH")) {
				Tok v = lex_next(&lx);
				db->layer_pitch[cur_layer] = parse_dbu_from_number(db->dbu_per_micron, v);
				for (;;) {
					Tok s = lex_next(&lx);
					if (s.kind == TOK_EOF || s.kind == TOK_SEMI) break;
				}
				continue;
			}

			if (t.kind == TOK_IDENT && tok_eq(t, "WIDTH")) {
				Tok v = lex_next(&lx);
				db->layer_width[cur_layer] = parse_dbu_from_number(db->dbu_per_micron, v);
				for (;;) {
					Tok s = lex_next(&lx);
					if (s.kind == TOK_EOF || s.kind == TOK_SEMI) break;
				}
				continue;
			}

			if (t.kind == TOK_IDENT && tok_eq(t, "SPACING")) {
				Tok v = lex_next(&lx);
				db->layer_spacing[cur_layer] = parse_dbu_from_number(db->dbu_per_micron, v);
				for (;;) {
					Tok s = lex_next(&lx);
					if (s.kind == TOK_EOF || s.kind == TOK_SEMI) break;
				}
				continue;
			}

			if (t.kind == TOK_IDENT && tok_eq(t, "END")) {
				Tok n = lex_next(&lx);
				(void)n;
				cur_layer = (LayerId)DB_INVALID_ID;
				cur_layer_name = 0;
				continue;
			}
		}

		if (t.kind == TOK_IDENT && tok_eq(t, "TRACKS")) {
			/* TRACKS X|Y <start> DO <n> STEP <pitch> LAYER <name> ; */
			Tok dir = lex_next(&lx);
			Tok start = lex_next(&lx);
			(void)lex_next(&lx); /* DO */
			Tok n = lex_next(&lx);
			(void)lex_next(&lx); /* STEP */
			Tok pitch = lex_next(&lx);

			for (;;) {
				Tok k = lex_next(&lx);
				if (k.kind == TOK_EOF) break;
				if (k.kind == TOK_IDENT && tok_eq(k, "LAYER")) {
					Tok lname = lex_next(&lx);
					uint32_t idx = tech_db_reserve_tracks(db, 1);
					if (idx == DB_INVALID_ID) return DB_ERR_CAPACITY;
					db->track_dir[idx] = (uint8_t)track_dir_from_tok(dir);
					db->track_start[idx] = parse_dbu_from_number(db->dbu_per_micron, start);
					db->track_pitch[idx] = parse_dbu_from_number(db->dbu_per_micron, pitch);
					db->track_count_n[idx] = (uint32_t)parse_i32(n.s, n.len);
					db->track_layer[idx] = arena_strdup_range(db->arena, lname.s, lname.len);
				}
				if (k.kind == TOK_SEMI) break;
			}
			continue;
		}
	}

	return DB_OK;
}
