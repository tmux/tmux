/* $OpenBSD$ */

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

#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <util.h>

#include "tmux.h"

/*
 * Each window is attached to a number of panes, each of which is a pty. This
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

void	window_pane_read_callback(struct bufferevent *, void *);
void	window_pane_error_callback(struct bufferevent *, short, void *);

RB_GENERATE(winlinks, winlink, entry, winlink_cmp);

int
winlink_cmp(struct winlink *wl1, struct winlink *wl2)
{
	return (wl1->idx - wl2->idx);
}

struct winlink *
winlink_find_by_window(struct winlinks *wwl, struct window *w)
{
	struct winlink	*wl;

	RB_FOREACH(wl, winlinks, wwl) {
		if (wl->window == w)
			return (wl);
	}

	return (NULL);
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
winlink_next_index(struct winlinks *wwl, int idx)
{
	int	i;

	i = idx;
	do {
		if (winlink_find_by_index(wwl, i) == NULL)
			return (i);
		if (i == INT_MAX)
			i = 0;
		else
			i++;
	} while (i != idx);
	return (-1);
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

	if (idx < 0) {
		if ((idx = winlink_next_index(wwl, -idx - 1)) == -1)
			return (NULL);
	} else if (winlink_find_by_index(wwl, idx) != NULL)
		return (NULL);

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
	if (wl->status_text != NULL)
		xfree(wl->status_text);
	xfree(wl);

	if (w->references == 0)
		fatal("bad reference count");
	w->references--;
	if (w->references == 0)
		window_destroy(w);
}

struct winlink *
winlink_next(struct winlink *wl)
{
	return (RB_NEXT(winlinks, wwl, wl));
}

struct winlink *
winlink_previous(struct winlink *wl)
{
	return (RB_PREV(winlinks, wwl, wl));
}

void
winlink_stack_push(struct winlink_stack *stack, struct winlink *wl)
{
	if (wl == NULL)
		return;

	winlink_stack_remove(stack, wl);
	TAILQ_INSERT_HEAD(stack, wl, sentry);
}

void
winlink_stack_remove(struct winlink_stack *stack, struct winlink *wl)
{
	struct winlink	*wl2;

	if (wl == NULL)
		return;

	TAILQ_FOREACH(wl2, stack, sentry) {
		if (wl2 == wl) {
			TAILQ_REMOVE(stack, wl, sentry);
			return;
		}
	}
}

int
window_index(struct window *s, u_int *i)
{
	for (*i = 0; *i < ARRAY_LENGTH(&windows); (*i)++) {
		if (s == ARRAY_ITEM(&windows, *i))
			return (0);
	}
	return (-1);
}

struct window *
window_create1(u_int sx, u_int sy)
{
	struct window	*w;
	u_int		 i;

	w = xcalloc(1, sizeof *w);
	w->name = NULL;
	w->flags = 0;

	TAILQ_INIT(&w->panes);
	w->active = NULL;

	w->lastlayout = -1;
	w->layout_root = NULL;

	w->sx = sx;
	w->sy = sy;

	queue_window_name(w);

	options_init(&w->options, &global_w_options);

	for (i = 0; i < ARRAY_LENGTH(&windows); i++) {
		if (ARRAY_ITEM(&windows, i) == NULL) {
			ARRAY_SET(&windows, i, w);
			break;
		}
	}
	if (i == ARRAY_LENGTH(&windows))
		ARRAY_ADD(&windows, w);
	w->references = 0;

	return (w);
}

struct window *
window_create(const char *name, const char *cmd, const char *shell,
    const char *cwd, struct environ *env, struct termios *tio,
    u_int sx, u_int sy, u_int hlimit,char **cause)
{
	struct window		*w;
	struct window_pane	*wp;

	w = window_create1(sx, sy);
	wp = window_add_pane(w, hlimit);
	layout_init(w);
	if (window_pane_spawn(wp, cmd, shell, cwd, env, tio, cause) != 0) {
		window_destroy(w);
		return (NULL);
	}
	w->active = TAILQ_FIRST(&w->panes);
	if (name != NULL) {
		w->name = xstrdup(name);
		options_set_number(&w->options, "automatic-rename", 0);
	} else
		w->name = default_window_name(w);
	return (w);
}

void
window_destroy(struct window *w)
{
	u_int	i;

	if (window_index(w, &i) != 0)
		fatalx("index not found");
	ARRAY_SET(&windows, i, NULL);
	while (!ARRAY_EMPTY(&windows) && ARRAY_LAST(&windows) == NULL)
		ARRAY_TRUNC(&windows, 1);

	if (w->layout_root != NULL)
		layout_free(w);

	evtimer_del(&w->name_timer);

	options_free(&w->options);

	window_destroy_panes(w);

	if (w->name != NULL)
		xfree(w->name);
	xfree(w);
}

void
window_resize(struct window *w, u_int sx, u_int sy)
{
	w->sx = sx;
	w->sy = sy;
}

void
window_set_active_pane(struct window *w, struct window_pane *wp)
{
	w->active = wp;
	while (!window_pane_visible(w->active)) {
		w->active = TAILQ_PREV(w->active, window_panes, entry);
		if (w->active == NULL)
			w->active = TAILQ_LAST(&w->panes, window_panes);
		if (w->active == wp)
			return;
	}
}

void
window_set_active_at(struct window *w, u_int x, u_int y)
{
	struct window_pane	*wp;

	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (!window_pane_visible(wp))
			continue;
		if (x < wp->xoff || x >= wp->xoff + wp->sx)
			continue;
		if (y < wp->yoff || y >= wp->yoff + wp->sy)
			continue;
		window_set_active_pane(w, wp);
		break;
	}
}

struct window_pane *
window_add_pane(struct window *w, u_int hlimit)
{
	struct window_pane	*wp;

	wp = window_pane_create(w, w->sx, w->sy, hlimit);
	if (TAILQ_EMPTY(&w->panes))
		TAILQ_INSERT_HEAD(&w->panes, wp, entry);
	else
		TAILQ_INSERT_AFTER(&w->panes, w->active, wp, entry);
	return (wp);
}

void
window_remove_pane(struct window *w, struct window_pane *wp)
{
	w->active = TAILQ_PREV(wp, window_panes, entry);
	if (w->active == NULL)
		w->active = TAILQ_NEXT(wp, entry);

	TAILQ_REMOVE(&w->panes, wp, entry);
	window_pane_destroy(wp);
}

struct window_pane *
window_pane_at_index(struct window *w, u_int idx)
{
	struct window_pane	*wp;
	u_int			 n;

	n = 0;
	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (n == idx)
			return (wp);
		n++;
	}
	return (NULL);
}

u_int
window_pane_index(struct window *w, struct window_pane *wp)
{
	struct window_pane	*wq;
	u_int			 n;

	n = 0;
	TAILQ_FOREACH(wq, &w->panes, entry) {
		if (wp == wq)
			break;
		n++;
	}
	return (n);
}

u_int
window_count_panes(struct window *w)
{
	struct window_pane	*wp;
	u_int			 n;

	n = 0;
	TAILQ_FOREACH(wp, &w->panes, entry)
		n++;
	return (n);
}

void
window_destroy_panes(struct window *w)
{
	struct window_pane	*wp;

	while (!TAILQ_EMPTY(&w->panes)) {
		wp = TAILQ_FIRST(&w->panes);
		TAILQ_REMOVE(&w->panes, wp, entry);
		window_pane_destroy(wp);
	}
}

struct window_pane *
window_pane_create(struct window *w, u_int sx, u_int sy, u_int hlimit)
{
	struct window_pane	*wp;

	wp = xcalloc(1, sizeof *wp);
	wp->window = w;

	wp->cmd = NULL;
	wp->shell = NULL;
	wp->cwd = NULL;

	wp->fd = -1;
	wp->event = NULL;

	wp->mode = NULL;

	wp->layout_cell = NULL;

	wp->xoff = 0;
	wp->yoff = 0;

	wp->sx = sx;
	wp->sy = sy;

	wp->pipe_fd = -1;
	wp->pipe_off = 0;
	wp->pipe_event = NULL;

	wp->saved_grid = NULL;

	screen_init(&wp->base, sx, sy, hlimit);
	wp->screen = &wp->base;

	input_init(wp);

	return (wp);
}

void
window_pane_destroy(struct window_pane *wp)
{
	if (wp->fd != -1) {
		close(wp->fd);
		bufferevent_free(wp->event);
	}

	input_free(wp);

	window_pane_reset_mode(wp);
	screen_free(&wp->base);
	if (wp->saved_grid != NULL)
		grid_destroy(wp->saved_grid);

	if (wp->pipe_fd != -1) {
		close(wp->pipe_fd);
		bufferevent_free(wp->pipe_event);
	}

	if (wp->cwd != NULL)
		xfree(wp->cwd);
	if (wp->shell != NULL)
		xfree(wp->shell);
	if (wp->cmd != NULL)
		xfree(wp->cmd);
	xfree(wp);
}

int
window_pane_spawn(struct window_pane *wp, const char *cmd, const char *shell,
    const char *cwd, struct environ *env, struct termios *tio, char **cause)
{
	struct winsize	 	 ws;
	int		 	 mode;
	char			*argv0, **varp, *var;
	ARRAY_DECL(, char *)	 varlist;
	struct environ_entry	*envent;
	const char		*ptr;
	struct termios		 tio2;
	u_int		 	 i;

	if (wp->fd != -1) {
		close(wp->fd);
		bufferevent_free(wp->event);
	}
	if (cmd != NULL) {
		if (wp->cmd != NULL)
			xfree(wp->cmd);
		wp->cmd = xstrdup(cmd);
	}
	if (shell != NULL) {
		if (wp->shell != NULL)
			xfree(wp->shell);
		wp->shell = xstrdup(shell);
	}
	if (cwd != NULL) {
		if (wp->cwd != NULL)
			xfree(wp->cwd);
		wp->cwd = xstrdup(cwd);
	}

	memset(&ws, 0, sizeof ws);
	ws.ws_col = screen_size_x(&wp->base);
	ws.ws_row = screen_size_y(&wp->base);

	switch (wp->pid = forkpty(&wp->fd, wp->tty, NULL, &ws)) {
	case -1:
		wp->fd = -1;
		xasprintf(cause, "%s: %s", cmd, strerror(errno));
		return (-1);
	case 0:
		if (chdir(wp->cwd) != 0)
			chdir("/");

		if (tcgetattr(STDIN_FILENO, &tio2) != 0)
			fatal("tcgetattr failed");
		if (tio != NULL)
			memcpy(tio2.c_cc, tio->c_cc, sizeof tio2.c_cc);
		tio2.c_cc[VERASE] = '\177';
		if (tcsetattr(STDIN_FILENO, TCSANOW, &tio2) != 0)
			fatal("tcgetattr failed");

		ARRAY_INIT(&varlist);
		for (varp = environ; *varp != NULL; varp++) {
			var = xstrdup(*varp);
			var[strcspn(var, "=")] = '\0';
			ARRAY_ADD(&varlist, var);
		}
		for (i = 0; i < ARRAY_LENGTH(&varlist); i++) {
			var = ARRAY_ITEM(&varlist, i);
			unsetenv(var);
		}
		RB_FOREACH(envent, environ, env) {
			if (envent->value != NULL)
				setenv(envent->name, envent->value, 1);
		}

		server_signal_clear();
		log_close();

		if (*wp->cmd != '\0') {
			/* Set SHELL but only if it is currently not useful. */
			shell = getenv("SHELL");
			if (shell == NULL || *shell == '\0' || areshell(shell))
				setenv("SHELL", wp->shell, 1);

			execl(_PATH_BSHELL, "sh", "-c", wp->cmd, (char *) NULL);
			fatal("execl failed");
		}

		/* No command; fork a login shell. */
		ptr = strrchr(wp->shell, '/');
		if (ptr != NULL && *(ptr + 1) != '\0')
			xasprintf(&argv0, "-%s", ptr + 1);
		else
			xasprintf(&argv0, "-%s", wp->shell);
		setenv("SHELL", wp->shell, 1);
		execl(wp->shell, argv0, (char *) NULL);
		fatal("execl failed");
	}

	if ((mode = fcntl(wp->fd, F_GETFL)) == -1)
		fatal("fcntl failed");
	if (fcntl(wp->fd, F_SETFL, mode|O_NONBLOCK) == -1)
		fatal("fcntl failed");
	if (fcntl(wp->fd, F_SETFD, FD_CLOEXEC) == -1)
		fatal("fcntl failed");
	wp->event = bufferevent_new(wp->fd,
	    window_pane_read_callback, NULL, window_pane_error_callback, wp);
	bufferevent_enable(wp->event, EV_READ|EV_WRITE);

	return (0);
}

