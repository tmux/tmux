/* $OpenBSD$ */

/*
 * Copyright (c) 2010 Nicholas Marriott <nicholas.marriott@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

#define LAYOUT_STRING_MAX 8192
#define LAYOUT_DEPTH_MAX 64

enum layout_format {
	LAYOUT_FORMAT_UNKNOWN,
	LAYOUT_FORMAT_LEGACY,
	LAYOUT_FORMAT
};

struct layout_parse_ctx {
	const char		*ptr;
	const char		*end;
	enum layout_format	 format;
	u_int			 depth;
	u_int			 cells;
};

struct layout_prepared {
	struct layout_cell	*root;
	struct layout_cell	*zoomed;
	enum layout_format	 format;
	u_int			 ncells;
	int			 pane_ids;
};

static struct layout_cell	*layout_find_bottomright(struct layout_cell *);
static u_short			 layout_checksum(const char *, const char *);
static int			 layout_append(struct window *,
				     struct layout_cell *, char *, size_t);
static int			 layout_append_legacy(struct layout_cell *, char *,
				     size_t);
static int			 layout_append_geometry(char *, size_t, u_int,
				     u_int, int, int);
static int			 layout_check(struct layout_cell *);
static int			 layout_check_geometry(u_int, u_int, int, int);
static int			 layout_parse_char(struct layout_parse_ctx *, char);
static int			 layout_construct(struct layout_parse_ctx *,
				     struct layout_cell *, struct layout_cell **);
static void			 layout_assign(struct window *, struct window_pane **,
				     struct layout_cell *, int);
static struct layout_cell	*layout_find_zindex(struct layout_cell *,
				     u_int);
static void			 layout_compact_zindexes(struct layout_cell *,
				     u_int);

/* Find the bottom-right cell. */
static struct layout_cell *
layout_find_bottomright(struct layout_cell *lc)
{
	if (lc->type == LAYOUT_WINDOWPANE)
		return (lc);
	lc = TAILQ_LAST(&lc->cells, layout_cells);
	return (layout_find_bottomright(lc));
}

/* Calculate layout checksum. */
static u_short
layout_checksum(const char *start, const char *end)
{
	u_short	csum = 0;

	for (; start != end; start++) {
		if (isspace((u_char)*start))
			continue;
		csum = (csum >> 1) + ((csum & 1) << 15);
		csum += *start;
	}
	return (csum);
}

/* Find the absolute position of a pane in the z-index list. */
static int
layout_absolute_zindex(struct window *w, struct window_pane *wp, u_int *z)
{
	struct window_pane	*loop;

	*z = 0;
	TAILQ_FOREACH(loop, &w->z_index, zentry) {
		if (loop == wp)
			return (0);
		(*z)++;
	}
	return (-1);
}

/* Format geometry using X-style explicit absolute offsets. */
static int
layout_append_geometry(char *buf, size_t len, u_int sx, u_int sy, int xoff,
    int yoff)
{
	unsigned long long	 x, y;
	const char		*xs = "+", *ys = "+";

	if (xoff < 0) {
		xs = "+-";
		x = -(long long)xoff;
	} else
		x = xoff;
	if (yoff < 0) {
		ys = "+-";
		y = -(long long)yoff;
	} else
		y = yoff;
	return (xsnprintf(buf, len, "%ux%u%s%llu%s%llu", sx, sy, xs, x, ys, y));
}

/* Dump layout as a string. */
char *
layout_dump(struct window *w, struct layout_cell *root)
{
	char	 layout[LAYOUT_STRING_MAX], *out;

	*layout = '\0';
	if (layout_append(w, root, layout, sizeof layout) != 0)
		return (NULL);

	xasprintf(&out, "%04hx,%s",
	    layout_checksum(layout, layout + strlen(layout)), layout);
	return (out);
}

/* Dump layout using the legacy syntax. */
char *
layout_dump_legacy(struct layout_cell *root)
{
	char	 layout[LAYOUT_STRING_MAX], *out;

	*layout = '\0';
	if (layout_append_legacy(root, layout, sizeof layout) != 0)
		return (NULL);

	xasprintf(&out, "%04hx,%s",
	    layout_checksum(layout, layout + strlen(layout)), layout);
	return (out);
}

