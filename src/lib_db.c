#include "lib_db.h"

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
			if (fscale > 1000000) {
				break;
			}
		}
	}

	/* microns * dbu_per_micron */
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
lib_db_init(LibDb *db, Arena *arena)
{
	memset(db, 0, sizeof(*db));
	db->arena = arena;
	db->dbu_per_micron = 1000;
}

DbResult
lib_db_alloc(LibDb *db, uint32_t master_cap, uint32_t pin_cap)
{
	Arena *a = db->arena;
	DbResult r;

	db->master_cap = master_cap;
	db->pin_cap = pin_cap;

	r = alloc_cptr(a, &db->master_name, master_cap); if (r) return r;
	r = alloc_i32(a, &db->master_w, master_cap); if (r) return r;
	r = alloc_i32(a, &db->master_h, master_cap); if (r) return r;
	r = alloc_u32(a, &db->master_pin_offset, master_cap); if (r) return r;
	r = alloc_u32(a, &db->master_pin_count, master_cap); if (r) return r;

	r = alloc_cptr(a, &db->pin_name, pin_cap); if (r) return r;
	db->pin_master = (MasterId *)arena_alloc(a, sizeof(MasterId) * (size_t)pin_cap, _Alignof(MasterId));
	if (!db->pin_master) return DB_ERR_OOM;
	r = alloc_u8(a, &db->pin_dir, pin_cap); if (r) return r;
	r = alloc_i32(a, &db->pin_cx, pin_cap); if (r) return r;
	r = alloc_i32(a, &db->pin_cy, pin_cap); if (r) return r;

	lib_db_reset(db);
	return DB_OK;
}

void
lib_db_reset(LibDb *db)
{
	db->master_count = 0;
	db->pin_count = 0;
	db->dbu_per_micron = 1000;
}

static MasterId
lib_db_reserve_masters(LibDb *db, uint32_t n)
{
	uint32_t base = db->master_count;
	if (base + n > db->master_cap) {
		return (MasterId)DB_INVALID_ID;
	}
	db->master_count = base + n;
	return (MasterId)base;
}

static uint32_t
lib_db_reserve_pins(LibDb *db, uint32_t n)
{
	uint32_t base = db->pin_count;
	if (base + n > db->pin_cap) {
		return DB_INVALID_ID;
	}
	db->pin_count = base + n;
	return base;
}

static PinDir
pin_dir_from_tok(Tok t)
{
	if (tok_eq(t, "INPUT")) return PIN_DIR_INPUT;
	if (tok_eq(t, "OUTPUT")) return PIN_DIR_OUTPUT;
	if (tok_eq(t, "INOUT")) return PIN_DIR_INOUT;
	return PIN_DIR_UNKNOWN;
}

