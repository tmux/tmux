/* $Id: window.c,v 1.57 2009-01-13 06:50:10 nicm Exp $ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
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
#include <sys/ioctl.h>

#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#ifndef NO_PATHS_H
#include <paths.h>
#endif

#ifdef USE_LIBUTIL_H
#include <libutil.h>
#else
#ifdef USE_PTY_H
#include <pty.h>
#else
#ifndef NO_FORKPTY
#include <util.h>
#endif
#endif
#endif

#include "tmux.h"

/*
 * Each window is attached to one or two panes, each of which is a pty. This
 * file contains code to handle them.
 *
 * A pane has two buffers attached, these are filled and emptied by the main
 * server poll loop. Output data is received from pty's in screen format,
 * translated and returned as a series of escape sequences and strings via
 * input_parse (in input.c). Input data is received as key codes and written
 * directly via input_key.
 *
 * Each pane also has a "virtual" screen (screen.c) which contains the current
 * state and is redisplayed when the window is reattached to a client.
 *
 * Windows are stored directly on a global array and wrapped in any number of
 * winlink structs to be linked onto local session RB trees. A reference count
 * is maintained and a window removed from the global list and destroyed when
 * it reaches zero.
 */

/* Global window list. */
struct windows windows;

RB_GENERATE(winlinks, winlink, entry, winlink_cmp);

int
winlink_cmp(struct winlink *wl1, struct winlink *wl2)
{
	return (wl1->idx - wl2->idx);
}

struct winlink *
winlink_find_by_index(struct winlinks *wwl, int idx)
{
	struct winlink	wl;

	if (idx < 0)
		fatalx("bad index");

	wl.idx = idx;
	return (RB_FIND(winlinks, wwl, &wl));
}

int
winlink_next_index(struct winlinks *wwl)
{
	u_int	i;

	for (i = 0; i < INT_MAX; i++) {
		if (winlink_find_by_index(wwl, i) == NULL)
			return (i);
	}

	fatalx("no free indexes");
}

u_int
winlink_count(struct winlinks *wwl)
{
	struct winlink	*wl;
	u_int		 n;

	n = 0;
	RB_FOREACH(wl, winlinks, wwl)
		n++;

	return (n);
}

struct winlink *
winlink_add(struct winlinks *wwl, struct window *w, int idx)
{
	struct winlink	*wl;

	if (idx == -1)
		idx = winlink_next_index(wwl);
	else if (winlink_find_by_index(wwl, idx) != NULL)
		return (NULL);

	if (idx < 0)
		fatalx("bad index");

	wl = xcalloc(1, sizeof *wl);
	wl->idx = idx;
	wl->window = w;
	RB_INSERT(winlinks, wwl, wl);

	w->references++;

	return (wl);
}

void
winlink_remove(struct winlinks *wwl, struct winlink *wl)
{
	struct window	*w = wl->window;

	RB_REMOVE(winlinks, wwl, wl);
	xfree(wl);

	if (w->references == 0)
		fatal("bad reference count");
	w->references--;
	if (w->references == 0)
		window_destroy(w);
}

struct winlink *
winlink_next(unused struct winlinks *wwl, struct winlink *wl)
{
	return (RB_NEXT(winlinks, wwl, wl));
}

struct winlink *
winlink_previous(unused struct winlinks *wwl, struct winlink *wl)
{
#ifdef RB_PREV
	return (RB_PREV(winlinks, wwl, wl));
#else
	struct winlink	*wk;
	int		 idx = wl->idx;

	wk = NULL;
	wl = RB_MIN(winlinks, wwl);
	while (wl != NULL && wl->idx < idx) {
		wk = wl;
		wl = RB_NEXT(winlinks, wwl, wl);
	}
	if (wl == NULL)
		return (NULL);
	return (wk);
#endif
}

void
winlink_stack_push(struct winlink_stack *stack, struct winlink *wl)
{
	if (wl == NULL)
		return;

	winlink_stack_remove(stack, wl);
	SLIST_INSERT_HEAD(stack, wl, sentry);
}

void
winlink_stack_remove(struct winlink_stack *stack, struct winlink *wl)
{
	struct winlink	*wl2;

	if (wl == NULL)
		return;

	SLIST_FOREACH(wl2, stack, sentry) {
		if (wl2 == wl) {
			SLIST_REMOVE(stack, wl, winlink, sentry);
			return;
		}
	}
}