/* Append information for a single cell using the legacy syntax. */
static int
layout_append_legacy(struct layout_cell *lc, char *buf, size_t len)
{
	struct layout_cell	*lcchild;
	char			 tmp[96];
	const char		*brackets = "][";
	size_t			 tmplen;
	u_int			 sx, sy;
	int			 xoff, yoff;

	if (len == 0)
		return (-1);
	if (lc == NULL)
		return (0);

	sx = lc->sx;
	sy = lc->sy;
	xoff = lc->xoff;
	yoff = lc->yoff;
	if (lc->flags & LAYOUT_CELL_HIDDEN) {
		if (lc->saved_sx != UINT_MAX)
			sx = lc->saved_sx;
		if (lc->saved_sy != UINT_MAX)
			sy = lc->saved_sy;
		if (lc->saved_xoff != INT_MAX)
			xoff = lc->saved_xoff;
		if (lc->saved_yoff != INT_MAX)
			yoff = lc->saved_yoff;
	}

	if (lc->wp != NULL) {
		tmplen = xsnprintf(tmp, sizeof tmp, "%ux%u,%d,%d,%u", sx, sy,
		    xoff, yoff, lc->wp->id);
	} else {
		tmplen = xsnprintf(tmp, sizeof tmp, "%ux%u,%d,%d", sx, sy,
		    xoff, yoff);
	}
	if (tmplen > (sizeof tmp) - 1 || strlcat(buf, tmp, len) >= len)
		return (-1);

	switch (lc->type) {
	case LAYOUT_LEFTRIGHT:
		brackets = "}{";
		/* FALLTHROUGH */
	case LAYOUT_TOPBOTTOM:
		if (strlcat(buf, &brackets[1], len) >= len)
			return (-1);
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (layout_append_legacy(lcchild, buf, len) != 0)
				return (-1);
			if (strlcat(buf, ",", len) >= len)
				return (-1);
		}
		buf[strlen(buf) - 1] = brackets[0];
		break;
	case LAYOUT_WINDOWPANE:
		break;
	}
	return (0);
}

/* Append information for a single cell. */
static int
layout_append(struct window *w, struct layout_cell *lc, char *buf, size_t len)
{
	struct layout_cell	*lcchild;
	char			 flags[4], geometry[64], tmp[96];
	const char		*brackets = "][";
	size_t			 tmplen;
	u_int			 sx, sy, z;
	int			 xoff, yoff, pos = 0;

	if (len == 0)
		return (-1);
	if (lc == NULL)
		return (0);

	sx = lc->sx;
	sy = lc->sy;
	xoff = lc->xoff;
	yoff = lc->yoff;
	if (lc->flags & LAYOUT_CELL_HIDDEN) {
		if (lc->saved_sx != UINT_MAX)
			sx = lc->saved_sx;
		if (lc->saved_sy != UINT_MAX)
			sy = lc->saved_sy;
		if (lc->saved_xoff != INT_MAX)
			xoff = lc->saved_xoff;
		if (lc->saved_yoff != INT_MAX)
			yoff = lc->saved_yoff;
	}

	if (lc->wp != NULL) {
		if (layout_absolute_zindex(w, lc->wp, &z) != 0)
			return (-1);
		if (lc->flags & LAYOUT_CELL_FLOATING)
			flags[pos++] = 'f';
		if (lc->flags & LAYOUT_CELL_HIDDEN)
			flags[pos++] = 'h';
		if (lc->wp->flags & PANE_ZOOMED)
			flags[pos++] = 'z';
		flags[pos] = '\0';
		if (layout_append_geometry(geometry, sizeof geometry, sx, sy,
		    xoff, yoff) >= (int)sizeof geometry)
			return (-1);

		if (pos != 0) {
			tmplen = xsnprintf(tmp, sizeof tmp,
			    "%%%u,%u:%s:%s", lc->wp->id, z, geometry, flags);
		} else {
			tmplen = xsnprintf(tmp, sizeof tmp,
			    "%%%u,%u:%s", lc->wp->id, z, geometry);
		}
	} else {
		tmplen = layout_append_geometry(tmp, sizeof tmp, sx, sy,
		    xoff, yoff);
	}
	if (tmplen > (sizeof tmp) - 1)
		return (-1);
	if (strlcat(buf, tmp, len) >= len)
		return (-1);

	switch (lc->type) {
	case LAYOUT_LEFTRIGHT:
		brackets = "}{";
		/* FALLTHROUGH */
	case LAYOUT_TOPBOTTOM:
		if (strlcat(buf, &brackets[1], len) >= len)
			return (-1);
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (layout_append(w, lcchild, buf, len) != 0)
				return (-1);
			if (strlcat(buf, ";", len) >= len)
				return (-1);
		}
		buf[strlen(buf) - 1] = brackets[0];
		break;
	case LAYOUT_WINDOWPANE:
		break;
	}

	return (0);
}

