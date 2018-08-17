/* $OpenBSD$ */

/*
 * Copyright (c) 2017 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

struct mode_tree_item;
TAILQ_HEAD(mode_tree_list, mode_tree_item);

struct mode_tree_data {
	int			  dead;
	u_int			  references;
	int			  zoomed;

	struct window_pane	 *wp;
	void			 *modedata;

	const char		**sort_list;
	u_int			  sort_size;
	u_int			  sort_type;

	mode_tree_build_cb        buildcb;
	mode_tree_draw_cb         drawcb;
	mode_tree_search_cb       searchcb;

	struct mode_tree_list	  children;
	struct mode_tree_list	  saved;

	struct mode_tree_line	 *line_list;
	u_int			  line_size;

	u_int			  depth;

	u_int			  width;
	u_int			  height;

	u_int			  offset;
	u_int			  current;

	struct screen		  screen;

	int			  preview;
	char			 *search;
	char			 *filter;
	int			  no_matches;
};

struct mode_tree_item {
	struct mode_tree_item		*parent;
	void				*itemdata;
	u_int				 line;

	uint64_t			 tag;
	const char			*name;
	const char			*text;

	int				 expanded;
	int				 tagged;

	struct mode_tree_list		 children;
	TAILQ_ENTRY(mode_tree_item)	 entry;
};

struct mode_tree_line {
	struct mode_tree_item		*item;
	u_int				 depth;
	int				 last;
	int				 flat;
};

static void mode_tree_free_items(struct mode_tree_list *);

static struct mode_tree_item *
mode_tree_find_item(struct mode_tree_list *mtl, uint64_t tag)
{
	struct mode_tree_item	*mti, *child;

	TAILQ_FOREACH(mti, mtl, entry) {
		if (mti->tag == tag)
			return (mti);
		child = mode_tree_find_item(&mti->children, tag);
		if (child != NULL)
			return (child);
	}
	return (NULL);
}

static void
mode_tree_free_item(struct mode_tree_item *mti)
{
	mode_tree_free_items(&mti->children);

	free((void *)mti->name);
	free((void *)mti->text);

	free(mti);
}

static void
mode_tree_free_items(struct mode_tree_list *mtl)
{
	struct mode_tree_item	*mti, *mti1;

	TAILQ_FOREACH_SAFE(mti, mtl, entry, mti1) {
		TAILQ_REMOVE(mtl, mti, entry);
		mode_tree_free_item(mti);
	}
}

static void
mode_tree_check_selected(struct mode_tree_data *mtd)
{
	/*
	 * If the current line would now be off screen reset the offset to the
	 * last visible line.
	 */
	if (mtd->current > mtd->height - 1)
		mtd->offset = mtd->current - mtd->height + 1;
}

static void
mode_tree_clear_lines(struct mode_tree_data *mtd)
{
	free(mtd->line_list);
	mtd->line_list = NULL;
	mtd->line_size = 0;
}

static void
mode_tree_build_lines(struct mode_tree_data *mtd,
    struct mode_tree_list *mtl, u_int depth)
{
	struct mode_tree_item	*mti;
	struct mode_tree_line	*line;
	u_int			 i;
	int			 flat = 1;

	mtd->depth = depth;
	TAILQ_FOREACH(mti, mtl, entry) {
		mtd->line_list = xreallocarray(mtd->line_list,
		    mtd->line_size + 1, sizeof *mtd->line_list);

		line = &mtd->line_list[mtd->line_size++];
		line->item = mti;
		line->depth = depth;
		line->last = (mti == TAILQ_LAST(mtl, mode_tree_list));

		mti->line = (mtd->line_size - 1);
		if (!TAILQ_EMPTY(&mti->children))
			flat = 0;
		if (mti->expanded)
			mode_tree_build_lines(mtd, &mti->children, depth + 1);
	}
	TAILQ_FOREACH(mti, mtl, entry) {
		for (i = 0; i < mtd->line_size; i++) {
			line = &mtd->line_list[i];
			if (line->item == mti)
				line->flat = flat;
		}
	}
}

static void
mode_tree_clear_tagged(struct mode_tree_list *mtl)
{
	struct mode_tree_item	*mti;

	TAILQ_FOREACH(mti, mtl, entry) {
		mti->tagged = 0;
		mode_tree_clear_tagged(&mti->children);
	}
}

