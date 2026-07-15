/* $OpenBSD: layout-custom.c,v 1.38 2026/07/16 12:36:58 nicm Exp $ */

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
#include <string.h>
#include <stdlib.h>

#include "tmux.h"

struct layout_string {
	char	*write;
	char	 dat[8192];
};

static struct layout_cell	*layout_find_bottomright(struct layout_cell *);
static u_short			 layout_checksum(const char *);
static int			 layout_append(struct layout_cell *,
				     struct layout_string *, size_t, int);
static struct layout_cell	*layout_construct(const char **, int, int);
static void			 layout_assign(struct window_pane **,
				     struct layout_cell *);
static struct layout_cell	*layout_custom_create_cell(struct layout_cell *,
				     const char **layout);

static void
layout_string_init(struct layout_string *ls)
{
	memset(ls->dat, 0, sizeof ls->dat);
	ls->write = ls->dat;
}

static int
layout_string_copy(struct layout_string *ls, const char *s)
{
	size_t	len, remaining = sizeof ls->dat - (ls->write - ls->dat);

	len = strlcat(ls->write, s, remaining);
	if (len >= remaining)
		return (-1);
	ls->write += len;
	return (0);
}

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
layout_checksum(const char *layout)
{
	u_short	csum;

	csum = 0;
	for (; *layout != '\0'; layout++) {
		csum = (csum >> 1) + ((csum & 1) << 15);
		csum += *layout;
	}
	return (csum);
}

/* Dump layout as a string. */
char *
layout_dump(__unused struct window *w, struct layout_cell *root, int flags)
{
	struct layout_string	 layout = { 0 };
	char			*out;

	layout_string_init(&layout);

	if (layout_append(root, &layout, sizeof layout.dat, flags) != 0)
		return (NULL);

	if (flags & LAYOUT_CUSTOM_OLD_FORMAT)
		xasprintf(&out, "%04hx,%s", layout_checksum(layout.dat), layout.dat);
	else
		xasprintf(&out, "{\"v\":2,\"l\":%s}", layout.dat);
	return (out);
}

static int
layout_append_v2(struct layout_cell *lc, struct layout_string *ls, size_t len)
{
	struct layout_cell	*lcchild;
	struct window_pane	*wp;
	struct window		*w;
	enum layout_type	 type = lc->type;
	char			 tmp[64], c;
	size_t			 tmpsz;
	u_int			 i;

	if (len == 0)
		return (-1);
	if (lc == NULL)
		return (0);

	if (type == LAYOUT_TOPBOTTOM)
		c = 'v';
	else if (type == LAYOUT_LEFTRIGHT)
		c = 'h';
	else if (LAYOUT_WINDOWPANE)
		c = 'p';
	else
		return (-1);

#define layout_string_format(fmt, ...)						\
	do {									\
		tmpsz = xsnprintf(tmp, sizeof tmp, (fmt), ##__VA_ARGS__);	\
		if (tmpsz > (sizeof (tmp)) - 1)					\
			return (-1);						\
		if (layout_string_copy(ls, tmp) != 0)				\
			return (-1);						\
	} while (0)

	layout_string_format("{\"t\":\"%c\",\"w\":%u,\"h\":%u,\"x\":%d"
	    ",\"y\":%d", c, lc->g.sx, lc->g.sy, lc->g.xoff, lc->g.yoff);
	if (type != LAYOUT_WINDOWPANE) {
		layout_string_copy(ls, ",\"c\":[");
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (layout_append_v2(lcchild, ls, len) != 0)
				return (-1);
			layout_string_copy(ls, ",");
		}
		*(--ls->write) = '\0'; /* removing last comma */
		layout_string_copy(ls, "]");
	} else {
		wp = lc->wp;
		w = wp->window;
		if (wp == w->active)
			layout_string_copy(ls, ",\"a\":true");
		if (wp == TAILQ_FIRST(&w->last_panes))
			layout_string_copy(ls, ",\"l\":true");
		if (lc->flags & LAYOUT_CELL_FLOATING) {
			window_pane_zindex(wp, &i);
			layout_string_format(",\"z\":%u", i);
		}
		layout_string_format(",\"i\":%d", lc->wp->id);
	}

	layout_string_copy(ls, "}");

	return (0);
}

