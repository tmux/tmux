/* $OpenBSD$ */

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

#include <string.h>

#include "tmux.h"

/*
 * List key bindings.
 */

int	cmd_list_keys_exec(struct cmd *, struct cmd_ctx *);

int	cmd_list_keys_table(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_list_keys_entry = {
	"list-keys", "lsk",
	"t:", 0, 0,
	"[-t key-table]",
	0,
	NULL,
	NULL,
	cmd_list_keys_exec
};

int
cmd_list_keys_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args		*args = self->args;
	struct key_binding	*bd;
	const char		*key;
	char			 tmp[BUFSIZ];
	size_t			 used;
	int			 width, keywidth;

	if (args_has(args, 't'))
		return (cmd_list_keys_table(self, ctx));

	width = 0;
	SPLAY_FOREACH(bd, key_bindings, &key_bindings) {
		key = key_string_lookup_key(bd->key & ~KEYC_PREFIX);
		if (key == NULL)
			continue;

		keywidth = strlen(key) + 1;
		if (!(bd->key & KEYC_PREFIX))
			keywidth += 2;
		if (keywidth > width)
			width = keywidth;
	}

	SPLAY_FOREACH(bd, key_bindings, &key_bindings) {
		key = key_string_lookup_key(bd->key & ~KEYC_PREFIX);
		if (key == NULL)
			continue;
		used = xsnprintf(tmp, sizeof tmp, "%*s: ", width, key);
		if (used >= sizeof tmp)
			continue;

		if (!(bd->key & KEYC_PREFIX)) {
			used = strlcat(tmp, "(no prefix) ", sizeof tmp);
			if (used >= sizeof tmp)
				continue;
		}
		if (bd->can_repeat) {
			used = strlcat(tmp, "(repeat) ", sizeof tmp);
			if (used >= sizeof tmp)
				continue;
		}
		cmd_list_print(bd->cmdlist, tmp + used, (sizeof tmp) - used);
		ctx->print(ctx, "%s", tmp);
	}

	return (0);
}

int
cmd_list_keys_table(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args			*args = self->args;
	const char			*tablename;
	const struct mode_key_table	*mtab;
	struct mode_key_binding		*mbind;
	const char			*key, *cmdstr, *mode;
	int			 	 width, keywidth;

	tablename = args_get(args, 't');
	if ((mtab = mode_key_findtable(tablename)) == NULL) {
		ctx->error(ctx, "unknown key table: %s", tablename);
		return (-1);
	}

	width = 0;
	SPLAY_FOREACH(mbind, mode_key_tree, mtab->tree) {
		key = key_string_lookup_key(mbind->key);
		if (key == NULL)
			continue;

		keywidth = strlen(key) + 1;
		if (keywidth > width)
			width = keywidth;
	}

	SPLAY_FOREACH(mbind, mode_key_tree, mtab->tree) {
		key = key_string_lookup_key(mbind->key);
		if (key == NULL)
			continue;

		mode = "";
		if (mbind->mode != 0)
			mode = "(command mode) ";
		cmdstr = mode_key_tostring(mtab->cmdstr, mbind->cmd);
		if (cmdstr != NULL)
			ctx->print(ctx, "%*s: %s%s", width, key, mode, cmdstr);
	}

	return (0);
}