static void
mode_tree_up(struct mode_tree_data *mtd, int wrap)
{
	if (mtd->current == 0) {
		if (wrap) {
			mtd->current = mtd->line_size - 1;
			if (mtd->line_size >= mtd->height)
				mtd->offset = mtd->line_size - mtd->height;
		}
	} else {
		mtd->current--;
		if (mtd->current < mtd->offset)
			mtd->offset--;
	}
}

void
mode_tree_down(struct mode_tree_data *mtd, int wrap)
{
	if (mtd->current == mtd->line_size - 1) {
		if (wrap) {
			mtd->current = 0;
			mtd->offset = 0;
		}
	} else {
		mtd->current++;
		if (mtd->current > mtd->offset + mtd->height - 1)
			mtd->offset++;
	}
}

void *
mode_tree_get_current(struct mode_tree_data *mtd)
{
	return (mtd->line_list[mtd->current].item->itemdata);
}

void
mode_tree_expand_current(struct mode_tree_data *mtd)
{
	if (!mtd->line_list[mtd->current].item->expanded) {
		mtd->line_list[mtd->current].item->expanded = 1;
		mode_tree_build(mtd);
	}
}

void
mode_tree_set_current(struct mode_tree_data *mtd, uint64_t tag)
{
	u_int	i;

	for (i = 0; i < mtd->line_size; i++) {
		if (mtd->line_list[i].item->tag == tag)
			break;
	}
	if (i != mtd->line_size) {
		mtd->current = i;
		if (mtd->current > mtd->height - 1)
			mtd->offset = mtd->current - mtd->height + 1;
		else
			mtd->offset = 0;
	} else {
		mtd->current = 0;
		mtd->offset = 0;
	}
}

u_int
mode_tree_count_tagged(struct mode_tree_data *mtd)
{
	struct mode_tree_item	*mti;
	u_int			 i, tagged;

	tagged = 0;
	for (i = 0; i < mtd->line_size; i++) {
		mti = mtd->line_list[i].item;
		if (mti->tagged)
			tagged++;
	}
	return (tagged);
}

void
mode_tree_each_tagged(struct mode_tree_data *mtd, mode_tree_each_cb cb,
    struct client *c, key_code key, int current)
{
	struct mode_tree_item	*mti;
	u_int			 i;
	int			 fired;

	fired = 0;
	for (i = 0; i < mtd->line_size; i++) {
		mti = mtd->line_list[i].item;
		if (mti->tagged) {
			fired = 1;
			cb(mtd->modedata, mti->itemdata, c, key);
		}
	}
	if (!fired && current) {
		mti = mtd->line_list[mtd->current].item;
		cb(mtd->modedata, mti->itemdata, c, key);
	}
}

struct mode_tree_data *
mode_tree_start(struct window_pane *wp, struct args *args,
    mode_tree_build_cb buildcb, mode_tree_draw_cb drawcb,
    mode_tree_search_cb searchcb, void *modedata, const char **sort_list,
    u_int sort_size, struct screen **s)
{
	struct mode_tree_data	*mtd;
	const char		*sort;
	u_int			 i;

	mtd = xcalloc(1, sizeof *mtd);
	mtd->references = 1;

	mtd->wp = wp;
	mtd->modedata = modedata;

	mtd->sort_list = sort_list;
	mtd->sort_size = sort_size;
	mtd->sort_type = 0;

	mtd->preview = !args_has(args, 'N');

	sort = args_get(args, 'O');
	if (sort != NULL) {
		for (i = 0; i < sort_size; i++) {
			if (strcasecmp(sort, sort_list[i]) == 0)
				mtd->sort_type = i;
		}
	}

	if (args_has(args, 'f'))
		mtd->filter = xstrdup(args_get(args, 'f'));
	else
		mtd->filter = NULL;

	mtd->buildcb = buildcb;
	mtd->drawcb = drawcb;
	mtd->searchcb = searchcb;

	TAILQ_INIT(&mtd->children);

	*s = &mtd->screen;
	screen_init(*s, screen_size_x(&wp->base), screen_size_y(&wp->base), 0);
	(*s)->mode &= ~MODE_CURSOR;

	return (mtd);
}

void
mode_tree_zoom(struct mode_tree_data *mtd, struct args *args)
{
	struct window_pane	*wp = mtd->wp;

	if (args_has(args, 'Z')) {
		mtd->zoomed = (wp->window->flags & WINDOW_ZOOMED);
		if (!mtd->zoomed && window_zoom(wp) == 0)
			server_redraw_window(wp->window);
	} else
		mtd->zoomed = -1;
}

