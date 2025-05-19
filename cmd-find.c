/* $OpenBSD$ */

/*
 * Copyright (c) 2015 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <fnmatch.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

static int	cmd_find_session_better(struct session *, struct session *,
		    int);
static struct session *cmd_find_best_session(struct session **, u_int, int);
static int	cmd_find_best_session_with_window(struct cmd_find_state *);
static int	cmd_find_best_winlink_with_window(struct cmd_find_state *);

static const char *cmd_find_map_table(const char *[][2], const char *);

static void	cmd_find_log_state(const char *, struct cmd_find_state *);
static int	cmd_find_get_session(struct cmd_find_state *, const char *);
static int	cmd_find_get_popup(struct cmd_find_state *, const char *);
static int	cmd_find_get_window(struct cmd_find_state *, const char *, int);
static int	cmd_find_get_window_with_session(struct cmd_find_state *,
		    const char *);
static int	cmd_find_get_pane(struct cmd_find_state *, const char *, int);
static int	cmd_find_get_pane_with_session(struct cmd_find_state *,
		    const char *);
static int	cmd_find_get_pane_with_window(struct cmd_find_state *,
		    const char *);

static const char *cmd_find_session_table[][2] = {
	{ NULL, NULL }
};
static const char *cmd_find_window_table[][2] = {
	{ "{start}", "^" },
	{ "{last}", "!" },
	{ "{end}", "$" },
	{ "{next}", "+" },
	{ "{previous}", "-" },
	{ NULL, NULL }
};
static const char *cmd_find_pane_table[][2] = {
	{ "{last}", "!" },
	{ "{next}", "+" },
	{ "{previous}", "-" },
	{ "{top}", "top" },
	{ "{bottom}", "bottom" },
	{ "{left}", "left" },
	{ "{right}", "right" },
	{ "{top-left}", "top-left" },
	{ "{top-right}", "top-right" },
	{ "{bottom-left}", "bottom-left" },
	{ "{bottom-right}", "bottom-right" },
	{ "{up-of}", "{up-of}" },
	{ "{down-of}", "{down-of}" },
	{ "{left-of}", "{left-of}" },
	{ "{right-of}", "{right-of}" },
	{ NULL, NULL }
};

/* Find pane containing client if any. */
static struct window_pane *
cmd_find_inside_pane(struct client *c)
{
	struct window_pane	*wp;
	struct environ_entry	*envent;

	if (c == NULL)
		return (NULL);

	RB_FOREACH(wp, window_pane_tree, &all_window_panes) {
		if (wp->fd != -1 && strcmp(wp->tty, c->ttyname) == 0)
			break;
	}
	if (wp == NULL) {
		envent = environ_find(c->environ, "TMUX_PANE");
		if (envent != NULL)
			wp = window_pane_find_by_id_str(envent->value);
	}
	if (wp != NULL)
		log_debug("%s: got pane %%%u (%s)", __func__, wp->id, wp->tty);
	return (wp);
}

/* Is this client better? */
static int
cmd_find_client_better(struct client *c, struct client *than)
{
	if (than == NULL)
		return (1);
	return (timercmp(&c->activity_time, &than->activity_time, >));
}

/* Find best client for session. */
struct client *
cmd_find_best_client(struct session *s)
{
	struct client	*c_loop, *c;

	if (s->attached == 0)
		s = NULL;

	c = NULL;
	TAILQ_FOREACH(c_loop, &clients, entry) {
		if (c_loop->session == NULL)
			continue;
		if (s != NULL && c_loop->session != s)
			continue;
		if (cmd_find_client_better(c_loop, c))
			c = c_loop;
	}
	return (c);
}

/* Is this session better? */
static int
cmd_find_session_better(struct session *s, struct session *than, int flags)
{
	int	attached;

	if (than == NULL)
		return (1);
	if (flags & CMD_FIND_PREFER_UNATTACHED) {
		attached = (than->attached != 0);
		if (attached && s->attached == 0)
			return (1);
		else if (!attached && s->attached != 0)
			return (0);
	}
	return (timercmp(&s->activity_time, &than->activity_time, >));
}

/* Find best session from a list, or all if list is NULL. */
static struct session *
cmd_find_best_session(struct session **slist, u_int ssize, int flags)
{
	struct session	 *s_loop, *s;
	u_int		  i;

	log_debug("%s: %u sessions to try", __func__, ssize);

	s = NULL;
	if (slist != NULL) {
		for (i = 0; i < ssize; i++) {
			if (cmd_find_session_better(slist[i], s, flags))
				s = slist[i];
		}
	} else {
		RB_FOREACH(s_loop, sessions, &sessions) {
			if (cmd_find_session_better(s_loop, s, flags))
				s = s_loop;
		}
	}
	return (s);
}

