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
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Layouts can be represented as strings in a JSON format (v2). The legacy
 * format (v1) will be deprecated in the future and should no longer be used.
 *
 * The current (v2) format is JSON. The top level has two keys:
 *    "V": version number, currently 2
 *    "L": root layout cell
 * Each cell is an object with:
 *    "t": cell type:
 *        "h": horizontal
 *        "v": vertical
 *        "p": pane
 *    "w": cell width
 *    "h": cell height
 *    "x": horizontal position
 *    "y": vertical position
 *  If the cell is a node cell (with child cells), it additionally has:
 *    "c": array of child cells
 *  If the cell is a leaf cell (that is, containing a pane and no child cells),
 *  it additionally has:
 *    "I": pane ID as %n
 *    "l": index into last panes list if visited and not the active pane
 *    "a": true if the active pane
 *    "i": pane index
 *    "z": z-index, if a floating pane
 */

#define LAYOUT_STRING_MAX	(1 << 14)

/* Layout string. */
struct layout_string {
	char	*write;
	char	 dat[LAYOUT_STRING_MAX];
};

struct layout_parse_cell_ctx {
	struct layout_cell	*lc;
	int			 active;
	int			 last;
	int			 index;
	int			 zindex;
};

struct layout_parse_ctx {
	int64_t				  version;
	int				  num_active;
	struct layout_cell		 *root;
	char				**cause;

#define CCTX_MAX	(LAYOUT_STRING_MAX >> 5) /* min 32 bytes per pane */
	int				  clen;
	struct layout_parse_cell_ctx	  cctxs[CCTX_MAX];
};

static struct layout_cell	*layout_find_bottomright(struct layout_cell *);
static u_short			 layout_checksum(const char *);
static int			 layout_append(struct layout_cell *,
				     struct layout_string *, int);
static int			 layout_construct(const char *,
				     struct layout_parse_ctx *);
static void			 layout_assign(struct window *,
				     struct layout_parse_ctx *);
static void			 layout_parse_apply_ctx(struct window *,
				     struct layout_parse_ctx *);
static struct layout_cell	*layout_parse_json_layout(
				     const struct json_node *,
				     struct layout_cell *,
				     struct layout_parse_ctx *);
static int			 layout_parse_ctx_check_indexes(
				     struct layout_parse_ctx *);

/* Compare cell contexts in ascending order of index. */
static int
layout_parse_index_cmp(const void *a, const void *b)
{
	const struct layout_parse_cell_ctx	*cca = a;
	const struct layout_parse_cell_ctx	*ccb = b;
	int					 retval = 0;

	if (cca->index < ccb->index)
		retval = -1;
	if (cca->index > ccb->index)
		retval = 1;
	return (retval);
}

/* Compare cell contexts in descending order of z-index. */
static int
layout_parse_zindex_cmp(const void *a, const void *b)
{
	const struct layout_parse_cell_ctx	*cca = a;
	const struct layout_parse_cell_ctx	*ccb = b;
	int					 retval = 0;

	if (cca->zindex > ccb->zindex)
		retval = -1;
	if (cca->zindex < ccb->zindex)
		retval = 1;
	return (retval);
}

/* Compare cell contexts in descending order of last. */
static int
layout_parse_last_cmp(const void *a, const void *b)
{
	const struct layout_parse_cell_ctx	*cca = a;
	const struct layout_parse_cell_ctx	*ccb = b;
	int					 retval = 0;

	if (cca->last > ccb->last)
		retval = -1;
	if (cca->last < ccb->last)
		retval = 1;
	return (retval);
}

/* Initialize a layout string. */
static void
layout_string_init(struct layout_string *ls)
{
	ls->dat[0] = '\0';
	ls->write = ls->dat;
}

/* Write an optionally formatted string to the end of the layout string. */
static int
layout_string_write(struct layout_string *ls, const char *fmt, ...)
{
	int	len, remaining = sizeof ls->dat - (ls->write - ls->dat);
	va_list ap;

	va_start(ap, fmt);

	len = vsnprintf(ls->write, remaining, fmt, ap);
	va_end(ap);

	if (len < 0 || len >= remaining)
		return (-1);
	ls->write += len;

	return (0);
}

static void
layout_parse_init_ctx(struct layout_parse_ctx *pctx, char **cause)
{
	pctx->version = -1;
	pctx->num_active = 0;
	pctx->root = NULL;
	pctx->cause = cause;
	pctx->clen = 0;
}