/* Check tiled layout sizes fit, ignoring floating subtrees. */
static int
layout_check(struct layout_cell *lc)
{
	struct layout_cell	*lcchild;
	unsigned long long	 n = 0;
	u_int			 children = 0;

	switch (lc->type) {
	case LAYOUT_WINDOWPANE:
		return (1);
	case LAYOUT_LEFTRIGHT:
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (!layout_has_tiled(lcchild))
				continue;
			if (lcchild->sy != lc->sy || !layout_check(lcchild))
				return (0);
			n += lcchild->sx + 1;
			children++;
		}
		if (children != 0 && n - 1 != lc->sx)
			return (0);
		break;
	case LAYOUT_TOPBOTTOM:
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (!layout_has_tiled(lcchild))
				continue;
			if (lcchild->sx != lc->sx || !layout_check(lcchild))
				return (0);
			n += lcchild->sy + 1;
			children++;
		}
		if (children != 0 && n - 1 != lc->sy)
			return (0);
		break;
	}
	return (1);
}

/* Check geometry is within the ranges used by the layout and drawing code. */
static int
layout_check_geometry(u_int sx, u_int sy, int xoff, int yoff)
{
	if (sx < PANE_MINIMUM || sx > PANE_MAXIMUM ||
	    sy < PANE_MINIMUM || sy > PANE_MAXIMUM)
		return (0);
	if (xoff < -PANE_MAXIMUM || xoff > PANE_MAXIMUM ||
	    yoff < -PANE_MAXIMUM || yoff > PANE_MAXIMUM)
		return (0);
	return (1);
}

/* Correct the top cell size accepted from some older tmux versions. */
static int
layout_fix_legacy_root(struct layout_cell *root)
{
	struct layout_cell	*lcchild;
	unsigned long long	 sx = 0, sy = 0;

	if (root->type == LAYOUT_WINDOWPANE)
		return (0);
	if (root->type == LAYOUT_LEFTRIGHT) {
		TAILQ_FOREACH(lcchild, &root->cells, entry) {
			sy = lcchild->sy + 1;
			sx += lcchild->sx + 1;
		}
	} else {
		TAILQ_FOREACH(lcchild, &root->cells, entry) {
			sx = lcchild->sx + 1;
			sy += lcchild->sy + 1;
		}
	}
	if (sx != 0 && sy != 0 &&
	    (root->sx != sx - 1 || root->sy != sy - 1)) {
		if (sx - 1 > PANE_MAXIMUM || sy - 1 > PANE_MAXIMUM)
			return (-1);
		root->sx = sx - 1;
		root->sy = sy - 1;
	}
	return (0);
}

/* Set and validate the format of the complete layout. */
static int
layout_set_format(struct layout_parse_ctx *ctx, enum layout_format format)
{
	if (ctx->format == LAYOUT_FORMAT_UNKNOWN)
		ctx->format = format;
	return (ctx->format == format ? 0 : -1);
}

/* Skip whitespace between layout tokens. */
static void
layout_skip_space(struct layout_parse_ctx *ctx)
{
	while (ctx->ptr != ctx->end && isspace((u_char)*ctx->ptr))
		ctx->ptr++;
}

/* Parse an unsigned decimal integer. */
static int
layout_parse_uint(struct layout_parse_ctx *ctx, u_int *value)
{
	unsigned long long	 n = 0;
	const char		*start = ctx->ptr;
	u_int			 digit;

	layout_skip_space(ctx);
	start = ctx->ptr;
	while (ctx->ptr != ctx->end && isdigit((u_char)*ctx->ptr)) {
		digit = *ctx->ptr - '0';
		if (n > (UINT_MAX - digit) / 10)
			return (-1);
		n = n * 10 + digit;
		ctx->ptr++;
	}
	if (ctx->ptr == start)
		return (-1);
	*value = n;
	return (0);
}