/* Find best session and winlink for window. */
static int
cmd_find_best_session_with_window(struct cmd_find_state *fs)
{
	struct session	**slist = NULL;
	u_int		  ssize;
	struct session	 *s;

	log_debug("%s: window is @%u", __func__, fs->w->id);

	ssize = 0;
	RB_FOREACH(s, sessions, &sessions) {
		if (!session_has(s, fs->w))
			continue;
		slist = xreallocarray(slist, ssize + 1, sizeof *slist);
		slist[ssize++] = s;
	}
	if (ssize == 0)
		goto fail;
	fs->s = cmd_find_best_session(slist, ssize, fs->flags);
	if (fs->s == NULL)
		goto fail;
	free(slist);
	return (cmd_find_best_winlink_with_window(fs));

fail:
	free(slist);
	return (-1);
}

/*
 * Find the best winlink for a window (the current if it contains the window,
 * otherwise the first).
 */
static int
cmd_find_best_winlink_with_window(struct cmd_find_state *fs)
{
	struct winlink	 *wl, *wl_loop;

	log_debug("%s: window is @%u", __func__, fs->w->id);

	wl = NULL;
	if (fs->s->curw != NULL && fs->s->curw->window == fs->w)
		wl = fs->s->curw;
	else {
		RB_FOREACH(wl_loop, winlinks, &fs->s->windows) {
			if (wl_loop->window == fs->w) {
				wl = wl_loop;
				break;
			}
		}
	}
	if (wl == NULL)
		return (-1);
	fs->wl = wl;
	fs->idx = fs->wl->idx;
	return (0);
}

/* Maps string in table. */
static const char *
cmd_find_map_table(const char *table[][2], const char *s)
{
	u_int	i;

	for (i = 0; table[i][0] != NULL; i++) {
		if (strcmp(s, table[i][0]) == 0)
			return (table[i][1]);
	}
	return (s);
}

static int
cmd_find_get_popup(struct cmd_find_state *fs, const char *popup)
{
	fs->pd = popup_find(popup);
	if (fs->pd == NULL)
		return (-1);
	return (0);
}

/* Find session from string. Fills in s. */
static int
cmd_find_get_session(struct cmd_find_state *fs, const char *session)
{
	struct session	*s, *s_loop;
	struct client	*c;

	log_debug("%s: %s", __func__, session);

	/* Check for session ids starting with $. */
	if (*session == '$') {
		fs->s = session_find_by_id_str(session);
		if (fs->s == NULL)
			return (-1);
		return (0);
	}

	/* Look for exactly this session. */
	fs->s = session_find(session);
	if (fs->s != NULL)
		return (0);

	/* Look for as a client. */
	c = cmd_find_client(NULL, session, 1);
	if (c != NULL && c->session != NULL) {
		fs->s = c->session;
		return (0);
	}

	/* Stop now if exact only. */
	if (fs->flags & CMD_FIND_EXACT_SESSION)
		return (-1);

	/* Otherwise look for prefix. */
	s = NULL;
	RB_FOREACH(s_loop, sessions, &sessions) {
		if (strncmp(session, s_loop->name, strlen(session)) == 0) {
			if (s != NULL)
				return (-1);
			s = s_loop;
		}
	}
	if (s != NULL) {
		fs->s = s;
		return (0);
	}

	/* Then as a pattern. */
	s = NULL;
	RB_FOREACH(s_loop, sessions, &sessions) {
		if (fnmatch(session, s_loop->name, 0) == 0) {
			if (s != NULL)
				return (-1);
			s = s_loop;
		}
	}
	if (s != NULL) {
		fs->s = s;
		return (0);
	}

	return (-1);
}

/* Find window from string. Fills in s, wl, w. */
static int
cmd_find_get_window(struct cmd_find_state *fs, const char *window, int only)
{
	log_debug("%s: %s", __func__, window);

	/* Check for window ids starting with @. */
	if (*window == '@') {
		fs->w = window_find_by_id_str(window);
		if (fs->w == NULL)
			return (-1);
		return (cmd_find_best_session_with_window(fs));
	}

	/* Not a window id, so use the current session. */
	fs->s = fs->current->s;

	/* We now only need to find the winlink in this session. */
	if (cmd_find_get_window_with_session(fs, window) == 0)
		return (0);

	/* Otherwise try as a session itself. */
	if (!only && cmd_find_get_session(fs, window) == 0) {
		fs->wl = fs->s->curw;
		fs->w = fs->wl->window;
		if (~fs->flags & CMD_FIND_WINDOW_INDEX)
			fs->idx = fs->wl->idx;
		return (0);
	}

	return (-1);
}

/*
 * Find window from string, assuming it is in given session. Needs s, fills in
 * wl and w.
 */
