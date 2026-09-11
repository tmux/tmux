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
 * 'save-layout'. This replays the recorded layout as a sequence of
 * new-session/new-window/split-window/select-layout commands - it starts a
 * fresh shell in each pane at the recorded working directory, it does not
 * try to relaunch whatever was actually running there before.
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
 * Build a script of tmux commands (understood by cmd-parse, same as a
 * configuration file) from one window's JSON object and append it to
 * 'script'. Returns 0 on success or -1 (with *cause set) on error.
 */
static int
cmd_load_layout_build_window(struct evbuffer *script, struct json_node *win,
    const char *sess_esc, long long base_index, int *first_window,
    char **cause)
{
	struct json_node	*panes, *pane, *layout;
	int64_t			 index, active_pane;
	const char		*name, *cwd;
	char			*name_esc, *cwd_esc, *layout_text, *layout_esc;
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

	name_esc = args_escape(name);
	cwd_esc = args_escape(cwd);
	if (*first_window) {
		evbuffer_add_printf(script, "set -g base-index %lld\n",
		    base_index);
		evbuffer_add_printf(script,
		    "new-session -d -s %s -n %s -c %s\n", sess_esc, name_esc,
		    cwd_esc);
		if (index != base_index) {
			evbuffer_add_printf(script,
			    "move-window -s %s:%lld -t %s:%lld\n", sess_esc,
			    base_index, sess_esc, (long long)index);
		}
		*first_window = 0;
	} else {
		evbuffer_add_printf(script,
		    "new-window -d -t %s:%lld -n %s -c %s\n", sess_esc,
		    (long long)index, name_esc, cwd_esc);
	}
	free(name_esc);
	free(cwd_esc);

	first_pane = 1;
	for (; pane != NULL; pane = json_array_next(pane)) {
		if (first_pane) {
			first_pane = 0;
			continue;
		}
		if (json_find_string(pane, "cwd", &cwd, cause) != 0)
			return (-1);
		cwd_esc = args_escape(cwd);
		evbuffer_add_printf(script, "split-window -t %s:%lld -c %s\n",
		    sess_esc, (long long)index, cwd_esc);
		free(cwd_esc);
	}

	if (json_find_object(win, "layout", &layout, NULL) == 0) {
		layout_text = json_to_string(layout);
		layout_esc = args_escape(layout_text);
		free(layout_text);
		evbuffer_add_printf(script, "select-layout -t %s:%lld %s\n",
		    sess_esc, (long long)index, layout_esc);
		free(layout_esc);
	}

	if (json_find_number(win, "active_pane", &active_pane, NULL) == 0) {
		evbuffer_add_printf(script, "select-pane -t %s:%lld.%lld\n",
		    sess_esc, (long long)index, active_pane);
	}

	return (0);
}

/* Build a script of tmux commands from one session's JSON object. */
static int
cmd_load_layout_build_session(struct evbuffer *script, struct json_node *sess,
    char **cause)
{
	struct json_node	*win_array, *win;
	int64_t			 base_index, current_window;
	const char		*name;
	char			*sess_esc;
	int			 first_window;

	if (json_find_string(sess, "name", &name, cause) != 0)
		return (-1);
	if (json_find_number(sess, "base_index", &base_index, cause) != 0)
		return (-1);
	if (json_find_number(sess, "current_window", &current_window,
	    cause) != 0)
		return (-1);
	if (json_find_array(sess, "windows", &win_array, cause) != 0)
		return (-1);

	sess_esc = args_escape(name);

	first_window = 1;
	for (win = json_array_first(win_array); win != NULL;
	    win = json_array_next(win)) {
		if (cmd_load_layout_build_window(script, win, sess_esc,
		    base_index, &first_window, cause) != 0) {
			free(sess_esc);
			return (-1);
		}
	}
	if (first_window) {
		xasprintf(cause, "session \"%s\" has no windows", name);
		free(sess_esc);
		return (-1);
	}

	evbuffer_add_printf(script, "select-window -t %s:%lld\n", sess_esc,
	    current_window);
	free(sess_esc);
	return (0);
}

/* Build a full script of tmux commands from the parsed layout file. */
static struct evbuffer *
cmd_load_layout_build_script(struct json_node *root, char **cause)
{
	struct evbuffer		*script;
	struct json_node	*sess_array, *sess;
	long long		 global_base_index;

	if (json_find_array(root, "sessions", &sess_array, cause) != 0)
		return (NULL);

	script = evbuffer_new();
	if (script == NULL)
		fatalx("out of memory");

	global_base_index = options_get_number(global_s_options,
	    "base-index");

	for (sess = json_array_first(sess_array); sess != NULL;
	    sess = json_array_next(sess)) {
		if (cmd_load_layout_build_session(script, sess, cause) != 0) {
			evbuffer_free(script);
			return (NULL);
		}
	}

	evbuffer_add_printf(script, "set -g base-index %lld\n",
	    global_base_index);
	return (script);
}

static void
cmd_load_layout_done(__unused struct client *c, const char *path, int error,
    int closed, struct evbuffer *buffer, void *data)
{
	struct cmd_load_layout_data	*cdata = data;
	struct cmdq_item		*item = cdata->item;
	struct cmd_find_state		*current = cmdq_get_current(item);
	char				*text, *cause = NULL;
	struct json_node		*root;
	struct evbuffer			*script;
	size_t				 size;

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

	script = cmd_load_layout_build_script(root, &cause);
	json_destroy_node(root);
	if (script == NULL) {
		cmdq_error(item, "%s: %s", path, cause);
		free(cause);
		goto done;
	}

	if (load_cfg_from_buffer(EVBUFFER_DATA(script),
	    EVBUFFER_LENGTH(script), path, cmdq_get_client(item), item,
	    current, 0, NULL) != 0)
		cfg_print_causes(item);
	evbuffer_free(script);

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