/* ARGSUSED */
void
window_pane_read_callback(unused struct bufferevent *bufev, void *data)
{
	struct window_pane *wp = data;

	window_pane_parse(wp);
}

/* ARGSUSED */
void
window_pane_error_callback(
    unused struct bufferevent *bufev, unused short what, void *data)
{
	struct window_pane *wp = data;

	server_destroy_pane(wp);
}

void
window_pane_resize(struct window_pane *wp, u_int sx, u_int sy)
{
	struct winsize	ws;

	if (sx == wp->sx && sy == wp->sy)
		return;
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
}

/*
 * Enter alternative screen mode. A copy of the visible screen is saved and the
 * history is not updated
 */
void
window_pane_alternate_on(struct window_pane *wp, struct grid_cell *gc)
{
	struct screen	*s = &wp->base;
	u_int		 sx, sy;

	if (wp->saved_grid != NULL)
		return;
	if (!options_get_number(&wp->window->options, "alternate-screen"))
		return;
	sx = screen_size_x(s);
	sy = screen_size_y(s);

	wp->saved_grid = grid_create(sx, sy, 0);
	grid_duplicate_lines(wp->saved_grid, 0, s->grid, screen_hsize(s), sy);
	wp->saved_cx = s->cx;
	wp->saved_cy = s->cy;
	memcpy(&wp->saved_cell, gc, sizeof wp->saved_cell);

	grid_view_clear(s->grid, 0, 0, sx, sy);

	wp->base.grid->flags &= ~GRID_HISTORY;

	wp->flags |= PANE_REDRAW;
}