static int
cmd_find_get_window_with_session(struct cmd_find_state *fs, const char *window)
{
	struct winlink	*wl;
	const char	*errstr;
	int		 idx, n, exact;
	struct session	*s;

	log_debug("%s: %s", __func__, window);
	exact = (fs->flags & CMD_FIND_EXACT_WINDOW);

	/*
	 * Start with the current window as the default. So if only an index is
	 * found, the window will be the current.
	 */
	fs->wl = fs->s->curw;
	fs->w = fs->wl->window;

	/* Check for window ids starting with @. */
	if (*window == '@') {
		fs->w = window_find_by_id_str(window);
		if (fs->w == NULL || !session_has(fs->s, fs->w))
			return (-1);
		return (cmd_find_best_winlink_with_window(fs));
	}

	/* Try as an offset. */
	if (!exact && (window[0] == '+' || window[0] == '-')) {
		if (window[1] != '\0')
			n = strtonum(window + 1, 1, INT_MAX, NULL);
		else
			n = 1;
		s = fs->s;
		if (fs->flags & CMD_FIND_WINDOW_INDEX) {
			if (window[0] == '+') {
				if (INT_MAX - s->curw->idx < n)
					return (-1);
				fs->idx = s->curw->idx + n;
			} else {
				if (n > s->curw->idx)
					return (-1);
				fs->idx = s->curw->idx - n;
			}
			return (0);
		}
		if (window[0] == '+')
			fs->wl = winlink_next_by_number(s->curw, s, n);
		else
			fs->wl = winlink_previous_by_number(s->curw, s, n);
		if (fs->wl != NULL) {
			fs->idx = fs->wl->idx;
			fs->w = fs->wl->window;
			return (0);
		}
	}

	/* Try special characters. */
	if (!exact) {
		if (strcmp(window, "!") == 0) {
			fs->wl = TAILQ_FIRST(&fs->s->lastw);
			if (fs->wl == NULL)
				return (-1);
			fs->idx = fs->wl->idx;
			fs->w = fs->wl->window;
			return (0);
		} else if (strcmp(window, "^") == 0) {
			fs->wl = RB_MIN(winlinks, &fs->s->windows);
			if (fs->wl == NULL)
				return (-1);
			fs->idx = fs->wl->idx;
			fs->w = fs->wl->window;
			return (0);
		} else if (strcmp(window, "$") == 0) {
			fs->wl = RB_MAX(winlinks, &fs->s->windows);
			if (fs->wl == NULL)
				return (-1);
			fs->idx = fs->wl->idx;
			fs->w = fs->wl->window;
			return (0);
		}
	}

	/* First see if this is a valid window index in this session. */
	if (window[0] != '+' && window[0] != '-') {
		idx = strtonum(window, 0, INT_MAX, &errstr);
		if (errstr == NULL) {
			fs->wl = winlink_find_by_index(&fs->s->windows, idx);
			if (fs->wl != NULL) {
				fs->idx = fs->wl->idx;
				fs->w = fs->wl->window;
				return (0);
			}
			if (fs->flags & CMD_FIND_WINDOW_INDEX) {
				fs->idx = idx;
				return (0);
			}
		}
	}

	/* Look for exact matches, error if more than one. */
	fs->wl = NULL;
	RB_FOREACH(wl, winlinks, &fs->s->windows) {
		if (strcmp(window, wl->window->name) == 0) {
			if (fs->wl != NULL)
				return (-1);
			fs->wl = wl;
		}
	}
	if (fs->wl != NULL) {
		fs->idx = fs->wl->idx;
		fs->w = fs->wl->window;
		return (0);
	}

	/* Stop now if exact only. */
	if (exact)
		return (-1);

	/* Try as the start of a window name, error if multiple. */
	fs->wl = NULL;
	RB_FOREACH(wl, winlinks, &fs->s->windows) {
		if (strncmp(window, wl->window->name, strlen(window)) == 0) {
			if (fs->wl != NULL)
				return (-1);
			fs->wl = wl;
		}
	}
	if (fs->wl != NULL) {
		fs->idx = fs->wl->idx;
		fs->w = fs->wl->window;
		return (0);
	}

	/* Now look for pattern matches, again error if multiple. */
	fs->wl = NULL;
	RB_FOREACH(wl, winlinks, &fs->s->windows) {
		if (fnmatch(window, wl->window->name, 0) == 0) {
			if (fs->wl != NULL)
				return (-1);
			fs->wl = wl;
		}
	}
	if (fs->wl != NULL) {
		fs->idx = fs->wl->idx;
		fs->w = fs->wl->window;
		return (0);
	}

	return (-1);
}

/* Find pane from string. Fills in s, wl, w, wp. */
static int
cmd_find_get_pane(struct cmd_find_state *fs, const char *pane, int only)
{
	log_debug("%s: %s", __func__, pane);

	/* Check for pane ids starting with %. */
	if (*pane == '%') {
		fs->wp = window_pane_find_by_id_str(pane);
		if (fs->wp == NULL)
			return (-1);
		fs->w = fs->wp->window;
		return (cmd_find_best_session_with_window(fs));
	}

	/* Not a pane id, so try the current session and window. */
	fs->s = fs->current->s;
	fs->wl = fs->current->wl;
	fs->idx = fs->current->idx;
	fs->w = fs->current->w;

	/* We now only need to find the pane in this window. */
	if (cmd_find_get_pane_with_window(fs, pane) == 0)
		return (0);

	/* Otherwise try as a window itself (this will also try as session). */
	if (!only && cmd_find_get_window(fs, pane, 0) == 0) {
		fs->wp = fs->w->active;
		return (0);
	}

	return (-1);
}

