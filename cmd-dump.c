/*
 * Copyright (c) 2026 asRizvi888 <asrizvi2357@gmail.com>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tmux.h"

/*
 * Write the layout (sessions, windows, panes, splits and per-pane working
 * directories) of every session on the server to a text file, so that it
 * can later be recreated with the 'restore' command. This does not record
 * or attempt to restore whatever process was running in each pane - only
 * the shape of the layout and where each pane's shell should start.
 */

static enum cmd_retval	cmd_dump_exec(struct cmd *, struct cmdq_item *);

/*
 * wp->cwd only records the directory the pane's shell was originally
 * spawned in - it never changes after that. What we want here is the
 * pane's current directory, which has to be queried live from the
 * process running in the pane (the same thing the pane_current_path
 * format variable does).
 */
static char *
cmd_dump_pane_cwd(struct window_pane *wp)
{
	char	*cwd;

	cwd = osdep_get_cwd(wp->fd);
	if (cwd != NULL)
		return (xstrdup(cwd));
	if (wp->cwd != NULL)
		return (xstrdup(wp->cwd));
	return (xstrdup(""));
}

const struct cmd_entry cmd_dump_entry = {
	.name = "dump",
	.alias = NULL,

	.args = { "f:", 0, 0, NULL },
	.usage = "[-f path]",

	.flags = 0,
	.exec = cmd_dump_exec
};

static enum cmd_retval
cmd_dump_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	const char		*path;
	char			*freeme = NULL;
	struct evbuffer		*evb;
	struct session		*s;
	struct winlink		*wl;
	struct window		*w;
	struct window_pane	*wp;
	char			*sess_esc, *name_esc, *cwd_esc, *layout, *layout_esc;
	char			*cwd;
	long long		 base_index, global_base_index;
	u_int			 active_pane_index;
	int			 first_window, npanes;
	time_t			 now;
	char			 tbuf[64];
	FILE			*f;
	void			*data;
	size_t			 size;

	if ((path = args_get(args, 'f')) == NULL) {
		if ((path = freeme = dump_get_path()) == NULL) {
			cmdq_error(item, "could not determine dump file "
			    "location");
			return (CMD_RETURN_ERROR);
		}
	}

	evb = evbuffer_new();
	if (evb == NULL)
		fatalx("out of memory");

	time(&now);
	strftime(tbuf, sizeof tbuf, "%Y-%m-%d %H:%M:%S", localtime(&now));
	evbuffer_add_printf(evb, "# tmux dump v1 - generated %s by tmux %s\n",
	    tbuf, getversion());
	evbuffer_add_printf(evb, "# recreated with 'tmux restore'; does not "
	    "relaunch previous pane processes\n");

	global_base_index = options_get_number(global_s_options, "base-index");

	RB_FOREACH(s, sessions, &sessions) {
		sess_esc = args_escape(s->name);
		base_index = options_get_number(s->options, "base-index");

		first_window = 1;
		RB_FOREACH(wl, winlinks, &s->windows) {
			w = wl->window;

			wp = TAILQ_FIRST(&w->panes);
			name_esc = args_escape(w->name);
			cwd = cmd_dump_pane_cwd(wp);
			cwd_esc = args_escape(cwd);
			free(cwd);

			if (first_window) {
				/* Pin base-index so this window lands at the
				 * recorded idx regardless of the restoring
				 * environment's own config. */
				evbuffer_add_printf(evb,
				    "set -g base-index %lld\n", base_index);
				evbuffer_add_printf(evb,
				    "new-session -d -s %s -n %s -c %s\n",
				    sess_esc, name_esc, cwd_esc);
				if ((long long)wl->idx != base_index) {
					evbuffer_add_printf(evb,
					    "move-window -s %s:%lld -t %s:%u\n",
					    sess_esc, base_index, sess_esc,
					    wl->idx);
				}
				first_window = 0;
			} else {
				evbuffer_add_printf(evb,
				    "new-window -d -t %s:%u -n %s -c %s\n",
				    sess_esc, wl->idx, name_esc, cwd_esc);
			}
			free(name_esc);
			free(cwd_esc);

			npanes = 0;
			TAILQ_FOREACH(wp, &w->panes, entry) {
				if (npanes > 0) {
					cwd = cmd_dump_pane_cwd(wp);
					cwd_esc = args_escape(cwd);
					free(cwd);
					evbuffer_add_printf(evb,
					    "split-window -t %s:%u -c %s\n",
					    sess_esc, wl->idx, cwd_esc);
					free(cwd_esc);
				}
				npanes++;
			}

			layout = layout_dump(w, w->layout_root);
			if (layout != NULL) {
				layout_esc = args_escape(layout);
				evbuffer_add_printf(evb,
				    "select-layout -t %s:%u %s\n", sess_esc,
				    wl->idx, layout_esc);
				free(layout_esc);
				free(layout);
			}

			if (window_pane_index(w->active, &active_pane_index)
			    == 0) {
				evbuffer_add_printf(evb,
				    "select-pane -t %s:%u.%u\n", sess_esc,
				    wl->idx, active_pane_index);
			}
		}

		if (s->curw != NULL) {
			evbuffer_add_printf(evb, "select-window -t %s:%u\n",
			    sess_esc, s->curw->idx);
		}
		free(sess_esc);
	}

	/* Put back the base-index that was in effect before the per-session overrides above. */
	evbuffer_add_printf(evb, "set -g base-index %lld\n", global_base_index);

	data = evbuffer_pullup(evb, -1);
	size = evbuffer_get_length(evb);

	f = fopen(path, "w");
	if (f == NULL) {
		cmdq_error(item, "%s: %s", path, strerror(errno));
		evbuffer_free(evb);
		free(freeme);
		return (CMD_RETURN_ERROR);
	}
	if (size != 0 && fwrite(data, 1, size, f) != size) {
		cmdq_error(item, "%s: %s", path, strerror(errno));
		fclose(f);
		evbuffer_free(evb);
		free(freeme);
		return (CMD_RETURN_ERROR);
	}
	fclose(f);
	evbuffer_free(evb);

	cmdq_print(item, "dumped layout to %s", path);
	free(freeme);
	return (CMD_RETURN_NORMAL);
}