/* Exit alternate screen mode and restore the copied grid. */
void
window_pane_alternate_off(struct window_pane *wp, struct grid_cell *gc)
{
	struct screen	*s = &wp->base;
	u_int		 sx, sy;

	if (wp->saved_grid == NULL)
		return;
	if (!options_get_number(&wp->window->options, "alternate-screen"))
		return;
	sx = screen_size_x(s);
	sy = screen_size_y(s);

	/*
	 * If the current size is bigger, temporarily resize to the old size
	 * before copying back.
	 */
	if (sy > wp->saved_grid->sy)
		screen_resize(s, sx, wp->saved_grid->sy);

	/* Restore the grid, cursor position and cell. */
	grid_duplicate_lines(s->grid, screen_hsize(s), wp->saved_grid, 0, sy);
	s->cx = wp->saved_cx;
	if (s->cx > screen_size_x(s) - 1)
		s->cx = screen_size_x(s) - 1;
	s->cy = wp->saved_cy;
	if (s->cy > screen_size_y(s) - 1)
		s->cy = screen_size_y(s) - 1;
	memcpy(gc, &wp->saved_cell, sizeof *gc);

	/*
	 * Turn history back on (so resize can use it) and then resize back to
	 * the current size.
	 */
	wp->base.grid->flags |= GRID_HISTORY;
	if (sy > wp->saved_grid->sy)
		screen_resize(s, sx, sy);

	grid_destroy(wp->saved_grid);
	wp->saved_grid = NULL;

	wp->flags |= PANE_REDRAW;
}