/*
 * Find pane from string, assuming it is in given session. Needs s, fills in wl
 * and w and wp.
 */
static int
cmd_find_get_pane_with_session(struct cmd_find_state *fs, const char *pane)
{
	log_debug("%s: %s", __func__, pane);

	/* Check for pane ids starting with %. */
	if (*pane == '%') {
		fs->wp = window_pane_find_by_id_str(pane);
		if (fs->wp == NULL)
			return (-1);
		fs->w = fs->wp->window;
		return (cmd_find_best_winlink_with_window(fs));
	}

	/* Otherwise use the current window. */
	fs->wl = fs->s->curw;
	fs->idx = fs->wl->idx;
	fs->w = fs->wl->window;

	/* Now we just need to look up the pane. */
	return (cmd_find_get_pane_with_window(fs, pane));
}

/*
 * Find pane from string, assuming it is in the given window. Needs w, fills in
 * wp.
 */
static int
cmd_find_get_pane_with_window(struct cmd_find_state *fs, const char *pane)
{
	const char		*errstr;
	int			 idx;
	struct window_pane	*wp;
	u_int			 n;

	log_debug("%s: %s", __func__, pane);

	/* Check for pane ids starting with %. */
	if (*pane == '%') {
		fs->wp = window_pane_find_by_id_str(pane);
		if (fs->wp == NULL)
			return (-1);
		if (fs->wp->window != fs->w)
			return (-1);
		return (0);
	}

	/* Try special characters. */
	if (strcmp(pane, "!") == 0) {
		fs->wp = TAILQ_FIRST(&fs->w->last_panes);
		if (fs->wp == NULL)
			return (-1);
		return (0);
	} else if (strcmp(pane, "{up-of}") == 0) {
		fs->wp = window_pane_find_up(fs->w->active);
		if (fs->wp == NULL)
			return (-1);
		return (0);
	} else if (strcmp(pane, "{down-of}") == 0) {
		fs->wp = window_pane_find_down(fs->w->active);
		if (fs->wp == NULL)
			return (-1);
		return (0);
	} else if (strcmp(pane, "{left-of}") == 0) {
		fs->wp = window_pane_find_left(fs->w->active);
		if (fs->wp == NULL)
			return (-1);
		return (0);
	} else if (strcmp(pane, "{right-of}") == 0) {
		fs->wp = window_pane_find_right(fs->w->active);
		if (fs->wp == NULL)
			return (-1);
		return (0);
	}

	/* Try as an offset. */
	if (pane[0] == '+' || pane[0] == '-') {
		if (pane[1] != '\0')
			n = strtonum(pane + 1, 1, INT_MAX, NULL);
		else
			n = 1;
		wp = fs->w->active;
		if (pane[0] == '+')
			fs->wp = window_pane_next_by_number(fs->w, wp, n);
		else
			fs->wp = window_pane_previous_by_number(fs->w, wp, n);
		if (fs->wp != NULL)
			return (0);
	}

	/* Get pane by index. */
	idx = strtonum(pane, 0, INT_MAX, &errstr);
	if (errstr == NULL) {
		fs->wp = window_pane_at_index(fs->w, idx);
		if (fs->wp != NULL)
			return (0);
	}

	/* Try as a description. */
	fs->wp = window_find_string(fs->w, pane);
	if (fs->wp != NULL)
		return (0);

	return (-1);
}

/* Clear state. */
void
cmd_find_clear_state(struct cmd_find_state *fs, int flags)
{
	memset(fs, 0, sizeof *fs);

	fs->flags = flags;

	fs->idx = -1;
}

/* Check if state is empty. */
int
cmd_find_empty_state(struct cmd_find_state *fs)
{
	if (fs->s == NULL && fs->wl == NULL && fs->w == NULL && fs->wp == NULL &&
	    fs->pd == NULL)
		return (1);
	return (0);
}

/* Check if a state if valid. */
int
cmd_find_valid_state(struct cmd_find_state *fs)
{
	struct winlink	*wl;

	if (fs->pd != NULL)
		return (1);

	if (fs->s == NULL || fs->wl == NULL || fs->w == NULL || fs->wp == NULL)
		return (0);

	if (!session_alive(fs->s))
		return (0);

	RB_FOREACH(wl, winlinks, &fs->s->windows) {
		if (wl->window == fs->w && wl == fs->wl)
			break;
	}
	if (wl == NULL)
		return (0);

	if (fs->w != fs->wl->window)
		return (0);

	return (window_has_pane(fs->w, fs->wp));
}

/* Copy a state. */
void
cmd_find_copy_state(struct cmd_find_state *dst, struct cmd_find_state *src)
{
	dst->s = src->s;
	dst->wl = src->wl;
	dst->idx = src->idx;
	dst->w = src->w;
	dst->wp = src->wp;
	dst->pd = src->pd;
}