void
mode_tree_build(struct mode_tree_data *mtd)
{
	struct screen	*s = &mtd->screen;
	uint64_t	 tag;

	if (mtd->line_list != NULL)
		tag = mtd->line_list[mtd->current].item->tag;
	else
		tag = 0;

	TAILQ_CONCAT(&mtd->saved, &mtd->children, entry);
	TAILQ_INIT(&mtd->children);

	mtd->buildcb(mtd->modedata, mtd->sort_type, &tag, mtd->filter);
	mtd->no_matches = TAILQ_EMPTY(&mtd->children);
	if (mtd->no_matches)
		mtd->buildcb(mtd->modedata, mtd->sort_type, &tag, NULL);

	mode_tree_free_items(&mtd->saved);
	TAILQ_INIT(&mtd->saved);

	mode_tree_clear_lines(mtd);
	mode_tree_build_lines(mtd, &mtd->children, 0);

	mode_tree_set_current(mtd, tag);

	mtd->width = screen_size_x(s);
	if (mtd->preview) {
		mtd->height = (screen_size_y(s) / 3) * 2;
		if (mtd->height > mtd->line_size)
			mtd->height = screen_size_y(s) / 2;
		if (mtd->height < 10)
			mtd->height = screen_size_y(s);
		if (screen_size_y(s) - mtd->height < 2)
			mtd->height = screen_size_y(s);
	} else
		mtd->height = screen_size_y(s);
	mode_tree_check_selected(mtd);
}

static void
mode_tree_remove_ref(struct mode_tree_data *mtd)
{
	if (--mtd->references == 0)
		free(mtd);
}

void
mode_tree_free(struct mode_tree_data *mtd)
{
	struct window_pane	*wp = mtd->wp;

	if (mtd->zoomed == 0)
		server_unzoom_window(wp->window);

	mode_tree_free_items(&mtd->children);
	mode_tree_clear_lines(mtd);
	screen_free(&mtd->screen);

	free(mtd->search);
	free(mtd->filter);

	mtd->dead = 1;
	mode_tree_remove_ref(mtd);
}

void
mode_tree_resize(struct mode_tree_data *mtd, u_int sx, u_int sy)
{
	struct screen	*s = &mtd->screen;

	screen_resize(s, sx, sy, 0);

	mode_tree_build(mtd);
	mode_tree_draw(mtd);

	mtd->wp->flags |= PANE_REDRAW;
}

struct mode_tree_item *
mode_tree_add(struct mode_tree_data *mtd, struct mode_tree_item *parent,
    void *itemdata, uint64_t tag, const char *name, const char *text,
    int expanded)
{
	struct mode_tree_item	*mti, *saved;

	log_debug("%s: %llu, %s %s", __func__, (unsigned long long)tag,
	    name, text);

	mti = xcalloc(1, sizeof *mti);
	mti->parent = parent;
	mti->itemdata = itemdata;

	mti->tag = tag;
	mti->name = xstrdup(name);
	mti->text = xstrdup(text);

	saved = mode_tree_find_item(&mtd->saved, tag);
	if (saved != NULL) {
		if (parent == NULL || (parent != NULL && parent->expanded))
			mti->tagged = saved->tagged;
		mti->expanded = saved->expanded;
	} else if (expanded == -1)
		mti->expanded = 1;
	else
		mti->expanded = expanded;

	TAILQ_INIT(&mti->children);

	if (parent != NULL)
		TAILQ_INSERT_TAIL(&parent->children, mti, entry);
	else
		TAILQ_INSERT_TAIL(&mtd->children, mti, entry);

	return (mti);
}

void
mode_tree_remove(struct mode_tree_data *mtd, struct mode_tree_item *mti)
{
	struct mode_tree_item	*parent = mti->parent;

	if (parent != NULL)
		TAILQ_REMOVE(&parent->children, mti, entry);
	else
		TAILQ_REMOVE(&mtd->children, mti, entry);
	mode_tree_free_item(mti);
}