/* Add a cell context to the parse context. */
static int
layout_parse_add_cctx(struct layout_parse_ctx *pctx, struct layout_cell *lc,
    int active, int last, int index, int zindex)
{
	struct layout_parse_cell_ctx	*cctx;

	if (pctx->clen == CCTX_MAX)
		return (-1);
	cctx = &pctx->cctxs[pctx->clen++];

	cctx->lc = lc;
	cctx->active = active;
	cctx->last = last;
	cctx->index = index;
	cctx->zindex = zindex;

	return (0);
}

/* Remove a cell context from the parse context. Does not preserve ordering. */
static int
layout_parse_remove_cctx(struct layout_parse_ctx *pctx, struct layout_cell *lc)
{
	struct layout_parse_cell_ctx	*cctx;
	int				 i;

	for (i = 0; i < pctx->clen; i++) {
		if (lc == pctx->cctxs[i].lc) {
			cctx = &pctx->cctxs[--pctx->clen];
			memmove(&pctx->cctxs[i], cctx, sizeof *cctx);
			return (0);
		}
	}
	return (-1);
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
	struct layout_string	 layout;
	char			*out;

	layout_string_init(&layout);

	if (layout_append(root, &layout, flags) != 0)
		return (NULL);

	if (flags & LAYOUT_CUSTOM_OLD_FORMAT)
		xasprintf(&out, "%04hx,%s", layout_checksum(layout.dat),
		    layout.dat);
	else
		xasprintf(&out, "{\"V\":2,\"L\":%s}", layout.dat);
	return (out);
}

/* Append information for a single cell in a JSON (v2) format. */
static int
layout_append_v2(struct layout_cell *lc, struct layout_string *ls)
{
	struct layout_cell	*lcchild;
	struct window_pane	*wp;
	enum layout_type	 type;
	char			 c;
	u_int			 i, n;

	if (lc == NULL)
		return (0);

	type = lc->type;
	if (type == LAYOUT_TOPBOTTOM)
		c = 'v';
	else if (type == LAYOUT_LEFTRIGHT)
		c = 'h';
	else if (type == LAYOUT_WINDOWPANE)
		c = 'p';
	else
		return (-1);

	if (layout_string_write(ls, "{\"t\":\"%c\",\"w\":%u,\"h\":%u,\"x\":%d"
	    ",\"y\":%d", c, lc->g.sx, lc->g.sy, lc->g.xoff, lc->g.yoff) != 0)
		return (-1);
	if (type != LAYOUT_WINDOWPANE) {
		if (layout_string_write(ls, ",\"c\":[") != 0)
			return (-1);
		n = 0;
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (layout_append_v2(lcchild, ls) != 0)
				return (-1);
			if (layout_string_write(ls, ",") != 0)
				return (-1);
			n++;
		}
		if (n == 0)
			return (-1);
		*(--ls->write) = '\0'; /* removing trailing comma */
		if (layout_string_write(ls, "]") != 0)
			return (-1);
	} else {
		wp = lc->wp;
		if (wp == NULL)
			return (-1);
		if (wp == wp->window->active) {
			if (layout_string_write(ls, ",\"a\":true") != 0)
				return (-1);
		} else {
			if (window_pane_last_index(wp, &i) == 0) {
				if (layout_string_write(ls, ",\"l\":%u", i)
				    != 0)
					return (-1);
			}
		}
		if (window_pane_index(wp, &i) == 0) {
			if (layout_string_write(ls, ",\"i\":%u", i) != 0)
				return (-1);
		}
		if (lc->flags & LAYOUT_CELL_FLOATING) {
			if (window_pane_zindex(wp, &i) == 0) {
				if (layout_string_write(ls, ",\"z\":%u", i)
				    != 0)
					return (-1);
			}
		}
		if (layout_string_write(ls, ",\"I\":\"%%%u\"", wp->id) != 0)
			return (-1);
	}

	if (layout_string_write(ls, "}") != 0)
		return (-1);

	return (0);
}