/* Log the result. */
static void
cmd_find_log_state(const char *prefix, struct cmd_find_state *fs)
{
	if (fs->s != NULL)
		log_debug("%s: s=$%u %s", prefix, fs->s->id, fs->s->name);
	else
		log_debug("%s: s=none", prefix);
	if (fs->wl != NULL) {
		log_debug("%s: wl=%u %d w=@%u %s", prefix, fs->wl->idx,
		    fs->wl->window == fs->w, fs->w->id, fs->w->name);
	} else
		log_debug("%s: wl=none", prefix);
	if (fs->wp != NULL)
		log_debug("%s: wp=%%%u", prefix, fs->wp->id);
	else
		log_debug("%s: wp=none", prefix);
	if (fs->idx != -1)
		log_debug("%s: idx=%d", prefix, fs->idx);
	else
		log_debug("%s: idx=none", prefix);
	if (fs->pd != NULL)
		log_debug("%s: pd=%%%u", prefix, fs->pd->id);
}

/* Find state from a session. */
void
cmd_find_from_session(struct cmd_find_state *fs, struct session *s, int flags)
{
	cmd_find_clear_state(fs, flags);

	fs->s = s;
	fs->wl = fs->s->curw;
	fs->w = fs->wl->window;
	fs->wp = fs->w->active;

	cmd_find_log_state(__func__, fs);
}

/* Find state from a winlink. */
void
cmd_find_from_winlink(struct cmd_find_state *fs, struct winlink *wl, int flags)
{
	cmd_find_clear_state(fs, flags);

	fs->s = wl->session;
	fs->wl = wl;
	fs->w = wl->window;
	fs->wp = wl->window->active;

	cmd_find_log_state(__func__, fs);
}

/* Find state from a session and window. */
int
cmd_find_from_session_window(struct cmd_find_state *fs, struct session *s,
    struct window *w, int flags)
{
	cmd_find_clear_state(fs, flags);

	fs->s = s;
	fs->w = w;
	if (cmd_find_best_winlink_with_window(fs) != 0) {
		cmd_find_clear_state(fs, flags);
		return (-1);
	}
	fs->wp = fs->w->active;

	cmd_find_log_state(__func__, fs);
	return (0);
}

/* Find state from a window. */
int
cmd_find_from_window(struct cmd_find_state *fs, struct window *w, int flags)
{
	cmd_find_clear_state(fs, flags);

	fs->w = w;
	if (cmd_find_best_session_with_window(fs) != 0) {
		cmd_find_clear_state(fs, flags);
		return (-1);
	}
	if (cmd_find_best_winlink_with_window(fs) != 0) {
		cmd_find_clear_state(fs, flags);
		return (-1);
	}
	fs->wp = fs->w->active;

	cmd_find_log_state(__func__, fs);
	return (0);
}

/* Find state from a winlink and pane. */
void
cmd_find_from_winlink_pane(struct cmd_find_state *fs, struct winlink *wl,
    struct window_pane *wp, int flags)
{
	cmd_find_clear_state(fs, flags);

	fs->s = wl->session;
	fs->wl = wl;
	fs->idx = fs->wl->idx;
	fs->w = fs->wl->window;
	fs->wp = wp;

	cmd_find_log_state(__func__, fs);
}

/* Find state from a pane. */
int
cmd_find_from_pane(struct cmd_find_state *fs, struct window_pane *wp, int flags)
{
	if (cmd_find_from_window(fs, wp->window, flags) != 0)
		return (-1);
	fs->wp = wp;

	cmd_find_log_state(__func__, fs);
	return (0);
}

/* Find state from nothing. */
int
cmd_find_from_nothing(struct cmd_find_state *fs, int flags)
{
	cmd_find_clear_state(fs, flags);

	fs->s = cmd_find_best_session(NULL, 0, flags);
	if (fs->s == NULL) {
		cmd_find_clear_state(fs, flags);
		return (-1);
	}
	fs->wl = fs->s->curw;
	fs->idx = fs->wl->idx;
	fs->w = fs->wl->window;
	fs->wp = fs->w->active;

	cmd_find_log_state(__func__, fs);
	return (0);
}

/* Find state from mouse. */
int
cmd_find_from_mouse(struct cmd_find_state *fs, struct mouse_event *m, int flags)
{
	cmd_find_clear_state(fs, flags);

	if (!m->valid)
		return (-1);

	fs->wp = cmd_mouse_pane(m, &fs->s, &fs->wl);
	if (fs->wp == NULL) {
		cmd_find_clear_state(fs, flags);
		return (-1);
	}
	fs->w = fs->wl->window;

	cmd_find_log_state(__func__, fs);
	return (0);
}