void
mode_tree_draw(struct mode_tree_data *mtd)
{
	struct window_pane	*wp = mtd->wp;
	struct screen		*s = &mtd->screen;
	struct mode_tree_line	*line;
	struct mode_tree_item	*mti;
	struct options		*oo = wp->window->options;
	struct screen_write_ctx	 ctx;
	struct grid_cell	 gc0, gc;
	u_int			 w, h, i, j, sy, box_x, box_y;
	char			*text, *start, key[7];
	const char		*tag, *symbol;
	size_t			 size, n;
	int			 keylen;

	if (mtd->line_size == 0)
		return;

	memcpy(&gc0, &grid_default_cell, sizeof gc0);
	memcpy(&gc, &grid_default_cell, sizeof gc);
	style_apply(&gc, oo, "mode-style");

	w = mtd->width;
	h = mtd->height;

	screen_write_start(&ctx, NULL, s);
	screen_write_clearscreen(&ctx, 8);

	if (mtd->line_size > 10)
		keylen = 6;
	else
		keylen = 4;

	for (i = 0; i < mtd->line_size; i++) {
		if (i < mtd->offset)
			continue;
		if (i > mtd->offset + h - 1)
			break;

		line = &mtd->line_list[i];
		mti = line->item;

		screen_write_cursormove(&ctx, 0, i - mtd->offset);

		if (i < 10)
			snprintf(key, sizeof key, "(%c)  ", '0' + i);
		else if (i < 36)
			snprintf(key, sizeof key, "(M-%c)", 'a' + (i - 10));
		else
			*key = '\0';

		if (line->flat)
			symbol = "";
		else if (TAILQ_EMPTY(&mti->children))
			symbol = "  ";
		else if (mti->expanded)
			symbol = "- ";
		else
			symbol = "+ ";

		if (line->depth == 0)
			start = xstrdup(symbol);
		else {
			size = (4 * line->depth) + 32;

			start = xcalloc(1, size);
			for (j = 1; j < line->depth; j++) {
				if (mti->parent != NULL &&
				    mtd->line_list[mti->parent->line].last)
					strlcat(start, "    ", size);
				else
					strlcat(start, "\001x\001   ", size);
			}
			if (line->last)
				strlcat(start, "\001mq\001> ", size);
			else
				strlcat(start, "\001tq\001> ", size);
			strlcat(start, symbol, size);
		}

		if (mti->tagged)
			tag = "*";
		else
			tag = "";
		xasprintf(&text, "%-*s%s%s%s: %s", keylen, key, start,
		    mti->name, tag, mti->text);
		free(start);

		if (mti->tagged) {
			gc.attr ^= GRID_ATTR_BRIGHT;
			gc0.attr ^= GRID_ATTR_BRIGHT;
		}

		if (i != mtd->current) {
			screen_write_nputs(&ctx, w, &gc0, "%s", text);
			screen_write_clearendofline(&ctx, 8);
		} else {
			screen_write_nputs(&ctx, w, &gc, "%s", text);
			screen_write_clearendofline(&ctx, gc.bg);
		}
		free(text);

		if (mti->tagged) {
			gc.attr ^= GRID_ATTR_BRIGHT;
			gc0.attr ^= GRID_ATTR_BRIGHT;
		}
	}

	sy = screen_size_y(s);
	if (!mtd->preview || sy <= 4 || h <= 4 || sy - h <= 4 || w <= 4) {
		screen_write_stop(&ctx);
		return;
	}

	line = &mtd->line_list[mtd->current];
	mti = line->item;

	screen_write_cursormove(&ctx, 0, h);
	screen_write_box(&ctx, w, sy - h);

	xasprintf(&text, " %s (sort: %s)", mti->name,
	    mtd->sort_list[mtd->sort_type]);
	if (w - 2 >= strlen(text)) {
		screen_write_cursormove(&ctx, 1, h);
		screen_write_puts(&ctx, &gc0, "%s", text);

		if (mtd->no_matches)
			n = (sizeof "no matches") - 1;
		else
			n = (sizeof "active") - 1;
		if (mtd->filter != NULL && w - 2 >= strlen(text) + 10 + n + 2) {
			screen_write_puts(&ctx, &gc0, " (filter: ");
			if (mtd->no_matches)
				screen_write_puts(&ctx, &gc, "no matches");
			else
				screen_write_puts(&ctx, &gc0, "active");
			screen_write_puts(&ctx, &gc0, ") ");
		}
	}
	free(text);

	box_x = w - 4;
	box_y = sy - h - 2;

	if (box_x != 0 && box_y != 0) {
		screen_write_cursormove(&ctx, 2, h + 1);
		mtd->drawcb(mtd->modedata, mti->itemdata, &ctx, box_x, box_y);
	}

	screen_write_stop(&ctx);
}

