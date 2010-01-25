/* $Id: cmd-unbind-key.c,v 1.22 2010-01-25 17:12:44 tcunha Exp $ */

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

#include "tmux.h"

/*
 * Unbind key from command.
 */

int	cmd_unbind_key_parse(struct cmd *, int, char **, char **);
int	cmd_unbind_key_exec(struct cmd *, struct cmd_ctx *);
void	cmd_unbind_key_free(struct cmd *);

int	cmd_unbind_key_table(struct cmd *, struct cmd_ctx *);

struct cmd_unbind_key_data {
	int	key;

	int	command_key;
	char   *tablename;
};

const struct cmd_entry cmd_unbind_key_entry = {
	"unbind-key", "unbind",
	"[-cn] [-t key-table] key",
	0, "",
	NULL,
	cmd_unbind_key_parse,
	cmd_unbind_key_exec,
	cmd_unbind_key_free,
	NULL
};

int
cmd_unbind_key_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_unbind_key_data	*data;
	int				 opt, no_prefix = 0;

	self->data = data = xmalloc(sizeof *data);
	data->command_key = 0;
	data->tablename = NULL;

	while ((opt = getopt(argc, argv, "cnt:")) != -1) {
		switch (opt) {
		case 'c':
			data->command_key = 1;
			break;
		case 'n':
			no_prefix = 1;
			break;
		case 't':
			if (data->tablename == NULL)
				data->tablename = xstrdup(optarg);
			break;
		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1)
		goto usage;

	if ((data->key = key_string_lookup_string(argv[0])) == KEYC_NONE) {
		xasprintf(cause, "unknown key: %s", argv[0]);
		goto error;
	}
	if (!no_prefix)
		data->key |= KEYC_PREFIX;

	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

error:
	xfree(data);
	return (-1);
}

int
cmd_unbind_key_exec(struct cmd *self, unused struct cmd_ctx *ctx)
{
	struct cmd_unbind_key_data	*data = self->data;

	if (data == NULL)
		return (0);
	if (data->tablename != NULL)
		return (cmd_unbind_key_table(self, ctx));

	key_bindings_remove(data->key);

	return (0);
}

int
cmd_unbind_key_table(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_unbind_key_data	*data = self->data;
	const struct mode_key_table	*mtab;
	struct mode_key_binding		*mbind, mtmp;

	if ((mtab = mode_key_findtable(data->tablename)) == NULL) {
		ctx->error(ctx, "unknown key table: %s", data->tablename);
		return (-1);
	}

	mtmp.key = data->key & ~KEYC_PREFIX;
	mtmp.mode = data->command_key ? 1 : 0;
	if ((mbind = SPLAY_FIND(mode_key_tree, mtab->tree, &mtmp)) != NULL) {
		SPLAY_REMOVE(mode_key_tree, mtab->tree, mbind);
		xfree(mbind);
	}
	return (0);
}

void
cmd_unbind_key_free(struct cmd *self)
{
	struct cmd_unbind_key_data	*data = self->data;

	if (data->tablename != NULL)
		xfree(data->tablename);
	xfree(data);
}