/* Find state from client. */
int
cmd_find_from_client(struct cmd_find_state *fs, struct client *c, int flags)
{
	struct window_pane	*wp;

	/* If no client, treat as from nothing. */
	if (c == NULL)
		return (cmd_find_from_nothing(fs, flags));

	/* If this is an attached client, all done. */
	if (c->session != NULL) {
		cmd_find_clear_state(fs, flags);

		fs->wp = server_client_get_pane(c);
		if (fs->wp == NULL) {
			cmd_find_from_session(fs, c->session, flags);
			return (0);
		}
		fs->s = c->session;
		fs->wl = fs->s->curw;
		fs->w = fs->wl->window;

		cmd_find_log_state(__func__, fs);
		return (0);
	}
	cmd_find_clear_state(fs, flags);

	/*
	 * If this is an unattached client running in a pane, we can use that
	 * to limit the list of sessions to those containing that pane.
	 */
	wp = cmd_find_inside_pane(c);
	if (wp == NULL)
		goto unknown_pane;

	/*
	 * Don't have a session, or it doesn't have this pane. Try all
	 * sessions.
	 */
	fs->w = wp->window;
	if (cmd_find_best_session_with_window(fs) != 0) {
		/*
		 * The window may have been destroyed but the pane
		 * still on all_window_panes due to something else
		 * holding a reference.
		 */
		goto unknown_pane;
	}
	fs->wl = fs->s->curw;
	fs->w = fs->wl->window;
	fs->wp = fs->w->active; /* use active pane */

	cmd_find_log_state(__func__, fs);
	return (0);

unknown_pane:
	/* We can't find the pane so need to guess. */
	return (cmd_find_from_nothing(fs, flags));
}

/* Find state from a popup. */
int
cmd_find_from_popup(struct cmd_find_state *fs, struct popup_data *pd)
{
	cmd_find_clear_state(fs, 0);
	fs->pd = pd;

	cmd_find_log_state(__func__, fs);
	return (0);
}

/*
 * Split target into pieces and resolve for the given type. Fills in the given
 * state. Returns 0 on success or -1 on error.
 */
