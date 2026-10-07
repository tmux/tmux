/* $OpenBSD$ */

/*
 * Copyright (c) 2026 Alexandre Fiori <fiorix@gmail.com>
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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Convert windows and panes to JSON and create them again from it.
 *
 * The file is an object with a version and one window. Keys the loader does
 * not know are ignored and an empty string is written by leaving its key out,
 * so a missing string key means an empty string.
 */

#define STATE_VERSION 1

/* Pane read from a state file. */
struct state_pane {
	char	 *title;
	char	 *cwd;
	int	  argc;
	char	**argv;
	int	  zoomed;
	int	  float_over_zoom;
};

/* Window read from a state file. */
struct state_window {
	char			*name;
	char			*layout;
	u_int			 sx;
	u_int			 sy;
	int			 zoomed;
	u_int			 npanes;
	struct state_pane	*panes;
};

/* Start a key in an object, with a comma unless it is the first. */
static void
state_add_key(struct evbuffer *evb, int *comma, const char *key)
{
	if (*comma)
		evbuffer_add(evb, ",", 1);
	evbuffer_add_printf(evb, "\"%s\":", key);
	*comma = 1;
}

/* Add a boolean key, or nothing if it is false. */
static void
state_add_boolean(struct evbuffer *evb, int *comma, const char *key, int value)
{
	if (!value)
		return;
	state_add_key(evb, comma, key);
	evbuffer_add(evb, "true", 4);
}

/* Add a string key, or nothing if the string is empty. */
static void
state_add_string(struct evbuffer *evb, int *comma, const char *key,
    const char *value)
{
	if (value == NULL || *value == '\0')
		return;
	state_add_key(evb, comma, key);
	json_write_string(evb, value);
}

/*
 * Add a name or title. These are stored encoded with vis(3), so decode them to
 * write them as they were given.
 */
static void
state_add_name(struct evbuffer *evb, int *comma, const char *key,
    const char *value)
{
	char	*decoded;
	size_t	 size = strlen(value) + 1;

	decoded = xmalloc(size);
	if (strunvis(decoded, value) == -1)
		strlcpy(decoded, value, size);
	state_add_string(evb, comma, key, decoded);
	free(decoded);
}

/* Add a pane. */
static void
state_add_pane(struct evbuffer *evb, struct window_pane *wp)
{
	const char	*cwd;
	int		 comma = 0, i;

	evbuffer_add(evb, "{", 1);
	state_add_name(evb, &comma, "title", wp->base.title);
	if ((cwd = osdep_get_cwd(wp->fd)) == NULL)
		cwd = wp->cwd;
	state_add_string(evb, &comma, "cwd", cwd);
	state_add_boolean(evb, &comma, "zoomed", wp->flags & PANE_ZOOMED);
	state_add_boolean(evb, &comma, "float-over-zoom",
	    wp->flags & PANE_FLOATOVERZOOM);
	if (wp->argc != 0) {
		state_add_key(evb, &comma, "command");
		evbuffer_add(evb, "[", 1);
		for (i = 0; i < wp->argc; i++) {
			if (i != 0)
				evbuffer_add(evb, ",", 1);
			evbuffer_add(evb, "{", 1);
			if (*wp->argv[i] != '\0') {
				evbuffer_add(evb, "\"arg\":", 6);
				json_write_string(evb, wp->argv[i]);
			}
			evbuffer_add(evb, "}", 1);
		}
		evbuffer_add(evb, "]", 1);
	}
	evbuffer_add(evb, "}", 1);
}

/* Add a window. */
static int
state_add_window(struct evbuffer *evb, struct window *w, char **cause)
{
	struct window_pane	*wp;
	struct layout_cell	*lcroot;
	char			*layout;
	int			 comma = 0;

	/* A zoomed window keeps its full layout aside. */
	if (w->saved_layout_root != NULL)
		lcroot = w->saved_layout_root;
	else
		lcroot = w->layout_root;
	if ((layout = layout_dump(w, lcroot, 0)) == NULL) {
		xasprintf(cause, "can't save layout of window @%u", w->id);
		return (-1);
	}

	evbuffer_add(evb, "{", 1);
	state_add_name(evb, &comma, "name", w->name);
	state_add_key(evb, &comma, "layout");
	evbuffer_add(evb, layout, strlen(layout));
	free(layout);
	state_add_boolean(evb, &comma, "zoomed", w->flags & WINDOW_ZOOMED);
	state_add_key(evb, &comma, "panes");
	evbuffer_add(evb, "[", 1);
	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (wp != TAILQ_FIRST(&w->panes))
			evbuffer_add(evb, ",", 1);
		state_add_pane(evb, wp);
	}
	evbuffer_add(evb, "]}", 2);
	return (0);
}