/* Parse a signed decimal integer. */
static int
layout_parse_int(struct layout_parse_ctx *ctx, int *value)
{
	unsigned long long	 n = 0, limit;
	const char		*start;
	int			 digit, negative = 0;

	layout_skip_space(ctx);
	if (ctx->ptr != ctx->end && *ctx->ptr == '-') {
		negative = 1;
		ctx->ptr++;
	}
	start = ctx->ptr;
	limit = negative ? (unsigned long long)INT_MAX + 1 : INT_MAX;
	while (ctx->ptr != ctx->end && isdigit((u_char)*ctx->ptr)) {
		digit = *ctx->ptr - '0';
		if (n > (limit - digit) / 10)
			return (-1);
		n = n * 10 + digit;
		ctx->ptr++;
	}
	if (ctx->ptr == start)
		return (-1);
	if (negative && n == (unsigned long long)INT_MAX + 1)
		*value = INT_MIN;
	else if (negative)
		*value = -(int)n;
	else
		*value = n;
	return (0);
}

/* Parse one X-style position. Return 0 if it is omitted. */
static int
layout_parse_position(struct layout_parse_ctx *ctx, int *value,
    int *relative)
{
	unsigned long long	 n = 0, limit;
	const char		*start;
	int			 absolute_negative = 0, digit;
	char			 sign;

	layout_skip_space(ctx);
	if (ctx->ptr == ctx->end ||
	    (*ctx->ptr != '+' && *ctx->ptr != '-'))
		return (0);
	sign = *ctx->ptr++;
	*relative = (sign == '-');
	if (sign == '+' && ctx->ptr != ctx->end) {
		if (*ctx->ptr == '+')
			ctx->ptr++;
		else if (*ctx->ptr == '-') {
			absolute_negative = 1;
			ctx->ptr++;
		}
	}
	start = ctx->ptr;
	limit = absolute_negative ? (unsigned long long)INT_MAX + 1 : INT_MAX;
	while (ctx->ptr != ctx->end && isdigit((u_char)*ctx->ptr)) {
		digit = *ctx->ptr - '0';
		if (n > (limit - digit) / 10)
			return (-1);
		n = n * 10 + digit;
		ctx->ptr++;
	}
	if (ctx->ptr == start)
		return (-1);
	if (absolute_negative && n == (unsigned long long)INT_MAX + 1)
		*value = INT_MIN;
	else if (absolute_negative)
		*value = -(int)n;
	else
		*value = n;
	return (1);
}

/* Parse legacy comma geometry or current X-style geometry. */
static int
layout_parse_geometry(struct layout_parse_ctx *ctx, int is_pane, u_int *sxp,
    u_int *syp, int *xoffp, int *yoffp, int *xrelative, int *yrelative)
{
	int	has_x, has_y;

	*xrelative = *yrelative = 0;
	if (layout_parse_uint(ctx, sxp) != 0 ||
	    layout_parse_char(ctx, 'x') != 0 ||
	    layout_parse_uint(ctx, syp) != 0)
		return (-1);
	layout_skip_space(ctx);
	if (!is_pane && ctx->ptr != ctx->end && *ctx->ptr == ',') {
		if (layout_set_format(ctx, LAYOUT_FORMAT_LEGACY) != 0 ||
		    layout_parse_char(ctx, ',') != 0 ||
		    layout_parse_int(ctx, xoffp) != 0 ||
		    layout_parse_char(ctx, ',') != 0 ||
		    layout_parse_int(ctx, yoffp) != 0)
			return (-1);
		if (!layout_check_geometry(*sxp, *syp, *xoffp, *yoffp))
			return (-1);
		return (0);
	}
	if (layout_set_format(ctx, LAYOUT_FORMAT) != 0)
		return (-1);
	*xoffp = *yoffp = 0;
	has_x = layout_parse_position(ctx, xoffp, xrelative);
	if (has_x < 0)
		return (-1);
	if (has_x == 0) {
		if (!layout_check_geometry(*sxp, *syp, *xoffp, *yoffp))
			return (-1);
		return (0);
	}
	has_y = layout_parse_position(ctx, yoffp, yrelative);
	if (has_y < 0)
		return (-1);
	if (!layout_check_geometry(*sxp, *syp, *xoffp, *yoffp))
		return (-1);
	return (0);
}

/* Consume one required character. */
static int
layout_parse_char(struct layout_parse_ctx *ctx, char ch)
{
	layout_skip_space(ctx);
	if (ctx->ptr == ctx->end || *ctx->ptr != ch)
		return (-1);
	ctx->ptr++;
	return (0);
}