int
cmd_find_target(struct cmd_find_state *fs, struct cmdq_item *item,
    const char *target, enum cmd_find_type type, int flags)
{
	struct mouse_event	*m;
	struct cmd_find_state	 current;
	char			*colon, *period, *copy = NULL, tmp[256];
	const char		*session, *window, *pane, *s, *popup;
	int			 window_only = 0, pane_only = 0;

	/* Can fail flag implies quiet. */
	if (flags & CMD_FIND_CANFAIL)
		flags |= CMD_FIND_QUIET;

	/* Log the arguments. */
	if (type == CMD_FIND_PANE)
		s = "pane";
	else if (type == CMD_FIND_WINDOW)
		s = "window";
	else if (type == CMD_FIND_SESSION)
		s = "session";
	else if (type == CMD_FIND_PANE_OR_POPUP)
		s = "pane_or_popup";
	else
		s = "unknown";
	*tmp = '\0';
	if (flags & CMD_FIND_PREFER_UNATTACHED)
		strlcat(tmp, "PREFER_UNATTACHED,", sizeof tmp);
	if (flags & CMD_FIND_QUIET)
		strlcat(tmp, "QUIET,", sizeof tmp);
	if (flags & CMD_FIND_WINDOW_INDEX)
		strlcat(tmp, "WINDOW_INDEX,", sizeof tmp);
	if (flags & CMD_FIND_DEFAULT_MARKED)
		strlcat(tmp, "DEFAULT_MARKED,", sizeof tmp);
	if (flags & CMD_FIND_EXACT_SESSION)
		strlcat(tmp, "EXACT_SESSION,", sizeof tmp);
	if (flags & CMD_FIND_EXACT_WINDOW)
		strlcat(tmp, "EXACT_WINDOW,", sizeof tmp);
	if (flags & CMD_FIND_CANFAIL)
		strlcat(tmp, "CANFAIL,", sizeof tmp);
	if (*tmp != '\0')
		tmp[strlen(tmp) - 1] = '\0';
	else
		strlcat(tmp, "NONE", sizeof tmp);
	log_debug("%s: target %s, type %s, item %p, flags %s", __func__,
	    target == NULL ? "none" : target, s, item, tmp);

	/* Clear new state. */
	cmd_find_clear_state(fs, flags);

	/* Find current state. */
	if (server_check_marked() && (flags & CMD_FIND_DEFAULT_MARKED)) {
		fs->current = &marked_pane;
		log_debug("%s: current is marked pane", __func__);
	} else if (cmd_find_valid_state(cmdq_get_current(item))) {
		fs->current = cmdq_get_current(item);
		log_debug("%s: current is from queue", __func__);
	} else if (cmd_find_from_client(&current, cmdq_get_client(item),
	    flags) == 0) {
		fs->current = &current;
		log_debug("%s: current is from client", __func__);
	} else {
		if (~flags & CMD_FIND_QUIET)
			cmdq_error(item, "no current target");
		goto error;
	}
	if (!cmd_find_valid_state(fs->current))
		fatalx("invalid current find state");

	/* An empty or NULL target is the current. */
	if (target == NULL || *target == '\0')
		goto current;

	/* Mouse target is a plain = or {mouse}. */
	if (strcmp(target, "=") == 0 || strcmp(target, "{mouse}") == 0) {
		m = &cmdq_get_event(item)->m;
		switch (type) {
		case CMD_FIND_PANE_OR_POPUP:
		case CMD_FIND_PANE:
			fs->wp = cmd_mouse_pane(m, &fs->s, &fs->wl);
			if (fs->wp != NULL) {
				fs->w = fs->wl->window;
				break;
			}
			/* FALLTHROUGH */
		case CMD_FIND_WINDOW:
		case CMD_FIND_SESSION:
			fs->wl = cmd_mouse_window(m, &fs->s);
			if (fs->wl == NULL && fs->s != NULL)
				fs->wl = fs->s->curw;
			if (fs->wl != NULL) {
				fs->w = fs->wl->window;
				fs->wp = fs->w->active;
			}
			break;
		}
		if (fs->wp == NULL) {
			if (~flags & CMD_FIND_QUIET)
				cmdq_error(item, "no mouse target");
			goto error;
		}
		goto found;
	}

	/* Marked target is a plain ~ or {marked}. */
	if (strcmp(target, "~") == 0 || strcmp(target, "{marked}") == 0) {
		if (!server_check_marked()) {
			if (~flags & CMD_FIND_QUIET)
				cmdq_error(item, "no marked target");
			goto error;
		}
		cmd_find_copy_state(fs, &marked_pane);
		goto found;
	}

	/* Find separators if they exist. */
	copy = xstrdup(target);
	colon = strchr(copy, ':');
	if (colon != NULL)
		*colon++ = '\0';
	if (colon == NULL)
		period = strchr(copy, '.');
	else
		period = strchr(colon, '.');
	if (period != NULL)
		*period++ = '\0';

	/* Set session, window and pane parts. */
	session = window = pane = popup = NULL;
	if (colon != NULL && period != NULL) {
		session = copy;
		window = colon;
		window_only = 1;
		pane = period;
		pane_only = 1;
	} else if (colon != NULL && period == NULL) {
		session = copy;
		window = colon;
		window_only = 1;
	} else if (colon == NULL && period != NULL) {
		window = copy;
		pane = period;
		pane_only = 1;
	} else {
		if (*copy == '$')
			session = copy;
		else if (*copy == '@')
			window = copy;
		else if (*copy == '%')
			pane = copy;
		else if (*copy == '^' && type == CMD_FIND_PANE_OR_POPUP)
			popup = copy;
		else {
			switch (type) {
			case CMD_FIND_SESSION:
				session = copy;
				break;
			case CMD_FIND_WINDOW:
				window = copy;
				break;
			case CMD_FIND_PANE:
			case CMD_FIND_PANE_OR_POPUP:
				pane = copy;
				break;
			}
		}
	}

	/* Set exact match flags. */
	if (session != NULL && *session == '=') {
		session++;
		fs->flags |= CMD_FIND_EXACT_SESSION;
	}
	if (window != NULL && *window == '=') {
		window++;
		fs->flags |= CMD_FIND_EXACT_WINDOW;
	}

	/* Empty is the same as NULL. */
	if (session != NULL && *session == '\0')
		session = NULL;
	if (window != NULL && *window == '\0')
		window = NULL;
	if (pane != NULL && *pane == '\0')
		pane = NULL;

	/* Map though conversion table. */
	if (session != NULL)
		session = cmd_find_map_table(cmd_find_session_table, session);
	if (window != NULL)
		window = cmd_find_map_table(cmd_find_window_table, window);
	if (pane != NULL)
		pane = cmd_find_map_table(cmd_find_pane_table, pane);

	if (session != NULL || window != NULL || pane != NULL) {
		log_debug("%s: target %s is %s%s%s%s%s%s%s%s",
		    __func__, target,
		    session == NULL ? "" : "session ",
		    session == NULL ? "" : session,
		    window == NULL ? "" : "window ",
		    window == NULL ? "" : window,
		    pane == NULL ? "" : "pane ",
		    pane == NULL ? "" : pane,
		    popup == NULL ? "" : "popup ",
		    popup == NULL ? "" : popup);
	}

	/* No pane is allowed if want an index. */
	if (pane != NULL && (flags & CMD_FIND_WINDOW_INDEX)) {
		if (~flags & CMD_FIND_QUIET)
			cmdq_error(item, "can't specify pane here");
		goto error;
	}

	if (popup != NULL) {
		if (cmd_find_get_popup(fs, popup) != 0)
			goto no_popup;
		goto found;
	}

	/* If the session isn't NULL, look it up. */
	if (session != NULL) {
		/* This will fill in session. */
		if (cmd_find_get_session(fs, session) != 0)
			goto no_session;

		/* If window and pane are NULL, use that session's current. */
		if (window == NULL && pane == NULL) {
			fs->wl = fs->s->curw;
			fs->idx = -1;
			fs->w = fs->wl->window;
			fs->wp = fs->w->active;
			goto found;
		}

		/* If window is present but pane not, find window in session. */
		if (window != NULL && pane == NULL) {
			/* This will fill in winlink and window. */
			if (cmd_find_get_window_with_session(fs, window) != 0)
				goto no_window;
			if (fs->wl != NULL) /* can be NULL if index only */
				fs->wp = fs->wl->window->active;
			goto found;
		}

		/* If pane is present but window not, find pane. */
		if (window == NULL && pane != NULL) {
			/* This will fill in winlink and window and pane. */
			if (cmd_find_get_pane_with_session(fs, pane) != 0)
				goto no_pane;
			goto found;
		}

		/*
		 * If window and pane are present, find both in session. This
		 * will fill in winlink and window.
		 */
		if (cmd_find_get_window_with_session(fs, window) != 0)
			goto no_window;
		/* This will fill in pane. */
		if (cmd_find_get_pane_with_window(fs, pane) != 0)
			goto no_pane;
		goto found;
	}

	/* No session. If window and pane, try them. */
	if (window != NULL && pane != NULL) {
		/* This will fill in session, winlink and window. */
		if (cmd_find_get_window(fs, window, window_only) != 0)
			goto no_window;
		/* This will fill in pane. */
		if (cmd_find_get_pane_with_window(fs, pane) != 0)
			goto no_pane;
		goto found;
	}

	/* If just window is present, try it. */
	if (window != NULL && pane == NULL) {
		/* This will fill in session, winlink and window. */
		if (cmd_find_get_window(fs, window, window_only) != 0)
			goto no_window;
		if (fs->wl != NULL) /* can be NULL if index only */
			fs->wp = fs->wl->window->active;
		goto found;
	}

	/* If just pane is present, try it. */
	if (window == NULL && pane != NULL) {
		/* This will fill in session, winlink, window and pane. */
		if (cmd_find_get_pane(fs, pane, pane_only) != 0)
			goto no_pane;
		goto found;
	}

current:
	/* Use the current session. */
	cmd_find_copy_state(fs, fs->current);
	if (flags & CMD_FIND_WINDOW_INDEX)
		fs->idx = -1;
	goto found;

error:
	fs->current = NULL;
	log_debug("%s: error", __func__);

	free(copy);
	if (flags & CMD_FIND_CANFAIL)
		return (0);
	return (-1);

found:
	fs->current = NULL;
	cmd_find_log_state(__func__, fs);

	free(copy);
	return (0);

no_session:
	if (~flags & CMD_FIND_QUIET)
		cmdq_error(item, "can't find session: %s", session);
	goto error;

no_window:
	if (~flags & CMD_FIND_QUIET)
		cmdq_error(item, "can't find window: %s", window);
	goto error;

no_pane:
	if (~flags & CMD_FIND_QUIET)
		cmdq_error(item, "can't find pane: %s", pane);
	goto error;
no_popup:
	if (~flags & CMD_FIND_QUIET)
		cmdq_error(item, "can't find popup: %s", popup);
	goto error;
}

