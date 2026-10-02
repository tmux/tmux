/* $OpenBSD: cmd-rotate-window.c,v 1.34 2026/06/22 08:47:45 nicm Exp $ */

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
	.alias = "rotatew",

	.args = { "Dt:UZ", 0, 0, NULL },
	.usage = "[-DUZ] " CMD_TARGET_WINDOW_USAGE,

	.target = { 't', CMD_FIND_WINDOW, 0 },

	.flags = 0,
	.exec = cmd_rotate_window_exec
};

struct cmd_rotate_window_slot {
	struct layout_cell	*lc;
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
	struct window_pane		*wp, *zwp = NULL, **all, **tiled;
	struct window_pane		**rotated;
	struct cmd_rotate_window_slot	*slots;
	u_int				 i, j, n = 0, nt = 0;
	int				 active = -1;

	if (args_has(args, 'Z'))
		zwp = window_zoomed_pane(w);

	/*
	 * Only tiled panes rotate. Floating panes keep their place in the
	 * list and their cell.
	 */
	TAILQ_FOREACH(wp, &w->panes, entry)
		n++;
	all = xcalloc(n, sizeof *all);
	tiled = xcalloc(n, sizeof *tiled);
	rotated = xcalloc(n, sizeof *rotated);
	slots = xcalloc(n, sizeof *slots);

	n = 0;
	TAILQ_FOREACH(wp, &w->panes, entry) {
		all[n++] = wp;
		if (!layout_cell_is_tiled(wp->layout_cell))
			continue;
		tiled[nt] = wp;
		slots[nt].lc = wp->layout_cell;
		slots[nt].xoff = wp->xoff;
		slots[nt].yoff = wp->yoff;
		slots[nt].sx = wp->sx;
		slots[nt].sy = wp->sy;
		if (wp == w->active)
			active = nt;
		nt++;
	}
	for (i = 0; i < nt; i++) {
		if (args_has(args, 'D'))
			rotated[i] = tiled[(i + nt - 1) % nt];
		else
			rotated[i] = tiled[(i + 1) % nt];
	}

	/* Put the rotated panes back into the tiled places in the list. */
	for (i = 0; i < n; i++)
		TAILQ_REMOVE(&w->panes, all[i], entry);
	for (i = j = 0; i < n; i++) {
		if (layout_cell_is_tiled(all[i]->layout_cell))
			wp = rotated[j++];
		else
			wp = all[i];
		TAILQ_INSERT_TAIL(&w->panes, wp, entry);
	}

	/* Each rotated pane takes over the cell of the place it moved to. */
	for (i = 0; i < nt; i++) {
		wp = rotated[i];
		wp->layout_cell = slots[i].lc;
		wp->layout_cell->wp = wp;
		wp->xoff = slots[i].xoff;
		wp->yoff = slots[i].yoff;
		window_pane_resize(wp, slots[i].sx, slots[i].sy);
	}

	if (active != -1) {
		wp = rotated[active];
		if (zwp != NULL)
			window_zoom_move(zwp, wp);
		window_set_active_pane(w, wp, 1);
		cmd_find_from_winlink_pane(current, wl, wp, 0);
	}
	redraw_invalidate_scene(w);
	server_redraw_window(w);

	free(all);
	free(tiled);
	free(rotated);
	free(slots);

	return (CMD_RETURN_NORMAL);
}
