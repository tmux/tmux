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
resize_window(struct window *w, u_int sx, u_int sy)
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
	window_resize(w, sx, sy);
	log_debug("%s: @%u resized to %u,%u; layout %u,%u", __func__, w->id,
	    sx, sy, w->layout_root->sx, w->layout_root->sy);

	/* Restore the window zoom state. */
	if (zoomed)
		window_zoom(w->active);

	tty_update_window_offset(w);
	server_redraw_window(w);
	notify_window("window-layout-changed", w);
}

static int
ignore_client_size(struct client *c)
{
	if (c->session == NULL)
		return (1);
	if (c->flags & CLIENT_NOSIZEFLAGS)
		return (1);
	if ((c->flags & CLIENT_CONTROL) && (~c->flags & CLIENT_SIZECHANGED))
		return (1);
	return (0);
}

void
default_window_size(struct session *s, struct window *w, u_int *sx, u_int *sy,
    int type)
{
	struct client	*c;
	u_int		 cx, cy;
	const char	*value;

	if (type == -1)
		type = options_get_number(global_w_options, "window-size");
	if (type == WINDOW_SIZE_MANUAL)
		goto manual;

	if (type == WINDOW_SIZE_LARGEST) {
		*sx = *sy = 0;
		TAILQ_FOREACH(c, &clients, entry) {
			if (ignore_client_size(c))
				continue;
			if (w != NULL && !session_has(c->session, w))
				continue;
			if (w == NULL && c->session != s)
				continue;

			cx = c->tty.sx;
			cy = c->tty.sy - status_line_size(c);

			if (cx > *sx)
				*sx = cx;
			if (cy > *sy)
				*sy = cy;
		}
		if (*sx == 0 || *sy == 0)
			goto manual;
	} else {
		*sx = *sy = UINT_MAX;
		TAILQ_FOREACH(c, &clients, entry) {
			if (ignore_client_size(c))
				continue;
			if (w != NULL && !session_has(c->session, w))
				continue;
			if (w == NULL && c->session != s)
				continue;

			cx = c->tty.sx;
			cy = c->tty.sy - status_line_size(c);

			if (cx < *sx)
				*sx = cx;
			if (cy < *sy)
				*sy = cy;
		}
		if (*sx == UINT_MAX || *sy == UINT_MAX)
			goto manual;
	}
	goto done;

manual:
	value = options_get_string(s->options, "default-size");
	if (sscanf(value, "%ux%u", sx, sy) != 2) {
		*sx = 80;
		*sy = 24;
	}

done:
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
recalculate_sizes(void)
{
	struct session	*s;
	struct client	*c;
	struct window	*w;
	u_int		 sx, sy, cx, cy;
	int		 type, current, has, changed;

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
		if (ignore_client_size(c))
			continue;
		if (c->tty.sy <= status_line_size(c))
			c->flags |= CLIENT_STATUSOFF;
		else
			c->flags &= ~CLIENT_STATUSOFF;
		c->session->attached++;
	}

	/* Walk each window and adjust the size. */
	RB_FOREACH(w, windows, &windows) {
		if (w->active == NULL)
			continue;
		log_debug("%s: @%u is %u,%u", __func__, w->id, w->sx, w->sy);

		type = options_get_number(w->options, "window-size");
		if (type == WINDOW_SIZE_MANUAL)
			continue;
		current = options_get_number(w->options, "aggressive-resize");

		changed = 1;
		if (type == WINDOW_SIZE_LARGEST) {
			sx = sy = 0;
			TAILQ_FOREACH(c, &clients, entry) {
				if (ignore_client_size(c))
					continue;
				s = c->session;

				if (current)
					has = (s->curw->window == w);
				else
					has = session_has(s, w);
				if (!has)
					continue;

				cx = c->tty.sx;
				cy = c->tty.sy - status_line_size(c);

				if (cx > sx)
					sx = cx;
				if (cy > sy)
					sy = cy;
			}
			if (sx == 0 || sy == 0)
				changed = 0;
		} else {
			sx = sy = UINT_MAX;
			TAILQ_FOREACH(c, &clients, entry) {
				if (ignore_client_size(c))
					continue;
				s = c->session;

				if (current)
					has = (s->curw->window == w);
				else
					has = session_has(s, w);
				if (!has)
					continue;

				cx = c->tty.sx;
				cy = c->tty.sy - status_line_size(c);

				if (cx < sx)
					sx = cx;
				if (cy < sy)
					sy = cy;
			}
			if (sx == UINT_MAX || sy == UINT_MAX)
				changed = 0;
		}
		if (w->sx == sx && w->sy == sy)
			changed = 0;

		if (!changed) {
			tty_update_window_offset(w);
			continue;
		}
		log_debug("%s: @%u changed to %u,%u", __func__, w->id, sx, sy);
		resize_window(w, sx, sy);
	}
}