/* Find the current client. */
static struct client *
cmd_find_current_client(struct cmdq_item *item, int quiet)
{
	struct client		*c = NULL, *found;
	struct session		*s;
	struct window_pane	*wp;
	struct cmd_find_state	 fs;

	if (item != NULL)
		c = cmdq_get_client(item);
	if (c != NULL && c->session != NULL)
		return (c);

	found = NULL;
	if (c != NULL && (wp = cmd_find_inside_pane(c)) != NULL) {
		cmd_find_clear_state(&fs, CMD_FIND_QUIET);
		fs.w = wp->window;
		if (cmd_find_best_session_with_window(&fs) == 0)
			found = cmd_find_best_client(fs.s);
	} else {
		s = cmd_find_best_session(NULL, 0, CMD_FIND_QUIET);
		if (s != NULL)
			found = cmd_find_best_client(s);
	}
	if (found == NULL && item != NULL && !quiet)
		cmdq_error(item, "no current client");
	log_debug("%s: no target, return %p", __func__, found);
	return (found);
}

/* Find the target client or report an error and return NULL. */
struct client *
cmd_find_client(struct cmdq_item *item, const char *target, int quiet)
{
	struct client	*c;
	char		*copy;
	size_t		 size;

	/* A NULL argument means the current client. */
	if (target == NULL)
		return (cmd_find_current_client(item, quiet));
	copy = xstrdup(target);

	/* Trim a single trailing colon if any. */
	size = strlen(copy);
	if (size != 0 && copy[size - 1] == ':')
		copy[size - 1] = '\0';

	/* Check name and path of each client. */
	TAILQ_FOREACH(c, &clients, entry) {
		if (c->session == NULL)
			continue;
		if (strcmp(copy, c->name) == 0)
			break;

		if (*c->ttyname == '\0')
			continue;
		if (strcmp(copy, c->ttyname) == 0)
			break;
		if (strncmp(c->ttyname, _PATH_DEV, (sizeof _PATH_DEV) - 1) != 0)
			continue;
		if (strcmp(copy, c->ttyname + (sizeof _PATH_DEV) - 1) == 0)
			break;
	}

	/* If no client found, report an error. */
	if (c == NULL && !quiet)
		cmdq_error(item, "can't find client: %s", copy);

	free(copy);
	log_debug("%s: target %s, return %p", __func__, target, c);
	return (c);
}