/* Append information for a single cell in the old format. */
static int
layout_append_v1(struct layout_cell *lc, char *buf, size_t len)
{
	struct layout_cell     *lcchild;
	char			tmp[64];
	size_t			tmplen;
	const char	       *brackets = "][";

	if (len == 0)
		return (-1);
	if (lc == NULL)
		return (0);
	if (lc->wp != NULL) {
		tmplen = xsnprintf(tmp, sizeof tmp, "%ux%u,%d,%d,%u",
		    lc->g.sx, lc->g.sy, lc->g.xoff, lc->g.yoff, lc->wp->id);
	} else {
		tmplen = xsnprintf(tmp, sizeof tmp, "%ux%u,%d,%d",
		    lc->g.sx, lc->g.sy, lc->g.xoff, lc->g.yoff);
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
			if (layout_append_v1(lcchild, buf, len) != 0)
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
/* Dispatch to the correct version. */
static int
layout_append(struct layout_cell *lc, struct layout_string *ls, size_t len,
    int flags)
{
	if (flags & LAYOUT_CUSTOM_OLD_FORMAT)
		return (layout_append_v1(lc, ls->dat, len));
	return (layout_append_v2(lc, ls, len));
}

/* Check layout sizes fit. */
static int
layout_check(struct layout_cell *lc)
{
	struct layout_cell	*lcchild;
	u_int			 n = 0;

	switch (lc->type) {
	case LAYOUT_WINDOWPANE:
		break;
	case LAYOUT_LEFTRIGHT:
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (lcchild->flags & LAYOUT_CELL_FLOATING)
				continue;
			if (lcchild->g.sy != lc->g.sy)
				return (0);
			if (!layout_check(lcchild))
				return (0);
			n += lcchild->g.sx + 1;
		}
		if (n - 1 != lc->g.sx)
			return (0);
		break;
	case LAYOUT_TOPBOTTOM:
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (lcchild->flags & LAYOUT_CELL_FLOATING)
				continue;
			if (lcchild->g.sx != lc->g.sx)
				return (0);
			if (!layout_check(lcchild))
				return (0);
			n += lcchild->g.sy + 1;
		}
		if (n - 1 != lc->g.sy)
			return (0);
		break;
	}
	return (1);
}

/* Parse a layout string and arrange window as layout. */
int
layout_parse(struct window *w, const char *layout, int flags, char **cause)
{
	struct layout_cell	*lcchild, *lc = NULL;
	struct window_pane	*wp;
	u_int			 npanes, ncells, sx = 0, sy = 0;
	u_short			 csum;
	int			 n, version;

	/* Check validity. */
	if (flags & LAYOUT_CUSTOM_OLD_FORMAT) {
		version = 1;
		if (sscanf(layout, "%hx,%n", &csum, &n) != 1 || n != 5) {
			*cause = xstrdup("malformed layout header");
			return (-1);
		}
	} else {
		if (sscanf(layout, "{\"v\":%d,\"L\":%n", &version, &n) != 1 ||
		    n != 11) {
			*cause = xstrdup("malformed layout header");
			return (-1);
		}
	}
	layout += n;
	if (flags & LAYOUT_CUSTOM_OLD_FORMAT) {
		if (csum != layout_checksum(layout)) {
			*cause = xstrdup("invalid layout checksum");
			return (-1);
		}
	}

	/* Build the layout. */
	lc = layout_construct(&layout, version, flags);
	if (lc == NULL) {
		*cause = xstrdup("invalid layout");
		return (-1);
	}
	layout++; /* skip '}' in outer container */
	if (*layout != '\0') {
		*cause = xstrdup("invalid layout");
		goto fail;
	}

	/* Check this window will fit into the layout. */
	npanes = window_count_panes(w, 1);
	for (;;) {
		ncells = layout_count_cells(lc);
		if (npanes > ncells) {
			xasprintf(cause, "have %u panes but need %u", npanes,
			    ncells);
			goto fail;
		}
		if (npanes == ncells)
			break;

		/*
		 * Fewer panes than cells, close the bottom right until none
		 * remain.
		 */
		lcchild = layout_find_bottomright(lc);
		layout_destroy_cell(w, lcchild, &lc);
	}

	/*
	 * It appears older versions of tmux were able to generate layouts with
	 * an incorrect top cell size - if it is larger than the top child then
	 * correct that (if this is still wrong the check code will catch it).
	 */
	switch (lc->type) {
	case LAYOUT_WINDOWPANE:
		break;
	case LAYOUT_LEFTRIGHT:
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (~lcchild->flags & LAYOUT_CELL_FLOATING) {
				sy = lcchild->g.sy + 1;
				sx += lcchild->g.sx + 1;
			}
		}
		break;
	case LAYOUT_TOPBOTTOM:
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (~lcchild->flags & LAYOUT_CELL_FLOATING) {
				sx = lcchild->g.sx + 1;
				sy += lcchild->g.sy + 1;
			}
		}
		break;
	}
	if (lc->type != LAYOUT_WINDOWPANE &&
	    (lc->g.sx != sx || lc->g.sy != sy)) {
		layout_print_cell(lc, __func__, 0);
		lc->g.sx = sx - 1; lc->g.sy = sy - 1;
	}

	/* Check the new layout. */
	if (!layout_check(lc)) {
		*cause = xstrdup("size mismatch after applying layout");
		goto fail;
	}

	/* Resize window to the layout size. */
	if (sx != 0 && sy != 0)
		window_resize(w, lc->g.sx, lc->g.sy, -1, -1);

	/* Destroy the old layout and swap to the new. */
	layout_free_cell(w->layout_root, 0);
	w->layout_root = lc;

	/* Assign the panes into the cells. */
	wp = TAILQ_FIRST(&w->panes);
	if (lc != NULL)
		layout_assign(&wp, lc);


	/* Update pane offsets and sizes. */
	layout_fix_zindexes(w);
	layout_fix_offsets(w);
	layout_fix_panes(w, NULL);
	recalculate_sizes();
	layout_print_cell(lc, __func__, 0);

	events_fire_window("window-layout-changed", w);

	return (0);

