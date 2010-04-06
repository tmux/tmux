/* $Id: cmd-join-pane.c,v 1.3 2010-04-06 21:59:59 nicm Exp $ */

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
 * Join a pane into another (like split/swap/kill).
 */

int	cmd_join_pane_parse(struct cmd *, int, char **, char **);
int	cmd_join_pane_exec(struct cmd *, struct cmd_ctx *);
void	cmd_join_pane_free(struct cmd *);
void	cmd_join_pane_init(struct cmd *, int);
size_t	cmd_join_pane_print(struct cmd *, char *, size_t);

struct cmd_join_pane_data {
	char	*src;
	char	*dst;
	int	 flag_detached;
	int	 flag_horizontal;
	int	 percentage;
	int	 size;
};

const struct cmd_entry cmd_join_pane_entry = {
	"join-pane", "joinp",
	"[-dhv] [-p percentage|-l size] [-s src-pane] [-t dst-pane] [command]",
	0, "",
	cmd_join_pane_init,
	cmd_join_pane_parse,
	cmd_join_pane_exec,
	cmd_join_pane_free,
	cmd_join_pane_print
};

void
cmd_join_pane_init(struct cmd *self, int key)
{
	struct cmd_join_pane_data	 *data;

	self->data = data = xmalloc(sizeof *data);
	data->src = NULL;
	data->dst = NULL;
	data->flag_detached = 0;
	data->flag_horizontal = 0;
	data->percentage = -1;
	data->size = -1;

	switch (key) {
	case '%':
		data->flag_horizontal = 1;
		break;
	case '"':
		data->flag_horizontal = 0;
		break;
	}
}

int
cmd_join_pane_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_join_pane_data	*data;
	int				 opt;
	const char			*errstr;

	self->entry->init(self, KEYC_NONE);
	data = self->data;

	while ((opt = getopt(argc, argv, "dhl:p:s:t:v")) != -1) {
		switch (opt) {
		case 'd':
			data->flag_detached = 1;
			break;
		case 'h':
			data->flag_horizontal = 1;
			break;
		case 's':
			if (data->src == NULL)
				data->src = xstrdup(optarg);
			break;
		case 't':
			if (data->dst == NULL)
				data->dst = xstrdup(optarg);
			break;
		case 'l':
			if (data->percentage != -1 || data->size != -1)
				break;
			data->size = strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr != NULL) {
				xasprintf(cause, "size %s", errstr);
				goto error;
			}
			break;
		case 'p':
			if (data->size != -1 || data->percentage != -1)
				break;
			data->percentage = strtonum(optarg, 1, 100, &errstr);
			if (errstr != NULL) {
				xasprintf(cause, "percentage %s", errstr);
				goto error;
			}
			break;
		case 'v':
			data->flag_horizontal = 0;
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
cmd_join_pane_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_join_pane_data	*data = self->data;
	struct session			*dst_s;
	struct winlink			*src_wl, *dst_wl;
	struct window			*src_w, *dst_w;
	struct window_pane		*src_wp, *dst_wp;
	int				 size;
	enum layout_type		 type;
	struct layout_cell		*lc;

	if ((dst_wl = cmd_find_pane(ctx, data->dst, &dst_s, &dst_wp)) == NULL)
		return (-1);
	dst_w = dst_wl->window;

	if ((src_wl = cmd_find_pane(ctx, data->src, NULL, &src_wp)) == NULL)
		return (-1);
	src_w = src_wl->window;

	if (src_w == dst_w) {
		ctx->error(ctx, "can't join a pane to its own window");
		return (-1);
	}

	type = LAYOUT_TOPBOTTOM;
	if (data->flag_horizontal)
		type = LAYOUT_LEFTRIGHT;

	size = -1;
	if (data->size != -1)
		size = data->size;
	else if (data->percentage != -1) {
		if (type == LAYOUT_TOPBOTTOM)
			size = (dst_wp->sy * data->percentage) / 100;
		else
			size = (dst_wp->sx * data->percentage) / 100;
	}

	if ((lc = layout_split_pane(dst_wp, type, size)) == NULL) {
		ctx->error(ctx, "create pane failed: pane too small");
		return (-1);
	}

	layout_close_pane(src_wp);

	if (src_w->active == src_wp) {
		src_w->active = TAILQ_PREV(src_wp, window_panes, entry);
		if (src_w->active == NULL)
			src_w->active = TAILQ_NEXT(src_wp, entry);
	}
	TAILQ_REMOVE(&src_w->panes, src_wp, entry);

	if (window_count_panes(src_w) == 0)
		server_kill_window(src_w);

	src_wp->window = dst_w;
	TAILQ_INSERT_AFTER(&dst_w->panes, dst_wp, src_wp, entry);
	layout_assign_pane(lc, src_wp);

	recalculate_sizes();

	server_redraw_window(src_w);
	server_redraw_window(dst_w);

	if (!data->flag_detached) {
		window_set_active_pane(dst_w, src_wp);
		session_select(dst_s, dst_wl->idx);
		server_redraw_session(dst_s);
	} else
		server_status_session(dst_s);

	return (0);
}

void
cmd_join_pane_free(struct cmd *self)
{
	struct cmd_join_pane_data	*data = self->data;

	if (data->src != NULL)
		xfree(data->src);
	if (data->dst != NULL)
		xfree(data->dst);
	xfree(data);
}

size_t
cmd_join_pane_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_join_pane_data	*data = self->data;
	size_t				 off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return (off);
	if (off < len && data->flag_detached)
		off += xsnprintf(buf + off, len - off, " -d");
	if (off < len && data->flag_horizontal)
		off += xsnprintf(buf + off, len - off, " -h");
	if (off < len && data->size > 0)
		off += xsnprintf(buf + off, len - off, " -l %d", data->size);
	if (off < len && data->percentage > 0) {
		off += xsnprintf(
		    buf + off, len - off, " -p %d", data->percentage);
	}
	if (off < len && data->src != NULL)
		off += cmd_prarg(buf + off, len - off, " -s ", data->src);
	if (off < len && data->dst != NULL)
		off += cmd_prarg(buf + off, len - off, " -t ", data->dst);
	return (off);
}