struct window *
window_create(const char *name, const char *cmd,
    const char *cwd, const char **envp, u_int sx, u_int sy, u_int hlimit)
{
	struct window	*w;
	char		*ptr, *copy;

	w = xmalloc(sizeof *w);
	w->flags = 0;
	w->panes[0] = NULL;
	w->panes[1] = NULL;

	w->sx = sx;
	w->sy = sy;

	options_init(&w->options, &global_window_options);

	if (name == NULL) {
		/* XXX */
		if (strncmp(cmd, "exec ", (sizeof "exec ") - 1) == 0)
			copy = xstrdup(cmd + (sizeof "exec ") - 1);
		else
			copy = xstrdup(cmd);
		if ((ptr = strchr(copy, ' ')) != NULL) {
			if (ptr != copy && ptr[-1] != '\\')
				*ptr = '\0';
			else {
				while ((ptr = strchr(ptr + 1, ' ')) != NULL) {
					if (ptr[-1] != '\\') {
						*ptr = '\0';
						break;
					}
				}
			}
		}
		w->name = xstrdup(xbasename(copy));
		xfree(copy);
	} else
		w->name = xstrdup(name);

	ARRAY_ADD(&windows, w);
	w->references = 0;

	if (window_add_pane(w, w->sy, cmd, cwd, envp, hlimit) < 0) {
		window_destroy(w);
		return (NULL);
	}
	w->active = w->panes[0];
	return (w);
}

void
window_destroy(struct window *w)
{
	u_int	i;

	for (i = 0; i < ARRAY_LENGTH(&windows); i++) {
		if (ARRAY_ITEM(&windows, i) == w)
			break;
	}
	ARRAY_REMOVE(&windows, i);

	options_free(&w->options);

	window_pane_destroy(w->panes[0]);
	if (w->panes[1] != NULL)
		window_pane_destroy(w->panes[1]);
	
	xfree(w->name);
	xfree(w);
}

int
window_resize(struct window *w, u_int sx, u_int sy)
{
	int	change;
	u_int	y0, y1;

	if (w->panes[1] == NULL)
		window_pane_resize(w->panes[0], sx, sy);
	else {
		if (sy <= 3) {
			y0 = 1;
			y1 = 1;
		} else {
			y0 = w->panes[0]->sy;
			y1 = w->panes[1]->sy;
			
			change = sy - w->sy;
			if (change > 0) {
				while (change > 0) {
					if (y1 < y0)
						y1++;
					else
						y0++;
					change--;
				}
			} else if (change < 0) {
				while (change < 0) {
					if (y1 > y0)
						y1--;
					else
						y0--;
					change++;
				}
			}
		}
		window_pane_resize(w->panes[0], sx, y0);
		window_pane_resize(w->panes[1], sx, y1);
		w->panes[1]->yoff = y0 + 1;
	}

	w->sx = sx;
	w->sy = sy;
	return (0);
}

int
window_add_pane(struct window *w, u_int y1,
    const char *cmd, const char *cwd, const char **envp, u_int hlimit)
{
	struct window_pane	*wp;
	u_int			 y0;

	if (w->panes[1] != NULL)
		return (-1);

	if (w->panes[0] == NULL) {
		/* No existing panes. */
		wp = w->panes[0] = window_pane_create(w, w->sx, w->sy, hlimit);
		wp->yoff = 0;
	} else {
		/* One existing pane. */
		if (y1 > w->sy)
			y1 = w->sy;
		y0 = w->sy - y1;
		if (y0 == 0) {
			y0 = 1;
			y1--;
		}
		if (y0 > 1)
			y0--;
		else if (y1 > 1)
			y1--;
		window_pane_resize(w->panes[0], w->sx, y0);

		wp = w->panes[1] = window_pane_create(w, w->sx, y1, hlimit);
		wp->yoff = y0 + 1;
	}

	if (window_pane_spawn(wp, cmd, cwd, envp) != 0) {
		window_remove_pane(w, wp);
		return (-1);
	}
	return (0);
}

int
window_remove_pane(struct window *w, struct window_pane *wp)
{
	int	pane;

	pane = 0;
	if (wp == w->panes[1])
		pane = 1;

	if (w->panes[1] != NULL) {
		window_pane_destroy(w->panes[pane]);
		w->panes[pane] = NULL;
		if (pane == 0) {
			w->panes[0] = w->panes[1];
			w->panes[1] = NULL;
		}
		w->active = w->panes[0];

		window_pane_resize(w->active, w->sx, w->sy);
		w->active->yoff = 0;
		return (0);
	}
	return (1);
}