fail:
	layout_free_cell(lc, 0);
	return (-1);
}

/* Assign panes into cells. */
static void
layout_assign(struct window_pane **wp, struct layout_cell *lc)
{
	struct layout_cell	*lcchild;

	if (lc == NULL)
		return;

	switch (lc->type) {
	case LAYOUT_WINDOWPANE:
		layout_make_leaf(lc, *wp);
		*wp = TAILQ_NEXT(*wp, entry);
		return;
	case LAYOUT_LEFTRIGHT:
	case LAYOUT_TOPBOTTOM:
		TAILQ_FOREACH(lcchild, &lc->cells, entry)
			layout_assign(wp, lcchild);
		return;
	}
}

/* Part of the old format. */
static struct layout_cell *
layout_construct_cell(struct layout_cell *lcparent, const char **layout)
{
	struct layout_cell     *lc;
	u_int			sx, sy;
	int			xoff, yoff;
	const char	       *saved;

	if (!isdigit((u_char) **layout))
		return (NULL);
	if (sscanf(*layout, "%ux%u,%d,%d", &sx, &sy, &xoff, &yoff) != 4)
		return (NULL);

	while (isdigit((u_char) **layout))
		(*layout)++;
	if (**layout != 'x')
		return (NULL);
	(*layout)++;
	while (isdigit((u_char) **layout))
		(*layout)++;
	if (**layout != ',')
		return (NULL);
	(*layout)++;
	while (isdigit((u_char) **layout))
		(*layout)++;
	if (**layout != ',')
		return (NULL);
	(*layout)++;
	while (isdigit((u_char) **layout))
		(*layout)++;
	if (**layout == ',') {
		saved = *layout;
		(*layout)++;
		while (isdigit((u_char) **layout))
			(*layout)++;
		if (**layout == 'x')
			*layout = saved;
	}

	lc = layout_create_cell(lcparent);
	lc->g.sx = sx;
	lc->g.sy = sy;
	lc->g.xoff = xoff;
	lc->g.yoff = yoff;

	return (lc);
}

/* Part of the old format. */
static struct layout_cell *
layout_construct_v1(struct layout_cell *lcparent, const char **layout)
{
	struct layout_cell	*lc, *lcchild;

	lc = layout_construct_cell(lcparent, layout);
	if (lc == NULL)
		return (NULL);

	switch (**layout) {
	case ',':
	case '}':
	case ']':
	case '\0':
		return (lc);
	case '{':
		(lc)->type = LAYOUT_LEFTRIGHT;
		break;
	case '[':
		(lc)->type = LAYOUT_TOPBOTTOM;
		break;
	default:
		goto fail;
	}

	do {
		(*layout)++;
		lcchild = layout_construct_v1(lc, layout);
		if (lcchild == NULL)
			goto fail;
		TAILQ_INSERT_TAIL(&lc->cells, lcchild, entry);
	} while (**layout == ',');

	switch (lc->type) {
	case LAYOUT_LEFTRIGHT:
		if (**layout != '}')
			goto fail;
		break;
	case LAYOUT_TOPBOTTOM:
		if (**layout != ']')
			goto fail;
		break;
	default:
		goto fail;
	}
	(*layout)++;

	return (lc);

fail:
	layout_free_cell(lc, 0);
	return (NULL);
}