/* Append information for a single cell in the legacy (v1) format. */
static int
layout_append_v1(struct layout_cell *lc, struct layout_string *ls)
{
	struct layout_cell	*lcchild;
	const char		*brackets = "[]";
	int			 n;

	if (lc == NULL)
		return (0);

	if (lc->wp != NULL) {
		if (layout_string_write(ls, "%ux%u,%d,%d,%u", lc->g.sx,
		    lc->g.sy, lc->g.xoff, lc->g.yoff, lc->wp->id) != 0)
			return (-1);
	} else {
		if (layout_string_write(ls, "%ux%u,%d,%d", lc->g.sx,
		    lc->g.sy, lc->g.xoff, lc->g.yoff) != 0)
			return (-1);
	}
	switch (lc->type) {
	case LAYOUT_LEFTRIGHT:
		brackets = "{}";
		/* FALLTHROUGH */
	case LAYOUT_TOPBOTTOM:
		if (layout_string_write(ls, "%c", brackets[0]) != 0)
			return (-1);
		n = 0;
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (!layout_cell_is_tiled(lcchild) &&
			    !layout_cell_has_tiled_child(lcchild))
				continue;
			if (layout_append_v1(lcchild, ls) != 0)
				return (-1);
			if (layout_string_write(ls, ",") != 0)
				return (-1);
			n++;
		}
		if (n == 0)
			return (-1);

		*(--ls->write) = '\0'; /* removing trailing comma */
		if (layout_string_write(ls, "%c", brackets[1]) != 0)
			return (-1);
		break;
	case LAYOUT_WINDOWPANE:
		break;
	}

	return (0);
}

/* Dispatch to append the appropriate version. */
static int
layout_append(struct layout_cell *lc, struct layout_string *ls, int flags)
{
	if (flags & LAYOUT_CUSTOM_OLD_FORMAT)
		return (layout_append_v1(lc, ls));
	return (layout_append_v2(lc, ls));
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
			if (!layout_cell_is_tiled(lcchild) &&
			    !layout_cell_has_tiled_child(lcchild))
				continue;
			if (lcchild->g.sy != lc->g.sy)
				return (0);
			if (!layout_check(lcchild))
				return (0);
			n += lcchild->g.sx + 1;
		}
		if (n != 0 && n - 1 != lc->g.sx)
			return (0);
		break;
	case LAYOUT_TOPBOTTOM:
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (!layout_cell_is_tiled(lcchild) &&
			    !layout_cell_has_tiled_child(lcchild))
				continue;
			if (lcchild->g.sx != lc->g.sx)
				return (0);
			if (!layout_check(lcchild))
				return (0);
			n += lcchild->g.sy + 1;
		}
		if (n != 0 && n - 1 != lc->g.sy)
			return (0);
		break;
	}
	return (1);
}

/* Parse a layout string and arrange window as layout. */
int
layout_parse(struct window *w, const char *layout, char **cause)
{
	struct window_pane	*wp;
	struct layout_cell	*lcchild, *lc = NULL;
	struct layout_parse_ctx	 pctx;
	u_int			 npanes, ncells, sx = 0, sy = 0;
	int			 with_floating;

	/* Build the layout. */
	layout_parse_init_ctx(&pctx, cause);
	if (layout_construct(layout, &pctx) != 0) {
		if (pctx.root != NULL)
			layout_free_cell(pctx.root, 0);
		return (-1);
	}
	with_floating = pctx.version >= 2;

	/* Check this window will fit into the layout. */
	npanes = window_count_panes(w, with_floating);
	for (;;) {
		ncells = layout_count_cells(pctx.root, with_floating);
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
		lcchild = layout_find_bottomright(pctx.root);
		if (pctx.version != 1 && layout_parse_remove_cctx(&pctx,
		    lcchild) != 0) {
			*cause = xstrdup("empty/missing layout parse context");
			goto fail;
		}
		layout_destroy_cell(w, lcchild, &pctx.root);
	}

	/* Stitch in floating panes to version 1. */
	if (pctx.version < 2) {
		TAILQ_FOREACH(wp, &w->panes, entry) {
			if (!window_pane_is_floating(wp))
				continue;
			lc = wp->layout_cell;
			TAILQ_REMOVE(&lc->parent->cells, lc, entry);
			/* A root node is guaranteed by this point. */
			TAILQ_INSERT_TAIL(&pctx.root->cells, lc, entry);
			lc->parent = pctx.root;
		}
	}
	lc = pctx.root;

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
			if (layout_cell_is_tiled(lcchild) ||
			    layout_cell_has_tiled_child(lcchild)) {
				sy = lcchild->g.sy + 1;
				sx += lcchild->g.sx + 1;
			}
		}
		break;
	case LAYOUT_TOPBOTTOM:
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (layout_cell_is_tiled(lcchild) ||
			    layout_cell_has_tiled_child(lcchild)) {
				sx = lcchild->g.sx + 1;
				sy += lcchild->g.sy + 1;
			}
		}
		break;
	}
	if (lc->type != LAYOUT_WINDOWPANE && sx != 0 && sy != 0 &&
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
	if (layout_cell_is_tiled(lc) ||
	    layout_cell_has_tiled_child(lc))
		window_resize(w, lc->g.sx, lc->g.sy, -1, -1);

	/* Destroy the old layout and swap to the new. */
	layout_free_cell(w->layout_root, 0);
	w->layout_root = lc;

	/* Assign the panes into the cells. */
	layout_assign(w, &pctx);

	/* Update pane attributes. */
	layout_fix_offsets(w);
	layout_fix_panes(w, NULL);
	if (pctx.version != 1)
		layout_parse_apply_ctx(w, &pctx);
	recalculate_sizes();
	layout_print_cell(lc, __func__, 0);

	/* Backwards compatibility. */
	if (pctx.version == 1)
		events_fire_window("window-layout-changed", w);

	return (0);

fail:
	layout_free_cell(lc, 0);
	return (-1);
}

