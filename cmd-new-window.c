/* $OpenBSD: cmd-new-window.c,v 1.13 2010/03/27 11:46:58 nicm Exp $ */

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

#include <stdlib.h>

#include "tmux.h"

/*
 * Create a new window.
 */

int	cmd_new_window_parse(struct cmd *, int, char **, char **);
int	cmd_new_window_exec(struct cmd *, struct cmd_ctx *);
void	cmd_new_window_free(struct cmd *);
void	cmd_new_window_init(struct cmd *, int);
size_t	cmd_new_window_print(struct cmd *, char *, size_t);

struct cmd_new_window_data {
	char	*target;
	char	*name;
	char	*cmd;
	int	 flag_insert_after;
	int	 flag_detached;
	int	 flag_kill;
};

const struct cmd_entry cmd_new_window_entry = {
	"new-window", "neww",
	"[-adk] [-n window-name] [-t target-window] [command]",
	0, "",
	cmd_new_window_init,
	cmd_new_window_parse,
	cmd_new_window_exec,
	cmd_new_window_free,
	cmd_new_window_print
};

/* ARGSUSED */
void
cmd_new_window_init(struct cmd *self, unused int arg)
{
	struct cmd_new_window_data	 *data;

	self->data = data = xmalloc(sizeof *data);
	data->target = NULL;
	data->name = NULL;
	data->cmd = NULL;
	data->flag_insert_after = 0;
	data->flag_detached = 0;
	data->flag_kill = 0;
}

int
cmd_new_window_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_new_window_data	*data;
	int				 opt;

	self->entry->init(self, KEYC_NONE);
	data = self->data;

	while ((opt = getopt(argc, argv, "adkt:n:")) != -1) {
		switch (opt) {
		case 'a':
			data->flag_insert_after = 1;
			break;
		case 'd':
			data->flag_detached = 1;
			break;
		case 'k':
			data->flag_kill = 1;
			break;
		case 't':
			if (data->target == NULL)
				data->target = xstrdup(optarg);
			break;
		case 'n':
			if (data->name == NULL)
				data->name = xstrdup(optarg);
			break;
		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 0 && argc != 1)
		goto usage;

	if (argc == 1)
		data->cmd = xstrdup(argv[0]);

	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

	self->entry->free(self);
	return (-1);
}

int
cmd_new_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_new_window_data	*data = self->data;
	struct session			*s;
	struct winlink			*wl;
	char				*cmd, *cwd, *cause;
	int				 idx, last;

	if (data == NULL)
		return (0);

	if (data->flag_insert_after) {
		if ((wl = cmd_find_window(ctx, data->target, &s)) == NULL)
			return (-1);
		idx = wl->idx + 1;

		/* Find the next free index. */
		for (last = idx; last < INT_MAX; last++) {
			if (winlink_find_by_index(&s->windows, last) == NULL)
				break;
		}
		if (last == INT_MAX) {
			ctx->error(ctx, "no free window indexes");
			return (-1);
		}

		/* Move everything from last - 1 to idx up a bit. */
		for (; last > idx; last--) {
			wl = winlink_find_by_index(&s->windows, last - 1);
			server_link_window(s, wl, s, last, 0, 0, NULL);
			server_unlink_window(s, wl);
		}
	} else {
		if ((idx = cmd_find_index(ctx, data->target, &s)) == -2)
			return (-1);
	}

	wl = NULL;
	if (idx != -1)
		wl = winlink_find_by_index(&s->windows, idx);
	if (wl != NULL && data->flag_kill) {
		/*
		 * Can't use session_detach as it will destroy session if this
		 * makes it empty.
		 */
		session_alert_cancel(s, wl);
		winlink_stack_remove(&s->lastw, wl);
		winlink_remove(&s->windows, wl);

		/* Force select/redraw if current. */
		if (wl == s->curw) {
			data->flag_detached = 0;
			s->curw = NULL;
		}
	}

	cmd = data->cmd;
	if (cmd == NULL)
		cmd = options_get_string(&s->options, "default-command");
	if (ctx->cmdclient == NULL || ctx->cmdclient->cwd == NULL)
		cwd = options_get_string(&s->options, "default-path");
	else
		cwd = ctx->cmdclient->cwd;

	if (idx == -1)
		idx = -1 - options_get_number(&s->options, "base-index");
	wl = session_new(s, data->name, cmd, cwd, idx, &cause);
	if (wl == NULL) {
		ctx->error(ctx, "create window failed: %s", cause);
		xfree(cause);
		return (-1);
	}
	if (!data->flag_detached) {
		session_select(s, wl->idx);
		server_redraw_session_group(s);
	} else
		server_status_session_group(s);

	return (0);
}

void
cmd_new_window_free(struct cmd *self)
{
	struct cmd_new_window_data	*data = self->data;

	if (data->target != NULL)
		xfree(data->target);
	if (data->name != NULL)
		xfree(data->name);
	if (data->cmd != NULL)
		xfree(data->cmd);
	xfree(data);
}

size_t
cmd_new_window_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_new_window_data	*data = self->data;
	size_t				 off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return (off);
	if (off < len && data->flag_detached)
		off += xsnprintf(buf + off, len - off, " -d");
	if (off < len && data->target != NULL)
		off += cmd_prarg(buf + off, len - off, " -t ", data->target);
	if (off < len && data->name != NULL)
		off += cmd_prarg(buf + off, len - off, " -n ", data->name);
	if (off < len && data->cmd != NULL)
		off += cmd_prarg(buf + off, len - off, " ", data->cmd);
	return (off);
}