/* Parse an optional four-digit checksum prefix. */
static int
layout_parse_checksum(struct layout_parse_ctx *ctx, u_short *csum)
{
	u_int	 n = 0;
	size_t	 i;
	int	 digit;

	layout_skip_space(ctx);
	if ((size_t)(ctx->end - ctx->ptr) < 5 || ctx->ptr[4] != ',')
		return (0);
	for (i = 0; i < 4; i++) {
		if (!isxdigit((u_char)ctx->ptr[i]))
			return (0);
		if (isdigit((u_char)ctx->ptr[i]))
			digit = ctx->ptr[i] - '0';
		else
			digit = tolower((u_char)ctx->ptr[i]) - 'a' + 10;
		n = (n << 4) | digit;
	}
	ctx->ptr += 5;
	layout_skip_space(ctx);
	*csum = n;
	return (1);
}

/* Construct one cell, accepting an optional checksum before nested cells. */
static int
layout_construct(struct layout_parse_ctx *ctx, struct layout_cell *parent,
    struct layout_cell **lcp)
{
	struct layout_cell	*lc = NULL, *lcchild;
	const char		*body, *saved;
	u_int			 sx, sy, id, z;
	u_short			 csum = 0;
	int			 is_pane, xoff, yoff, xrelative, yrelative;
	int			 has_checksum;
	int			 have_flags[3] = { 0 };
	char			 open, close, separator;

	*lcp = NULL;
	if (ctx->depth >= LAYOUT_DEPTH_MAX || ctx->cells >= LAYOUT_STRING_MAX)
		return (-1);
	ctx->depth++;
	ctx->cells++;

	has_checksum = layout_parse_checksum(ctx, &csum);
	body = ctx->ptr;
	layout_skip_space(ctx);
	is_pane = (ctx->ptr != ctx->end && *ctx->ptr == '%');
	if (is_pane) {
		ctx->ptr++;
		if (layout_parse_uint(ctx, &id) != 0 ||
		    layout_parse_char(ctx, ',') != 0 ||
		    layout_parse_uint(ctx, &z) != 0 ||
		    layout_parse_char(ctx, ':') != 0)
			goto fail;
	}
	if (layout_parse_geometry(ctx, is_pane, &sx, &sy, &xoff, &yoff,
	    &xrelative, &yrelative) != 0)
		goto fail;

	lc = layout_create_cell(parent);
	layout_set_size(lc, sx, sy, xoff, yoff);
	if (xrelative)
		lc->flags |= LAYOUT_CELL_X_RELATIVE;
	if (yrelative)
		lc->flags |= LAYOUT_CELL_Y_RELATIVE;
	if (!is_pane && (xrelative || yrelative))
		goto fail;
	layout_skip_space(ctx);

	if (is_pane) {
		lc->pane_id = id;
		lc->z_index = z;
		layout_skip_space(ctx);
		if (ctx->ptr != ctx->end && *ctx->ptr == ':') {
			ctx->ptr++;
			layout_skip_space(ctx);
			if (ctx->ptr == ctx->end ||
			    (*ctx->ptr != 'f' && *ctx->ptr != 'h' &&
			    *ctx->ptr != 'z'))
				goto fail;
			while (ctx->ptr != ctx->end) {
				if (*ctx->ptr == 'f') {
					if (have_flags[0]++)
						goto fail;
					lc->flags |= LAYOUT_CELL_FLOATING;
				} else if (*ctx->ptr == 'h') {
					if (have_flags[1]++)
						goto fail;
					lc->flags |= LAYOUT_CELL_HIDDEN;
				} else if (*ctx->ptr == 'z') {
					if (have_flags[2]++)
						goto fail;
					lc->flags |= LAYOUT_CELL_ZOOMED;
				} else
					break;
				ctx->ptr++;
			}
			if (have_flags[1] && have_flags[2])
				goto fail;
		}
		if ((lc->flags & (LAYOUT_CELL_X_RELATIVE|
		    LAYOUT_CELL_Y_RELATIVE)) &&
		    (lc->flags & LAYOUT_CELL_FLOATING) == 0)
			goto fail;
		if (lc->flags & LAYOUT_CELL_HIDDEN) {
			lc->saved_sx = lc->sx;
			lc->saved_sy = lc->sy;
			lc->saved_xoff = lc->xoff;
			lc->saved_yoff = lc->yoff;
		}
	} else if (ctx->ptr != ctx->end &&
	    (*ctx->ptr == '{' || *ctx->ptr == '[')) {
		open = *ctx->ptr++;
		close = (open == '{' ? '}' : ']');
		lc->type = (open == '{' ? LAYOUT_LEFTRIGHT : LAYOUT_TOPBOTTOM);
		layout_skip_space(ctx);
		if (ctx->ptr == ctx->end || *ctx->ptr == close)
			goto fail;
		for (;;) {
			if (layout_construct(ctx, lc, &lcchild) != 0)
				goto fail;
			TAILQ_INSERT_TAIL(&lc->cells, lcchild, entry);
			layout_skip_space(ctx);
			if (ctx->ptr == ctx->end)
				goto fail;
			if (*ctx->ptr == close) {
				ctx->ptr++;
				break;
			}
			separator = *ctx->ptr++;
			if (separator == ';') {
				if (layout_set_format(ctx, LAYOUT_FORMAT) != 0)
					goto fail;
			} else if (separator == ',') {
				if (layout_set_format(ctx, LAYOUT_FORMAT_LEGACY) != 0)
					goto fail;
			} else
				goto fail;
		}
	} else {
		if (layout_set_format(ctx, LAYOUT_FORMAT_LEGACY) != 0)
			goto fail;
		layout_skip_space(ctx);
		if (ctx->ptr != ctx->end && *ctx->ptr == ',') {
			saved = ctx->ptr++;
			if (layout_parse_uint(ctx, &id) != 0) {
				ctx->ptr = saved;
			} else {
				layout_skip_space(ctx);
				if (ctx->ptr != ctx->end && *ctx->ptr == 'x')
					ctx->ptr = saved;
				else
					lc->pane_id = id;
			}
		}
	}