static struct mode_tree_item *
mode_tree_search_for(struct mode_tree_data *mtd)
{
	struct mode_tree_item	*mti, *last, *next;

	if (mtd->search == NULL)
		return (NULL);

	mti = last = mtd->line_list[mtd->current].item;
	for (;;) {
		if (!TAILQ_EMPTY(&mti->children))
			mti = TAILQ_FIRST(&mti->children);
		else if ((next = TAILQ_NEXT(mti, entry)) != NULL)
			mti = next;
		else {
			for (;;) {
				mti = mti->parent;
				if (mti == NULL)
					break;
				if ((next = TAILQ_NEXT(mti, entry)) != NULL) {
					mti = next;
					break;
				}
			}
		}
		if (mti == NULL)
			mti = TAILQ_FIRST(&mtd->children);
		if (mti == last)
			break;

		if (mtd->searchcb == NULL) {
			if (strstr(mti->name, mtd->search) != NULL)
				return (mti);
			continue;
		}
		if (mtd->searchcb(mtd->modedata, mti->itemdata, mtd->search))
			return (mti);
	}
	return (NULL);
}

static void
mode_tree_search_set(struct mode_tree_data *mtd)
{
	struct mode_tree_item	*mti, *loop;
	uint64_t		 tag;

	mti = mode_tree_search_for(mtd);
	if (mti == NULL)
		return;
	tag = mti->tag;

	loop = mti->parent;
	while (loop != NULL) {
		loop->expanded = 1;
		loop = loop->parent;
	}

	mode_tree_build(mtd);
	mode_tree_set_current(mtd, tag);
	mode_tree_draw(mtd);
	mtd->wp->flags |= PANE_REDRAW;
}

static int
mode_tree_search_callback(__unused struct client *c, void *data, const char *s,
    __unused int done)
{
	struct mode_tree_data	*mtd = data;

	if (mtd->dead)
		return (0);

	free(mtd->search);
	if (s == NULL || *s == '\0') {
		mtd->search = NULL;
		return (0);
	}
	mtd->search = xstrdup(s);
	mode_tree_search_set(mtd);

	return (0);
}

static void
mode_tree_search_free(void *data)
{
	mode_tree_remove_ref(data);
}

static int
mode_tree_filter_callback(__unused struct client *c, void *data, const char *s,
    __unused int done)
{
	struct mode_tree_data	*mtd = data;

	if (mtd->dead)
		return (0);

	if (mtd->filter != NULL)
		free(mtd->filter);
	if (s == NULL || *s == '\0')
		mtd->filter = NULL;
	else
		mtd->filter = xstrdup(s);

	mode_tree_build(mtd);
	mode_tree_draw(mtd);
	mtd->wp->flags |= PANE_REDRAW;

	return (0);
}

static void
mode_tree_filter_free(void *data)
{
	mode_tree_remove_ref(data);
}

int
mode_tree_key(struct mode_tree_data *mtd, struct client *c, key_code *key,
    struct mouse_event *m, u_int *xp, u_int *yp)
{
	struct mode_tree_line	*line;
	struct mode_tree_item	*current, *parent;
	u_int			 i, x, y;
	int			 choice;
	key_code		 tmp;

	if (KEYC_IS_MOUSE(*key)) {
		if (cmd_mouse_at(mtd->wp, m, &x, &y, 0) != 0) {
			*key = KEYC_NONE;
			return (0);
		}
		if (xp != NULL)
			*xp = x;
		if (yp != NULL)
			*yp = y;
		if (x > mtd->width || y > mtd->height) {
			if (!mtd->preview)
				*key = KEYC_NONE;
			return (0);
		}
		if (mtd->offset + y < mtd->line_size) {
			if (*key == KEYC_MOUSEDOWN1_PANE ||
			    *key == KEYC_DOUBLECLICK1_PANE)
				mtd->current = mtd->offset + y;
			if (*key == KEYC_DOUBLECLICK1_PANE)
				*key = '\r';
			else
				*key = KEYC_NONE;
		} else
			*key = KEYC_NONE;
		return (0);
	}

	line = &mtd->line_list[mtd->current];
	current = line->item;

	choice = -1;
	if (*key >= '0' && *key <= '9')
		choice = (*key) - '0';
	else if (((*key) & KEYC_MASK_MOD) == KEYC_ESCAPE) {
		tmp = (*key) & KEYC_MASK_KEY;
		if (tmp >= 'a' && tmp <= 'z')
			choice = 10 + (tmp - 'a');
	}
	if (choice != -1) {
		if ((u_int)choice > mtd->line_size - 1) {
			*key = KEYC_NONE;
			return (0);
		}
		mtd->current = choice;
		*key = '\r';
		return (0);
	}

