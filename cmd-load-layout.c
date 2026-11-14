/*
 * Copyright (c) 2026 Mohammad Alfi Sharin Rizvi <asrizvi2357@gmail.com>
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Recreate the session/window/pane layout previously written by
 * 'save-layout'. This walks the parsed JSON directly and calls the same
 * internal functions new-session/new-window/split-window/select-layout use
 * (session_create, spawn_window, spawn_pane, layout_parse) - it does not
 * generate any tmux commands. It starts a fresh shell in each pane at the
 * recorded working directory; it does not try to relaunch whatever was
 * actually running there before.
 */

static enum cmd_retval	cmd_load_layout_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_load_layout_entry = {
	.name = "load-layout",
	.alias = NULL,

	.args = { "f:", 0, 0, NULL },
	.usage = "[-f path]",

	.flags = CMD_STARTSERVER,
	.exec = cmd_load_layout_exec
};

struct cmd_load_layout_data {
	struct cmdq_item	*item;
	char			*path;
};

/*
 * Split off one more pane from the last pane in 'wl's window and spawn a
 * shell in it at 'cwd'. The split shape here does not matter - it only
 * exists to get the right number of panes in place before layout_parse()
 * reshapes them into the recorded geometry.
 */
static int
cmd_load_layout_add_pane(struct cmdq_item *item, struct winlink *wl,
    const char *cwd, char **cause)
{
	struct window_pane	*wp0;
	struct layout_cell	*lc;
	struct spawn_context	 sc;

	wp0 = TAILQ_LAST(&wl->window->panes, window_panes);
	if ((lc = layout_split_pane(wp0, LAYOUT_TOPBOTTOM, -1, 0)) == NULL) {
		xasprintf(cause, "no space for pane in window %d", wl->idx);
		return (-1);
	}

	memset(&sc, 0, sizeof sc);
	sc.item = item;
	sc.s = wl->session;
	sc.wl = wl;
	sc.wp0 = wp0;
	sc.lc = lc;
	sc.idx = -1;
	sc.cwd = cwd;
	sc.flags = 0;

	if (spawn_pane(&sc, cause) == NULL)
		return (-1);
	return (0);
}

/* Create one window (and its panes) from its JSON object. */
static int
cmd_load_layout_build_window(struct cmdq_item *item, struct session *s,
    struct json_node *win, int first, char **cause)
{
	struct json_node	*panes, *pane, *layout;
	struct winlink		*wl;
	int64_t			 index;
	const char		*name, *cwd;
	char			*layout_text;
	struct spawn_context	 sc;
	int			 first_pane;

	if (json_find_number(win, "index", &index, cause) != 0)
		return (-1);
	if (json_find_string(win, "name", &name, cause) != 0)
		return (-1);
	if (json_find_array(win, "panes", &panes, cause) != 0)
		return (-1);

	if ((pane = json_array_first(panes)) == NULL) {
		xasprintf(cause, "window %lld has no panes", (long long)index);
		return (-1);
	}
	if (json_find_string(pane, "cwd", &cwd, cause) != 0)
		return (-1);

	memset(&sc, 0, sizeof sc);
	sc.item = item;
	sc.s = s;
	sc.name = name;
	sc.idx = (int)index;
	sc.cwd = cwd;
	sc.flags = first ? 0 : SPAWN_DETACHED;

	if ((wl = spawn_window(&sc, cause)) == NULL)
		return (-1);

	first_pane = 1;
	for (; pane != NULL; pane = json_array_next(pane)) {
		if (first_pane) {
			first_pane = 0;
			continue;
		}
		if (json_find_string(pane, "cwd", &cwd, cause) != 0)
			return (-1);
		if (cmd_load_layout_add_pane(item, wl, cwd, cause) != 0)
			return (-1);
	}

	if (json_find_object(win, "layout", &layout, NULL) == 0) {
		layout_text = json_to_string(layout);
		if (layout_parse(wl->window, layout_text, cause) != 0) {
			free(layout_text);
			return (-1);
		}
		free(layout_text);
	}

	return (0);
}

