/* $Id: cmd-capture-pane.c,v 1.5 2010-12-31 01:59:47 tcunha Exp $ */

/*
 * Copyright (c) 2009 Jonathan Alvarado <radobobo@users.sourceforge.net>
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

#include "tmux.h"

/*
 * Write the entire contents of a pane to a buffer.
 */

int	cmd_capture_pane_parse(struct cmd *, int, char **, char **);
int	cmd_capture_pane_exec(struct cmd *, struct cmd_ctx *);
void	cmd_capture_pane_free(struct cmd *);
void	cmd_capture_pane_init(struct cmd *, int);
size_t	cmd_capture_pane_print(struct cmd *, char *, size_t);

struct cmd_capture_pane_data {
	char	*target;
	int	 buffer;
};

const struct cmd_entry cmd_capture_pane_entry = {
	"capture-pane", "capturep",
	"[-b buffer-index] [-t target-pane]",
	0, "",
	cmd_capture_pane_init,
	cmd_capture_pane_parse,
	cmd_capture_pane_exec,
	cmd_capture_pane_free,
	cmd_capture_pane_print
};

/* ARGSUSED */
void
cmd_capture_pane_init(struct cmd *self, unused int arg)
{
	struct cmd_capture_pane_data	*data;

	self->data = data = xmalloc(sizeof *data);
	data->buffer = -1;
	data->target = NULL;
}

int
cmd_capture_pane_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_capture_pane_data	*data;
	const char			*errstr;
	int				 n, opt;

	self->entry->init(self, KEYC_NONE);
	data = self->data;

	while ((opt = getopt(argc, argv, "b:t:")) != -1) {
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

	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

error:
	self->entry->free(self);
	return (-1);
}

int
cmd_capture_pane_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_capture_pane_data	*data = self->data;
	struct window_pane		*wp;
	char 				*buf, *line;
	struct screen			*s;
	u_int			 	 i, limit;
	size_t         		 	 len, linelen;

	if (cmd_find_pane(ctx, data->target, NULL, &wp) == NULL)
		return (-1);
	s = &wp->base;

	buf = NULL;
	len = 0;

	for (i = 0; i < screen_size_y(s); i++) {
	       line = grid_view_string_cells(s->grid, 0, i, screen_size_x(s));
	       linelen = strlen(line);

	       buf = xrealloc(buf, 1, len + linelen + 1);
	       memcpy(buf + len, line, linelen);
	       len += linelen;
	       buf[len++] = '\n';

	       xfree(line);
	}

	limit = options_get_number(&global_options, "buffer-limit");
	if (data->buffer == -1) {
		paste_add(&global_buffers, buf, len, limit);
		return (0);
	}
	if (paste_replace(&global_buffers, data->buffer, buf, len) != 0) {
		ctx->error(ctx, "no buffer %d", data->buffer);
		xfree(buf);
		return (-1);
	}
	return (0);
}

void
cmd_capture_pane_free(struct cmd *self)
{
	struct cmd_capture_pane_data	*data = self->data;

	if (data->target != NULL)
		xfree(data->target);
	xfree(data);
}

size_t
cmd_capture_pane_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_capture_pane_data	*data = self->data;
	size_t				 off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return (off);
	if (off < len && data->buffer != -1)
		off += xsnprintf(buf + off, len - off, " -b %d", data->buffer);
	if (off < len && data->target != NULL)
		off += xsnprintf(buf + off, len - off, " -t %s", data->target);
	return (off);
}