/* Assign panes into cells from the cell contexts. */
static void
layout_assign_from_ctx(struct window *w, struct layout_parse_ctx *pctx)
{
	struct layout_cell	*lc;
	struct window_pane	*wp;
	int			 i;

	qsort(pctx->cctxs, pctx->clen, sizeof pctx->cctxs[0],
	    layout_parse_index_cmp);

	wp = TAILQ_FIRST(&w->panes);
	for (i = 0; i < pctx->clen; i++) {
		lc = pctx->cctxs[i].lc;
		layout_make_leaf(lc, wp);
		wp = TAILQ_NEXT(wp, entry);
	}
}

/*
 * Assign panes into cells when there are no cell contexts. This will be removed
 * when the non-JSON format is deprecated.
 */
static void
layout_assign_fallback(struct window_pane **wp, struct layout_cell *lc)
{
	struct layout_cell	*lcchild;

	if (lc == NULL)
		return;

	switch (lc->type) {
	case LAYOUT_WINDOWPANE:
		while ((*wp)->layout_cell != NULL) /* skipping floats */
			*wp = TAILQ_NEXT(*wp, entry);
		layout_make_leaf(lc, *wp);
		*wp = TAILQ_NEXT(*wp, entry);
		return;
	case LAYOUT_LEFTRIGHT:
	case LAYOUT_TOPBOTTOM:
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (lcchild->flags & LAYOUT_CELL_FLOATING)
				continue;
			layout_assign_fallback(wp, lcchild);
		}
		return;
	}
}

/* Assign panes into cells. Number of cells must match the number of panes. */
static void
layout_assign(struct window *w, struct layout_parse_ctx *pctx)
{
	struct window_pane	*wp = TAILQ_FIRST(&w->panes);

	if (pctx->clen > 0)
		layout_assign_from_ctx(w, pctx);
	else
		layout_assign_fallback(&wp, pctx->root);
}

/* Construct a cell from the legacy (v1) format. */
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

/* Construct a layout from the legacy (v1) format. */
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

/*
 * Evaluate parsed JSON. Check metadata at the top level and return the new
 * layout root. Consumes json input.
 */
static int
layout_parse_json(struct json_node *jnroot, struct layout_parse_ctx *pctx)
{
	struct json_node	 *jn, *object;
	int64_t			  num;
	char			**cause = pctx->cause;

	if (json_get_object(jnroot, &jn) != 0) {
		*cause = xstrdup("invalid layout json");
		goto fail;
	}

	if (json_find_number(jn, "V", &num, cause) != 0)
		goto fail;
	pctx->version = num;

	if (json_find_object(jn, "L", &object, cause) != 0)
		goto fail;
	pctx->root = layout_parse_json_layout(object, NULL, pctx);
	if (pctx->root == NULL)
		goto fail;

	json_destroy_node(jnroot);

	return (0);

fail:
	json_destroy_node(jnroot);
	if (pctx->root != NULL)
		layout_free_cell(pctx->root, 0);
	pctx->root = NULL;
	return (-1);
}

/* Parse nodes into layout cells. */
static struct layout_cell *
layout_parse_json_layout(const struct json_node *node,
    struct layout_cell *lcparent, struct layout_parse_ctx *pctx)
{
	struct json_node	 *member, *array;
	struct layout_cell	 *lc = layout_create_cell(lcparent), *lcchild;
	const char		 *str;
	int64_t			  num;
	char			**cause = pctx->cause;
	int			  boolean, index, zindex, active = -1;
	int			  last = -1;

