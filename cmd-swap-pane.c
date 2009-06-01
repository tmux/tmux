/* $OpenBSD$ */

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

#include "tmux.h"

/*
 * Swap two panes.
 */

int	cmd_swap_pane_parse(struct cmd *, int, char **, char **);
int	cmd_swap_pane_exec(struct cmd *, struct cmd_ctx *);
void	cmd_swap_pane_send(struct cmd *, struct buffer *);
void	cmd_swap_pane_recv(struct cmd *, struct buffer *);
void	cmd_swap_pane_free(struct cmd *);
void	cmd_swap_pane_init(struct cmd *, int);
size_t	cmd_swap_pane_print(struct cmd *, char *, size_t);

struct cmd_swap_pane_data {
	char	*target;
        int	 src;
	int	 dst;
	int	 flag_detached;
	int	 flag_up;
	int	 flag_down;
};

const struct cmd_entry cmd_swap_pane_entry = {
	"swap-pane", "swapp",
	"[-dDU] [-t target-window] [-p src-index] [-q dst-index]",
	0,
	cmd_swap_pane_init,
	cmd_swap_pane_parse,
	cmd_swap_pane_exec,
	cmd_swap_pane_send,
	cmd_swap_pane_recv,
	cmd_swap_pane_free,
	cmd_swap_pane_print
};

void
cmd_swap_pane_init(struct cmd *self, int key)
{
	struct cmd_swap_pane_data	 *data;

	self->data = data = xmalloc(sizeof *data);
	data->target = NULL;
	data->src = -1;
	data->dst = -1;
	data->flag_detached = 0;
	data->flag_up = 0;
	data->flag_down = 0;

	switch (key) {
	case '{':
		data->flag_up = 1;
		break;
	case '}':
		data->flag_down = 1;
		break;
	}
}

int
cmd_swap_pane_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_swap_pane_data	*data;
	int				 opt, n;
	const char			*errstr;

	self->entry->init(self, 0);
	data = self->data;

	while ((opt = getopt(argc, argv, "dDt:p:q:U")) != -1) {
		switch (opt) {
		case 'd':
			data->flag_detached = 1;
			break;
		case 'D':
			data->flag_up = 0;
			data->flag_down = 1;
			data->dst = -1;
			break;
		case 't':
			if (data->target == NULL)
				data->target = xstrdup(optarg);
			break;
		case 'p':
			if (data->src == -1) {
				n = strtonum(optarg, 0, INT_MAX, &errstr);
				if (errstr != NULL) {
					xasprintf(cause, "src %s", errstr);
					goto error;
				}
				data->src = n;
			}
			break;
		case 'q':
			if (data->dst == -1) {
				n = strtonum(optarg, 0, INT_MAX, &errstr);
				if (errstr != NULL) {
					xasprintf(cause, "dst %s", errstr);
					goto error;
				}
				data->dst = n;
			}
			data->flag_up = 0;
			data->flag_down = 0;
			break;
		case 'U':
			data->flag_up = 1;
			data->flag_down = 0;
			data->dst = -1;
			break;

		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 0)
		goto usage;

	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

error:
	self->entry->free(self);
	return (-1);
}

int
cmd_swap_pane_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_swap_pane_data	*data = self->data;
	struct winlink			*wl;
	struct window			*w;
	struct window_pane		*tmp_wp, *src_wp, *dst_wp;
	u_int				 xx, yy;

	if (data == NULL)
		return (0);

	if ((wl = cmd_find_window(ctx, data->target, NULL)) == NULL)
		return (-1);
	w = wl->window;

	if (data->src == -1)
		src_wp = w->active;
	else {
		src_wp = window_pane_at_index(w, data->src);
		if (src_wp == NULL) {
			ctx->error(ctx, "no pane: %d", data->src);
			return (-1);
		}
	}
	if (data->dst == -1)
		dst_wp = w->active;
	else {
		dst_wp = window_pane_at_index(w, data->dst);
		if (dst_wp == NULL) {
			ctx->error(ctx, "no pane: %d", data->dst);
			return (-1);
		}
	}

	if (data->dst == -1 && data->flag_up) {
		if ((dst_wp = TAILQ_PREV(src_wp, window_panes, entry)) == NULL)
			dst_wp = TAILQ_LAST(&w->panes, window_panes);
	}
	if (data->dst == -1 && data->flag_down) {
		if ((dst_wp = TAILQ_NEXT(src_wp, entry)) == NULL)
			dst_wp = TAILQ_FIRST(&w->panes);
	}

	if (src_wp == dst_wp)
		return (0);

	tmp_wp = TAILQ_PREV(dst_wp, window_panes, entry);
	TAILQ_REMOVE(&w->panes, dst_wp, entry);
	TAILQ_REPLACE(&w->panes, src_wp, dst_wp, entry);
	if (tmp_wp == src_wp)
		tmp_wp = dst_wp;
	if (tmp_wp == NULL)
		TAILQ_INSERT_HEAD(&w->panes, src_wp, entry);
	else
		TAILQ_INSERT_AFTER(&w->panes, tmp_wp, src_wp, entry);

	xx = src_wp->xoff;
	yy = src_wp->yoff;
 	src_wp->xoff = dst_wp->xoff;
 	src_wp->yoff = dst_wp->yoff;
 	dst_wp->xoff = xx;
 	dst_wp->yoff = yy;

	xx = src_wp->sx;
	yy = src_wp->sy;
	window_pane_resize(src_wp, dst_wp->sx, dst_wp->sy);
	window_pane_resize(dst_wp, xx, yy);

	if (!data->flag_detached) {
		window_set_active_pane(w, dst_wp);
		layout_refresh(w, 0);
	}

	return (0);
}

void
cmd_swap_pane_send(struct cmd *self, struct buffer *b)
{
	struct cmd_swap_pane_data	*data = self->data;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->target);
}

void
cmd_swap_pane_recv(struct cmd *self, struct buffer *b)
{
	struct cmd_swap_pane_data	*data;

	self->data = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->target = cmd_recv_string(b);
}

void
cmd_swap_pane_free(struct cmd *self)
{
	struct cmd_swap_pane_data	*data = self->data;

	if (data->target != NULL)
		xfree(data->target);
	xfree(data);
}

size_t
cmd_swap_pane_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_swap_pane_data	*data = self->data;
	size_t				 off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return (off);
	if (off < len &&
	    (data->flag_down || data->flag_up || data->flag_detached)) {
		off += xsnprintf(buf + off, len - off, " -");
		if (off < len && data->flag_detached)
			off += xsnprintf(buf + off, len - off, "d");
		if (off < len && data->flag_up)
			off += xsnprintf(buf + off, len - off, "D");
		if (off < len && data->flag_down)
			off += xsnprintf(buf + off, len - off, "U");
	}
	if (off < len && data->target != NULL)
		off += cmd_prarg(buf + off, len - off, " -t ", data->target);
	if (off < len && data->src != -1)
		off += xsnprintf(buf + off, len - off, " -p %d", data->src);
	if (off < len && data->dst != -1)
		off += xsnprintf(buf + off, len - off, " -q %d", data->dst);
	return (off);
}
