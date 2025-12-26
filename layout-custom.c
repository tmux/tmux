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
#include <string.h>

#include "tmux.h"

static struct layout_cell	*layout_find_bottomright(struct layout_cell *);
static u_short			 layout_checksum(const char *);
static int			 layout_append(struct layout_cell *, char *,
				     size_t);
static int			 layout_construct(struct layout_cell *,
				     const char **, struct layout_cell **,
				     struct layout_cell **);
static void			 layout_assign(struct window_pane **,
				     struct layout_cell *, int);

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
layout_dump(struct window *w, struct layout_cell *root)
{
	char			 layout[8192], *out;
	int			 braket;
	struct window_pane	*wp;

	*layout = '\0';
	if (layout_append(root, layout, sizeof layout) != 0)
		return (NULL);

	braket = 0;
	TAILQ_FOREACH(wp, &w->z_index, zentry) {
		if (~wp->flags & PANE_FLOATING)
			break;
		if (!braket) {
			strcat(layout, "<");
			braket = 1;
		}
		if (layout_append(wp->layout_cell, layout, sizeof layout) != 0)
			return (NULL);
		strcat(layout, ",");
	}
	if (braket) {
		/* Overwrite the trailing ','. */
		layout[strlen(layout) - 1] = '>';
	}

	xasprintf(&out, "%04hx,%s", layout_checksum(layout), layout);
	return (out);
}

/* Append information for a single cell. */
static int
layout_append(struct layout_cell *lc, char *buf, size_t len)
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
		tmplen = xsnprintf(tmp, sizeof tmp, "%ux%u,%u,%u,%u",
		    lc->sx, lc->sy, lc->xoff, lc->yoff, lc->wp->id);
	} else {
		tmplen = xsnprintf(tmp, sizeof tmp, "%ux%u,%u,%u",
		    lc->sx, lc->sy, lc->xoff, lc->yoff);
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
			if (layout_append(lcchild, buf, len) != 0)
				return (-1);
			if (strlcat(buf, ",", len) >= len)
				return (-1);
		}
		buf[strlen(buf) - 1] = brackets[0];
		break;
	case LAYOUT_FLOATING:
	case LAYOUT_WINDOWPANE:
		break;
	}

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
	case LAYOUT_FLOATING:
		break;
	case LAYOUT_LEFTRIGHT:
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (lcchild->sy != lc->sy)
				return (0);
			if (!layout_check(lcchild))
				return (0);
			n += lcchild->sx + 1;
		}
		if (n - 1 != lc->sx)
			return (0);
		break;
	case LAYOUT_TOPBOTTOM:
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (lcchild->sx != lc->sx)
				return (0);
			if (!layout_check(lcchild))
				return (0);
			n += lcchild->sy + 1;
		}
		if (n - 1 != lc->sy)
			return (0);
		break;
	}
	return (1);
}

/* Parse a layout string and arrange window as layout. */
int
layout_parse(struct window *w, const char *layout, char **cause)
{
	struct layout_cell	*lcchild, *tiled_lc = NULL, *floating_lc = NULL;
	struct window_pane	*wp;
	u_int			 npanes, ncells, sx = 0, sy = 0;
	u_short			 csum;

	/* Check validity. */
	if (sscanf(layout, "%hx,", &csum) != 1) {
		*cause = xstrdup("invalid layout");
		return (-1);
	}
	layout += 5;
	if (csum != layout_checksum(layout)) {
		*cause = xstrdup("invalid layout");
		return (-1);
	}

	/* Build the layout. */
	if (layout_construct(NULL, &layout, &tiled_lc, &floating_lc) != 0) {
		*cause = xstrdup("invalid layout");
		return (-1);
	}
	if (tiled_lc == NULL) {
		/* A stub layout cell for an empty window. */
		tiled_lc = layout_create_cell(NULL);
		tiled_lc->type = LAYOUT_LEFTRIGHT;
		layout_set_size(tiled_lc, w->sx, w->sy, 0, 0);
	}
	if (*layout != '\0') {
		*cause = xstrdup("invalid layout");
		goto fail;
	}

	/* Check this window will fit into the layout. */
	for (;;) {
		npanes = window_count_panes(w);
		ncells = layout_count_cells(tiled_lc);
		ncells += layout_count_cells(floating_lc);
		if (npanes > ncells) {
			/* Modify this to open a new pane */
			xasprintf(cause, "have %u panes but need %u", npanes,
			    ncells);
			goto fail;
		}
		if (npanes == ncells)
			break;

		/*
		 * Fewer panes than cells - close floating panes first
		 * then close the bottom right until.
		 */
		if (floating_lc && ! TAILQ_EMPTY(&floating_lc->cells)) {
			lcchild = TAILQ_FIRST(&floating_lc->cells);
			layout_destroy_cell(w, lcchild, &floating_lc);
		} else {
			lcchild = layout_find_bottomright(tiled_lc);
			layout_destroy_cell(w, lcchild, &tiled_lc);
		}
	}

	/*
	 * It appears older versions of tmux were able to generate layouts with
	 * an incorrect top cell size - if it is larger than the top child then
	 * correct that (if this is still wrong the check code will catch it).
	 */

	switch (tiled_lc->type) {
	case LAYOUT_WINDOWPANE:
		break;
	case LAYOUT_LEFTRIGHT:
		TAILQ_FOREACH(lcchild, &tiled_lc->cells, entry) {
			sy = lcchild->sy + 1;
			sx += lcchild->sx + 1;
		}
		break;
	case LAYOUT_TOPBOTTOM:
		TAILQ_FOREACH(lcchild, &tiled_lc->cells, entry) {
			sx = lcchild->sx + 1;
			sy += lcchild->sy + 1;
		}
		break;
	case LAYOUT_FLOATING:
		*cause = xstrdup("invalid layout");
		goto fail;
	}
	if (tiled_lc->type != LAYOUT_WINDOWPANE &&
	    (tiled_lc->sx != sx || tiled_lc->sy != sy)) {
		log_debug("fix layout %u,%u to %u,%u", tiled_lc->sx,
		    tiled_lc->sy, sx,sy);
		layout_print_cell(tiled_lc, __func__, 0);
		tiled_lc->sx = sx - 1; tiled_lc->sy = sy - 1;
	}

	/* Check the new layout. */
	if (!layout_check(tiled_lc)) {
		*cause = xstrdup("size mismatch after applying layout");
		goto fail;
	}

	/* Resize window to the layout size. */
	if (sx != 0 && sy != 0)
		window_resize(w, tiled_lc->sx, tiled_lc->sy, -1, -1);

	/* Destroy the old layout and swap to the new. */
	layout_free_cell(w->layout_root);
	w->layout_root = tiled_lc;

	/* Assign the panes into the cells. */
	wp = TAILQ_FIRST(&w->panes);
	layout_assign(&wp, tiled_lc, 0);
	layout_assign(&wp, floating_lc, 1);

	/* Fix z_indexes. */
	while (!TAILQ_EMPTY(&w->z_index)) {
		wp = TAILQ_FIRST(&w->z_index);
		TAILQ_REMOVE(&w->z_index, wp, zentry);
	}
	layout_fix_zindexes(w, floating_lc);
	layout_fix_zindexes(w, tiled_lc);

	/* Update pane offsets and sizes. */
	layout_fix_offsets(w);
	layout_fix_panes(w, NULL);
	recalculate_sizes();

	layout_print_cell(tiled_lc, __func__, 0);
	layout_print_cell(floating_lc, __func__, 0);

	/* Free the floating layout cell, no longer needed. */
	layout_free_cell(floating_lc);

	notify_window("window-layout-changed", w);

	return (0);

fail:
	layout_free_cell(tiled_lc);
	layout_free_cell(floating_lc);
	return (-1);
}

