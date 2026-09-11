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
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tmux.h"

/*
 * Write the layout (sessions, windows, panes, splits, per-window layout
 * trees and per-pane working directories) of every session on the server
 * to a JSON file, so that it can later be recreated with 'load-layout'.
 * This does not record or attempt to restore whatever process was running
 * in each pane - only the shape of the layout and where each pane's shell
 * should start.
 */

static enum cmd_retval	cmd_save_layout_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_save_layout_entry = {
	.name = "save-layout",
	.alias = NULL,

	.args = { "f:", 0, 0, NULL },
	.usage = "[-f path]",

	.flags = CMD_AFTERHOOK,
	.exec = cmd_save_layout_exec
};

struct cmd_save_layout_data {
	struct cmdq_item	*item;
	char			*path;
};

/* Escape a string for embedding in a JSON string literal. */
static char *
cmd_save_layout_json_escape(const char *s)
{
	struct evbuffer	*evb;
	char		*out;
	unsigned char	 c;

	evb = evbuffer_new();
	if (evb == NULL)
		fatalx("out of memory");
	for (; *s != '\0'; s++) {
		c = (unsigned char)*s;
		switch (c) {
		case '"':
			evbuffer_add(evb, "\\\"", 2);
			break;
		case '\\':
			evbuffer_add(evb, "\\\\", 2);
			break;
		case '\n':
			evbuffer_add(evb, "\\n", 2);
			break;
		case '\r':
			evbuffer_add(evb, "\\r", 2);
			break;
		case '\t':
			evbuffer_add(evb, "\\t", 2);
			break;
		default:
			if (c < 0x20)
				evbuffer_add_printf(evb, "\\u%04x", c);
			else
				evbuffer_add(evb, &c, 1);
			break;
		}
	}
	evbuffer_add(evb, "", 1);
	out = xstrdup((const char *)evbuffer_pullup(evb, -1));
	evbuffer_free(evb);
	return (out);
}

/*
 * wp->cwd only records the directory the pane's shell was originally
 * spawned in - it never changes after that. What we want here is the
 * pane's current directory, which has to be queried live from the
 * process running in the pane (the same thing the pane_current_path
 * format variable does).
 */
static char *
cmd_save_layout_pane_cwd(struct window_pane *wp)
{
	char	*cwd;

	cwd = osdep_get_cwd(wp->fd);
	if (cwd != NULL)
		return (xstrdup(cwd));
	if (wp->cwd != NULL)
		return (xstrdup(wp->cwd));
	return (xstrdup(""));
}

static void
cmd_save_layout_done(__unused struct client *c, const char *path, int error,
    int closed, __unused struct evbuffer *buffer, void *data)
{
	struct cmd_save_layout_data	*cdata = data;
	struct cmdq_item		*item = cdata->item;

	if (!closed)
		return;

	if (error != 0)
		cmdq_error(item, "%s: %s", path, strerror(error));
	else
		cmdq_print(item, "saved layout to %s", path);
	cmdq_continue(item);

	free(cdata->path);
	free(cdata);
}

static enum cmd_retval
cmd_save_layout_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = cmd_get_args(self);
	struct cmd_save_layout_data	*cdata;
	const char			*path;
	char				*freeme = NULL;
	struct evbuffer			*evb;
	struct session			*s;
	struct winlink			*wl;
	struct window			*w;
	struct window_pane		*wp;
	char				*name_esc, *cwd, *cwd_esc, *layout;
	long long			 base_index;
	u_int				 active_pane;
	int				 first_session, first_window, first_pane;
	time_t				 now;
	char				 tbuf[32];
	void				*data;
	size_t				 size;

	if ((path = args_get(args, 'f')) == NULL) {
		if ((path = freeme = layout_get_path()) == NULL) {
			cmdq_error(item, "could not determine layout file "
			    "location");
			return (CMD_RETURN_ERROR);
		}
	}

	evb = evbuffer_new();
	if (evb == NULL)
		fatalx("out of memory");

	time(&now);
	strftime(tbuf, sizeof tbuf, "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
	evbuffer_add_printf(evb,
	    "{\"version\":1,\"generated\":\"%s\",\"tmux_version\":\"%s\","
	    "\"sessions\":[", tbuf, getversion());

	first_session = 1;
	RB_FOREACH(s, sessions, &sessions) {
		if (!first_session)
			evbuffer_add(evb, ",", 1);
		first_session = 0;

		name_esc = cmd_save_layout_json_escape(s->name);
		base_index = options_get_number(s->options, "base-index");
		evbuffer_add_printf(evb,
		    "{\"name\":\"%s\",\"base_index\":%lld,\"current_window\":"
		    "%u,\"windows\":[", name_esc, base_index,
		    s->curw != NULL ? s->curw->idx : 0);
		free(name_esc);

		first_window = 1;
		RB_FOREACH(wl, winlinks, &s->windows) {
			w = wl->window;
			if (!first_window)
				evbuffer_add(evb, ",", 1);
			first_window = 0;

			name_esc = cmd_save_layout_json_escape(w->name);
			evbuffer_add_printf(evb, "{\"index\":%u,\"name\":"
			    "\"%s\"", wl->idx, name_esc);
			free(name_esc);

			layout = layout_dump(w, w->layout_root, 0);
			if (layout != NULL) {
				evbuffer_add_printf(evb, ",\"layout\":%s",
				    layout);
				free(layout);
			}

			if (window_pane_index(w->active, &active_pane) == 0) {
				evbuffer_add_printf(evb,
				    ",\"active_pane\":%u", active_pane);
			}

			evbuffer_add(evb, ",\"panes\":[", 10);
			first_pane = 1;
			TAILQ_FOREACH(wp, &w->panes, entry) {
				if (!first_pane)
					evbuffer_add(evb, ",", 1);
				first_pane = 0;

				window_pane_index(wp, &active_pane);
				cwd = cmd_save_layout_pane_cwd(wp);
				cwd_esc = cmd_save_layout_json_escape(cwd);
				free(cwd);

				evbuffer_add_printf(evb,
				    "{\"index\":%u,\"cwd\":\"%s\"}",
				    active_pane, cwd_esc);
				free(cwd_esc);
			}
			evbuffer_add(evb, "]}", 2);
		}
		evbuffer_add(evb, "]}", 2);
	}
	evbuffer_add(evb, "]}", 2);

	data = evbuffer_pullup(evb, -1);
	size = evbuffer_get_length(evb);

	cdata = xcalloc(1, sizeof *cdata);
	cdata->item = item;
	cdata->path = xstrdup(path);

	file_write(cmdq_get_client(item), path, O_TRUNC, data, size,
	    cmd_save_layout_done, cdata);

	evbuffer_free(evb);
	free(freeme);
	return (CMD_RETURN_WAIT);
}
