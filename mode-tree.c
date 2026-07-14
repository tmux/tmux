/* $OpenBSD: mode-tree.c,v 1.100 2026/07/14 19:07:03 nicm Exp $ */

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

enum mode_tree_search_dir {
	MODE_TREE_SEARCH_FORWARD,
	MODE_TREE_SEARCH_BACKWARD
};

enum mode_tree_preview {
	MODE_TREE_PREVIEW_OFF,
	MODE_TREE_PREVIEW_NORMAL,
	MODE_TREE_PREVIEW_BIG
};

#define MODE_TREE_PREFIX_STYLE \
	"#[fg=themelightgrey]#[bg=default]#[noacs]"

#define MODE_TREE_PREFIX_FORMAT \
	MODE_TREE_PREFIX_STYLE \
	"#{p/#{mode_tree_key_width}:" \
	"#{?#{!=:#{mode_tree_key},},(#{mode_tree_key}),}}" \
	"#{R:#{?mode_tree_parent_last,    ," \
	"#[acs]x" MODE_TREE_PREFIX_STYLE "   }," \
	"#{mode_tree_repeat}}" \
	"#{?mode_tree_branch," \
	"#[acs]#{?mode_tree_last,mq,tq}" MODE_TREE_PREFIX_STYLE "> ,}" \
	"#{?mode_tree_has_children," \
	"#{?mode_tree_expanded,#[fg=themered]-" MODE_TREE_PREFIX_STYLE " ," \
	"#[fg=themegreen]+" MODE_TREE_PREFIX_STYLE " }," \
	"#{?mode_tree_flat,,  }}"

struct mode_tree_item;
struct mode_tree_prompt;
TAILQ_HEAD(mode_tree_list, mode_tree_item);

struct mode_tree_data {
	int			  dead;
	u_int			  references;
	int			  zoomed;

	struct window_pane	 *wp;
	void			 *modedata;
	const struct menu_item	 *menu;

	struct sort_criteria	  sort_crit;
	const char		 *view_name;

	mode_tree_build_cb	  buildcb;
	mode_tree_draw_cb	  drawcb;
	mode_tree_search_cb	  searchcb;
	mode_tree_menu_cb	  menucb;
	mode_tree_height_cb	  heightcb;
	mode_tree_key_cb	  keycb;
	mode_tree_swap_cb	  swapcb;
	mode_tree_sort_cb	  sortcb;
	mode_tree_help_cb	  helpcb;

	struct mode_tree_list	  children;
	struct mode_tree_list	  saved;

	struct mode_tree_line	 *line_list;
	u_int			  line_size;

	u_int			  depth;
	u_int			  maxdepth;

	u_int			  width;
	u_int			  height;

	u_int			  offset;
	u_int			  current;

	struct screen		  screen;
	struct prompt		 *prompt;
	struct mode_tree_prompt	 *prompt_data;
	u_int			  prompt_cx;
	int			  prompt_top;

	int			  preview;
	char			 *search;
	char			 *filter;
	int			  no_matches;
	enum mode_tree_search_dir search_dir;
	int			  search_icase;
	int			  help;
};

struct mode_tree_item {
	struct mode_tree_item		*parent;
	void				*itemdata;
	u_int				 line;

	key_code			 key;
	const char			*keystr;
	size_t				 keylen;

	uint64_t			 tag;
	const char			*name;
	const char			*text;

	int				 expanded;
	int				 tagged;

	int				 draw_as_parent;
	int				 no_tag;
	int				 align;

	struct mode_tree_list		 children;
	TAILQ_ENTRY(mode_tree_item)	 entry;
};

struct mode_tree_line {
	struct mode_tree_item		*item;
	u_int				 depth;
	int				 last;
	int				 flat;
};

struct mode_tree_menu {
	struct mode_tree_data		*data;
	struct client			*c;
	u_int				 line;
};

/*
 * Wrapper around a prompt owned by a mode tree. The mode tree holds a reference
 * while the prompt is alive; the wrapper callbacks forward to the caller's
 * callbacks and drop that reference when the prompt is freed.
 */
struct mode_tree_prompt {
	struct mode_tree_data		*mtd;
	struct client			*c;
	mode_tree_prompt_input_cb	 inputcb;
	prompt_free_cb			 freecb;
	void				*data;
};

static void	mode_tree_free_items(struct mode_tree_list *);
static void	mode_tree_draw_help(struct mode_tree_data *,
		    struct screen_write_ctx *);
static void	mode_tree_draw_prompt(struct mode_tree_data *,
		    struct screen_write_ctx *);
static enum cmd_retval mode_tree_prompt_accept(struct cmdq_item *, void *);

static const struct menu_item mode_tree_menu_items[] = {
	{ "Scroll Left", '<', NULL },
	{ "Scroll Right", '>', NULL },
	{ "", KEYC_NONE, NULL },
	{ "Cancel", 'q', NULL },

	{ NULL, KEYC_NONE, NULL }
};

