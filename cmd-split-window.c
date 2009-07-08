/* $OpenBSD: cmd-split-window.c,v 1.3 2009/07/07 07:01:10 nicm Exp $ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicm@users.sourceforge.net>
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
#include <unistd.h>

#include "tmux.h"

/*
 * Split a window (add a new pane).
 */

int	cmd_split_window_parse(struct cmd *, int, char **, char **);
int	cmd_split_window_exec(struct cmd *, struct cmd_ctx *);
void	cmd_split_window_send(struct cmd *, struct buffer *);
void	cmd_split_window_recv(struct cmd *, struct buffer *);
void	cmd_split_window_free(struct cmd *);
void	cmd_split_window_init(struct cmd *, int);
size_t	cmd_split_window_print(struct cmd *, char *, size_t);

struct cmd_split_window_data {
	char	*target;
	char	*cmd;
	int	 flag_detached;
	int	 percentage;
	int	 lines;
};

const struct cmd_entry cmd_split_window_entry = {
	"split-window", "splitw",
	"[-d] [-p percentage|-l lines] [-t target-window] [command]",
	0,
	cmd_split_window_init,
	cmd_split_window_parse,
	cmd_split_window_exec,
	cmd_split_window_send,
	cmd_split_window_recv,
	cmd_split_window_free,
	cmd_split_window_print
};

void
cmd_split_window_init(struct cmd *self, unused int arg)
{
	struct cmd_split_window_data	 *data;

	self->data = data = xmalloc(sizeof *data);
	data->target = NULL;
	data->cmd = NULL;
	data->flag_detached = 0;
	data->percentage = -1;
	data->lines = -1;
}

int
cmd_split_window_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_split_window_data	*data;
	int				 opt, n;
	const char			*errstr;

	self->entry->init(self, 0);
	data = self->data;

	while ((opt = getopt(argc, argv, "dl:p:t:")) != -1) {
		switch (opt) {
		case 'd':
			data->flag_detached = 1;
			break;
		case 't':
			if (data->target == NULL)
				data->target = xstrdup(optarg);
			break;
		case 'l':
			if (data->percentage == -1 && data->lines == -1) {
				n = strtonum(optarg, 1, INT_MAX, &errstr);
				if (errstr != NULL) {
					xasprintf(cause, "lines %s", errstr);
					goto error;
				}
				data->lines = n;
			}
			break;
		case 'p':
			if (data->lines == -1 && data->percentage == -1) {
				n = strtonum(optarg, 1, 100, &errstr);
				if (errstr != NULL) {
					xasprintf(
					    cause, "percentage %s", errstr);
					goto error;
				}
				data->percentage = n;
			}
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

error:
	self->entry->free(self);
	return (-1);
}

int
cmd_split_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_split_window_data	*data = self->data;
	struct session			*s;
	struct winlink			*wl;
	struct window			*w;
	struct window_pane		*wp;
	const char		       **env;
	char		 		*cmd, *cwd, *cause;
	u_int				 hlimit;
	int				 lines;

	if ((wl = cmd_find_window(ctx, data->target, &s)) == NULL)
		return (-1);
	w = wl->window;

	env = server_fill_environ(s);

	cmd = data->cmd;
	if (cmd == NULL)
		cmd = options_get_string(&s->options, "default-command");
	if (ctx->cmdclient == NULL || ctx->cmdclient->cwd == NULL)
		cwd = options_get_string(&s->options, "default-path");
	else
		cwd = ctx->cmdclient->cwd;

	lines = -1;
	if (data->lines != -1)
		lines = data->lines;
	else if (data->percentage != -1)
		lines = (w->active->sy * data->percentage) / 100;

	hlimit = options_get_number(&s->options, "history-limit");
	wp = window_add_pane(w, lines, cmd, cwd, env, hlimit, &cause);
	if (wp == NULL) {
		ctx->error(ctx, "create pane failed: %s", cause);
		xfree(cause);
		return (-1);
	}
	server_redraw_window(w);

	if (!data->flag_detached) {
		window_set_active_pane(w, wp);
		session_select(s, wl->idx);
		server_redraw_session(s);
	} else
		server_status_session(s);
	layout_refresh(w, 0);

	return (0);
}

void
cmd_split_window_send(struct cmd *self, struct buffer *b)
{
	struct cmd_split_window_data	*data = self->data;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->target);
	cmd_send_string(b, data->cmd);
}

void
cmd_split_window_recv(struct cmd *self, struct buffer *b)
{
	struct cmd_split_window_data	*data;

	self->data = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->target = cmd_recv_string(b);
	data->cmd = cmd_recv_string(b);
}

void
cmd_split_window_free(struct cmd *self)
{
	struct cmd_split_window_data	*data = self->data;

	if (data->target != NULL)
		xfree(data->target);
	if (data->cmd != NULL)
		xfree(data->cmd);
	xfree(data);
}

size_t
cmd_split_window_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_split_window_data	*data = self->data;
	size_t				 off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return (off);
	if (off < len && data->flag_detached)
		off += xsnprintf(buf + off, len - off, " -d");
	if (off < len && data->target != NULL)
		off += cmd_prarg(buf + off, len - off, " -t ", data->target);
	if (off < len && data->cmd != NULL)
		off += cmd_prarg(buf + off, len - off, " ", data->cmd);
	return (off);
}
