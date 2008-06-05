/* $Id: cmd-link-window.c,v 1.18 2008-06-05 17:12:10 nicm Exp $ */

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

#include <getopt.h>
#include <stdlib.h>

#include "tmux.h"

/*
 * Link a window into another session.
 */

int	cmd_link_window_parse(struct cmd *, int, char **, char **);
void	cmd_link_window_exec(struct cmd *, struct cmd_ctx *);
void	cmd_link_window_send(struct cmd *, struct buffer *);
void	cmd_link_window_recv(struct cmd *, struct buffer *);
void	cmd_link_window_free(struct cmd *);
void	cmd_link_window_print(struct cmd *, char *, size_t);

struct cmd_link_window_data {
	char	*cname;
	char	*sname;
	int	 idx;
	int	 flag_detached;
	int	 flag_kill;
	int	 srcidx;
	char	*srcname;
};

const struct cmd_entry cmd_link_window_entry = {
	"link-window", "linkw",
	"[-dk] [-c client-tty|-s session-name] [-i index] session-name index",
	0,
	cmd_link_window_parse,
	cmd_link_window_exec,
	cmd_link_window_send,
	cmd_link_window_recv,
	cmd_link_window_free,
	NULL,
	cmd_link_window_print
};

int
cmd_link_window_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_link_window_data	*data;
	const char			*errstr;
	int				 opt;

	self->data = data = xmalloc(sizeof *data);
	data->cname = NULL;
	data->sname = NULL;
	data->flag_detached = 0;
	data->flag_kill = 0;
	data->idx = -1;
	data->srcidx = -1;
	data->srcname = NULL;

	while ((opt = getopt(argc, argv, "c:di:ks:")) != EOF) {
		switch (opt) {
		case 'c':
			if (data->sname != NULL)
				goto usage;
			if (data->cname == NULL)
				data->cname = xstrdup(optarg);
			break;
		case 'd':
			data->flag_detached = 1;
			break;
		case 'i':
			data->idx = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL) {
				xasprintf(cause, "index %s", errstr);
				goto error;
			}
			break;
		case 'k':
			data->flag_kill = 1;
			break;
		case 's':
			if (data->cname != NULL)
				goto usage;
			if (data->sname == NULL)
				data->sname = xstrdup(optarg);
			break;
		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 2)
		goto usage;

	data->srcname = xstrdup(argv[0]);
	data->srcidx = strtonum(argv[1], 0, INT_MAX, &errstr);
	if (errstr != NULL) {
		xasprintf(cause, "index %s", errstr);
		goto error;
	}

	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

error:
	self->entry->free(self);
	return (-1);
}

void
cmd_link_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_link_window_data	*data = self->data;
	struct session			*s, *src;
	struct winlink			*wl, *wl2;

	if (data == NULL)
		return;

	if ((s = cmd_find_session(ctx, data->cname, data->sname)) == NULL)
		return;

	if ((src = session_find(data->srcname)) == NULL) {
		ctx->error(ctx, "session not found: %s", data->srcname);
		return;
	}

	if (data->srcidx < 0)
		data->srcidx = -1;
	if (data->srcidx == -1)
		wl = src->curw;
	else {
		wl = winlink_find_by_index(&src->windows, data->srcidx);
		if (wl == NULL) {
			ctx->error(ctx, "no window %d", data->srcidx);
			return;
		}
	}

	if (data->idx < 0)
		data->idx = -1;
	if (data->flag_kill && data->idx != -1) {
		wl2 = winlink_find_by_index(&s->windows, data->idx);
		if (wl2 == NULL) {
			ctx->error(ctx, "no window %d", data->idx);
			return;
		}

		/*
		 * Can't use session_detach as it will destroy session if this
		 * makes it empty.
		 */
		session_alert_cancel(s, wl2);
		winlink_remove(&s->windows, wl2);

		/* Force select/redraw if current. */
		if (wl2 == s->curw) {
			data->flag_detached = 0;
			s->curw = NULL;
		}
		if (wl2 == s->lastw)
			s->lastw = NULL;

		/*
		 * Can't error out after this or there could be an empty
		 * session!
		 */
	}

	wl = session_attach(s, wl->window, data->idx);
	if (wl == NULL) {
		ctx->error(ctx, "index in use: %d", data->idx);
		return;
	}

	if (!data->flag_detached) {
		session_select(s, wl->idx);
		server_redraw_session(s);
	} else
		server_status_session(s);

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}

void
cmd_link_window_send(struct cmd *self, struct buffer *b)
{
	struct cmd_link_window_data	*data = self->data;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->cname);
	cmd_send_string(b, data->sname);
	cmd_send_string(b, data->srcname);
}

void
cmd_link_window_recv(struct cmd *self, struct buffer *b)
{
	struct cmd_link_window_data	*data;

	self->data = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->cname = cmd_recv_string(b);
	data->sname = cmd_recv_string(b);
	data->srcname = cmd_recv_string(b);
}

void
cmd_link_window_free(struct cmd *self)
{
	struct cmd_link_window_data	*data = self->data;

	if (data->cname != NULL)
		xfree(data->cname);
	if (data->sname != NULL)
		xfree(data->sname);
	if (data->srcname != NULL)
		xfree(data->srcname);
	xfree(data);
}

void
cmd_link_window_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_link_window_data	*data = self->data;
	size_t				 off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return;
	if (off < len && data->flag_detached)
		off += xsnprintf(buf + off, len - off, " -d");
	if (off < len && data->flag_kill)
		off += xsnprintf(buf + off, len - off, " -k");
	if (off < len && data->cname != NULL)
		off += xsnprintf(buf + off, len - off, " -c %s", data->cname);
	if (off < len && data->sname != NULL)
		off += xsnprintf(buf + off, len - off, " -s %s", data->sname);
	if (off < len && data->idx != -1)
		off += xsnprintf(buf + off, len - off, " -i %d", data->idx);
	if (off < len && data->srcname != NULL)
		off += xsnprintf(buf + off, len - off, " %s", data->srcname);
	if (off < len && data->srcidx != -1)
		off += xsnprintf(buf + off, len - off, " %d", data->srcidx);
}
