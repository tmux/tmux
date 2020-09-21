/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <string.h>

#include "tmux.h"

void
resize_window(struct window *w, u_int sx, u_int sy, int xpixel, int ypixel)
{
	int	zoomed;

	/* Check size limits. */
	if (sx < WINDOW_MINIMUM)
		sx = WINDOW_MINIMUM;
	if (sx > WINDOW_MAXIMUM)
		sx = WINDOW_MAXIMUM;
	if (sy < WINDOW_MINIMUM)
		sy = WINDOW_MINIMUM;
	if (sy > WINDOW_MAXIMUM)
		sy = WINDOW_MAXIMUM;

	/* If the window is zoomed, unzoom. */
	zoomed = w->flags & WINDOW_ZOOMED;
	if (zoomed)
		window_unzoom(w);

	/* Resize the layout first. */
	layout_resize(w, sx, sy);

	/* Resize the window, it can be no smaller than the layout. */
	if (sx < w->layout_root->sx)
		sx = w->layout_root->sx;
	if (sy < w->layout_root->sy)
		sy = w->layout_root->sy;
	window_resize(w, sx, sy, xpixel, ypixel);
	log_debug("%s: @%u resized to %u,%u; layout %u,%u", __func__, w->id,
	    sx, sy, w->layout_root->sx, w->layout_root->sy);

	/* Restore the window zoom state. */
	if (zoomed)
		window_zoom(w->active);

	tty_update_window_offset(w);
	server_redraw_window(w);
	notify_window("window-layout-changed", w);
	w->flags &= ~WINDOW_RESIZE;
}

static int
ignore_client_size(struct client *c)
{
	struct client	*loop;

	if (c->session == NULL)
		return (1);
	if (c->flags & CLIENT_NOSIZEFLAGS)
		return (1);
	if (c->flags & CLIENT_IGNORESIZE) {
		/*
		 * Ignore flagged clients if there are any attached clients
		 * that aren't flagged.
		 */
		TAILQ_FOREACH (loop, &clients, entry) {
			if (loop->session == NULL)
				continue;
			if (loop->flags & CLIENT_NOSIZEFLAGS)
				continue;
			if (~loop->flags & CLIENT_IGNORESIZE)
				return (1);
		}
	}
	if ((c->flags & CLIENT_CONTROL) && (~c->flags & CLIENT_SIZECHANGED))
		return (1);
	return (0);
}

static int
clients_calculate_size(int type, struct window *w,
	int (*skip_client) (struct client *),
	u_int *sx, u_int *sy, u_int *xpixel, u_int *ypixel)
{
	struct client	*loop;
	u_int		 cx, cy, n;

	if (type == WINDOW_SIZE_MANUAL)
		return 0;

	if (type == WINDOW_SIZE_LARGEST)
		*sx = *sy = 0;
	else
		*sx = *sy = UINT_MAX;

	n = *xpixel = *ypixel = 0;

	if (type == WINDOW_SIZE_LATEST) {
		TAILQ_FOREACH(loop, &clients, entry) {
			if (!ignore_client_size(loop) &&
			    session_has(loop->session, w)) {
				if (++n > 1)
					break;
			}
		}
	}

	TAILQ_FOREACH(loop, &clients, entry) {
		if (ignore_client_size(loop))
			continue;

		if (type == WINDOW_SIZE_LATEST) {
			if (n > 1 && loop != w->latest)
				continue;
		}

		if (skip_client(loop))
			continue;

		cx = loop->tty.sx;
		cy = loop->tty.sy - status_line_size(loop);

		if (type == WINDOW_SIZE_LARGEST) {
			if (cx > *sx)
				*sx = cx;
			if (cy > *sy)
				*sy = cy;
		} else {
			if (cx < *sx)
				*sx = cx;
			if (cy < *sy)
				*sy = cy;
		}

		if (loop->tty.xpixel > *xpixel && loop->tty.ypixel > *ypixel) {
			*xpixel = loop->tty.xpixel;
			*ypixel = loop->tty.ypixel;
		}
	}

	if (type == WINDOW_SIZE_LARGEST) {
		if (*sx == 0 || *sy == 0)
			return 0;
	} else {
		if (*sx == UINT_MAX || *sy == UINT_MAX)
			return 0;
	}

	return 1;
}

