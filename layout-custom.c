/* $OpenBSD$ */

/*
 * Copyright (c) 2010 Nicholas Marriott <nicm@users.sourceforge.net>
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

struct layout_cell     *layout_find_bottomright(struct layout_cell *);
u_short			layout_checksum(const char *);
int			layout_append(struct layout_cell *, char *, size_t);
struct layout_cell     *layout_construct(struct layout_cell *, const char **);
void			layout_assign(struct window_pane **, struct layout_cell *);

/* Find the bottom-right cell. */
struct layout_cell *
layout_find_bottomright(struct layout_cell *lc)
{
	if (lc->type == LAYOUT_WINDOWPANE)
		return (lc);
	lc = TAILQ_LAST(&lc->cells, layout_cells);
	return (layout_find_bottomright(lc));
}

/* Calculate layout checksum. */
u_short
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
layout_dump(struct layout_cell *root)
{
	char	layout[BUFSIZ], *out;

	*layout = '\0';
	if (layout_append(root, layout, sizeof layout) != 0)
		return (NULL);

	xasprintf(&out, "%04x,%s", layout_checksum(layout), layout);
	return (out);
}

/* Append information for a single cell. */
int
layout_append(struct layout_cell *lc, char *buf, size_t len)
{
	struct layout_cell     *lcchild;
	char			tmp[64];
	size_t			tmplen;
	const char	       *brackets = "][";

	if (len == 0)
		return (-1);

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
	case LAYOUT_WINDOWPANE:
		break;
	}

	return (0);
}

/* Parse a layout string and arrange window as layout. */
int
layout_parse(struct window *w, const char *layout)
{
	struct layout_cell	*lc, *lcchild;
	struct window_pane	*wp;
	u_int			 npanes, ncells, sx, sy;
	u_short			 csum;

	/* Check validity. */
	if (sscanf(layout, "%hx,", &csum) != 1)
		return (-1);
	layout += 5;
	if (csum != layout_checksum(layout))
		return (-1);

	/* Build the layout. */
	lc = layout_construct(NULL, &layout);
	if (lc == NULL)
		return (-1);
	if (*layout != '\0')
		goto fail;

	/* Check this window will fit into the layout. */
	for (;;) {
		npanes = window_count_panes(w);
		ncells = layout_count_cells(lc);
		if (npanes > ncells)
			goto fail;
		if (npanes == ncells)
			break;

		/* Fewer panes than cells - close the bottom right. */
		lcchild = layout_find_bottomright(lc);
		layout_destroy_cell(lcchild, &lc);
	}

	/* Save the old window size and resize to the layout size. */
	sx = w->sx; sy = w->sy;
	window_resize(w, lc->sx, lc->sy);

	/* Destroy the old layout and swap to the new. */
	layout_free_cell(w->layout_root);
	w->layout_root = lc;

	/* Assign the panes into the cells. */
	wp = TAILQ_FIRST(&w->panes);
	layout_assign(&wp, lc);

	/* Update pane offsets and sizes. */
	layout_fix_offsets(lc);
	layout_fix_panes(w, lc->sx, lc->sy);

	/* Then resize the layout back to the original window size. */
	layout_resize(w, sx, sy);
	window_resize(w, sx, sy);

	layout_print_cell(lc, __func__, 0);

	notify_window_layout_changed(w);

	return (0);

fail:
	layout_free_cell(lc);
	return (-1);
}

/* Assign panes into cells. */
void
layout_assign(struct window_pane **wp, struct layout_cell *lc)
{
	struct layout_cell	*lcchild;

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

/* Construct a cell from all or part of a layout tree. */
struct layout_cell *
layout_construct(struct layout_cell *lcparent, const char **layout)
{
	struct layout_cell     *lc, *lcchild;
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

	switch (**layout) {
	case ',':
	case '}':
	case ']':
	case '\0':
		return (lc);
	case '{':
		lc->type = LAYOUT_LEFTRIGHT;
		break;
	case '[':
		lc->type = LAYOUT_TOPBOTTOM;
		break;
	default:
		goto fail;
	}

	do {
		(*layout)++;
		lcchild = layout_construct(lc, layout);
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
	layout_free_cell(lc);
	return (NULL);
}