/* Save a window as JSON. */
int
state_save_window(struct window *w, struct evbuffer *evb, char **cause)
{
	evbuffer_add_printf(evb, "{\"version\":%d,\"window\":", STATE_VERSION);
	if (state_add_window(evb, w, cause) != 0)
		return (-1);
	evbuffer_add(evb, "}\n", 2);
	return (0);
}

/* Get a string key, which is empty if missing. */
static int
state_get_string(struct json_node *jn, const char *key, char **out,
    char **cause)
{
	const char	*s;

	if (json_find(jn, key) == NULL) {
		*out = xstrdup("");
		return (0);
	}
	if (json_find_string(jn, key, &s, cause) != 0)
		return (-1);
	if ((*out = json_decode_string(s)) == NULL) {
		xasprintf(cause, "key \"%s\" has an invalid string", key);
		return (-1);
	}
	return (0);
}

/* Get an optional boolean key, which is false if missing. */
static int
state_get_boolean(struct json_node *jn, const char *key, int *out,
    char **cause)
{
	*out = 0;
	if (json_find(jn, key) == NULL)
		return (0);
	return (json_find_boolean(jn, key, out, cause));
}

/* Free a window read from a state file. */
static void
state_free_window(struct state_window *sw)
{
	struct state_pane	*sp;
	u_int			 i;

	for (i = 0; i < sw->npanes; i++) {
		sp = &sw->panes[i];
		free(sp->title);
		free(sp->cwd);
		cmd_free_argv(sp->argc, sp->argv);
	}
	free(sw->panes);
	free(sw->name);
	free(sw->layout);
}

/* Read a pane. */
static int
state_read_pane(struct json_node *jn, struct state_pane *sp, char **cause)
{
	struct json_node	*array, *member;
	char			*arg;

	if (json_get_object(jn, &jn) != 0) {
		*cause = xstrdup("pane is not an object");
		return (-1);
	}
	if (state_get_string(jn, "title", &sp->title, cause) != 0)
		return (-1);
	if (!check_name(sp->title)) {
		*cause = xstrdup("invalid pane title");
		return (-1);
	}
	if (state_get_string(jn, "cwd", &sp->cwd, cause) != 0)
		return (-1);
	if (state_get_boolean(jn, "zoomed", &sp->zoomed, cause) != 0)
		return (-1);
	if (state_get_boolean(jn, "float-over-zoom", &sp->float_over_zoom,
	    cause) != 0)
		return (-1);

	if (json_find(jn, "command") == NULL)
		return (0);
	if (json_find_array(jn, "command", &array, cause) != 0)
		return (-1);
	member = json_array_first(array);
	while (member != NULL) {
		if (state_get_string(member, "arg", &arg, cause) != 0)
			return (-1);
		sp->argv = xreallocarray(sp->argv, sp->argc + 1,
		    sizeof *sp->argv);
		sp->argv[sp->argc++] = arg;
		member = json_array_next(member);
	}
	return (0);
}

/* Read a window. Nothing is created, so this can fail without side effects. */
static int
state_read_window(struct json_node *jn, struct state_window *sw, char **cause)
{
	struct json_node	*layout, *root, *array, *member;
	int64_t			 sx, sy;

	if (state_get_string(jn, "name", &sw->name, cause) != 0)
		return (-1);
	if (!check_name(sw->name)) {
		*cause = xstrdup("invalid window name");
		return (-1);
	}
	if (state_get_boolean(jn, "zoomed", &sw->zoomed, cause) != 0)
		return (-1);

	if (json_find_object(jn, "layout", &layout, cause) != 0)
		return (-1);
	sw->layout = json_to_string(layout);

	if (json_find_array(jn, "panes", &array, cause) != 0)
		return (-1);
	member = json_array_first(array);
	while (member != NULL) {
		sw->panes = xreallocarray(sw->panes, sw->npanes + 1,
		    sizeof *sw->panes);
		memset(&sw->panes[sw->npanes], 0, sizeof *sw->panes);
		sw->npanes++;
		if (state_read_pane(member, &sw->panes[sw->npanes - 1],
		    cause) != 0)
			return (-1);
		member = json_array_next(member);
	}
	if (sw->npanes == 0) {
		*cause = xstrdup("window has no panes");
		return (-1);
	}

	/* The checked layout has a root cell with a size. */
	if (layout_check_string(sw->layout, sw->npanes, cause) != 0)
		return (-1);
	json_find_object(layout, "L", &root, NULL);
	json_find_number(root, "w", &sx, NULL);
	json_find_number(root, "h", &sy, NULL);
	sw->sx = sx;
	sw->sy = sy;
	return (0);
}

