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
	"[-t key-table]",
	0, 0,
	cmd_target_init,
	cmd_target_parse,
	cmd_list_keys_exec,
	cmd_target_free,
	cmd_target_print
};

int
cmd_list_keys_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data	*data = self->data;
	struct key_binding	*bd;
	const char		*key;
	char			 tmp[BUFSIZ], keytmp[64];
	int			 width, keywidth;

	if (data->target != NULL)
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

		*tmp = '\0';
		cmd_list_print(bd->cmdlist, tmp, sizeof tmp);
		if (!(bd->key & KEYC_PREFIX)) {
			xsnprintf(keytmp, sizeof keytmp, "[%s]", key);
			key = keytmp;
		}
		ctx->print(ctx, "%*s: %s", width, key, tmp);
	}

	return (0);
}

int
cmd_list_keys_table(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data		*data = self->data;
	const struct mode_key_table	*mtab;
	struct mode_key_binding		*mbind;
	const char			*key, *cmdstr, *mode;
	int			 	 width, keywidth;

	for (mtab = mode_key_tables; mtab->name != NULL; mtab++) {
		if (strcasecmp(data->target, mtab->name) == 0)
			break;
	}
	if (mtab->name == NULL) {
		ctx->error(ctx, "unknown key table: %s", data->target);
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