struct window_pane *
window_pane_create(struct window *w, u_int sx, u_int sy, u_int hlimit)
{
	struct window_pane	*wp;

	wp = xmalloc(sizeof *wp);
	wp->window = w;

	wp->cmd = NULL;
	wp->cwd = NULL;

	wp->fd = -1;
	wp->in = buffer_create(BUFSIZ);
	wp->out = buffer_create(BUFSIZ);

	wp->mode = NULL;

	wp->sx = sx;
	wp->sy = sy;

 	wp->yoff = 0;

	screen_init(&wp->base, sx, sy, hlimit);
	wp->screen = &wp->base;

	input_init(wp);

	return (wp);
}

void
window_pane_destroy(struct window_pane *wp)
{
	if (wp->fd != -1)
		close(wp->fd);

	input_free(wp);

	window_pane_reset_mode(wp);
	screen_free(&wp->base);

	buffer_destroy(wp->in);
	buffer_destroy(wp->out);

	if (wp->cwd != NULL)
		xfree(wp->cwd);
	if (wp->cmd != NULL)
		xfree(wp->cmd);
	xfree(wp);
}

int
window_pane_spawn(struct window_pane *wp,
    const char *cmd, const char *cwd, const char **envp)
{
	struct winsize	 ws;
	int		 mode;
	const char     **envq;

	if (wp->fd != -1)
		close(wp->fd);
	if (cmd != NULL) {
		if (wp->cmd != NULL)
			xfree(wp->cmd);
		wp->cmd = xstrdup(cmd);
	}
	if (cwd != NULL) {
		if (wp->cwd != NULL)
			xfree(wp->cwd);
		wp->cwd = xstrdup(cwd);
	}

	memset(&ws, 0, sizeof ws);
	ws.ws_col = screen_size_x(&wp->base);
	ws.ws_row = screen_size_y(&wp->base);

 	switch (forkpty(&wp->fd, NULL, NULL, &ws)) {
	case -1:
		return (1);
	case 0:
		if (chdir(wp->cwd) != 0)
			chdir("/");
		for (envq = envp; *envq != NULL; envq++) {
			if (putenv(*envq) != 0)
				fatal("putenv failed");
		}
		sigreset();
		log_close();

		execl(_PATH_BSHELL, "sh", "-c", wp->cmd, (char *) NULL);
		fatal("execl failed");
	}

	if ((mode = fcntl(wp->fd, F_GETFL)) == -1)
		fatal("fcntl failed");
	if (fcntl(wp->fd, F_SETFL, mode|O_NONBLOCK) == -1)
		fatal("fcntl failed");
	if (fcntl(wp->fd, F_SETFD, FD_CLOEXEC) == -1)
		fatal("fcntl failed");

	return (0);
}

int
window_pane_resize(struct window_pane *wp, u_int sx, u_int sy)
{
	struct winsize	ws;

	if (sx == wp->sx && sy == wp->sy)
		return (-1);
	wp->sx = sx;
	wp->sy = sy;

	memset(&ws, 0, sizeof ws);
	ws.ws_col = sx;
	ws.ws_row = sy;

	screen_resize(&wp->base, sx, sy);
	if (wp->mode != NULL)
		wp->mode->resize(wp, sx, sy);

	if (wp->fd != -1 && ioctl(wp->fd, TIOCSWINSZ, &ws) == -1)
		fatal("ioctl failed");
	return (0);
}

int
window_pane_set_mode(struct window_pane *wp, const struct window_mode *mode)
{
	struct screen	*s;

	if (wp->mode != NULL || wp->mode == mode)
		return (1);

	wp->mode = mode;

	if ((s = wp->mode->init(wp)) != NULL)
		wp->screen = s;
	server_redraw_window(wp->window);
	return (0);
}

void
window_pane_reset_mode(struct window_pane *wp)
{
	if (wp->mode == NULL)
		return;

	wp->mode->free(wp);
	wp->mode = NULL;

	wp->screen = &wp->base;
	server_redraw_window(wp->window);
}

void
window_pane_parse(struct window_pane *wp)
{
	input_parse(wp);
}

void
window_pane_key(struct window_pane *wp, struct client *c, int key)
{
	if (wp->mode != NULL)
		wp->mode->key(wp, c, key);
	else
		input_key(wp, key);
}