	if (json_find_string(node, "t", &str, cause) != 0)
		goto fail;
	if (strcmp(str, "p") == 0)
		lc->type = LAYOUT_WINDOWPANE;
	else if (strcmp(str, "v") == 0)
		lc->type = LAYOUT_TOPBOTTOM;
	else if (strcmp(str, "h") == 0)
		lc->type = LAYOUT_LEFTRIGHT;
	else {
		xasprintf(cause, "unknown cell type \"%s\"", str);
		goto fail;
	}

	if (json_find_number(node, "w", &num, cause) != 0)
		goto fail;
	if (num < PANE_MINIMUM || num > PANE_MAXIMUM) {
		xasprintf(cause, "invalid width %lld", (long long)num);
		goto fail;
	}
	lc->g.sx = num;

	if (json_find_number(node, "h", &num, cause) != 0)
		goto fail;
	if (num < PANE_MINIMUM || num > PANE_MAXIMUM) {
		xasprintf(cause, "invalid height %lld", (long long)num);
		goto fail;
	}
	lc->g.sy = num;

	if (json_find_number(node, "x", &num, cause) != 0)
		goto fail;
	if (num < -WINDOW_MAXIMUM || num > WINDOW_MAXIMUM) {
		xasprintf(cause, "invalid x-offset %lld", (long long)num);
		goto fail;
	}
	lc->g.xoff = num;

	if (json_find_number(node, "y", &num, cause) != 0)
		goto fail;
	if (num < -WINDOW_MAXIMUM || num > WINDOW_MAXIMUM) {
		xasprintf(cause, "invalid y-offset %lld", (long long)num);
		goto fail;
	}
	lc->g.yoff = num;

	if (lc->type == LAYOUT_WINDOWPANE) { /* "I" is currently ignored */
		if (json_find(node, "c") != NULL) {
			*cause = xstrdup("panes cannot have children");
			goto fail;
		}
		if (json_find_number(node, "i", &num, cause) != 0)
			goto fail;
		if (num < 0 || num > INT_MAX) {
			xasprintf(cause, "invalid index %lld", (long long)num);
			goto fail;
		}
		index = num;

		if (json_find(node, "a") != NULL) {
			if (json_find_boolean(node, "a", &boolean, cause) != 0)
				goto fail;
			active = boolean;
			if (active)
				pctx->num_active++;
		} else if (json_find(node, "l") != NULL) {
			if (json_find_number(node, "l", &num, cause) != 0)
				goto fail;
			if (num < 0 || num > INT_MAX) {
				xasprintf(cause, "invalid last %lld",
				    (long long)num);
				goto fail;
			}
			last = num;
		}

		if (json_find(node, "z") != NULL) {
			if (json_find_number(node, "z", &num, cause) != 0)
				goto fail;
			if (num < 0 || num > INT_MAX - 1) {
				xasprintf(cause, "invalid floating zindex %lld",
				    (long long)num);
				goto fail;
			}
			zindex = num;
			lc->flags |= LAYOUT_CELL_FLOATING;
		} else
			zindex = INT_MAX;

		if (layout_parse_add_cctx(pctx, lc, active, last, index,
		    zindex) != 0) {
			*cause = xstrdup("too many panes");
			goto fail;
		}
	} else {
		if (json_find_array(node, "c", &array, cause) != 0)
			goto fail;
		if ((member = json_array_first(array)) == NULL) {
			*cause = xstrdup("nodes must have children");
			goto fail;
		}
		while (member != NULL) {
			lcchild = layout_parse_json_layout(member, lc,
				pctx);
			if (lcchild == NULL)
				goto fail;
			TAILQ_INSERT_TAIL(&lc->cells, lcchild, entry);
			member = json_array_next(member);
		}
	}

	return (lc);

fail:
	layout_free_cell(lc, 0);
	return (NULL);
}