	if (has_checksum && csum != layout_checksum(body, ctx->ptr))
		goto fail;
	ctx->depth--;
	*lcp = lc;
	return (0);

fail:
	ctx->depth--;
	layout_free_cell(lc, 0);
	return (-1);
}

/* Resolve right and bottom offsets against the complete window geometry. */
static int
layout_resolve_relative(struct layout_cell *lc, u_int sx, u_int sy)
{
	struct layout_cell	*lcchild;
	long long		 resolved;

	if (lc->type != LAYOUT_WINDOWPANE) {
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (layout_resolve_relative(lcchild, sx, sy) != 0)
				return (-1);
		}
		return (0);
	}
	if (lc->flags & LAYOUT_CELL_X_RELATIVE) {
		resolved = (long long)sx - lc->sx - lc->xoff;
		if (resolved < INT_MIN || resolved > INT_MAX)
			return (-1);
		lc->xoff = resolved;
		lc->flags &= ~LAYOUT_CELL_X_RELATIVE;
	}
	if (lc->flags & LAYOUT_CELL_Y_RELATIVE) {
		resolved = (long long)sy - lc->sy - lc->yoff;
		if (resolved < INT_MIN || resolved > INT_MAX)
			return (-1);
		lc->yoff = resolved;
		lc->flags &= ~LAYOUT_CELL_Y_RELATIVE;
	}
	if (lc->flags & LAYOUT_CELL_HIDDEN) {
		lc->saved_sx = lc->sx;
		lc->saved_sy = lc->sy;
		lc->saved_xoff = lc->xoff;
		lc->saved_yoff = lc->yoff;
	}
	if (!layout_check_geometry(lc->sx, lc->sy, lc->xoff, lc->yoff))
		return (-1);
	return (0);
}

/* Count zoomed cells and validate z-indexes and floating ordering. */
static int
layout_validate_new(struct layout_cell *root, u_int ncells,
    struct layout_cell **zoomed)
{
	struct layout_cell	*lc;
	u_int			 z;
	int			 seen_tiled = 0;

	*zoomed = NULL;
	for (z = 0; z < ncells; z++) {
		lc = layout_find_zindex(root, z);
		if (lc == NULL)
			return (-1);
		if (lc->flags & LAYOUT_CELL_FLOATING) {
			if (seen_tiled)
				return (-1);
		} else
			seen_tiled = 1;
		if (lc->flags & LAYOUT_CELL_ZOOMED) {
			if (*zoomed != NULL)
				return (-1);
			*zoomed = lc;
		}
	}
	return (0);
}

/* Find a leaf with a given parsed z-index. */
static struct layout_cell *
layout_find_zindex(struct layout_cell *lc, u_int z)
{
	struct layout_cell	*lcchild, *found;

	if (lc->type == LAYOUT_WINDOWPANE)
		return (lc->z_index == z ? lc : NULL);
	TAILQ_FOREACH(lcchild, &lc->cells, entry) {
		found = layout_find_zindex(lcchild, z);
		if (found != NULL)
			return (found);
	}
	return (NULL);
}