/*
 * Create a window from what was read. The panes are created empty, so nothing
 * runs in them, but keep the command and working directory for respawn-pane.
 */
static struct winlink *
state_build_window(struct state_window *sw, struct session *s, int idx,
    struct client *tc, int detached, char **cause)
{
	struct spawn_context	 sc;
	struct winlink		*wl;
	struct window		*w;
	struct window_pane	*wp, *zoomed;
	struct state_pane	*sp;
	struct layout_geometry	 lg;
	char			*name, *error;
	u_int			 i;
	int			 size;

	/*
	 * The item is left out of the spawn context so the working directory
	 * is used as it is rather than expanded as a format. The window is not
	 * made current until it is complete.
	 */
	memset(&sc, 0, sizeof sc);
	sc.s = s;
	sc.tc = tc;
	sc.name = name = clean_name(sw->name, 0);
	sc.idx = idx;
	sc.cwd = sw->panes[0].cwd;
	sc.argc = sw->panes[0].argc;
	sc.argv = sw->panes[0].argv;
	sc.flags = SPAWN_EMPTY|SPAWN_DETACHED;
	wl = spawn_window(&sc, &error);
	free(name);
	if (wl == NULL) {
		xasprintf(cause, "create window failed: %s", error);
		free(error);
		return (NULL);
	}
	w = wl->window;

	/*
	 * Give the other panes floating cells, which need no space. The
	 * layout replaces them.
	 */
	lg.sx = w->sx;
	lg.sy = w->sy;
	lg.xoff = 0;
	lg.yoff = 0;
	for (i = 1; i < sw->npanes; i++) {
		sp = &sw->panes[i];

		memset(&sc, 0, sizeof sc);
		sc.s = s;
		sc.wl = wl;
		sc.tc = tc;
		sc.wp0 = TAILQ_LAST(&w->panes, window_panes);
		sc.lc = layout_floating_pane(w, sc.wp0, &lg);
		sc.idx = -1;
		sc.cwd = sp->cwd;
		sc.argc = sp->argc;
		sc.argv = sp->argv;
		sc.flags = SPAWN_EMPTY|SPAWN_DETACHED|SPAWN_NONOTIFY;
		if (spawn_pane(&sc, cause) == NULL)
			goto fail;
	}

	/*
	 * A window with a manual size keeps it, so give it the size of the
	 * layout before applying it.
	 */
	size = options_get_number(w->options, "window-size");
	if (size == WINDOW_SIZE_MANUAL) {
		w->manual_sx = sw->sx;
		w->manual_sy = sw->sy;
	}
	if (layout_parse(w, sw->layout, cause) != 0)
		goto fail;

	/*
	 * Mark the floating panes that stay above a zoomed pane before
	 * zooming, so window_zoom leaves one of them active if it was.
	 */
	zoomed = NULL;
	i = 0;
	TAILQ_FOREACH(wp, &w->panes, entry) {
		sp = &sw->panes[i++];
		if (sp->float_over_zoom)
			wp->flags |= PANE_FLOATOVERZOOM;
		if (sp->zoomed)
			zoomed = wp;
	}
	if (sw->zoomed)
		window_zoom(zoomed != NULL ? zoomed : w->active);

	i = 0;
	TAILQ_FOREACH(wp, &w->panes, entry)
		screen_set_title(&wp->base, sw->panes[i++].title, 0);

	if (!detached)
		session_select(s, wl->idx);
	events_fire_window("window-layout-changed", w);
	return (wl);

fail:
	session_detach(s, wl);
	return (NULL);
}

/* Create a window from JSON written by state_save_window. */
struct winlink *
state_load_window(const char *data, struct session *s, int idx,
    struct client *tc, int detached, char **cause)
{
	struct json_node	*root, *jn;
	struct state_window	 sw;
	struct winlink		*wl = NULL;
	int64_t			 version;
	char			*error = NULL;

	memset(&sw, 0, sizeof sw);
	if ((root = json_parse(data, &error)) == NULL) {
		xasprintf(cause, "invalid JSON: %s", error);
		free(error);
		return (NULL);
	}
	if (json_find_number(root, "version", &version, cause) != 0)
		goto out;
	if (version != STATE_VERSION) {
		xasprintf(cause, "unsupported version %lld",
		    (long long)version);
		goto out;
	}
	if (json_find_object(root, "window", &jn, cause) != 0)
		goto out;
	if (state_read_window(jn, &sw, cause) != 0)
		goto out;
	wl = state_build_window(&sw, s, idx, tc, detached, cause);

out:
	state_free_window(&sw);
	json_destroy_node(root);
	return (wl);
}