static const char* mode_tree_help_start[] = {
	"#[fg=themelightgrey]"
	"      Up, k #[#{E:tree-mode-border-style},acs]x#[default] Move cursor up",
	"#[fg=themelightgrey]"
	"    Down, j #[#{E:tree-mode-border-style},acs]x#[default] Move cursor down",
	"#[fg=themelightgrey]"
	"          g #[#{E:tree-mode-border-style},acs]x#[default] Go to top",
	"#[fg=themelightgrey]"
	"          G #[#{E:tree-mode-border-style},acs]x#[default] Go to bottom",
	"#[fg=themelightgrey]"
	" PPage, C-b #[#{E:tree-mode-border-style},acs]x#[default] Page up",
	"#[fg=themelightgrey]"
	" NPage, C-f #[#{E:tree-mode-border-style},acs]x#[default] Page down",
	"#[fg=themelightgrey]"
	"    Left, h #[#{E:tree-mode-border-style},acs]x#[default] Collapse %1",
	"#[fg=themelightgrey]"
	"   Right, l #[#{E:tree-mode-border-style},acs]x#[default] Expand %1",
	"#[fg=themelightgrey]"
	"        M-- #[#{E:tree-mode-border-style},acs]x#[default] Collapse all %1s",
	"#[fg=themelightgrey]"
	"        M-+ #[#{E:tree-mode-border-style},acs]x#[default] Expand all %1s",
	"#[fg=themelightgrey]"
	"          t #[#{E:tree-mode-border-style},acs]x#[default] Toggle %1 tag",
	"#[fg=themelightgrey]"
	"          T #[#{E:tree-mode-border-style},acs]x#[default] Untag all %1s",
	"#[fg=themelightgrey]"
	"        C-t #[#{E:tree-mode-border-style},acs]x#[default] Tag all %1s",
	"#[fg=themelightgrey]"
	"        C-s #[#{E:tree-mode-border-style},acs]x#[default] Search forward",
	"#[fg=themelightgrey]"
	"          n #[#{E:tree-mode-border-style},acs]x#[default] Repeat search forward",
	"#[fg=themelightgrey]"
	"          N #[#{E:tree-mode-border-style},acs]x#[default] Repeat search backward",
	"#[fg=themelightgrey]"
	"          f #[#{E:tree-mode-border-style},acs]x#[default] Filter %1s",
	"#[fg=themelightgrey]"
	"          O #[#{E:tree-mode-border-style},acs]x#[default] Change sort order",
	"#[fg=themelightgrey]"
	"          r #[#{E:tree-mode-border-style},acs]x#[default] Reverse sort order",
	"#[fg=themelightgrey]"
	"          v #[#{E:tree-mode-border-style},acs]x#[default] Toggle preview",
	NULL
};
static const char* mode_tree_help_end[] = {
	"#[fg=themelightgrey]"
	"  q, Escape #[#{E:tree-mode-border-style},acs]x#[default] Exit mode",
	NULL
};
#define MODE_TREE_HELP_DEFAULT_WIDTH 39

static int
mode_tree_is_lowercase(const char *ptr)
{
	while (*ptr != '\0') {
		if (*ptr != tolower((u_char)*ptr))
			return (0);
		++ptr;
	}
	return (1);
}

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
	free((void *)mti->keystr);

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
	if (depth > mtd->maxdepth)
		mtd->maxdepth = depth;
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

		if (mtd->keycb != NULL) {
			mti->key = mtd->keycb(mtd->modedata, mti->itemdata,
			    mti->line);
			if (mti->key == KEYC_UNKNOWN)
				mti->key = KEYC_NONE;
		} else if (mti->line < 10)
			mti->key = '0' + mti->line;
		else if (mti->line < 36)
			mti->key = KEYC_META|('a' + mti->line - 10);
		else
			mti->key = KEYC_NONE;
		if (mti->key != KEYC_NONE) {
			mti->keystr = xstrdup(key_string_lookup_key(mti->key,
			    0));
			mti->keylen = strlen(mti->keystr);
		} else {
			mti->keystr = NULL;
			mti->keylen = 0;
		}
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

