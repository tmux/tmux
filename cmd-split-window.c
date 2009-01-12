/* $Id: cmd-split-window.c,v 1.3 2009-01-12 18:22:47 nicm Exp $ */

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
 * Create a new window.
 */

int	cmd_split_window_parse(struct cmd *, int, char **, char **);
void	cmd_split_window_exec(struct cmd *, struct cmd_ctx *);
void	cmd_split_window_send(struct cmd *, struct buffer *);
void	cmd_split_window_recv(struct cmd *, struct buffer *);
void	cmd_split_window_free(struct cmd *);
void	cmd_split_window_init(struct cmd *, int);
void	cmd_split_window_print(struct cmd *, char *, size_t);

struct cmd_split_window_data {
	char	*target;
	char	*cmd;
	int	 flag_detached;
	
	int	 lines;
	int	 percentage;
};

const struct cmd_entry cmd_split_window_entry = {
	"split-window", "splitw",
	"[-d] [-t target-window] [-l lines|-p percentage] [command]",
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
	data->lines = -1;
	data->percentage = -1;
}

int
cmd_split_window_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_split_window_data	*data;
	int				 opt, n;
	const char		        *errstr;

	self->entry->init(self, 0);
	data = self->data;

	while ((opt = getopt(argc, argv, "dl:p:t:")) != -1) {
		switch (opt) {
		case 'd':
			data->flag_detached = 1;
			break;
		case 'l':
			if (data->percentage != -1)
				goto usage;
			if (data->lines == -1) {
				n = strtonum(optarg, 0, INT_MAX, &errstr);
				if (errstr != NULL) {
					xasprintf(cause, "number %s", errstr);
					goto error;
				}
				data->lines = n;
			}
			break;
		case 'p':
			if (data->lines != -1)
				goto usage;
			if (data->percentage == -1) {
				n = strtonum(optarg, 0, INT_MAX, &errstr);
				if (errstr != NULL) {
					xasprintf(cause, "number %s", errstr);
					goto error;
				}
				data->percentage = n;
			}
			break;
		case 't':
			if (data->target == NULL)
				data->target = xstrdup(optarg);
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

void
cmd_split_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_split_window_data	*data = self->data;
	struct session			*s;
	struct winlink			*wl;
	struct window			*w;
	const char			*env[] = CHILD_ENVIRON;
	char		 		 buf[256];
	char				*cmd, *cwd;
	u_int				 i, hlimit, size;

	if ((wl = cmd_find_window(ctx, data->target, &s)) == NULL)
		return;
	w = wl->window;

	if (w->panes[1] != NULL) {
		ctx->error(ctx, "window is already split");
		return;
	}

	if (session_index(s, &i) != 0)
		fatalx("session not found");
	xsnprintf(buf, sizeof buf, "TMUX=%ld,%u", (long) getpid(), i);
	env[0] = buf;

	cmd = data->cmd;
	if (cmd == NULL)
		cmd = options_get_string(&s->options, "default-command");
	if (ctx->cmdclient == NULL || ctx->cmdclient->cwd == NULL)
		cwd = options_get_string(&global_options, "default-path");
	else
		cwd = ctx->cmdclient->cwd;

	size = w->sy / 2;
	if (data->lines != -1)
		size = data->lines;
	if (data->percentage != -1) {
		if (data->percentage > 100)
			data->percentage = 100;
		size = (w->sy * data->percentage) / 100;
	}
	if (size > w->sy)
		size = w->sy;	/* let window_add_pane sort it out */

	hlimit = options_get_number(&s->options, "history-limit");
	if (window_add_pane(w, size, cmd, cwd, env, hlimit) < 0) {
		ctx->error(ctx, "command failed: %s", cmd);
		return;
	}
	server_redraw_window(w);
	
	if (!data->flag_detached) {
		w->active = w->panes[1];
		session_select(s, wl->idx);
		server_redraw_session(s);
	} else
		server_status_session(s);

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
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

void
cmd_split_window_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_split_window_data	*data = self->data;
	size_t				 off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return;
	if (off < len && data->flag_detached)
		off += xsnprintf(buf + off, len - off, " -d");
	if (off < len && data->target != NULL)
		off += xsnprintf(buf + off, len - off, " -t %s", data->target);
	if (off < len && data->cmd != NULL)
		off += xsnprintf(buf + off, len - off, " %s", data->cmd);
}
