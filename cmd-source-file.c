/* $OpenBSD$ */

/*
 * Copyright (c) 2008 Tiago Cunha <me@tiagocunha.org>
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

#include "tmux.h"

/*
 * Sources a configuration file.
 */

int	cmd_source_file_parse(struct cmd *, int, char **, char **);
int	cmd_source_file_exec(struct cmd *, struct cmd_ctx *);
void	cmd_source_file_free(struct cmd *);
void	cmd_source_file_init(struct cmd *, int);
size_t	cmd_source_file_print(struct cmd *, char *, size_t);

struct cmd_source_file_data {
	char *path;
};

const struct cmd_entry cmd_source_file_entry = {
	"source-file", "source",
	"path",
	0, "",
	cmd_source_file_init,
	cmd_source_file_parse,
	cmd_source_file_exec,
	cmd_source_file_free,
	cmd_source_file_print
};

/* ARGSUSED */
void
cmd_source_file_init(struct cmd *self, unused int arg)
{
	struct cmd_source_file_data	*data;

	self->data = data = xmalloc(sizeof *data);
	data->path = NULL;
}

int
cmd_source_file_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_source_file_data	*data;
	int				 opt;

	self->entry->init(self, KEYC_NONE);
	data = self->data;

	while ((opt = getopt(argc, argv, "")) != -1) {
		switch (opt) {
		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1)
		goto usage;

	data->path = xstrdup(argv[0]);
	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

	self->entry->free(self);
	return (-1);
}

int
cmd_source_file_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_source_file_data	*data = self->data;
	struct causelist		 causes;
	char				*cause;
	struct window_pane		*wp;
	int				 retval;
	u_int				 i;

	ARRAY_INIT(&causes);

	retval = load_cfg(data->path, ctx, &causes);
	if (ARRAY_EMPTY(&causes))
		return (retval);

	if (retval == 1 && !RB_EMPTY(&sessions) && ctx->cmdclient != NULL) {
		wp = RB_MIN(sessions, &sessions)->curw->window->active;
		window_pane_set_mode(wp, &window_copy_mode);
		window_copy_init_for_output(wp);
		for (i = 0; i < ARRAY_LENGTH(&causes); i++) {
			cause = ARRAY_ITEM(&causes, i);
			window_copy_add(wp, "%s", cause);
			xfree(cause);
		}
	} else {
		for (i = 0; i < ARRAY_LENGTH(&causes); i++) {
			cause = ARRAY_ITEM(&causes, i);
			ctx->print(ctx, "%s", cause);
			xfree(cause);
		}
	}
	ARRAY_FREE(&causes);

	return (retval);
}

void
cmd_source_file_free(struct cmd *self)
{
	struct cmd_source_file_data	*data = self->data;

	if (data->path != NULL)
		xfree(data->path);
	xfree(data);
}

size_t
cmd_source_file_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_source_file_data	*data = self->data;
	size_t				off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return (off);
	if (off < len && data->path != NULL)
		off += cmd_prarg(buf + off, len - off, " ", data->path);
	return (off);
}