	switch (*key) {
	case 'q':
	case '\033': /* Escape */
	case '\007': /* C-g */
		return (1);
	case KEYC_UP:
	case 'k':
	case KEYC_WHEELUP_PANE:
	case '\020': /* C-p */
		mode_tree_up(mtd, 1);
		break;
	case KEYC_DOWN:
	case 'j':
	case KEYC_WHEELDOWN_PANE:
	case '\016': /* C-n */
		mode_tree_down(mtd, 1);
		break;
	case KEYC_PPAGE:
	case '\002': /* C-b */
		for (i = 0; i < mtd->height; i++) {
			if (mtd->current == 0)
				break;
			mode_tree_up(mtd, 1);
		}
		break;
	case KEYC_NPAGE:
	case '\006': /* C-f */
		for (i = 0; i < mtd->height; i++) {
			if (mtd->current == mtd->line_size - 1)
				break;
			mode_tree_down(mtd, 1);
		}
		break;
	case KEYC_HOME:
		mtd->current = 0;
		mtd->offset = 0;
		break;
	case KEYC_END:
		mtd->current = mtd->line_size - 1;
		if (mtd->current > mtd->height - 1)
			mtd->offset = mtd->current - mtd->height + 1;
		else
			mtd->offset = 0;
		break;
	case 't':
		/*
		 * Do not allow parents and children to both be tagged: untag
		 * all parents and children of current.
		 */
		if (!current->tagged) {
			parent = current->parent;
			while (parent != NULL) {
				parent->tagged = 0;
				parent = parent->parent;
			}
			mode_tree_clear_tagged(&current->children);
			current->tagged = 1;
		} else
			current->tagged = 0;
		mode_tree_down(mtd, 0);
		break;
	case 'T':
		for (i = 0; i < mtd->line_size; i++)
			mtd->line_list[i].item->tagged = 0;
		break;
	case '\024': /* C-t */
		for (i = 0; i < mtd->line_size; i++) {
			if (mtd->line_list[i].item->parent == NULL)
				mtd->line_list[i].item->tagged = 1;
			else
				mtd->line_list[i].item->tagged = 0;
		}
		break;
	case 'O':
		mtd->sort_type++;
		if (mtd->sort_type == mtd->sort_size)
			mtd->sort_type = 0;
		mode_tree_build(mtd);
		break;
	case KEYC_LEFT:
	case 'h':
	case '-':
		if (line->flat || !current->expanded)
			current = current->parent;
		if (current == NULL)
			mode_tree_up(mtd, 0);
		else {
			current->expanded = 0;
			mtd->current = current->line;
			mode_tree_build(mtd);
		}
		break;
	case KEYC_RIGHT:
	case 'l':
	case '+':
		if (line->flat || current->expanded)
			mode_tree_down(mtd, 0);
		else if (!line->flat) {
			current->expanded = 1;
			mode_tree_build(mtd);
		}
		break;
	case '\023': /* C-s */
		mtd->references++;
		status_prompt_set(c, "(search) ", "",
		    mode_tree_search_callback, mode_tree_search_free, mtd,
		    PROMPT_NOFORMAT);
		break;
	case 'n':
		mode_tree_search_set(mtd);
		break;
	case 'f':
		mtd->references++;
		status_prompt_set(c, "(filter) ", mtd->filter,
		    mode_tree_filter_callback, mode_tree_filter_free, mtd,
		    PROMPT_NOFORMAT);
		break;
	case 'v':
		mtd->preview = !mtd->preview;
		mode_tree_build(mtd);
		if (mtd->preview)
			mode_tree_check_selected(mtd);
		break;
	}
	return (0);
}

void
mode_tree_run_command(struct client *c, struct cmd_find_state *fs,
    const char *template, const char *name)
{
	struct cmdq_item	*new_item;
	struct cmd_list		*cmdlist;
	char			*command, *cause;

	command = cmd_template_replace(template, name, 1);
	if (command == NULL || *command == '\0') {
		free(command);
		return;
	}

	cmdlist = cmd_string_parse(command, NULL, 0, &cause);
	if (cmdlist == NULL) {
		if (cause != NULL && c != NULL) {
			*cause = toupper((u_char)*cause);
			status_message_set(c, "%s", cause);
		}
		free(cause);
	} else {
		new_item = cmdq_get_command(cmdlist, fs, NULL, 0);
		cmdq_append(c, new_item);
		cmd_list_free(cmdlist);
	}

	free(command);
}
