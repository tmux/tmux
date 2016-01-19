/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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
 * List key bindings.
 */

enum cmd_retval	 cmd_list_keys_exec(struct cmd *, struct cmd_q *);

enum cmd_retval	 cmd_list_keys_table(struct cmd *, struct cmd_q *);
enum cmd_retval	 cmd_list_keys_commands(struct cmd_q *);

const struct cmd_entry cmd_list_keys_entry = {
	.name = "list-keys",
	.alias = "lsk",

	.args = { "t:T:", 0, 0 },
	.usage = "[-t mode-table] [-T key-table]",

	.flags = CMD_STARTSERVER,
	.exec = cmd_list_keys_exec
};

const struct cmd_entry cmd_list_commands_entry = {
	.name = "list-commands",
	.alias = "lscm",

	.args = { "", 0, 0 },
	.usage = "",

	.flags = CMD_STARTSERVER,
	.exec = cmd_list_keys_exec
};

enum cmd_retval
cmd_list_keys_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct key_table	*table;
	struct key_binding	*bd;
	const char		*key, *tablename, *r;
	char			*cp, tmp[BUFSIZ];
	int			 repeat, width, tablewidth, keywidth;

	if (self->entry == &cmd_list_commands_entry)
		return (cmd_list_keys_commands(cmdq));

	if (args_has(args, 't'))
		return (cmd_list_keys_table(self, cmdq));

	tablename = args_get(args, 'T');
	if (tablename != NULL && key_bindings_get_table(tablename, 0) == NULL) {
		cmdq_error(cmdq, "table %s doesn't exist", tablename);
		return (CMD_RETURN_ERROR);
	}

	repeat = 0;
	tablewidth = keywidth = 0;
	RB_FOREACH(table, key_tables, &key_tables) {
		if (tablename != NULL && strcmp(table->name, tablename) != 0)
			continue;
		RB_FOREACH(bd, key_bindings, &table->key_bindings) {
			key = key_string_lookup_key(bd->key);

			if (bd->can_repeat)
				repeat = 1;

			width = utf8_cstrwidth(table->name);
			if (width > tablewidth)
				tablewidth = width;
			width = utf8_cstrwidth(key);
			if (width > keywidth)
				keywidth = width;
		}
	}

	RB_FOREACH(table, key_tables, &key_tables) {
		if (tablename != NULL && strcmp(table->name, tablename) != 0)
			continue;
		RB_FOREACH(bd, key_bindings, &table->key_bindings) {
			key = key_string_lookup_key(bd->key);

			if (!repeat)
				r = "";
			else if (bd->can_repeat)
				r = "-r ";
			else
				r = "   ";
			xsnprintf(tmp, sizeof tmp, "%s-T ", r);

			cp = utf8_padcstr(table->name, tablewidth);
			strlcat(tmp, cp, sizeof tmp);
			strlcat(tmp, " ", sizeof tmp);
			free(cp);

			cp = utf8_padcstr(key, keywidth);
			strlcat(tmp, cp, sizeof tmp);
			strlcat(tmp, " ", sizeof tmp);
			free(cp);

			cp = cmd_list_print(bd->cmdlist);
			strlcat(tmp, cp, sizeof tmp);
			free(cp);

			cmdq_print(cmdq, "bind-key %s", tmp);
		}
	}

	return (CMD_RETURN_NORMAL);
}

enum cmd_retval
cmd_list_keys_table(struct cmd *self, struct cmd_q *cmdq)
{
	struct args			*args = self->args;
	const char			*tablename;
	const struct mode_key_table	*mtab;
	struct mode_key_binding		*mbind;
	const char			*key, *cmdstr, *mode;
	int			 	 width, keywidth, any_mode;

	tablename = args_get(args, 't');
	if ((mtab = mode_key_findtable(tablename)) == NULL) {
		cmdq_error(cmdq, "unknown key table: %s", tablename);
		return (CMD_RETURN_ERROR);
	}

	width = 0;
	any_mode = 0;
	RB_FOREACH(mbind, mode_key_tree, mtab->tree) {
		key = key_string_lookup_key(mbind->key);

		if (mbind->mode != 0)
			any_mode = 1;

		keywidth = strlen(key);
		if (keywidth > width)
			width = keywidth;
	}

	RB_FOREACH(mbind, mode_key_tree, mtab->tree) {
		key = key_string_lookup_key(mbind->key);

		mode = "";
		if (mbind->mode != 0)
			mode = "c";
		cmdstr = mode_key_tostring(mtab->cmdstr, mbind->cmd);
		if (cmdstr != NULL) {
			cmdq_print(cmdq, "bind-key -%st %s%s %*s %s%s%s%s",
			    mode, any_mode && *mode == '\0' ? " " : "",
			    mtab->name, (int) width, key, cmdstr,
			    mbind->arg != NULL ? " \"" : "",
			    mbind->arg != NULL ? mbind->arg : "",
			    mbind->arg != NULL ? "\"": "");
		}
	}

	return (CMD_RETURN_NORMAL);
}

enum cmd_retval
cmd_list_keys_commands(struct cmd_q *cmdq)
{
	const struct cmd_entry	**entryp;
	const struct cmd_entry	 *entry;

	for (entryp = cmd_table; *entryp != NULL; entryp++) {
		entry = *entryp;
		if (entry->alias == NULL) {
			cmdq_print(cmdq, "%s %s", entry->name, entry->usage);
			continue;
		}
		cmdq_print(cmdq, "%s (%s) %s", entry->name, entry->alias,
		    entry->usage);
	}

	return (CMD_RETURN_NORMAL);
}