/* Compact z-indexes while preserving the order of the remaining cells. */
static void
layout_compact_zindexes(struct layout_cell *root, u_int old_ncells)
{
	struct layout_cell	*lc;
	u_int			 old_z, new_z = 0;

	for (old_z = 0; old_z < old_ncells; old_z++) {
		lc = layout_find_zindex(root, old_z);
		if (lc != NULL)
			lc->z_index = new_z++;
	}
}

/* Count leaves containing a pane ID. */
static u_int
layout_count_pane_id(struct layout_cell *lc, u_int pane_id)
{
	struct layout_cell	*lcchild;
	u_int			 count = 0;

	if (lc->type == LAYOUT_WINDOWPANE)
		return (lc->pane_id == pane_id);
	TAILQ_FOREACH(lcchild, &lc->cells, entry) {
		count += layout_count_pane_id(lcchild, pane_id);
	}
	return (count);
}

/* Return whether every parsed pane ID is unique. */
static int
layout_pane_ids_unique(struct layout_cell *root, struct layout_cell *lc)
{
	struct layout_cell	*lcchild;

	if (lc->type == LAYOUT_WINDOWPANE)
		return (layout_count_pane_id(root, lc->pane_id) == 1);
	TAILQ_FOREACH(lcchild, &lc->cells, entry) {
		if (!layout_pane_ids_unique(root, lcchild))
			return (0);
	}
	return (1);
}

/* Count parsed pane IDs belonging to the target window. */
static u_int
layout_pane_ids_matching(struct window *w, struct layout_cell *lc)
{
	struct layout_cell	*lcchild;
	struct window_pane	*wp;
	u_int			 matches = 0;

	if (lc->type == LAYOUT_WINDOWPANE) {
		wp = window_pane_find_by_id(lc->pane_id);
		return (wp != NULL && wp->window == w);
	}
	TAILQ_FOREACH(lcchild, &lc->cells, entry)
		matches += layout_pane_ids_matching(w, lcchild);
	return (matches);
}

/* Return 1 to assign by ID, 0 by tree order, or -1 for ambiguous IDs. */
static int
layout_pane_ids_status(struct window *w, struct layout_cell *root,
    u_int ncells)
{
	u_int	matches;

	matches = layout_pane_ids_matching(w, root);
	if (matches == 0)
		return (0);
	if (matches == ncells)
		return (1);
	return (-1);
}

/* Parse and validate a layout without changing the window. */
struct layout_prepared *
layout_prepare(struct window *w, const char *layout, char **cause)
{
	struct layout_parse_ctx	 ctx;
	struct layout_cell	*lcchild, *root = NULL, *zoomed = NULL;
	struct layout_prepared	*prepared;
	const char		*body;
	u_int			 npanes, ncells, old_ncells;
	u_short			 csum;
	int			 has_checksum, pane_ids = 0;

	if (strlen(layout) >= LAYOUT_STRING_MAX) {
		*cause = xstrdup("invalid layout");
		return (NULL);
	}
	ctx.ptr = layout;
	ctx.end = layout + strlen(layout);
	ctx.format = LAYOUT_FORMAT_UNKNOWN;
	ctx.depth = 0;
	ctx.cells = 0;
	has_checksum = layout_parse_checksum(&ctx, &csum);
	body = ctx.ptr;
	if (has_checksum && csum != layout_checksum(body, ctx.end)) {
		*cause = xstrdup("invalid layout checksum");
		return (NULL);
	}
	if (layout_construct(&ctx, NULL, &root) != 0) {
		*cause = xstrdup("invalid layout");
		layout_free_cell(root, 0);
		return (NULL);
	}
	layout_skip_space(&ctx);
	if (ctx.ptr != ctx.end || root == NULL) {
		*cause = xstrdup("invalid layout");
		layout_free_cell(root, 0);
		return (NULL);
	}
	if (layout_resolve_relative(root,
	    root->type == LAYOUT_WINDOWPANE ? w->sx : root->sx,
	    root->type == LAYOUT_WINDOWPANE ? w->sy : root->sy) != 0) {
		*cause = xstrdup("invalid layout");
		layout_free_cell(root, 0);
		return (NULL);
	}

	npanes = window_count_panes(w, 1);
	ncells = layout_count_cells(root);
	if (ctx.format == LAYOUT_FORMAT) {
		old_ncells = ncells;
		if (npanes > ncells ||
		    layout_validate_new(root, ncells, &zoomed) != 0 ||
		    !layout_pane_ids_unique(root, root)) {
			*cause = xstrdup("invalid layout");
			goto fail;
		}
		if (!layout_check(root)) {
			*cause = xstrdup("size mismatch before applying layout");
			goto fail;
		}
		if (npanes < ncells) {
			while (npanes < ncells) {
				lcchild = layout_find_bottomright(root);
				layout_destroy_cell(w, lcchild, &root);
				ncells--;
			}
			layout_compact_zindexes(root, old_ncells);
			if (layout_validate_new(root, ncells, &zoomed) != 0) {
				*cause = xstrdup("invalid layout");
				goto fail;
			}
			pane_ids = 0;
		} else {
			pane_ids = layout_pane_ids_status(w, root, ncells);
			if (pane_ids == -1) {
				*cause = xstrdup("invalid or ambiguous pane IDs");
				goto fail;
			}
		}
		if (zoomed != NULL && npanes == 1) {
			*cause = xstrdup("invalid layout");
			goto fail;
		}
	} else {
		while (npanes < ncells) {
			lcchild = layout_find_bottomright(root);
			layout_destroy_cell(w, lcchild, &root);
			ncells--;
		}
		if (npanes > ncells) {
			xasprintf(cause, "have %u panes but need %u", npanes,
			    ncells);
			goto fail;
		}
		if (layout_fix_legacy_root(root) != 0) {
			*cause = xstrdup("invalid layout");
			goto fail;
		}
	}

	if (!layout_check(root)) {
		*cause = xstrdup("size mismatch after applying layout");
		goto fail;
	}

	prepared = xcalloc(1, sizeof *prepared);
	prepared->root = root;
	prepared->zoomed = zoomed;
	prepared->format = ctx.format;
	prepared->ncells = ncells;
	prepared->pane_ids = pane_ids;
	return (prepared);

fail:
	layout_free_cell(root, 0);
	return (NULL);
}

