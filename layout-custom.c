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

#include "tmux.h"

static struct layout_cell	*layout_find_bottomright(struct layout_cell *);
static u_short			 layout_checksum(const char *);
static int			 layout_append(struct layout_cell *, char *,
				     size_t);
static struct layout_cell	*layout_construct(struct layout_cell *,
				     const char **);
static void			 layout_assign(struct window_pane **,
				     struct layout_cell *);

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
layout_dump(__unused struct window *w, struct layout_cell *root)
{
	char	layout[8192], *out;

	*layout = '\0';
	if (layout_append(root, layout, sizeof layout) != 0)
		return (NULL);

	xasprintf(&out, "%04hx,%s", layout_checksum(layout), layout);
	return (out);
}

/* Append information for a single cell. */
static int
layout_append(struct layout_cell *lc, char *buf, size_t len)
{
	struct layout_cell	*lcchild;
	enum layout_type	 type = lc->type;
	char			 tmp[64];
	size_t			 buflen, tmplen;
	const char		*brackets = "][";

	if (len == 0)
		return (-1);
	if (lc == NULL)
		return (0);

	if (type == LAYOUT_LEFTRIGHT)
		brackets = "}{";
	else if (type == LAYOUT_WINDOWPANE)
		brackets = ")(";
	if (strlcat(buf, &brackets[1], len) >= len)
		return (-1);

	if (lc->wp != NULL) {
		tmplen = xsnprintf(tmp, sizeof tmp, "%ux%u,%d,%d,%u%s",
		    lc->g.sx, lc->g.sy, lc->g.xoff, lc->g.yoff, lc->wp->id,
		    lc->flags & LAYOUT_CELL_FLOATING ? ",f" : "\0");
	} else {
		tmplen = xsnprintf(tmp, sizeof tmp, "%ux%u,%d,%d",
		    lc->g.sx, lc->g.sy, lc->g.xoff, lc->g.yoff);
	}
	if (tmplen > (sizeof tmp) - 1)
		return (-1);
	if (strlcat(buf, tmp, len) >= len)
		return (-1);

	if (type != LAYOUT_WINDOWPANE) {
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (layout_append(lcchild, buf, len) != 0)
				return (-1);
		}
	}

	// TODO: clean this up.
	buflen = strlen(buf);
	if (buflen + 1 >= len)
		return (-1);
	buf[buflen] = brackets[0];
	buf[buflen + 1] = '\0';

	return (0);
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
layout_parse(struct window *w, const char *layout, char **cause)
{
	struct layout_cell	*lcchild, *lc = NULL;
	struct window_pane	*wp;
	u_int			 npanes, ncells, sx = 0, sy = 0;
	u_short			 csum;
	int			 n = 0;

	/* Check validity. */
	if (sscanf(layout, "%hx,%n", &csum, &n) != 1 || n != 5) {
		*cause = xstrdup("invalid layout");
		return (-1);
	}
	layout += n;
	if (csum != layout_checksum(layout)) {
		*cause = xstrdup("invalid layout");
		return (-1);
	}

	/* Build the layout. */
	lc = layout_construct(NULL, &layout);
	if (lc == NULL) {
		*cause = xstrdup("invalid layout");
		return (-1);
	}
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

static int
layout_custom_set_flags(struct layout_cell *lc, const char **layout)
{
	switch (**layout) {
		case 'f':
			lc->flags |= LAYOUT_CELL_FLOATING;
			(*layout)++;
			return (1);
		case ')':
			return (0);
		default:
			return (-1);
	}
}

static struct layout_cell *
layout_custom_create_cell(struct layout_cell *lcparent, const char **layout)
{
	struct layout_cell	*lc;
	enum layout_type	 type;
	int			 result;

	switch (**layout) {
		case '{':
			type = LAYOUT_LEFTRIGHT;
			break;
		case '[':
			type = LAYOUT_TOPBOTTOM;
			break;
		case '(':
			type = LAYOUT_WINDOWPANE;
			break;
		default:
			return (NULL);
	}
	(*layout)++;
	lc = layout_create_cell(lcparent);
	lc->type = type;

	if (sscanf(*layout, "%ux%u,%d,%d", &lc->g.sx, &lc->g.sy, &lc->g.xoff,
	    &lc->g.yoff) != 4)
		goto fail;

	/* Skip past the geometry. */
	while (isdigit((u_char) **layout))
		(*layout)++;
	if (**layout != 'x')
		goto fail;
	(*layout)++;
	while (isdigit((u_char) **layout))
		(*layout)++;
	if (**layout != ',')
		goto fail;
	(*layout)++;
	while (isdigit((u_char) **layout))
		(*layout)++;
	if (**layout != ',')
		goto fail;
	(*layout)++;
	while (isdigit((u_char) **layout))
		(*layout)++;

	/* If lc is a node or a pane with no id(?) nor flags, then done. */
	if (type != LAYOUT_WINDOWPANE || **layout == ')')
		return (lc);
	(*layout)++;

	/* Advance past pane id. Why have this if it is ignored? */
	while (isdigit((u_char) **layout))
		(*layout)++;

	/* Pane with no flags. */
	if (**layout == ')')
		return (lc);
	(*layout)++;

	while (1) {
		result = layout_custom_set_flags(lc, layout);
		if (result == 0)
			break;
		if (result == -1)
			goto fail;
	}

	return (lc);
fail:
	free(lc);
	return (NULL);
}

/*
 * Recursively constructs a layout given a layout string. Does not validate
 * geometry. If the layout string represents a malformed layout, will either
 * return NULL or will not advance through the entire input string.
 */
static struct layout_cell *
layout_construct(struct layout_cell *lcparent, const char **layout)
{
	struct layout_cell	*lcchild, *lc;

	lc = layout_custom_create_cell(lcparent, layout);
	if (lc == NULL)
		goto fail;

	while (**layout == '(' || **layout == '{' || **layout == '[') {
		lcchild = layout_construct(lc, layout);
		if (lcchild == NULL)
			goto fail;
		TAILQ_INSERT_TAIL(&lc->cells, lcchild, entry);
	}

	switch (lc->type) {
	case LAYOUT_LEFTRIGHT:
		if (**layout != '}' || TAILQ_FIRST(&lc->cells) == NULL)
			goto fail;
		break;
	case LAYOUT_TOPBOTTOM:
		if (**layout != ']' || TAILQ_FIRST(&lc->cells) == NULL)
			goto fail;
		break;
	case LAYOUT_WINDOWPANE:
		if (**layout != ')')
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
