/* $Id: cmd-paste-buffer.c,v 1.27 2010-06-06 00:01:36 tcunha Exp $ */

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
#include <string.h>
#include <vis.h>

#include "tmux.h"

/*
 * Paste paste buffer if present.
 */

struct cmd_paste_buffer_data {
	char	*target;
	int	 buffer;

	int	 flag_delete;
	char	*sepstr;
};

void	cmd_paste_buffer_init(struct cmd *, int);
int	cmd_paste_buffer_parse(struct cmd *, int, char **, char **);
int	cmd_paste_buffer_exec(struct cmd *, struct cmd_ctx *);
void	cmd_paste_buffer_filter(
	    struct window_pane *, const char *, size_t, char *);
void	cmd_paste_buffer_free(struct cmd *);
size_t	cmd_paste_buffer_print(struct cmd *, char *, size_t);

const struct cmd_entry cmd_paste_buffer_entry = {
	"paste-buffer", "pasteb",
	"[-dr] [-s separator] [-b buffer-index] [-t target-window]",
	0, "",
	cmd_paste_buffer_init,
	cmd_paste_buffer_parse,
	cmd_paste_buffer_exec,
	cmd_paste_buffer_free,
	cmd_paste_buffer_print
};

/* ARGSUSED */
void
cmd_paste_buffer_init(struct cmd *self, unused int arg)
{
	struct cmd_paste_buffer_data	*data;

	self->data = data = xmalloc(sizeof *data);
	data->target = NULL;
	data->buffer = -1;
	data->flag_delete = 0;
	data->sepstr = xstrdup("\r");
}

int
cmd_paste_buffer_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_paste_buffer_data	*data;
	int			 opt, n;
	const char		*errstr;

	cmd_paste_buffer_init(self, 0);
	data = self->data;

	while ((opt = getopt(argc, argv, "b:ds:t:r")) != -1) {
		switch (opt) {
		case 'b':
			if (data->buffer == -1) {
				n = strtonum(optarg, 0, INT_MAX, &errstr);
				if (errstr != NULL) {
					xasprintf(cause, "buffer %s", errstr);
					goto error;
				}
				data->buffer = n;
			}
			break;
		case 'd':
			data->flag_delete = 1;
			break;
		case 's':
			if (data->sepstr != NULL)
				xfree(data->sepstr);
			data->sepstr = xstrdup(optarg);
			break;
		case 't':
			if (data->target == NULL)
				data->target = xstrdup(optarg);
			break;
		case 'r':
			if (data->sepstr != NULL)
				xfree(data->sepstr);
			data->sepstr = xstrdup("\n");
			break;
		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;

	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

error:
	self->entry->free(self);
	return (-1);
}

int
cmd_paste_buffer_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_paste_buffer_data	*data = self->data;
	struct window_pane		*wp;
	struct session			*s;
	struct paste_buffer		*pb;

	if (cmd_find_pane(ctx, data->target, &s, &wp) == NULL)
		return (-1);

	if (data->buffer == -1)
		pb = paste_get_top(&s->buffers);
	else {
		if ((pb = paste_get_index(&s->buffers, data->buffer)) == NULL) {
			ctx->error(ctx, "no buffer %d", data->buffer);
			return (-1);
		}
	}

	if (pb != NULL)
		cmd_paste_buffer_filter(wp, pb->data, pb->size, data->sepstr);

	/* Delete the buffer if -d. */
	if (data->flag_delete) {
		if (data->buffer == -1)
			paste_free_top(&s->buffers);
		else
			paste_free_index(&s->buffers, data->buffer);
	}

	return (0);
}

/* Add bytes to a buffer and filter '\n' according to separator. */
void
cmd_paste_buffer_filter(
    struct window_pane *wp, const char *data, size_t size, char *sep)
{
	const char	*end = data + size;
	const char	*lf;
	size_t		 seplen;

	seplen = strlen(sep);
	while ((lf = memchr(data, '\n', end - data)) != NULL) {
		if (lf != data)
			bufferevent_write(wp->event, data, lf - data);
		bufferevent_write(wp->event, sep, seplen);
		data = lf + 1;
	}

	if (end != data)
		bufferevent_write(wp->event, data, end - data);
}

void
cmd_paste_buffer_free(struct cmd *self)
{
	struct cmd_paste_buffer_data	*data = self->data;

	if (data->target != NULL)
		xfree(data->target);
	if (data->sepstr != NULL)
		xfree(data->sepstr);
	xfree(data);
}

size_t
cmd_paste_buffer_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_paste_buffer_data	*data = self->data;
	size_t				 off = 0;
	char                             tmp[BUFSIZ];
	int				 r_flag;

	r_flag = 0;
	if (data->sepstr != NULL)
		r_flag = (data->sepstr[0] == '\n' && data->sepstr[1] == '\0');

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return (off);
	if (off < len && data->flag_delete)
		off += xsnprintf(buf + off, len - off, " -d");
	if (off < len && r_flag)
		off += xsnprintf(buf + off, len - off, " -r");
	if (off < len && data->buffer != -1)
		off += xsnprintf(buf + off, len - off, " -b %d", data->buffer);
	if (off < len && data->sepstr != NULL && !r_flag) {
		strnvis(
		    tmp, data->sepstr, sizeof tmp, VIS_OCTAL|VIS_TAB|VIS_NL);
		off += cmd_prarg(buf + off, len - off, " -s ", tmp);
	}
	if (off < len && data->target != NULL)
		off += cmd_prarg(buf + off, len - off, " -t ", data->target);
	return (off);
}