/* Free a prepared layout without applying it. */
void
layout_free_prepared(struct layout_prepared *prepared)
{
	if (prepared == NULL)
		return;
	layout_free_cell(prepared->root, 0);
	free(prepared);
}

/* Apply and free a prepared layout. This cannot fail. */
void
layout_apply_prepared(struct window *w, struct layout_prepared *prepared)
{
	struct layout_cell	*lcchild, *root = prepared->root;
	struct window_pane	*wp;
	u_int			 z;

	prepared->root = NULL;

	/* The layout was fully validated before the existing layout is changed. */
	layout_free_cell(w->layout_root, 0);
	w->layout_root = root;
	wp = TAILQ_FIRST(&w->panes);
	layout_assign(w, &wp, root, prepared->pane_ids);

	while (!TAILQ_EMPTY(&w->z_index)) {
		wp = TAILQ_FIRST(&w->z_index);
		TAILQ_REMOVE(&w->z_index, wp, zentry);
	}
	if (prepared->format == LAYOUT_FORMAT) {
		for (z = 0; z < prepared->ncells; z++) {
			lcchild = layout_find_zindex(root, z);
			TAILQ_INSERT_TAIL(&w->z_index, lcchild->wp, zentry);
		}
	} else
		layout_fix_zindexes(w, root);

	if (layout_has_tiled(root))
		window_resize(w, root->sx, root->sy, -1, -1);
	layout_fix_offsets(w);
	layout_fix_panes(w, NULL);
	layout_print_cell(root, __func__, 0);

	if (prepared->zoomed != NULL) {
		prepared->zoomed->flags &= ~LAYOUT_CELL_ZOOMED;
		window_zoom(prepared->zoomed->wp);
	}
	free(prepared);
}

/* Assign panes to cells in tree order. */
static void
layout_assign(struct window *w, struct window_pane **wp,
    struct layout_cell *lc, int by_id)
{
	struct layout_cell	*lcchild;
	struct window_pane	*new_wp;

	if (lc == NULL)
		return;
	switch (lc->type) {
	case LAYOUT_WINDOWPANE:
		if (by_id) {
			new_wp = window_pane_find_by_id(lc->pane_id);
			if (new_wp == NULL || new_wp->window != w)
				return;
		} else {
			new_wp = *wp;
			*wp = TAILQ_NEXT(*wp, entry);
		}
		layout_make_leaf(lc, new_wp);
		return;
	case LAYOUT_LEFTRIGHT:
	case LAYOUT_TOPBOTTOM:
		TAILQ_FOREACH(lcchild, &lc->cells, entry)
			layout_assign(w, wp, lcchild, by_id);
		return;
	}
}