void
default_window_size(struct client *c, struct session *s, struct window *w,
    u_int *sx, u_int *sy, u_int *xpixel, u_int *ypixel, int type)
{
	const char	*value;
	int		 changed = -1;

	if (type == -1)
		type = options_get_number(global_w_options, "window-size");

	if (type == WINDOW_SIZE_LATEST) {
		if (c != NULL && !ignore_client_size(c)) {
			*sx = c->tty.sx;
			*sy = c->tty.sy - status_line_size(c);
			*xpixel = c->tty.xpixel;
			*ypixel = c->tty.ypixel;
			changed = 1;
		} else if (w == NULL)
			changed = 0;
	}

	if (changed == -1) {
		int skip_client (struct client *loop) {
			if (type == WINDOW_SIZE_LATEST) {
				s = loop->session;
				return 0;
			}
			if (w != NULL && !session_has(loop->session, w))
				return 1;
			if (w == NULL && loop->session != s)
				return 1;
			return 0;
		}

		changed = clients_calculate_size(type, w, skip_client, sx, sy,
			xpixel, ypixel);
	}

	if (changed == 0) {
		value = options_get_string(s->options, "default-size");
		if (sscanf(value, "%ux%u", sx, sy) != 2) {
			*sx = 80;
			*sy = 24;
		}
	}

	if (*sx < WINDOW_MINIMUM)
		*sx = WINDOW_MINIMUM;
	if (*sx > WINDOW_MAXIMUM)
		*sx = WINDOW_MAXIMUM;
	if (*sy < WINDOW_MINIMUM)
		*sy = WINDOW_MINIMUM;
	if (*sy > WINDOW_MAXIMUM)
		*sy = WINDOW_MAXIMUM;
}

void
recalculate_size(struct window *w, int now)
{
	u_int		 sx, sy, xpixel = 0, ypixel = 0;
	int		 type, current, changed;
	int		 skip_client (struct client *loop) {
		if (current)
			return (loop->session->curw->window != w);
		else 
			return (session_has(loop->session, w) == 0);
	}

	if (w->active == NULL)
		return;
	log_debug("%s: @%u is %u,%u", __func__, w->id, w->sx, w->sy);

	type = options_get_number(w->options, "window-size");
	current = options_get_number(w->options, "aggressive-resize");

	changed = clients_calculate_size(type, w, skip_client, &sx, &sy,
		&xpixel, &ypixel);

	if (w->flags & WINDOW_RESIZE) {
		if (!now && changed && w->new_sx == sx && w->new_sy == sy)
			changed = 0;
	} else {
		if (!now && changed && w->sx == sx && w->sy == sy)
			changed = 0;
	}

	if (!changed) {
		tty_update_window_offset(w);
		return;
	}
	log_debug("%s: @%u new size %u,%u", __func__, w->id, sx, sy);
	if (now || type == WINDOW_SIZE_MANUAL)
		resize_window(w, sx, sy, xpixel, ypixel);
	else {
		w->new_sx = sx;
		w->new_sy = sy;
		w->new_xpixel = xpixel;
		w->new_ypixel = ypixel;

		w->flags |= WINDOW_RESIZE;
		tty_update_window_offset(w);
	}
}

void
recalculate_sizes(void)
{
	recalculate_sizes_now(0);
}

void
recalculate_sizes_now(int now)
{
	struct session	*s;
	struct client	*c;
	struct window	*w;

	/*
	 * Clear attached count and update saved status line information for
	 * each session.
	 */
	RB_FOREACH(s, sessions, &sessions) {
		s->attached = 0;
		status_update_cache(s);
	}

	/*
	 * Increment attached count and check the status line size for each
	 * client.
	 */
	TAILQ_FOREACH(c, &clients, entry) {
		s = c->session;
		if (s != NULL && !(c->flags & CLIENT_UNATTACHEDFLAGS))
			s->attached++;
		if (ignore_client_size(c))
			continue;
		if (c->tty.sy <= s->statuslines || (c->flags & CLIENT_CONTROL))
			c->flags |= CLIENT_STATUSOFF;
		else
			c->flags &= ~CLIENT_STATUSOFF;
	}

	/* Walk each window and adjust the size. */
	RB_FOREACH(w, windows, &windows)
		recalculate_size(w, now);
}
