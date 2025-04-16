/* $OpenBSD$ */

/*
 * Copyright (c) 2024 Nicholas Marriott <nicholas.marriott@gmail.com>
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
 * Trigger a key binding as if the key were pressed.
 */

static enum cmd_retval	cmd_trigger_bind_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_trigger_bind_entry = {
	.name = "trigger-bind",
	.alias = NULL,

	.args = { "nT:", 1, 1 },
	.usage = "[-n] [-T key-table] key",

	.flags = CMD_AFTERHOOK, /* same as bind-key */
	.exec = cmd_trigger_bind_exec
};

static enum cmd_retval
cmd_trigger_bind_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct key_table	*table;
	struct key_binding	*bd;
	key_code		 key;
	const char		*tablename, *keystr;
	struct cmdq_state	*new_state;
	struct cmdq_item	*new_item;

	keystr = args_string(args, 0);
	key = key_string_lookup_string(keystr);
	if (key == KEYC_NONE || key == KEYC_UNKNOWN) {
		cmdq_error(item, "unknown key: %s", keystr);
		return (CMD_RETURN_ERROR);
	}
	key &= KEYC_MASK_KEY; /* clear modifiers, not needed for lookup */

	if (args_has(args, 'T'))
		tablename = args_get(args, 'T');
	else if (args_has(args, 'n'))
		tablename = "root";
	else
		tablename = "prefix";

	table = key_bindings_get_table(tablename, 0);
	if (table == NULL) {
		cmdq_error(item, "table %s not found", tablename);
		return (CMD_RETURN_ERROR);
	}

	bd = key_bindings_get(table, key);
	if (bd == NULL) {
		cmdq_error(item, "key %s not bound in table %s", keystr,
		    tablename);
		return (CMD_RETURN_ERROR);
	}
	if (bd->cmdlist == NULL) {
		cmdq_error(item, "key %s is unbound in table %s", keystr,
		    tablename);
		return (CMD_RETURN_ERROR);
	}

	/* Create a new state based on the current item's target */
	new_state = cmdq_new_state(target, NULL, 0); /* no event, no flags */

	/* Create a command item from the binding's command list */
	new_item = cmdq_get_command(bd->cmdlist, new_state);

	/* Free the temporary state */
	cmdq_free_state(new_state);

	/* Insert the new command item to run after this one */
	cmdq_insert_after(item, new_item);

	return (CMD_RETURN_NORMAL);
}