/* Construct a layout root from a formatted string. */
static int
layout_construct(const char *layout, struct layout_parse_ctx *pctx)
{
	struct json_node	*json;
	u_short			 csum;
	int			 n = 0;

	while (isspace((u_char) *layout))
		layout++;

	if (*layout != '{') { /* sniffing version */
		if (sscanf(layout, "%hx,%n", &csum, &n) != 1 || n != 5) {
			*pctx->cause = xstrdup("malformed layout header");
			return (-1);
		}
		layout += n;
		if (csum != layout_checksum(layout)) {
			*pctx->cause = xstrdup("invalid layout checksum");
			return (-1);
		}
		if ((pctx->root = layout_construct_v1(NULL, &layout)) == NULL) {
			*pctx->cause = xstrdup("invalid layout");
			return (-1);
		}
		pctx->version = 1;
	} else {
		if ((json = json_parse(layout, pctx->cause)) == NULL)
			return (-1);

		if (layout_parse_json(json, pctx) != 0)
			return (-1);

		if (pctx->version != 2) {
			*pctx->cause = xstrdup("version mismatch");
			return (-1);
		}
		if (pctx->num_active > 1) {
			*pctx->cause = xstrdup("more than one active pane");
			return (-1);
		}
		if (pctx->clen == 0) {
			*pctx->cause = xstrdup("no panes");
			return (-1);
		}
		if (!layout_parse_ctx_check_indexes(pctx))
			return (-1);
	}

	return (0);
}

/* Apply the remaining context to the layout. */
static void
layout_parse_apply_ctx(struct window *w, struct layout_parse_ctx *pctx)
{
	struct layout_parse_cell_ctx	 *cctx;
	struct window_pane		 *wp, *wpnext;
	int				  i;

	/* Apply z-indexes. */
	wp = TAILQ_FIRST(&w->z_index);
	while (wp != NULL) {
		wpnext = TAILQ_NEXT(wp, zentry);
		if (window_pane_is_floating(wp))
			TAILQ_REMOVE(&w->z_index, wp, zentry);
		wp = wpnext;
	}

	qsort(pctx->cctxs, pctx->clen, sizeof pctx->cctxs[0],
	    layout_parse_zindex_cmp);

	for (i = 0; i < pctx->clen; i++) {
		cctx = &pctx->cctxs[i];
		wp = cctx->lc->wp;
		if (window_pane_is_floating(wp))
			TAILQ_INSERT_HEAD(&w->z_index, wp, zentry);
	}

	/* Set the active pane. */
	for (i = 0; i < pctx->clen; i++) {
		cctx = &pctx->cctxs[i];
		if (cctx->active == 1) {
			window_set_active_pane(w, cctx->lc->wp, 1);
			break;
		}
	}

	/* Apply last panes. */
	while (!TAILQ_EMPTY(&w->last_panes)) {
		wp = TAILQ_FIRST(&w->last_panes);
		window_pane_stack_remove(&w->last_panes, wp);
	}

	qsort(pctx->cctxs, pctx->clen, sizeof pctx->cctxs[0],
	    layout_parse_last_cmp);

	for (i = 0; i < pctx->clen; i++) {
		cctx = &pctx->cctxs[i];
		wp = cctx->lc->wp;
		if (cctx->last < 0 || cctx->active == 1)
			continue;
		window_pane_stack_push(&w->last_panes, wp);
	}
}

/* Checks for duplicate pane indexes, z-indexes, and last indexes. */
static int
layout_parse_ctx_check_indexes(struct layout_parse_ctx *pctx)
{
	int	i, n;

	qsort(pctx->cctxs, pctx->clen, sizeof pctx->cctxs[0],
	    layout_parse_index_cmp);

	for (i = 1; i < pctx->clen; i++) {
		if (pctx->cctxs[i].index == pctx->cctxs[i - 1].index) {
			*pctx->cause = xstrdup("duplicate pane index");
			return (0);
		}
	}

	qsort(pctx->cctxs, pctx->clen, sizeof pctx->cctxs[0],
	    layout_parse_zindex_cmp);

	/*
	 * Sorted in descending order, so the panes without a z-index come first
	 * and the floating panes run to the end.
	 */
	n = 0;
	while (n < pctx->clen && pctx->cctxs[n].zindex == INT_MAX)
		n++;
	for (i = n + 1; i < pctx->clen; i++) {
		if (pctx->cctxs[i].zindex == pctx->cctxs[i - 1].zindex) {
			*pctx->cause = xstrdup("duplicate pane z-index");
			return (0);
		}
	}

	qsort(pctx->cctxs, pctx->clen, sizeof pctx->cctxs[0],
	    layout_parse_last_cmp);

	/*
	 * Sorted in descending order, so the panes without a last index come
	 * last.
	 */
	n = 0;
	while (n < pctx->clen && pctx->cctxs[n].last >= 0)
		n++;
	for (i = 1; i < n; i++) {
		if (pctx->cctxs[i].last == pctx->cctxs[i - 1].last) {
			*pctx->cause = xstrdup("duplicate last pane index");
			return (0);
		}
	}

	return (1);
}
