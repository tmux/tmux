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
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

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
 *    "i": pane ID as %n
 *    "l": index into last panes list, if not the active pane
 *    "a": true if the active pane
 *    "z": z-index, if a floating pane
 */

/* Layout string. */
struct layout_string {
	char	*write;
	char	 dat[8192];
};

struct layout_parse_cell_ctx {
	struct layout_cell	*lc
	int			 id;
	int			 last;
	int			 active;
	int			 zindex;
};

TAILQ_HEAD(layout_parse_cell_ctxs, layout_parse_cell_ctx);

struct layout_parse_ctx {
	int				 version;
	struct layout_cell		*root;
	struct layout_parse_cell_ctxs	 ctxs;
};

static struct layout_cell	*layout_find_bottomright(struct layout_cell *);  
static u_short			 layout_checksum(const char *);                      
static int			 layout_append(struct layout_cell *,                     
				     struct layout_string *, int);                           
static struct layout_cell	*layout_construct(const char *, char **);        
static void			 layout_assign(struct window_pane **,                
				     struct layout_cell *);                                  

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
			if (window_pane_last_index(wp, &i) != -1) {
				if (layout_string_write(ls, ",\"l\":%u", i)
				    != 0)
					return (-1);
			}
		}
		if (lc->flags & LAYOUT_CELL_FLOATING) {
			if (window_pane_zindex(wp, &i) != -1) {
				if (layout_string_write(ls, ",\"z\":%u", i)
				    != 0)
					return (-1);
			}
		}
		if (layout_string_write(ls, ",\"i\":\"%%%u\"", wp->id) != 0)
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
	struct layout_cell	*lcchild, *lc = NULL;
	struct window_pane	*wp;
	u_int			 npanes, ncells, sx = 0, sy = 0;

	/* Build the layout. */
	lc = layout_construct(layout, cause);
	if (lc == NULL)
		return (-1);

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
	wp = TAILQ_FIRST(&w->panes);
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
 * layout root. Consumes the layout_node tree.
 */
static struct layout_cell *
layout_parse_json(struct json_node *root, int version, const char **cause)
{
	struct layout_node	*field;
	struct layout_cell	*lcroot = NULL;
	int			 ver = -1;

	TAILQ_FOREACH(field, &root->val.fields, entry) {
		switch (field->type) {
		case NODE_NUMBER:
			if (layout_key_is_eq(field, "V"))
				ver = field->val.num;
			break;
		case NODE_OBJECT:
			if (layout_key_is_eq(field, "L")) {
				if (lcroot != NULL)
					goto fail;
				lcroot = layout_evaluate_layout(field, NULL);
				if (lcroot == NULL)
					goto fail;
			}
			break;
		default:
			break;
		}
	}
	if (ver != version)
		goto fail;
	if (lcroot == NULL)
		goto fail;
	layout_node_destroy(root);

	return (lcroot);
fail:
	layout_node_destroy(root);
	if (lcroot != NULL)
		layout_free_cell(lcroot, 0);
	return (NULL);
}

/* Evaluate nodes into layout cells. */
static struct layout_cell *
layout_parse_json_layout(struct json_node *node, struct layout_cell *lcparent)
{
	struct json_node	*field, *fieldc;
	struct layout_cell	*lc = layout_create_cell(lcparent), *lcchild;
	enum layout_type	 type = LAYOUT_WINDOWPANE;
	const char		*numstr;
	u_int			 sx = UINT_MAX, sy = UINT_MAX;
	int			 xoff = INT_MAX, yoff = INT_MAX;

	TAILQ_FOREACH(field, &node->val.fields, entry) {
		switch (field->type) {
		case NODE_NUMBER:
			if (layout_key_is_eq(field, "w"))
				sx = field->val.num;
			else if (layout_key_is_eq(field, "h"))
				sy = field->val.num;
			else if (layout_key_is_eq(field, "x"))
				xoff = field->val.num;
			else if (layout_key_is_eq(field, "y"))
				yoff = field->val.num;
			else if (layout_key_is_eq(field, "l"))
				last = field->val.num; /* unused */
			else if (layout_key_is_eq(field, "z")) {
				zindex = field->val.num; /* unused */
				lc->flags |= LAYOUT_CELL_FLOATING;
			}
			break;
		case NODE_BOOLEAN:
			if (layout_key_is_eq(field, "a"))
				active = field->val.bool; /* unused */
			break;
		case NODE_STRING:
			if (layout_key_is_eq(field, "t")) {
				if (layout_val_is_eq(field, "h"))
					type = LAYOUT_LEFTRIGHT;
				else if (layout_val_is_eq(field, "v"))
					type = LAYOUT_TOPBOTTOM;
				else if (layout_val_is_eq(field, "p"))
					type = LAYOUT_WINDOWPANE;
				else
					goto fail;
				lc->type = type;
			} else if (layout_key_is_eq(field, "i")) {
				errno = 0;
				numstr = field->val.lsv.ptr;
				if (*numstr != '%')
					goto fail;
				id = strtol(numstr + 1, NULL, 10); /* unused. */
				if (errno != 0)
					goto fail;
			}
			break;
		case NODE_ARRAY:
			if (!layout_key_is_eq(field, "c"))
				break;
			TAILQ_FOREACH(fieldc, &field->val.fields, entry) {
				lcchild = layout_evaluate_layout(fieldc, lc);
				if (lcchild == NULL)
					goto fail;
				TAILQ_INSERT_TAIL(&lc->cells, lcchild, entry);
			}
			break;
		default:
			break;
		}
	}
	if (type == LAYOUT_WINDOWPANE && !TAILQ_EMPTY(&lc->cells))
		goto fail;
	if (type != LAYOUT_WINDOWPANE && TAILQ_EMPTY(&lc->cells))
		goto fail;

	if (sx == UINT_MAX || sy == UINT_MAX || xoff == INT_MAX ||
	    yoff == INT_MAX)
		goto fail;
	layout_set_size(lc, sx, sy, xoff, yoff);
	return (lc);
fail:
	/*
	 * Ensure the children are freed if the child field is parsed and fails
	 * before the layout type is set.
	 */
	if (type == LAYOUT_WINDOWPANE && !TAILQ_EMPTY(&lc->cells))
		lc->type = LAYOUT_TOPBOTTOM;

	layout_free_cell(lc, 0);
	return (NULL);
}


/* Construct a layout root from a formatted string. */
static struct layout_cell *
layout_construct(const char *layout, char **cause)
{
	struct layout_cell	*lcroot;
	struct json_node	*json = NULL;
	u_short			 csum;
	int			 n;

	while (isspace((u_char) *layout))
		layout++;

	if (*layout != '{') { /* sniffing version */
		if (sscanf(layout, "%hx,%n", &csum, &n) != 1 || n != 5) {
			*cause = xstrdup("malformed layout header");
			return (NULL);
		}
		layout += n;
		if (csum != layout_checksum(layout)) {
			*cause = xstrdup("invalid layout checksum");
			return (NULL);
		}
		lcroot = layout_construct_v1(NULL, &layout);
	} else {
		if ((json = json_parse(layout, cause)) == NULL)
			return (NULL);

		if ((lcroot = layout_evaluate_nodes(node, 2, cause)) == NULL)
			return (NULL);
	}

	return (lcroot);
}
