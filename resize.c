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

static u_int
clients_with_window(struct window *w)
{
	struct client	*loop;
	u_int		 n = 0;

	TAILQ_FOREACH(loop, &clients, entry) {
		if (ignore_client_size(loop) || !session_has(loop->session, w))
			continue;
		if (++n > 1)
			break;
	}
	return (n);
}

static int
clients_calculate_size(int type, int current, struct client *c,
    struct session *s, struct window *w, int (*skip_client)(struct client *,
    int, int, struct session *, struct window *), u_int *sx, u_int *sy,
    u_int *xpixel, u_int *ypixel)
{
	struct client	*loop;
	u_int		 cx, cy, n = 0;

	/* Manual windows do not have their size changed based on a client. */
	if (type == WINDOW_SIZE_MANUAL) {
		log_debug("%s: type is manual", __func__);
		return (0);
	}

	/*
	 * Start comparing with 0 for largest and UINT_MAX for smallest or
	 * latest.
	 */
	if (type == WINDOW_SIZE_LARGEST)
		*sx = *sy = 0;
	else
		*sx = *sy = UINT_MAX;
	*xpixel = *ypixel = 0;

	/*
	 * For latest, count the number of clients with this window. We only
	 * care if there is more than one.
	 */
	if (type == WINDOW_SIZE_LATEST && w != NULL)
		n = clients_with_window(w);

	/* Loop over the clients and work out the size. */
	TAILQ_FOREACH(loop, &clients, entry) {
		if (loop != c && ignore_client_size(loop)) {
			log_debug("%s: ignoring %s", __func__, loop->name);
			continue;
		}
		if (loop != c && skip_client(loop, type, current, s, w)) {
			log_debug("%s: skipping %s", __func__, loop->name);
			continue;
		}

		/*
		 * If there are multiple clients attached, only accept the
		 * latest client; otherwise let the only client be chosen as
		 * for smallest.
		 */
		if (type == WINDOW_SIZE_LATEST && n > 1 && loop != w->latest) {
			log_debug("%s: %s is not latest", __func__, loop->name);
			continue;
		}

		/* Work out this client's size. */
		cx = loop->tty.sx;
		cy = loop->tty.sy - status_line_size(loop);

		/*
		 * If it is larger or smaller than the best so far, update the
		 * new size.
		 */
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
		log_debug("%s: after %s (%ux%u), size is %ux%u", __func__,
		    loop->name, cx, cy, *sx, *sy);
	}

	/* Return whether a suitable size was found. */
	if (type == WINDOW_SIZE_LARGEST) {
		log_debug("%s: type is largest", __func__);
		return (*sx != 0 && *sy != 0);
	}
	if (type == WINDOW_SIZE_LATEST)
		log_debug("%s: type is latest", __func__);
	else
		log_debug("%s: type is smallest", __func__);
	return (*sx != UINT_MAX && *sy != UINT_MAX);
}

static int
default_window_size_skip_client(struct client *loop, int type,
    __unused int current, struct session *s, struct window *w)
{
	/*
	 * Latest checks separately, so do not check here. Otherwise only
	 * include clients where the session contains the window or where the
	 * session is the given session.
	 */
	if (type == WINDOW_SIZE_LATEST)
		return (0);
	if (w != NULL && !session_has(loop->session, w))
		return (1);
	if (w == NULL && loop->session != s)
		return (1);
	return (0);
}

void
default_window_size(struct client *c, struct session *s, struct window *w,
	u_int *sx, u_int *sy, u_int *xpixel, u_int *ypixel, int type)
{
	const char	*value;

	/* Get type if not provided. */
	if (type == -1)
		type = options_get_number(global_w_options, "window-size");

	/*
	 * Latest clients can use the given client if suitable. If there is no
	 * client and no window, use the default size as for manual type.
	 */
	if (type == WINDOW_SIZE_LATEST) {
		if (c != NULL && !ignore_client_size(c)) {
			*sx = c->tty.sx;
			*sy = c->tty.sy - status_line_size(c);
			*xpixel = c->tty.xpixel;
			*ypixel = c->tty.ypixel;
			log_debug("%s: using %ux%u from %s", __func__, *sx, *sy,
			    c->name);
			goto done;
		}
	}

	/*
	 * Look for a client to base the size on. If none exists (or the type
	 * is manual), use the default-size option.
	 */
	if (!clients_calculate_size(type, 0, c, s, w,
	    default_window_size_skip_client, sx, sy, xpixel, ypixel)) {
		value = options_get_string(s->options, "default-size");
		if (sscanf(value, "%ux%u", sx, sy) != 2) {
			*sx = 80;
			*sy = 24;
		}
		log_debug("%s: using %ux%u from default-size", __func__, *sx,
		    *sy);
	}

done:
	/* Make sure the limits are enforced. */
	if (*sx < WINDOW_MINIMUM)
		*sx = WINDOW_MINIMUM;
	if (*sx > WINDOW_MAXIMUM)
		*sx = WINDOW_MAXIMUM;
	if (*sy < WINDOW_MINIMUM)
		*sy = WINDOW_MINIMUM;
	if (*sy > WINDOW_MAXIMUM)
		*sy = WINDOW_MAXIMUM;
	log_debug("%s: resulting size is %ux%u", __func__, *sx, *sy);
}

static int
recalculate_size_skip_client(struct client *loop, __unused int type,
    int current, __unused struct session *s, struct window *w)
{
	/*
	 * If the current flag is set, then skip any client where this window
	 * is not the current window - this is used for aggressive-resize.
	 * Otherwise skip any session that doesn't contain the window.
	 */
	if (current)
		return (loop->session->curw->window != w);
	return (session_has(loop->session, w) == 0);
}

void
recalculate_size(struct window *w, int now)
{
	u_int	sx, sy, xpixel = 0, ypixel = 0;
	int	type, current, changed;

	/*
	 * Do not attempt to resize windows which have no pane, they must be on
	 * the way to destruction.
	 */
	if (w->active == NULL)
		return;
	log_debug("%s: @%u is %u,%u", __func__, w->id, w->sx, w->sy);

	/*
	 * Type is manual, smallest, largest, latest. Current is the
	 * aggressive-resize option (do not resize based on clients where the
	 * window is not the current window).
	 */
	type = options_get_number(w->options, "window-size");
	current = options_get_number(w->options, "aggressive-resize");

	/* Look for a suitable client and get the new size. */
	changed = clients_calculate_size(type, current, NULL, NULL, w,
	    recalculate_size_skip_client, &sx, &sy, &xpixel, &ypixel);

	/*
	 * Make sure the size has actually changed. If the window has already
	 * got a resize scheduled, then use the new size; otherwise the old.
	 */
	if (w->flags & WINDOW_RESIZE) {
		if (!now && changed && w->new_sx == sx && w->new_sy == sy)
			changed = 0;
	} else {
		if (!now && changed && w->sx == sx && w->sy == sy)
			changed = 0;
	}

	/*
	 * If the size hasn't changed, update the window offset but not the
	 * size.
	 */
	if (!changed) {
		tty_update_window_offset(w);
		return;
	}

	/*
	 * If the now flag is set or if the window is sized manually, change
	 * the size immediately. Otherwise set the flag and it will be done
	 * later.
	 */
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