static int
layout_custom_parse_field(struct layout_cell *lc, const char **layout)
{
	struct layout_cell	*lcchild;
	long long		 ll;
	char			*endptr, saved;

	if (**layout != '"')
		return (0);
	(*layout)++;

	switch (**layout) {
	case 't': /* type */
		if (strncmp(*layout, "t\":\"", 4) != 0)
			return (-1);
		(*layout) += 4;
		switch (**layout) {
			case 'h':
				lc->type = LAYOUT_LEFTRIGHT;
				break;
			case 'v':
				lc->type = LAYOUT_TOPBOTTOM;
				break;
			case 'p':
				lc->type = LAYOUT_WINDOWPANE;
				break;
			default:
				return (-1);
		}
		(*layout)++;
		if (**layout != '"')
			return (-1);
		(*layout)++;
		break;
	case 'w': /* width */
	case 'h': /* height */
		saved = **layout;
		(*layout)++;
		if (strncmp(*layout, "\":", 2) != 0)
			return (-1);
		(*layout) += 2;
		ll = strtoll(*layout, &endptr, 10);
		if (*endptr != ',' && *endptr != '}')
			return (-1);
		if (saved == 'w')
			lc->g.sx = ll;
		else
			lc->g.sy = ll;
		*layout = endptr;
		break;
	case 'x': /* x-position */
	case 'y': /* y-position */
		saved = **layout;
		(*layout)++;
		if (strncmp(*layout, "\":", 2) != 0)
			return (-1);
		(*layout) += 2;
		ll = strtoll(*layout, &endptr, 10);
		if (*endptr != ',' && *endptr != '}')
			return (-1);
		if (saved == 'x')
			lc->g.xoff = ll;
		else
			lc->g.yoff = ll;
		*layout = endptr;
		break;
	case 'i': /* pane id */
		/* Pane ids are not used when reconstructing the layout. */
		if (strncmp(*layout, "i\":", 3) != 0)
			return (-1);
		(*layout) += 3;
		while (**layout != ',' && **layout != '}')
			(*layout)++;
		break;
	case 'z': /* z-index */
		if (strncmp(*layout, "z\":", 3) != 0)
			return (-1);
		(*layout) += 3;
		lc->flags |= LAYOUT_CELL_FLOATING;
		while (**layout != ',' && **layout != '}')
			(*layout)++;
		break;
	case 'a': /* active */
	case 'l': /* last */
		/* Properties of the window, not the cell. */
		(*layout)++;
		if (strncmp(*layout, "\":", 2) != 0)
			return (-1);
		(*layout) += 2;
		while (**layout != ',' && **layout != '}')
			(*layout)++;
		break;
	case 'c': /* children */
		if (strncmp(*layout, "c\":[", 4) != 0)
			return (-1);
		(*layout) += 4;
		while (1) {
			lcchild = layout_custom_create_cell(lc, layout);
			if (lcchild == NULL)
				return (-1);
			TAILQ_INSERT_TAIL(&lc->cells, lcchild, entry);
			if (**layout == ',')
				(*layout)++;
			else if (**layout == ']') {
				(*layout)++;
				break;
			} else
				return (-1);
		}
		break;
	}
	return (0);
}

static struct layout_cell *
layout_custom_create_cell(struct layout_cell *lcparent, const char **layout)
{
	struct layout_cell	*lc;

	if (**layout != '{')
		return (NULL);
	(*layout)++;

	lc = layout_create_cell(lcparent);
	if (lc == NULL)
		goto fail;

	while (1) {
		if (layout_custom_parse_field(lc, layout) != 0)
			goto fail;
		if (**layout == ',')
			(*layout)++;
		else if (**layout == '}') {
			(*layout)++;
			break;
		} else
			goto fail;
	}

	return (lc);
fail:
	layout_free_cell(lc, 0);
	return (NULL);
}

/*
 * Recursively constructs a layout given a layout string. Does not validate
 * geometry. If the layout string represents a malformed layout, will either
 * return NULL or will not advance through the entire input string.
 */
static struct layout_cell *
layout_construct_v2(const char **layout)
{
	return (layout_custom_create_cell(NULL, layout));
}

/*
 * Validates that the version of the layout string matches the version obtained
 * from the flags, then dispatches to the associated parsing function.
 */
static struct layout_cell *
layout_construct(const char **layout, int version, int flags)
{
	if (flags & LAYOUT_CUSTOM_OLD_FORMAT) {
		if (version != 1)
			return (NULL);
		return (layout_construct_v1(NULL, layout));
	}
	if (version != 2)
		return (NULL);
	return (layout_construct_v2(layout));
}