/* Assign panes into cells. */
static void
layout_assign(struct window_pane **wp, struct layout_cell *lc, int floating)
{
	struct layout_cell	*lcchild;

	if (lc == NULL)
		return;

	switch (lc->type) {
	case LAYOUT_WINDOWPANE:
		layout_make_leaf(lc, *wp);
		if (floating) {
			(*wp)->flags |= PANE_FLOATING;
		}
		*wp = TAILQ_NEXT(*wp, entry);
		return;
	case LAYOUT_LEFTRIGHT:
	case LAYOUT_TOPBOTTOM:
	case LAYOUT_FLOATING:
		TAILQ_FOREACH(lcchild, &lc->cells, entry)
			layout_assign(wp, lcchild, 1);
		return;
	}
}

static struct layout_cell *
layout_construct_cell(struct layout_cell *lcparent, const char **layout)
{
	struct layout_cell     *lc;
	u_int			sx, sy, xoff, yoff;
	const char	       *saved;

	if (!isdigit((u_char) **layout))
		return (NULL);
	if (sscanf(*layout, "%ux%u,%u,%u", &sx, &sy, &xoff, &yoff) != 4)
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
	lc->sx = sx;
	lc->sy = sy;
	lc->xoff = xoff;
	lc->yoff = yoff;

	return (lc);
}


/*
 * Given a character string layout, recursively construct cells.
 * Possible return values:
 *  lc LAYOUT_WINDOWPANE, no children
 *  lc LAYOUT_LEFTRIGHT or LAYOUT_TOPBOTTOM, with children
 *  floating_lc LAYOUT_FLOATING, with children
 */
static int
layout_construct(struct layout_cell *lcparent, const char **layout,
    struct layout_cell **lc, struct layout_cell **floating_lc)
{
	struct layout_cell	*lcchild, *saved_lc;

	*lc = layout_construct_cell(lcparent, layout);

	switch (**layout) {
	case ',':
	case '}':
	case ']':
	case '>':
	case '\0':
		return (0);
	case '{':
		(*lc)->type = LAYOUT_LEFTRIGHT;
		break;
	case '[':
		(*lc)->type = LAYOUT_TOPBOTTOM;
		break;
	case '<':
		saved_lc = *lc;
		*lc = layout_create_cell(lcparent);
		(*lc)->type = LAYOUT_FLOATING;
		break;
	default:
		goto fail;
	}

	do {
		(*layout)++;
		if (layout_construct(*lc, layout, &lcchild, floating_lc) != 0)
			goto fail;
		TAILQ_INSERT_TAIL(&(*lc)->cells, lcchild, entry);
	} while (**layout == ',');

	switch ((*lc)->type) {
	case LAYOUT_LEFTRIGHT:
		if (**layout != '}')
			goto fail;
		break;
	case LAYOUT_TOPBOTTOM:
		if (**layout != ']')
			goto fail;
		break;
	case LAYOUT_FLOATING:
		if (**layout != '>')
			goto fail;
		*floating_lc = *lc;
		*lc = saved_lc;
		break;
	default:
		goto fail;
	}
	(*layout)++;

	return (0);

fail:
	layout_free_cell(*lc);
	layout_free_cell(*floating_lc);
	return (-1);
}
