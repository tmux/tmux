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
default_window_size(struct client *c, struct session *s, struct window *w,
    u_int *sx, u_int *sy, u_int *xpixel, u_int *ypixel, int type)
{
	struct client	*loop;
	u_int		 cx, cy, n;
	const char	*value;

	if (type == -1)
		type = options_get_number(global_w_options, "window-size");
	switch (type) {
	case WINDOW_SIZE_LARGEST:
		*sx = *sy = 0;
		*xpixel = *ypixel = 0;
		TAILQ_FOREACH(loop, &clients, entry) {
			if (ignore_client_size(loop))
				continue;
			if (w != NULL && !session_has(loop->session, w))
				continue;
			if (w == NULL && loop->session != s)
				continue;

			cx = loop->tty.sx;
			cy = loop->tty.sy - status_line_size(loop);

			if (cx > *sx)
				*sx = cx;
			if (cy > *sy)
				*sy = cy;

			if (loop->tty.xpixel > *xpixel &&
			    loop->tty.ypixel > *ypixel) {
				*xpixel = loop->tty.xpixel;
				*ypixel = loop->tty.ypixel;
			}
		}
		if (*sx == 0 || *sy == 0)
			goto manual;
		break;
	case WINDOW_SIZE_SMALLEST:
		*sx = *sy = UINT_MAX;
		*xpixel = *ypixel = 0;
		TAILQ_FOREACH(loop, &clients, entry) {
			if (ignore_client_size(loop))
				continue;
			if (w != NULL && !session_has(loop->session, w))
				continue;
			if (w == NULL && loop->session != s)
				continue;

			cx = loop->tty.sx;
			cy = loop->tty.sy - status_line_size(loop);

			if (cx < *sx)
				*sx = cx;
			if (cy < *sy)
				*sy = cy;

			if (loop->tty.xpixel > *xpixel &&
			    loop->tty.ypixel > *ypixel) {
				*xpixel = loop->tty.xpixel;
				*ypixel = loop->tty.ypixel;
			}
		}
		if (*sx == UINT_MAX || *sy == UINT_MAX)
			goto manual;
		break;
	case WINDOW_SIZE_LATEST:
		if (c != NULL && !ignore_client_size(c)) {
			*sx = c->tty.sx;
			*sy = c->tty.sy - status_line_size(c);
			*xpixel = c->tty.xpixel;
		        *ypixel = c->tty.ypixel;
		} else {
			if (w == NULL)
				goto manual;
			n = 0;
			TAILQ_FOREACH(loop, &clients, entry) {
				if (!ignore_client_size(loop) &&
				    session_has(loop->session, w)) {
					if (++n > 1)
						break;
				}
			}
			*sx = *sy = UINT_MAX;
			*xpixel = *ypixel = 0;
			TAILQ_FOREACH(loop, &clients, entry) {
				if (ignore_client_size(loop))
					continue;
				if (n > 1 && loop != w->latest)
					continue;
				s = loop->session;

				cx = loop->tty.sx;
				cy = loop->tty.sy - status_line_size(loop);

				if (cx < *sx)
					*sx = cx;
				if (cy < *sy)
					*sy = cy;

				if (loop->tty.xpixel > *xpixel &&
				    loop->tty.ypixel > *ypixel) {
					*xpixel = loop->tty.xpixel;
					*ypixel = loop->tty.ypixel;
				}
			}
			if (*sx == UINT_MAX || *sy == UINT_MAX)
				goto manual;
		}
		break;
	case WINDOW_SIZE_MANUAL:
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
recalculate_size(struct window *w)
{
	struct session	*s;
	struct client	*c;
	u_int		 sx, sy, cx, cy, xpixel = 0, ypixel = 0, n;
	int		 type, current, has, changed;

	if (w->active == NULL)
		return;
	log_debug("%s: @%u is %u,%u", __func__, w->id, w->sx, w->sy);

	type = options_get_number(w->options, "window-size");
	current = options_get_number(w->options, "aggressive-resize");

	changed = 1;
	switch (type) {
	case WINDOW_SIZE_LARGEST:
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

			if (c->tty.xpixel > xpixel && c->tty.ypixel > ypixel) {
				xpixel = c->tty.xpixel;
				ypixel = c->tty.ypixel;
			}
		}
		if (sx == 0 || sy == 0)
			changed = 0;
		break;
	case WINDOW_SIZE_SMALLEST:
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

			if (c->tty.xpixel > xpixel && c->tty.ypixel > ypixel) {
				xpixel = c->tty.xpixel;
				ypixel = c->tty.ypixel;
			}
		}
		if (sx == UINT_MAX || sy == UINT_MAX)
			changed = 0;
		break;
	case WINDOW_SIZE_LATEST:
		n = 0;
		TAILQ_FOREACH(c, &clients, entry) {
			if (!ignore_client_size(c) &&
			    session_has(c->session, w)) {
				if (++n > 1)
					break;
			}
		}
		sx = sy = UINT_MAX;
		TAILQ_FOREACH(c, &clients, entry) {
			if (ignore_client_size(c))
				continue;
			if (n > 1 && c != w->latest)
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

			if (c->tty.xpixel > xpixel && c->tty.ypixel > ypixel) {
				xpixel = c->tty.xpixel;
				ypixel = c->tty.ypixel;
			}
		}
		if (sx == UINT_MAX || sy == UINT_MAX)
			changed = 0;
		break;
	case WINDOW_SIZE_MANUAL:
		changed = 0;
		break;
	}
	if (changed && w->sx == sx && w->sy == sy)
		changed = 0;

	if (!changed) {
		tty_update_window_offset(w);
		return;
	}
	log_debug("%s: @%u changed to %u,%u (%ux%u)", __func__, w->id, sx, sy,
	    xpixel, ypixel);
	resize_window(w, sx, sy, xpixel, ypixel);
}

void
recalculate_sizes(void)
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
		if (ignore_client_size(c))
			continue;
		s = c->session;
		if (c->tty.sy <= s->statuslines || (c->flags & CLIENT_CONTROL))
			c->flags |= CLIENT_STATUSOFF;
		else
			c->flags &= ~CLIENT_STATUSOFF;
		s->attached++;
	}

	/* Walk each window and adjust the size. */
	RB_FOREACH(w, windows, &windows)
		recalculate_size(w);
}