int
window_pane_set_mode(struct window_pane *wp, const struct window_mode *mode)
{
	struct screen	*s;

	if (wp->mode != NULL)
		return (1);
	wp->mode = mode;

	if ((s = wp->mode->init(wp)) != NULL)
		wp->screen = s;
	wp->flags |= PANE_REDRAW;
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
	wp->flags |= PANE_REDRAW;
}

void
window_pane_parse(struct window_pane *wp)
{
	char   *data;
	size_t	new_size;

	new_size = EVBUFFER_LENGTH(wp->event->input) - wp->pipe_off;
	if (wp->pipe_fd != -1 && new_size > 0) {
		data = EVBUFFER_DATA(wp->event->input);
		bufferevent_write(wp->pipe_event, data, new_size);
	}

	input_parse(wp);

	wp->pipe_off = EVBUFFER_LENGTH(wp->event->input);
}

void
window_pane_key(struct window_pane *wp, struct client *c, int key)
{
	struct window_pane	*wp2;

	if (!window_pane_visible(wp))
		return;

	if (wp->mode != NULL) {
		if (wp->mode->key != NULL)
			wp->mode->key(wp, c, key);
		return;
	}

	if (wp->fd == -1)
		return;
	input_key(wp, key);
	if (options_get_number(&wp->window->options, "synchronize-panes")) {
		TAILQ_FOREACH(wp2, &wp->window->panes, entry) {
			if (wp2 == wp || wp2->mode != NULL)
				continue;
			if (wp2->fd != -1 && window_pane_visible(wp2))
				input_key(wp2, key);
		}
	}
}