/* Create one session (and its windows) from its JSON object. */
static int
cmd_load_layout_build_session(struct cmdq_item *item, struct json_node *sess,
    char **cause)
{
	struct json_node	*win_array, *win, *first_panes, *first_pane;
	struct session		*s;
	struct options		*oo;
	struct environ		*env;
	int64_t			 current_window;
	const char		*name, *first_cwd;
	int			 first_window;

	if (json_find_string(sess, "name", &name, cause) != 0)
		return (-1);
	if (session_find(name) != NULL) {
		xasprintf(cause, "session \"%s\" already exists, skipping",
		    name);
		return (-1);
	}
	if (json_find_number(sess, "current_window", &current_window,
	    cause) != 0)
		return (-1);
	if (json_find_array(sess, "windows", &win_array, cause) != 0)
		return (-1);
	if ((win = json_array_first(win_array)) == NULL) {
		xasprintf(cause, "session \"%s\" has no windows", name);
		return (-1);
	}
	if (json_find_array(win, "panes", &first_panes, cause) != 0)
		return (-1);
	if ((first_pane = json_array_first(first_panes)) == NULL) {
		xasprintf(cause, "session \"%s\" has an empty window", name);
		return (-1);
	}
	if (json_find_string(first_pane, "cwd", &first_cwd, cause) != 0)
		return (-1);

	oo = options_create(global_s_options);
	env = environ_create();
	s = session_create(NULL, name, first_cwd, env, oo, NULL);

	first_window = 1;
	for (; win != NULL; win = json_array_next(win)) {
		if (cmd_load_layout_build_window(item, s, win, first_window,
		    cause) != 0) {
			session_destroy(s, 0, __func__);
			return (-1);
		}
		first_window = 0;
	}

	session_select(s, (int)current_window);
	events_fire_session("session-created", s);
	return (0);
}

static void
cmd_load_layout_done(__unused struct client *c, const char *path, int error,
    int closed, struct evbuffer *buffer, void *data)
{
	struct cmd_load_layout_data	*cdata = data;
	struct cmdq_item		*item = cdata->item;
	char				*text, *cause = NULL;
	struct json_node		*root, *sess_array, *sess;
	size_t				 size;
	int				 any_causes = 0;

	if (!closed)
		return;

	if (error != 0) {
		cmdq_error(item, "%s: %s", path, strerror(error));
		goto done;
	}

	size = EVBUFFER_LENGTH(buffer);
	if (size == 0) {
		cmdq_error(item, "%s: empty layout file", path);
		goto done;
	}
	text = xmalloc(size + 1);
	memcpy(text, EVBUFFER_DATA(buffer), size);
	text[size] = '\0';
	root = json_parse(text, &cause);
	free(text);
	if (root == NULL) {
		cmdq_error(item, "%s: %s", path, cause);
		free(cause);
		goto done;
	}

	if (json_find_array(root, "sessions", &sess_array, &cause) != 0) {
		cmdq_error(item, "%s: %s", path, cause);
		free(cause);
		json_destroy_node(root);
		goto done;
	}

	for (sess = json_array_first(sess_array); sess != NULL;
	    sess = json_array_next(sess)) {
		cause = NULL;
		if (cmd_load_layout_build_session(item, sess, &cause) != 0) {
			cfg_add_cause("%s: %s", path, cause);
			free(cause);
			any_causes = 1;
		}
	}
	json_destroy_node(root);
	if (any_causes)
		cfg_print_causes(item);

done:
	cmdq_continue(item);
	free(cdata->path);
	free(cdata);
}

static enum cmd_retval
cmd_load_layout_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = cmd_get_args(self);
	struct cmd_load_layout_data	*cdata;
	const char			*path;
	char				*freeme = NULL;

	if ((path = args_get(args, 'f')) == NULL) {
		if ((path = freeme = layout_get_path()) == NULL) {
			cmdq_error(item, "could not determine layout file "
			    "location");
			return (CMD_RETURN_ERROR);
		}
	}

	cdata = xcalloc(1, sizeof *cdata);
	cdata->item = item;
	cdata->path = xstrdup(path);

	file_read(cmdq_get_client(item), path, cmd_load_layout_done, cdata);

	free(freeme);
	return (CMD_RETURN_WAIT);
}
