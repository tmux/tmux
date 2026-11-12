/* $OpenBSD: cmd-rotate-window.c,v 1.35 2026/10/09 13:12:14 nicm Exp $ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include "tmux.h"

/*
 * Rotate the panes in a window.
 */

static enum cmd_retval	cmd_rotate_window_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_rotate_window_entry = {
	.name = "rotate-window",
	.description = "Rotate panes in a window.",
	.alias = "rotatew",

	.args = { "Dt:UZ", 0, 0, NULL },
	.usage = "[-DUZ] " CMD_TARGET_WINDOW_USAGE,

	.target = { 't', CMD_FIND_WINDOW, 0 },

	.flags = 0,
	.exec = cmd_rotate_window_exec
};

/* A tiled pane and its original position, in window coordinates. */
struct cmd_rotate_window_slot {
	struct window_pane	*wp; /* original pane */
	struct layout_cell	*lc; /* original tiled position */
	int			 xoff;
	int			 yoff;
	u_int			 sx;
	u_int			 sy;
};

static enum cmd_retval
cmd_rotate_window_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = cmd_get_args(self);
	struct cmd_find_state		*current = cmdq_get_current(item);
	struct cmd_find_state		*target = cmdq_get_target(item);
	struct winlink			*wl = target->wl;
	struct window			*w = wl->window;
	struct window_pane		*wp, *zwp = NULL, **all;
	struct layout_cell		*active = NULL;
	struct cmd_rotate_window_slot	*slots, *slot;
	u_int				 i, j, n, nt = 0, offset = 1;

	if (args_has(args, 'Z'))
		zwp = window_zoomed_pane(w);
	window_unzoom(w, 1);

	/*
	 * Only tiled panes rotate. Floating panes keep their place in the
	 * list and their cell.
	 */
	n = window_count_panes(w, 1);
	all = xcalloc(n, sizeof *all);
	slots = xcalloc(n, sizeof *slots);

	n = 0;
	TAILQ_FOREACH(wp, &w->panes, entry) {
		all[n++] = wp;
		if (!layout_cell_is_tiled(wp->layout_cell))
			continue;
		slot = &slots[nt];
		slot->wp = wp;
		slot->lc = wp->layout_cell;
		slot->xoff = wp->xoff;
		slot->yoff = wp->yoff;
		slot->sx = wp->sx;
		slot->sy = wp->sy;
		if (wp == w->active)
			active = wp->layout_cell;
		nt++;
	}
	if (nt != 0 && args_has(args, 'D'))
		offset = nt - 1;

	/* Put the rotated panes back into the tiled places in the list. */
	for (i = 0; i < n; i++)
		TAILQ_REMOVE(&w->panes, all[i], entry);
	for (i = j = 0; i < n; i++) {
		wp = all[i];
		if (layout_cell_is_tiled(wp->layout_cell)) {
			wp = slots[(j + offset) % nt].wp;
			j++;
		}
		TAILQ_INSERT_TAIL(&w->panes, wp, entry);
	}

	/* Each rotated pane takes over the cell of the place it moved to. */
	for (i = 0; i < nt; i++) {
		slot = &slots[i];
		wp = slots[(i + offset) % nt].wp;
		wp->layout_cell = slot->lc;
		slot->lc->wp = wp;
		wp->xoff = slot->xoff;
		wp->yoff = slot->yoff;
		window_pane_resize(wp, slot->sx, slot->sy);
	}

	if (active != NULL) {
		wp = active->wp;
		if (zwp != NULL)
			window_zoom_move(zwp, wp);
		window_set_active_pane(w, wp, 1);
		cmd_find_from_winlink_pane(current, wl, wp, 0);
	}
	redraw_invalidate_scene(w);
	server_redraw_window(w);

	free(all);
	free(slots);

	return (CMD_RETURN_NORMAL);
}