void
window_pane_mouse(
    struct window_pane *wp, struct client *c, struct mouse_event *m)
{
	if (!window_pane_visible(wp))
		return;

	if (m->x < wp->xoff || m->x >= wp->xoff + wp->sx)
		return;
	if (m->y < wp->yoff || m->y >= wp->yoff + wp->sy)
		return;
	m->x -= wp->xoff;
	m->y -= wp->yoff;

	if (wp->mode != NULL) {
		if (wp->mode->mouse != NULL)
			wp->mode->mouse(wp, c, m);
	} else if (wp->fd != -1)
		input_mouse(wp, m);
}

int
window_pane_visible(struct window_pane *wp)
{
	struct window	*w = wp->window;

	if (wp->xoff >= w->sx || wp->yoff >= w->sy)
		return (0);
	if (wp->xoff + wp->sx > w->sx || wp->yoff + wp->sy > w->sy)
		return (0);
	return (1);
}

char *
window_pane_search(struct window_pane *wp, const char *searchstr, u_int *lineno)
{
	struct screen	*s = &wp->base;
	char		*newsearchstr, *line, *msg;
	u_int	 	 i;

	msg = NULL;
	xasprintf(&newsearchstr, "*%s*", searchstr);

	for (i = 0; i < screen_size_y(s); i++) {
		line = grid_view_string_cells(s->grid, 0, i, screen_size_x(s));
		if (fnmatch(newsearchstr, line, 0) == 0) {
			msg = line;
			if (lineno != NULL)
				*lineno = i;
			break;
		}
		xfree(line);
	}

	xfree(newsearchstr);
	return (msg);
}