DbResult
lib_db_parse_lef_file(LibDb *db, const char *path)
{
	FILE *f;
	long fsz;
	char *buf;
	size_t rd;
	LefLex lx;
	Tok t;

	MasterId cur_master = (MasterId)DB_INVALID_ID;
	uint32_t cur_pin_off = DB_INVALID_ID;
	uint32_t cur_pin_count = 0;

	uint32_t cur_pin_idx = DB_INVALID_ID;
	PinDir cur_pin_dir = PIN_DIR_UNKNOWN;
	int32_t pin_min_x = 0, pin_min_y = 0, pin_max_x = 0, pin_max_y = 0;
	int pin_bbox_valid = 0;

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

		if (t.kind == TOK_IDENT && tok_eq(t, "MACRO")) {
			Tok name = lex_next(&lx);
			MasterId m = lib_db_reserve_masters(db, 1);
			if (m == (MasterId)DB_INVALID_ID) return DB_ERR_CAPACITY;
			db->master_name[m] = arena_strdup_range(db->arena, name.s, name.len);
			db->master_w[m] = 0;
			db->master_h[m] = 0;
			db->master_pin_offset[m] = 0;
			db->master_pin_count[m] = 0;

			cur_master = m;
			cur_pin_off = db->pin_count;
			cur_pin_count = 0;
			continue;
		}

		if (cur_master != (MasterId)DB_INVALID_ID) {
			if (t.kind == TOK_IDENT && tok_eq(t, "SIZE")) {
				Tok w = lex_next(&lx);
				(void)lex_next(&lx); /* BY */
				Tok h = lex_next(&lx);
				(void)lex_next(&lx); /* ; */
				if (w.kind == TOK_NUMBER && h.kind == TOK_NUMBER) {
					db->master_w[cur_master] = parse_dbu_from_number(db->dbu_per_micron, w);
					db->master_h[cur_master] = parse_dbu_from_number(db->dbu_per_micron, h);
				}
				continue;
			}

			if (t.kind == TOK_IDENT && tok_eq(t, "PIN")) {
				Tok pname = lex_next(&lx);
				uint32_t idx = lib_db_reserve_pins(db, 1);
				if (idx == DB_INVALID_ID) return DB_ERR_CAPACITY;
				db->pin_name[idx] = arena_strdup_range(db->arena, pname.s, pname.len);
				db->pin_master[idx] = cur_master;
				db->pin_dir[idx] = (uint8_t)PIN_DIR_UNKNOWN;
				db->pin_cx[idx] = 0;
				db->pin_cy[idx] = 0;

				cur_pin_idx = idx;
				cur_pin_dir = PIN_DIR_UNKNOWN;
				pin_bbox_valid = 0;
				cur_pin_count++;
				continue;
			}

			if (cur_pin_idx != DB_INVALID_ID) {
				if (t.kind == TOK_IDENT && tok_eq(t, "DIRECTION")) {
					Tok d = lex_next(&lx);
					cur_pin_dir = pin_dir_from_tok(d);
					db->pin_dir[cur_pin_idx] = (uint8_t)cur_pin_dir;
					/* consume to ';' */
					for (;;) {
						Tok s = lex_next(&lx);
						if (s.kind == TOK_EOF || s.kind == TOK_SEMI) break;
					}
					continue;
				}

				if (t.kind == TOK_IDENT && tok_eq(t, "RECT")) {
					Tok x1 = lex_next(&lx);
					Tok y1 = lex_next(&lx);
					Tok x2 = lex_next(&lx);
					Tok y2 = lex_next(&lx);
					(void)lex_next(&lx); /* ; */
					if (x1.kind == TOK_NUMBER && y1.kind == TOK_NUMBER &&
					    x2.kind == TOK_NUMBER && y2.kind == TOK_NUMBER) {
						int32_t ax1 = parse_dbu_from_number(db->dbu_per_micron, x1);
						int32_t ay1 = parse_dbu_from_number(db->dbu_per_micron, y1);
						int32_t ax2 = parse_dbu_from_number(db->dbu_per_micron, x2);
						int32_t ay2 = parse_dbu_from_number(db->dbu_per_micron, y2);
						if (!pin_bbox_valid) {
							pin_min_x = ax1; pin_max_x = ax2;
							pin_min_y = ay1; pin_max_y = ay2;
							pin_bbox_valid = 1;
						} else {
							if (ax1 < pin_min_x) pin_min_x = ax1;
							if (ay1 < pin_min_y) pin_min_y = ay1;
							if (ax2 > pin_max_x) pin_max_x = ax2;
							if (ay2 > pin_max_y) pin_max_y = ay2;
						}
					}
					continue;
				}

				if (t.kind == TOK_IDENT && tok_eq(t, "END")) {
					Tok endname = lex_next(&lx);
					if (endname.kind == TOK_IDENT) {
						/* END <pinname> */
						if (pin_bbox_valid) {
							db->pin_cx[cur_pin_idx] = (pin_min_x + pin_max_x) / 2;
							db->pin_cy[cur_pin_idx] = (pin_min_y + pin_max_y) / 2;
						}
						cur_pin_idx = DB_INVALID_ID;
						cur_pin_dir = PIN_DIR_UNKNOWN;
						pin_bbox_valid = 0;
						(void)endname;
						continue;
					}
				}
			}

			if (t.kind == TOK_IDENT && tok_eq(t, "END")) {
				Tok endname = lex_next(&lx);
				if (endname.kind == TOK_IDENT) {
					/* END <macroname> */
					if (cur_master != (MasterId)DB_INVALID_ID) {
						db->master_pin_offset[cur_master] = cur_pin_off;
						db->master_pin_count[cur_master] = cur_pin_count;
					}
					cur_master = (MasterId)DB_INVALID_ID;
					cur_pin_off = DB_INVALID_ID;
					cur_pin_count = 0;
					cur_pin_idx = DB_INVALID_ID;
					continue;
				}
			}
		}
	}

	return DB_OK;
}