void
mode_tree_up(struct mode_tree_data *mtd, int wrap)
{
	if (mtd->line_size == 0)
		return;
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

int
mode_tree_down(struct mode_tree_data *mtd, int wrap)
{
	if (mtd->line_size == 0)
		return (0);
	if (mtd->current == mtd->line_size - 1) {
		if (wrap) {
			mtd->current = 0;
			mtd->offset = 0;
		} else
			return (0);
	} else {
		mtd->current++;
		if (mtd->current > mtd->offset + mtd->height - 1)
			mtd->offset++;
	}
	return (1);
}

static void
mode_tree_swap(struct mode_tree_data *mtd, int direction)
{
	u_int	current_depth = mtd->line_list[mtd->current].depth;
	u_int	swap_with, swap_with_depth;

	if (mtd->swapcb == NULL)
		return;

	/* Find the next line at the same depth with the same parent . */
	swap_with = mtd->current;
	do {
		if (direction < 0 && swap_with < (u_int)-direction)
			return;
		if (direction > 0 && swap_with + direction >= mtd->line_size)
			return;
		swap_with += direction;
		swap_with_depth = mtd->line_list[swap_with].depth;
	} while (swap_with_depth > current_depth);
	if (swap_with_depth != current_depth)
		return;

	if (mtd->swapcb(mtd->line_list[mtd->current].item->itemdata,
	    mtd->line_list[swap_with].item->itemdata, &mtd->sort_crit)) {
		mtd->current = swap_with;
		mode_tree_build(mtd);
	}
}

void *
mode_tree_get_current(struct mode_tree_data *mtd)
{
	if (mtd->line_size == 0)
		return (NULL);
	return (mtd->line_list[mtd->current].item->itemdata);
}

const char *
mode_tree_get_current_name(struct mode_tree_data *mtd)
{
	return (mtd->line_list[mtd->current].item->name);
}

void
mode_tree_select_top(struct mode_tree_data *mtd)
{
	mtd->current = 0;
	mtd->offset = 0;
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
mode_tree_collapse_current(struct mode_tree_data *mtd)
{
	if (mtd->line_list[mtd->current].item->expanded) {
		mtd->line_list[mtd->current].item->expanded = 0;
		mode_tree_build(mtd);
	}
}

static int
mode_tree_get_tag(struct mode_tree_data *mtd, uint64_t tag, u_int *found)
{
	u_int	i;

	for (i = 0; i < mtd->line_size; i++) {
		if (mtd->line_list[i].item->tag == tag)
			break;
	}
	if (i != mtd->line_size) {
		*found = i;
		return (1);
	}
	return (0);
}

void
mode_tree_expand(struct mode_tree_data *mtd, uint64_t tag)
{
	u_int	found;

	if (!mode_tree_get_tag(mtd, tag, &found))
		return;
	if (!mtd->line_list[found].item->expanded) {
		mtd->line_list[found].item->expanded = 1;
		mode_tree_build(mtd);
	}
}

int
mode_tree_set_current(struct mode_tree_data *mtd, uint64_t tag)
{
	u_int	found;

	if (mode_tree_get_tag(mtd, tag, &found)) {
		mtd->current = found;
		if (mtd->current > mtd->height - 1)
			mtd->offset = mtd->current - mtd->height + 1;
		else
			mtd->offset = 0;
		return (1);
	}
	if (mtd->current >= mtd->line_size) {
		if (mtd->line_size == 0)
			return (0);
		mtd->current = mtd->line_size - 1;
		if (mtd->current > mtd->height - 1)
			mtd->offset = mtd->current - mtd->height + 1;
		else
			mtd->offset = 0;
	}
	return (0);
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
    mode_tree_search_cb searchcb, mode_tree_menu_cb menucb,
    mode_tree_height_cb heightcb, mode_tree_key_cb keycb,
    mode_tree_swap_cb swapcb, mode_tree_sort_cb sortcb,
    mode_tree_help_cb helpcb, void *modedata, const struct menu_item *menu,
    struct screen **s)
{
	struct mode_tree_data	*mtd;

	mtd = xcalloc(1, sizeof *mtd);
	mtd->references = 1;

	mtd->wp = wp;
	mtd->modedata = modedata;
	mtd->menu = menu;

	if (drawcb == NULL)
		mtd->preview = MODE_TREE_PREVIEW_OFF;
	else if (args_has(args, 'N') > 1)
		mtd->preview = MODE_TREE_PREVIEW_BIG;
	else if (args_has(args, 'N'))
		mtd->preview = MODE_TREE_PREVIEW_OFF;
	else
		mtd->preview = MODE_TREE_PREVIEW_NORMAL;

	mtd->sort_crit.order = sort_order_from_string(args_get(args, 'O'));
	mtd->sort_crit.reversed = args_has(args, 'r');

	if (args_has(args, 'f'))
		mtd->filter = xstrdup(args_get(args, 'f'));
	else
		mtd->filter = NULL;

	mtd->buildcb = buildcb;
	mtd->drawcb = drawcb;
	mtd->searchcb = searchcb;
	mtd->menucb = menucb;
	mtd->heightcb = heightcb;
	mtd->keycb = keycb;
	mtd->swapcb = swapcb;
	mtd->sortcb = sortcb;
	mtd->helpcb = helpcb;

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

static void
mode_tree_set_height(struct mode_tree_data *mtd)
{
	struct screen	*s = &mtd->screen;
	u_int		 height;

	if (mtd->heightcb != NULL) {
		height = mtd->heightcb(mtd, screen_size_y(s));
		if (height < screen_size_y(s))
		    mtd->height = screen_size_y(s) - height;
	} else {
		if (mtd->preview == MODE_TREE_PREVIEW_NORMAL) {
			mtd->height = (screen_size_y(s) / 3) * 2;
			if (mtd->height > mtd->line_size)
				mtd->height = screen_size_y(s) / 2;
			if (mtd->height < 10)
				mtd->height = screen_size_y(s);
		} else if (mtd->preview == MODE_TREE_PREVIEW_BIG) {
			mtd->height = screen_size_y(s) / 4;
			if (mtd->height > mtd->line_size)
				mtd->height = mtd->line_size;
			if (mtd->height < 2)
				mtd->height = 2;
		} else
			mtd->height = screen_size_y(s);
	}
	if (screen_size_y(s) - mtd->height < 2)
		mtd->height = screen_size_y(s);
}

void
mode_tree_build(struct mode_tree_data *mtd)
{
	struct screen	*s = &mtd->screen;
	uint64_t	 tag;

	if (mtd->line_list != NULL)
		tag = mtd->line_list[mtd->current].item->tag;
	else
		tag = UINT64_MAX;

	TAILQ_CONCAT(&mtd->saved, &mtd->children, entry);
	TAILQ_INIT(&mtd->children);

	if (mtd->sortcb != NULL)
		mtd->sortcb(&mtd->sort_crit);
	mtd->buildcb(mtd->modedata, &mtd->sort_crit, &tag, mtd->filter);
	mtd->no_matches = TAILQ_EMPTY(&mtd->children);
	if (mtd->no_matches)
		mtd->buildcb(mtd->modedata, &mtd->sort_crit, &tag, NULL);

	mode_tree_free_items(&mtd->saved);
	TAILQ_INIT(&mtd->saved);

	mode_tree_clear_lines(mtd);
	mtd->maxdepth = 0;
	mode_tree_build_lines(mtd, &mtd->children, 0);

	if (mtd->line_list != NULL && tag == UINT64_MAX)
		tag = mtd->line_list[mtd->current].item->tag;
	mode_tree_set_current(mtd, tag);

	mtd->width = screen_size_x(s);
	if (mtd->preview != MODE_TREE_PREVIEW_OFF)
		mode_tree_set_height(mtd);
	else
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

	mode_tree_clear_prompt(mtd);
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
	    name, (text == NULL ? "" : text));

	mti = xcalloc(1, sizeof *mti);
	mti->parent = parent;
	mti->itemdata = itemdata;

	mti->tag = tag;
	mti->name = xstrdup(name);
	if (text != NULL)
		mti->text = xstrdup(text);

	saved = mode_tree_find_item(&mtd->saved, tag);
	if (saved != NULL) {
		if (parent == NULL || parent->expanded)
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
mode_tree_view_name(struct mode_tree_data *mtd, const char *name)
{
	mtd->view_name = name;
}

void
mode_tree_draw_as_parent(struct mode_tree_item *mti)
{
	mti->draw_as_parent = 1;
}

void
mode_tree_no_tag(struct mode_tree_item *mti)
{
	mti->no_tag = 1;
}

/*
 * Set the alignment of the item name: -1 to align left, 0 (default) to not
 * align, or 1 to align right.
 */
void
mode_tree_align(struct mode_tree_item *mti, int align)
{
	mti->align = align;
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
	struct format_tree	*ft;
	struct grid_cell	 gc0, gc, box_gc;
	u_int			 w, h, i, sy, box_x, box_y;
	u_int			 width, text_width, prefix_width, left;
	char			*text, *prefix;
	const char		*tag, *separator;
	size_t			 n;
	int			 keylen, alignlen[mtd->maxdepth + 1];
	int			 dfg, dfg0;

	if (mtd->line_size == 0)
		return;

	w = mtd->width;
	h = mtd->height;
	if (w == 0 || h == 0)
		return;

	memcpy(&gc0, &grid_default_cell, sizeof gc0);
	memcpy(&gc, &grid_default_cell, sizeof gc);
	style_apply(&gc, oo, "tree-mode-selection-style", NULL);
	memcpy(&box_gc, &grid_default_cell, sizeof box_gc);
	style_apply(&box_gc, oo, "tree-mode-border-style", NULL);

	dfg = gc.fg;
	dfg0 = gc0.fg;

	screen_write_start(&ctx, s);
	screen_write_clearscreen(&ctx, 8);
	ft = format_create_defaults(NULL, NULL, NULL, NULL, wp);

	keylen = 0;
	for (i = 0; i < mtd->line_size; i++) {
		mti = mtd->line_list[i].item;
		if (mti->key == KEYC_NONE)
			continue;
		if ((int)mti->keylen + 3 > keylen)
			keylen = mti->keylen + 3;
	}

	for (i = 0; i < mtd->maxdepth + 1; i++)
		alignlen[i] = 0;
	for (i = 0; i < mtd->line_size; i++) {
		line = &mtd->line_list[i];
		mti = line->item;
		if (mti->align &&
		    (int)strlen(mti->name) > alignlen[line->depth])
			alignlen[line->depth] = strlen(mti->name);
	}

	for (i = 0; i < mtd->line_size; i++) {
		if (i < mtd->offset)
			continue;
		if (i > mtd->offset + h - 1)
			break;
		line = &mtd->line_list[i];
		mti = line->item;

		screen_write_cursormove(&ctx, 0, i - mtd->offset, 0);

		if (mti->key != KEYC_NONE)
			format_add(ft, "mode_tree_key", "%s", mti->keystr);
		else
			format_add(ft, "mode_tree_key", "%s", "");
		format_add(ft, "mode_tree_key_width", "%d", keylen);
		format_add(ft, "mode_tree_selected", "%d", i == mtd->current);
		if (line->depth == 0) {
			format_add(ft, "mode_tree_repeat", "%u", 0);
			format_add(ft, "mode_tree_branch", "0");
			format_add(ft, "mode_tree_parent_last", "0");
		} else {
			format_add(ft, "mode_tree_repeat", "%u",
			    line->depth - 1);
			format_add(ft, "mode_tree_branch", "1");
			if (mti->parent != NULL &&
			    mtd->line_list[mti->parent->line].last)
				format_add(ft, "mode_tree_parent_last", "1");
			else
				format_add(ft, "mode_tree_parent_last", "0");
		}
		if (TAILQ_EMPTY(&mti->children))
			format_add(ft, "mode_tree_has_children", "0");
		else
			format_add(ft, "mode_tree_has_children", "1");
		format_add(ft, "mode_tree_last", "%d", line->last);
		format_add(ft, "mode_tree_expanded", "%d", mti->expanded);
		format_add(ft, "mode_tree_flat", "%d", line->flat);
		prefix = format_expand(ft, MODE_TREE_PREFIX_FORMAT);
		prefix_width = format_width(prefix);
		if (prefix_width > w)
			prefix_width = w;

		if (mti->tagged)
			tag = "*";
		else
			tag = "";
		if (mti->text != NULL)
			separator = "#[fg=themelightgrey]: #[default]";
		else
			separator = "";
		xasprintf(&text, "%*s%s%s",
		    mti->align * alignlen[line->depth], mti->name, tag, separator);
		text_width = format_width(text);
		left = (prefix_width < w) ? (w - prefix_width) : 0;
		if (text_width > left)
			text_width = left;
		width = prefix_width + text_width;

		if (mti->tagged) {
			gc.fg = COLOUR_THEME_CYAN|COLOUR_FLAG_THEME;
			gc0.fg = COLOUR_THEME_CYAN|COLOUR_FLAG_THEME;
		}

		if (i != mtd->current) {
			screen_write_clearendofline(&ctx, 8);
			format_draw(&ctx, &grid_default_cell, prefix_width,
			    prefix, NULL, 0);
			if (left != 0) {
				screen_write_cursormove(&ctx, prefix_width,
				    i - mtd->offset, 0);
				format_draw(&ctx, &gc0, left, text, NULL, 0);
				if (mti->text != NULL && width < w) {
					screen_write_cursormove(&ctx, width,
					    i - mtd->offset, 0);
					format_draw(&ctx, &gc0, w - width,
					    mti->text, NULL, 0);
				}
			}
		} else {
			screen_write_clearendofline(&ctx, gc.bg);
			format_draw(&ctx, &gc, prefix_width, prefix, NULL, 1);
			if (left != 0) {
				screen_write_cursormove(&ctx, prefix_width,
				    i - mtd->offset, 0);
				format_draw(&ctx, &gc, left, text, NULL, 1);
				if (mti->text != NULL && width < w) {
					screen_write_cursormove(&ctx, width,
					    i - mtd->offset, 0);
					format_draw(&ctx, &gc, w - width,
					    mti->text, NULL, 1);
				}
			}
		}
		free(text);
		free(prefix);

		if (mti->tagged) {
			gc.fg = dfg;
			gc0.fg = dfg0;
		}
	}
	format_free(ft);

	if (mtd->preview == MODE_TREE_PREVIEW_OFF)
		goto done;

	sy = screen_size_y(s);
	if (sy <= 4 || h < 2 || sy - h <= 4 || w <= 4)
		goto done;

	line = &mtd->line_list[mtd->current];
	mti = line->item;
	if (mti->draw_as_parent)
		mti = mti->parent;

	screen_write_cursormove(&ctx, 0, h, 0);
	screen_write_box(&ctx, w, sy - h, BOX_LINES_DEFAULT, &box_gc, NULL);

	if (mtd->sort_crit.order_seq != NULL) {
		xasprintf(&text, " %s (sort: %s%s)%s%s%s", mti->name,
		    sort_order_to_string(mtd->sort_crit.order),
		    mtd->sort_crit.reversed ? ", reversed" : "",
		    mtd->view_name == NULL ? "" : " (view: ",
		    mtd->view_name == NULL ? "" : mtd->view_name,
		    mtd->view_name == NULL ? "" : ")");
	} else
		xasprintf(&text, " %s", mti->name);
	if (w - 2 >= strlen(text)) {
		screen_write_cursormove(&ctx, 1, h, 0);
		screen_write_puts(&ctx, &box_gc, "%s", text);

		if (mtd->no_matches)
			n = (sizeof "no matches") - 1;
		else
			n = (sizeof "active") - 1;
		if (mtd->filter != NULL && w - 2 >= strlen(text) + 10 + n + 2) {
			screen_write_puts(&ctx, &box_gc, " (filter: ");
			if (mtd->no_matches)
				screen_write_puts(&ctx, &box_gc, "no matches");
			else
				screen_write_puts(&ctx, &box_gc, "active");
			screen_write_puts(&ctx, &box_gc, ") ");
		} else
			screen_write_puts(&ctx, &box_gc, " ");
	}
	free(text);

	box_x = w - 4;
	box_y = sy - h - 2;

	if (box_x != 0 && box_y != 0) {
		screen_write_cursormove(&ctx, 2, h + 1, 0);
		mtd->drawcb(mtd->modedata, mti->itemdata, &ctx, box_x, box_y);
	}

done:
	if (mtd->help)
		mode_tree_draw_help(mtd, &ctx);
	if (mtd->prompt != NULL)
		mode_tree_draw_prompt(mtd, &ctx);
	else {
		s->mode &= ~MODE_CURSOR;
		screen_write_cursormove(&ctx, 0, mtd->current - mtd->offset, 0);
	}
	screen_write_stop(&ctx);
}

static void
mode_tree_draw_prompt(struct mode_tree_data *mtd, struct screen_write_ctx *ctx)
{
	struct screen		*s = &mtd->screen;
	struct prompt_draw_data	 pdd;
	u_int			 sx = screen_size_x(s), sy = screen_size_y(s);
	u_int			 py;

	if (sx == 0 || sy == 0)
		return;

	if (mtd->prompt_top)
		py = 0;
	else
		py = sy - 1;

	pdd.ctx = ctx;
	pdd.cursor_x = &mtd->prompt_cx;
	pdd.area_x = 0;
	pdd.area_width = sx;
	pdd.prompt_line = py;

	s->mode |= MODE_CURSOR;
	prompt_draw(mtd->prompt, &pdd);
	screen_write_cursormove(ctx, mtd->prompt_cx, py, 0);
}

void
mode_tree_clear_prompt(struct mode_tree_data *mtd)
{
	struct prompt	*prompt = mtd->prompt;

	if (mtd->prompt != NULL) {
		mtd->prompt = NULL;
		prompt_free(prompt);
		mtd->screen.mode &= ~MODE_CURSOR;
	}
}

int
mode_tree_has_prompt(struct mode_tree_data *mtd)
{
	return (mtd->prompt != NULL);
}

static enum cmd_retval
mode_tree_prompt_accept(struct cmdq_item *item, void *data)
{
	struct mode_tree_data	*mtd = data;
	struct client		*c = cmdq_get_client(item);
	key_code		 key = 'y';

	if (mtd->prompt != NULL && c != NULL)
		mode_tree_key(mtd, c, &key, NULL, NULL, NULL);

	mode_tree_remove_ref(mtd);
	return (CMD_RETURN_NORMAL);
}

static enum prompt_result
mode_tree_prompt_input_callback(void *data, const char *s,
    enum prompt_key_result key)
{
	struct mode_tree_prompt	*mtp = data;

	if (mtp->inputcb != NULL)
		return (mtp->inputcb(mtp->c, mtp->data, s, key));
	return (PROMPT_CLOSE);
}

static void
mode_tree_prompt_free_callback(void *data)
{
	struct mode_tree_prompt	*mtp = data;

	if (mtp->mtd->prompt_data == mtp)
		mtp->mtd->prompt_data = NULL;
	if (mtp->freecb != NULL)
		mtp->freecb(mtp->data);
	mode_tree_remove_ref(mtp->mtd);
	free(mtp);
}

void
mode_tree_set_prompt(struct mode_tree_data *mtd, struct client *c,
    const char *prompt, const char *input, enum prompt_type type, int flags,
    mode_tree_prompt_input_cb inputcb, prompt_free_cb freecb, void *data)
{
	struct session			*s;
	struct options			*oo;
	struct prompt_create_data	 pd;
	struct mode_tree_prompt		*mtp;

	if (c != NULL && c->session != NULL) {
		s = c->session;
		oo = s->options;
	} else {
		s = NULL;
		oo = global_s_options;
	}

	mode_tree_clear_prompt(mtd);

	mtp = xcalloc(1, sizeof *mtp);
	mtp->mtd = mtd;
	mtp->c = c;
	mtp->inputcb = inputcb;
	mtp->freecb = freecb;
	mtp->data = data;

	mtd->references++;
	mtd->prompt_top = options_get_number(oo, "status-position") == 0;

	memset(&pd, 0, sizeof pd);
	prompt_set_options(&pd, s);
	pd.prompt = prompt;
	pd.input = input;
	pd.type = type;
	pd.flags = flags|PROMPT_ISMODE;
	pd.inputcb = mode_tree_prompt_input_callback;
	pd.freecb = mode_tree_prompt_free_callback;
	pd.data = mtp;
	mtd->prompt = prompt_create(&pd);
	mtd->prompt_data = mtp;

	mode_tree_draw(mtd);
	mtd->wp->flags |= PANE_REDRAW;

	if ((flags & PROMPT_SINGLE) && (flags & PROMPT_ACCEPT) && c != NULL) {
		mtd->references++;
		cmdq_append(c, cmdq_get_callback(mode_tree_prompt_accept, mtd));
	}
}

static struct mode_tree_item *
mode_tree_search_backward(struct mode_tree_data *mtd)
{
	struct mode_tree_item	*mti, *last, *prev;
	int			 icase = mtd->search_icase;

	if (mtd->search == NULL)
		return (NULL);

	mti = last = mtd->line_list[mtd->current].item;
	for (;;) {
		if ((prev = TAILQ_PREV(mti, mode_tree_list, entry)) != NULL) {
			/* Point to the last child in the previous subtree. */
			while (!TAILQ_EMPTY(&prev->children)) {
				prev = TAILQ_LAST(&prev->children,
				    mode_tree_list);
			}
			mti = prev;
		} else {
			/* If prev is NULL, jump to the parent. */
			mti = mti->parent;
		}

		if (mti == NULL) {
			/* Point to the last child in the last root subtree. */
			prev = TAILQ_LAST(&mtd->children, mode_tree_list);
			while (!TAILQ_EMPTY(&prev->children)) {
				prev = TAILQ_LAST(&prev->children,
				    mode_tree_list);
			}
			mti = prev;
		}
		if (mti == last)
			break;

		if (mtd->searchcb == NULL) {
			if (!icase && strstr(mti->name, mtd->search) != NULL)
				return (mti);
			if (icase && strcasestr(mti->name, mtd->search) != NULL)
				return (mti);
			continue;
		}
		if (mtd->searchcb(mtd->modedata, mti->itemdata, mtd->search,
		    icase))
			return (mti);
	}
	return (NULL);
}

static struct mode_tree_item *
mode_tree_search_forward(struct mode_tree_data *mtd)
{
	struct mode_tree_item	*mti, *last, *next;
	int			 icase = mtd->search_icase;

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
			if (!icase && strstr(mti->name, mtd->search) != NULL)
				return (mti);
			if (icase && strcasestr(mti->name, mtd->search) != NULL)
				return (mti);
			continue;
		}
		if (mtd->searchcb(mtd->modedata, mti->itemdata, mtd->search,
		    icase))
			return (mti);
	}
	return (NULL);
}

static void
mode_tree_search_set(struct mode_tree_data *mtd)
{
	struct mode_tree_item	*mti, *loop;
	uint64_t		 tag;

	if (mtd->search_dir == MODE_TREE_SEARCH_FORWARD)
		mti = mode_tree_search_forward(mtd);
	else
		mti = mode_tree_search_backward(mtd);
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

static enum prompt_result
mode_tree_search_callback(__unused struct client *c, void *data, const char *s,
    enum prompt_key_result key)
{
	struct mode_tree_data	*mtd = data;

	if (mtd->dead)
		return (PROMPT_CLOSE);

	free(mtd->search);
	if (s == NULL || *s == '\0')
		mtd->search = NULL;
	else {
		mtd->search = xstrdup(s);
		mtd->search_icase = mode_tree_is_lowercase(s);
		mode_tree_search_set(mtd);
	}

	if (key == PROMPT_KEY_HANDLED)
		return (PROMPT_CONTINUE);
	return (PROMPT_CLOSE);
}

static enum prompt_result
mode_tree_filter_callback(__unused struct client *c, void *data, const char *s,
    enum prompt_key_result key)
{
	struct mode_tree_data	*mtd = data;

	if (mtd->dead)
		return (PROMPT_CLOSE);

	if (mtd->filter != NULL)
		free(mtd->filter);
	if (s == NULL || *s == '\0')
		mtd->filter = NULL;
	else
		mtd->filter = xstrdup(s);

	mode_tree_build(mtd);
	mode_tree_draw(mtd);
	mtd->wp->flags |= PANE_REDRAW;

	if (key == PROMPT_KEY_HANDLED)
		return (PROMPT_CONTINUE);
	return (PROMPT_CLOSE);
}

static void
mode_tree_clear_filter(struct mode_tree_data *mtd)
{
	free(mtd->filter);
	mtd->filter = NULL;

	mode_tree_build(mtd);
	mode_tree_draw(mtd);
	mtd->wp->flags |= PANE_REDRAW;
}

static void
mode_tree_menu_callback(__unused struct menu *menu, __unused u_int idx,
    key_code key, void *data)
{
	struct mode_tree_menu	*mtm = data;
	struct mode_tree_data	*mtd = mtm->data;

	if (mtd->dead || key == KEYC_NONE)
		goto out;

	if (mtm->line >= mtd->line_size)
		goto out;
	mtd->current = mtm->line;
	mtd->menucb(mtd->modedata, mtm->c, key);

out:
	mode_tree_remove_ref(mtd);
	free(mtm);
}

static void
mode_tree_display_menu(struct mode_tree_data *mtd, struct client *c, u_int x,
    u_int y, int outside)
{
	struct mode_tree_item	*mti;
	struct menu		*menu;
	const struct menu_item	*items;
	struct mode_tree_menu	*mtm;
	char			*title;
	u_int			 line;

	if (mtd->offset + y > mtd->line_size - 1)
		line = mtd->current;
	else
		line = mtd->offset + y;
	mti = mtd->line_list[line].item;

	if (!outside) {
		items = mtd->menu;
		xasprintf(&title, "#[align=centre]%s", mti->name);
	} else {
		items = mode_tree_menu_items;
		title = xstrdup("");
	}
	menu = menu_create(title);
	menu_add_items(menu, items, NULL, c, NULL);
	free(title);

	mtm = xmalloc(sizeof *mtm);
	mtm->data = mtd;
	mtm->c = c;
	mtm->line = line;
	mtd->references++;

	if (x >= (menu->width + 4) / 2)
		x -= (menu->width + 4) / 2;
	else
		x = 0;
	x += mtd->wp->xoff;
	y += mtd->wp->yoff;
	if (menu_display(menu, 0, 0, NULL, x, y, c, BOX_LINES_DEFAULT, NULL,
	    NULL, NULL, NULL, mode_tree_menu_callback, mtm) != 0) {
		mode_tree_remove_ref(mtd);
		free(mtm);
		menu_free(menu);
	}
}

static void
mode_tree_draw_help_line(struct screen_write_ctx *ctx,
    const struct grid_cell *gc, struct format_tree *ft, const char *line,
    const char *item, u_int x, u_int y, u_int w)
{
	char	*expanded, *replaced;

	replaced = cmd_template_replace(line, item, 1);
	expanded = format_expand(ft, replaced);
	free(replaced);
	screen_write_cursormove(ctx, x, y, 0);
	screen_write_clearcharacter(ctx, w, gc->bg);
	screen_write_cursormove(ctx, x, y, 0);
	format_draw(ctx, gc, w, expanded, NULL, 0);
	free(expanded);
}

static void
mode_tree_draw_help(struct mode_tree_data *mtd, struct screen_write_ctx *ctx)
{
	struct screen		 *s = &mtd->screen;
	struct options		 *oo = mtd->wp->window->options;
	struct grid_cell	  box_gc, gc;
	struct format_tree	 *ft;
	const char		**line, **lines = NULL, *item = "item";
	u_int			  sx = screen_size_x(s), sy = screen_size_y(s);
	u_int			  x, y, w, h = 0, box_w, box_h;

	if (mtd->helpcb == NULL)
		w = MODE_TREE_HELP_DEFAULT_WIDTH;
	else {
		lines = mtd->helpcb(&w, &item);
		if (w < MODE_TREE_HELP_DEFAULT_WIDTH)
			w = MODE_TREE_HELP_DEFAULT_WIDTH;
	}
	for (line = mode_tree_help_start; *line != NULL; line++)
		h++;
	for (line = lines; line != NULL && *line != NULL; line++)
		h++;
	for (line = mode_tree_help_end; *line != NULL; line++)
		h++;

	box_w = w + 2;
	box_h = h + 2;
	if (sx < box_w || sy < box_h)
		return;
	x = (sx - box_w) / 2;
	y = (sy - box_h) / 2;

	memcpy(&box_gc, &grid_default_cell, sizeof box_gc);
	style_apply(&box_gc, oo, "tree-mode-border-style", NULL);
	memcpy(&gc, &grid_default_cell, sizeof gc);
	ft = format_create_defaults(NULL, NULL, NULL, NULL, mtd->wp);
	screen_write_cursormove(ctx, x, y, 0);
	screen_write_box(ctx, box_w, box_h, BOX_LINES_DEFAULT, &box_gc, NULL);

	y++;
	x++;
	for (line = mode_tree_help_start; *line != NULL; line++, y++)
		mode_tree_draw_help_line(ctx, &gc, ft, *line, item, x, y, w);
	for (line = lines; line != NULL && *line != NULL; line++, y++)
		mode_tree_draw_help_line(ctx, &gc, ft, *line, item, x, y, w);
	for (line = mode_tree_help_end; *line != NULL; line++, y++)
		mode_tree_draw_help_line(ctx, &gc, ft, *line, item, x, y, w);
	format_free(ft);
}

static void
mode_tree_display_help(struct mode_tree_data *mtd)
{
	mtd->help = 1;
	mode_tree_draw(mtd);
}

int
mode_tree_key(struct mode_tree_data *mtd, struct client *c, key_code *key,
    struct mouse_event *m, u_int *xp, u_int *yp)
{
	struct mode_tree_line	*line;
	struct mode_tree_item	*current, *parent, *mti;
	u_int			 i, x, y, py, sx;
	int			 choice, preview;
	enum prompt_key_result	 result;
	int			 redraw;
	struct prompt		*prompt;
	struct mode_tree_prompt	*mtp;

	if (mtd->line_size == 0) {
		*key = KEYC_NONE;
		return (1);
	}

	if (mtd->prompt != NULL) {
		redraw = 0;
		prompt = mtd->prompt;

		mtp = mtd->prompt_data;
		if (mtp != NULL)
			mtp->c = c;
		if (KEYC_IS_MOUSE(*key)) {
			if (m == NULL ||
			    MOUSE_BUTTONS(m->b) != MOUSE_BUTTON_1 ||
			    MOUSE_DRAG(m->b) || MOUSE_RELEASE(m->b) ||
			    cmd_mouse_at(mtd->wp, m, &x, &y, 0) != 0)
				result = PROMPT_KEY_NOT_HANDLED;
			else {
				sx = screen_size_x(&mtd->screen);
				if (mtd->prompt_top)
					py = 0;
				else
					py = screen_size_y(&mtd->screen) - 1;
				if (y == py) {
					result = prompt_mouse(prompt, x, 0, sx,
					    &redraw);
				} else
					result = PROMPT_KEY_NOT_HANDLED;
			}
		} else
			result = prompt_key(prompt, *key, &redraw);
		if (mtd->prompt_data == mtp && mtp != NULL)
			mtp->c = NULL;

		/*
		 * Only an explicit close or the prompt marking itself closed
		 * ends it; cursor movement and editing keep it open.
		 */
		if (mtd->prompt == prompt &&
		    (result == PROMPT_KEY_CLOSE || prompt_closed(prompt)))
			mode_tree_clear_prompt(mtd);

		if (redraw || mtd->prompt != prompt) {
			mode_tree_draw(mtd);
			mtd->wp->flags |= PANE_REDRAW;
		}
		if (result != PROMPT_KEY_NOT_HANDLED) {
			*key = KEYC_NONE;
			return (0);
		}
	}

	if (mtd->help) {
		if (KEYC_IS_MOUSE(*key)) {
			*key = KEYC_NONE;
			return (0);
		}
		if (*key == KEYC_FOCUS_IN || *key == KEYC_FOCUS_OUT) {
			*key = KEYC_NONE;
			return (0);
		}
		mtd->help = 0;
		mode_tree_draw(mtd);
		*key = KEYC_NONE;
		return (0);
	}

	if (KEYC_IS_MOUSE(*key) && m != NULL) {
		if (cmd_mouse_at(mtd->wp, m, &x, &y, 0) != 0) {
			*key = KEYC_NONE;
			return (0);
		}
		if (xp != NULL)
			*xp = x;
		if (yp != NULL)
			*yp = y;
		if (x > mtd->width || y > mtd->height) {
			preview = mtd->preview;
			if (*key == KEYC_MOUSEDOWN3_PANE)
				mode_tree_display_menu(mtd, c, x, y, 1);
			if (preview == MODE_TREE_PREVIEW_OFF)
				*key = KEYC_NONE;
			return (0);
		}
		if (mtd->offset + y < mtd->line_size) {
			if (*key == KEYC_MOUSEDOWN1_PANE ||
			    *key == KEYC_MOUSEDOWN3_PANE ||
			    *key == KEYC_DOUBLECLICK1_PANE)
				mtd->current = mtd->offset + y;
			if (*key == KEYC_DOUBLECLICK1_PANE)
				*key = '\r';
			else {
				if (*key == KEYC_MOUSEDOWN3_PANE)
					mode_tree_display_menu(mtd, c, x, y, 0);
				*key = KEYC_NONE;
			}
		} else {
			if (*key == KEYC_MOUSEDOWN3_PANE)
				mode_tree_display_menu(mtd, c, x, y, 0);
			*key = KEYC_NONE;
		}
		return (0);
	}

	line = &mtd->line_list[mtd->current];
	current = line->item;

	choice = -1;
	for (i = 0; i < mtd->line_size; i++) {
		if (*key == mtd->line_list[i].item->key) {
			choice = i;
			break;
		}
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
	case '['|KEYC_CTRL:
	case 'g'|KEYC_CTRL:
		return (1);
	case KEYC_F1:
	case 'h'|KEYC_CTRL:
		mode_tree_display_help(mtd);
		break;
	case KEYC_UP:
	case 'k':
	case KEYC_WHEELUP_PANE:
	case 'p'|KEYC_CTRL:
		mode_tree_up(mtd, 1);
		break;
	case KEYC_DOWN:
	case 'j':
	case KEYC_WHEELDOWN_PANE:
	case 'n'|KEYC_CTRL:
		mode_tree_down(mtd, 1);
		break;
	case KEYC_UP|KEYC_SHIFT:
	case 'K':
		mode_tree_swap(mtd, -1);
		break;
	case KEYC_DOWN|KEYC_SHIFT:
	case 'J':
		mode_tree_swap(mtd, 1);
		break;
	case KEYC_PPAGE:
	case 'b'|KEYC_CTRL:
		for (i = 0; i < mtd->height; i++) {
			if (mtd->current == 0)
				break;
			mode_tree_up(mtd, 1);
		}
		break;
	case KEYC_NPAGE:
	case 'f'|KEYC_CTRL:
		for (i = 0; i < mtd->height; i++) {
			if (mtd->current == mtd->line_size - 1)
				break;
			mode_tree_down(mtd, 1);
		}
		break;
	case 'g':
	case KEYC_HOME:
		mtd->current = 0;
		mtd->offset = 0;
		break;
	case 'G':
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
		if (current->no_tag)
			break;
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
		if (m != NULL)
			mode_tree_down(mtd, 0);
		break;
	case 'T':
		for (i = 0; i < mtd->line_size; i++)
			mtd->line_list[i].item->tagged = 0;
		break;
	case 't'|KEYC_CTRL:
		for (i = 0; i < mtd->line_size; i++) {
			if ((mtd->line_list[i].item->parent == NULL &&
			    !mtd->line_list[i].item->no_tag) ||
			    (mtd->line_list[i].item->parent != NULL &&
			    mtd->line_list[i].item->parent->no_tag))
				mtd->line_list[i].item->tagged = 1;
			else
				mtd->line_list[i].item->tagged = 0;
		}
		break;
	case 'O':
		sort_next_order(&mtd->sort_crit);
		mode_tree_build(mtd);
		break;
	case 'r':
		mtd->sort_crit.reversed = !mtd->sort_crit.reversed;
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
	case '-'|KEYC_META:
		TAILQ_FOREACH(mti, &mtd->children, entry)
			mti->expanded = 0;
		mode_tree_build(mtd);
		break;
	case '+'|KEYC_META:
		TAILQ_FOREACH(mti, &mtd->children, entry)
			mti->expanded = 1;
		mode_tree_build(mtd);
		break;
	case '?':
	case '/':
	case 's'|KEYC_CTRL:
		mtd->search_dir = MODE_TREE_SEARCH_FORWARD;
		mode_tree_set_prompt(mtd, c, "(search) ", "",
		    PROMPT_TYPE_SEARCH, PROMPT_NOFORMAT,
		    mode_tree_search_callback, NULL, mtd);
		break;
	case 'n':
		mtd->search_dir = MODE_TREE_SEARCH_FORWARD;
		mode_tree_search_set(mtd);
		break;
	case 'N':
		mtd->search_dir = MODE_TREE_SEARCH_BACKWARD;
		mode_tree_search_set(mtd);
		break;
	case 'f':
		mode_tree_set_prompt(mtd, c, "(filter) ", mtd->filter,
		    PROMPT_TYPE_SEARCH, PROMPT_NOFORMAT,
		    mode_tree_filter_callback, NULL, mtd);
		break;
	case 'c':
		mode_tree_clear_prompt(mtd);
		mode_tree_clear_filter(mtd);
		break;
	case 'v':
		switch (mtd->preview) {
		case MODE_TREE_PREVIEW_OFF:
			mtd->preview = MODE_TREE_PREVIEW_BIG;
			break;
		case MODE_TREE_PREVIEW_NORMAL:
			mtd->preview = MODE_TREE_PREVIEW_OFF;
			break;
		case MODE_TREE_PREVIEW_BIG:
			mtd->preview = MODE_TREE_PREVIEW_NORMAL;
			break;
		}
		mode_tree_build(mtd);
		if (mtd->preview != MODE_TREE_PREVIEW_OFF)
			mode_tree_check_selected(mtd);
		break;
	}
	return (0);
}

void
mode_tree_run_command(struct client *c, struct cmd_find_state *fs,
    const char *template, const char *name)
{
	struct cmdq_state	*state;
	char			*command, *error;
	enum cmd_parse_status	 status;

	command = cmd_template_replace(template, name, 1);
	if (command != NULL && *command != '\0') {
		state = cmdq_new_state(fs, NULL, 0);
		status = cmd_parse_and_append(command, NULL, c, state, &error);
		if (status == CMD_PARSE_ERROR) {
			if (c != NULL) {
				*error = toupper((u_char)*error);
				status_message_set(c, -1, 1, 0, 0, "%s", error);
			}
			free(error);
		}
		cmdq_free_state(state);
	}
	free(command);
}
