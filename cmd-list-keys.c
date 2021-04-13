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

static enum cmd_retval	cmd_list_keys_exec(struct cmd *, struct cmdq_item *);

static enum cmd_retval	cmd_list_keys_commands(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_list_keys_entry = {
	.name = "list-keys",
	.alias = "lsk",

	.args = { "1aNP:T:", 0, 1 },
	.usage = "[-1aN] [-P prefix-string] [-T key-table] [key]",

	.flags = CMD_STARTSERVER|CMD_AFTERHOOK,
	.exec = cmd_list_keys_exec
};

const struct cmd_entry cmd_list_commands_entry = {
	.name = "list-commands",
	.alias = "lscm",

	.args = { "F:", 0, 1 },
	.usage = "[-F format] [command]",

	.flags = CMD_STARTSERVER|CMD_AFTERHOOK,
	.exec = cmd_list_keys_exec
};

static u_int
cmd_list_keys_get_width(const char *tablename, key_code only)
{
	struct key_table	*table;
	struct key_binding	*bd;
	u_int			 width, keywidth = 0;

	table = key_bindings_get_table(tablename, 0);
	if (table == NULL)
		return (0);
	bd = key_bindings_first(table);
	while (bd != NULL) {
		if ((only != KEYC_UNKNOWN && bd->key != only) ||
		    KEYC_IS_MOUSE(bd->key) ||
		    bd->note == NULL ||
		    *bd->note == '\0') {
			bd = key_bindings_next(table, bd);
			continue;
		}
		width = utf8_cstrwidth(key_string_lookup_key(bd->key, 0));
		if (width > keywidth)
			keywidth = width;

		bd = key_bindings_next(table, bd);
	}
	return (keywidth);
}

static int
cmd_list_keys_print_notes(struct cmdq_item *item, struct args *args,
    const char *tablename, u_int keywidth, key_code only, const char *prefix)
{
	struct client		*tc = cmdq_get_target_client(item);
	struct key_table	*table;
	struct key_binding	*bd;
	const char		*key;
	char			*tmp, *note;
	int	                 found = 0;

	table = key_bindings_get_table(tablename, 0);
	if (table == NULL)
		return (0);
	bd = key_bindings_first(table);
	while (bd != NULL) {
		if ((only != KEYC_UNKNOWN && bd->key != only) ||
		    KEYC_IS_MOUSE(bd->key) ||
		    ((bd->note == NULL || *bd->note == '\0') &&
		    !args_has(args, 'a'))) {
			bd = key_bindings_next(table, bd);
			continue;
		}
		found = 1;
		key = key_string_lookup_key(bd->key, 0);

		if (bd->note == NULL || *bd->note == '\0')
			note = cmd_list_print(bd->cmdlist, 1);
		else
			note = xstrdup(bd->note);
		tmp = utf8_padcstr(key, keywidth + 1);
		if (args_has(args, '1') && tc != NULL) {
			status_message_set(tc, -1, 1, 0, "%s%s%s", prefix, tmp,
			    note);
		} else
			cmdq_print(item, "%s%s%s", prefix, tmp, note);
		free(tmp);
		free(note);

		if (args_has(args, '1'))
			break;
		bd = key_bindings_next(table, bd);
	}
	return (found);
}

static char *
cmd_list_keys_get_prefix(struct args *args, key_code *prefix)
{
	char	*s;

	*prefix = options_get_number(global_s_options, "prefix");
	if (!args_has(args, 'P')) {
		if (*prefix != KEYC_NONE)
			xasprintf(&s, "%s ", key_string_lookup_key(*prefix, 0));
		else
			s = xstrdup("");
	} else
		s = xstrdup(args_get(args, 'P'));
	return (s);
}

static enum cmd_retval
cmd_list_keys_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct key_table	*table;
	struct key_binding	*bd;
	const char		*tablename, *r;
	char			*key, *cp, *tmp, *start, *empty;
	key_code		 prefix, only = KEYC_UNKNOWN;
	int			 repeat, width, tablewidth, keywidth, found = 0;
	size_t			 tmpsize, tmpused, cplen;

	if (cmd_get_entry(self) == &cmd_list_commands_entry)
		return (cmd_list_keys_commands(self, item));

	if (args->argc != 0) {
		only = key_string_lookup_string(args->argv[0]);
		if (only == KEYC_UNKNOWN) {
			cmdq_error(item, "invalid key: %s", args->argv[0]);
			return (CMD_RETURN_ERROR);
		}
		only &= (KEYC_MASK_KEY|KEYC_MASK_MODIFIERS);
	}

	tablename = args_get(args, 'T');
	if (tablename != NULL && key_bindings_get_table(tablename, 0) == NULL) {
		cmdq_error(item, "table %s doesn't exist", tablename);
		return (CMD_RETURN_ERROR);
	}

	if (args_has(args, 'N')) {
		if (tablename == NULL) {
			start = cmd_list_keys_get_prefix(args, &prefix);
			keywidth = cmd_list_keys_get_width("root", only);
			if (prefix != KEYC_NONE) {
				width = cmd_list_keys_get_width("prefix", only);
				if (width == 0)
					prefix = KEYC_NONE;
				else if (width > keywidth)
					keywidth = width;
			}
			empty = utf8_padcstr("", utf8_cstrwidth(start));

			found = cmd_list_keys_print_notes(item, args, "root",
			    keywidth, only, empty);
			if (prefix != KEYC_NONE) {
				if (cmd_list_keys_print_notes(item, args,
				    "prefix", keywidth, only, start))
					found = 1;
			}
			free(empty);
		} else {
			if (args_has(args, 'P'))
				start = xstrdup(args_get(args, 'P'));
			else
				start = xstrdup("");
			keywidth = cmd_list_keys_get_width(tablename, only);
			found = cmd_list_keys_print_notes(item, args, tablename,
			    keywidth, only, start);

		}
		free(start);
		goto out;
	}

	repeat = 0;
	tablewidth = keywidth = 0;
	table = key_bindings_first_table ();
	while (table != NULL) {
		if (tablename != NULL && strcmp(table->name, tablename) != 0) {
			table = key_bindings_next_table(table);
			continue;
		}
		bd = key_bindings_first(table);
		while (bd != NULL) {
			if (only != KEYC_UNKNOWN && bd->key != only) {
				bd = key_bindings_next(table, bd);
				continue;
			}
			key = args_escape(key_string_lookup_key(bd->key, 0));

			if (bd->flags & KEY_BINDING_REPEAT)
				repeat = 1;

			width = utf8_cstrwidth(table->name);
			if (width > tablewidth)
				tablewidth = width;
			width = utf8_cstrwidth(key);
			if (width > keywidth)
				keywidth = width;

			free(key);
			bd = key_bindings_next(table, bd);
		}
		table = key_bindings_next_table(table);
	}

	tmpsize = 256;
	tmp = xmalloc(tmpsize);

	table = key_bindings_first_table ();
	while (table != NULL) {
		if (tablename != NULL && strcmp(table->name, tablename) != 0) {
			table = key_bindings_next_table(table);
			continue;
		}
		bd = key_bindings_first(table);
		while (bd != NULL) {
			if (only != KEYC_UNKNOWN && bd->key != only) {
				bd = key_bindings_next(table, bd);
				continue;
			}
			found = 1;
			key = args_escape(key_string_lookup_key(bd->key, 0));

			if (!repeat)
				r = "";
			else if (bd->flags & KEY_BINDING_REPEAT)
				r = "-r ";
			else
				r = "   ";
			tmpused = xsnprintf(tmp, tmpsize, "%s-T ", r);

			cp = utf8_padcstr(table->name, tablewidth);
			cplen = strlen(cp) + 1;
			while (tmpused + cplen + 1 >= tmpsize) {
				tmpsize *= 2;
				tmp = xrealloc(tmp, tmpsize);
			}
			strlcat(tmp, cp, tmpsize);
			tmpused = strlcat(tmp, " ", tmpsize);
			free(cp);

			cp = utf8_padcstr(key, keywidth);
			cplen = strlen(cp) + 1;
			while (tmpused + cplen + 1 >= tmpsize) {
				tmpsize *= 2;
				tmp = xrealloc(tmp, tmpsize);
			}
			strlcat(tmp, cp, tmpsize);
			tmpused = strlcat(tmp, " ", tmpsize);
			free(cp);

			cp = cmd_list_print(bd->cmdlist, 1);
			cplen = strlen(cp);
			while (tmpused + cplen + 1 >= tmpsize) {
				tmpsize *= 2;
				tmp = xrealloc(tmp, tmpsize);
			}
			strlcat(tmp, cp, tmpsize);
			free(cp);

			cmdq_print(item, "bind-key %s", tmp);

			free(key);
			bd = key_bindings_next(table, bd);
		}
		table = key_bindings_next_table(table);
	}

	free(tmp);

out:
	if (only != KEYC_UNKNOWN && !found) {
		cmdq_error(item, "unknown key: %s", args->argv[0]);
		return (CMD_RETURN_ERROR);
	}
	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_list_keys_commands(struct cmd *self, struct cmdq_item *item)
{
	struct args		 *args = cmd_get_args(self);
	const struct cmd_entry	**entryp;
	const struct cmd_entry	 *entry;
	struct format_tree	 *ft;
	const char		 *template, *s, *command = NULL;
	char			 *line;

	if (args->argc != 0)
		command = args->argv[0];

	if ((template = args_get(args, 'F')) == NULL) {
		template = "#{command_list_name}"
		    "#{?command_list_alias, (#{command_list_alias}),} "
		    "#{command_list_usage}";
	}

	ft = format_create(cmdq_get_client(item), item, FORMAT_NONE, 0);
	format_defaults(ft, NULL, NULL, NULL, NULL);

	for (entryp = cmd_table; *entryp != NULL; entryp++) {
		entry = *entryp;
		if (command != NULL &&
		    (strcmp(entry->name, command) != 0 &&
		    (entry->alias == NULL ||
		    strcmp(entry->alias, command) != 0)))
		    continue;

		format_add(ft, "command_list_name", "%s", entry->name);
		if (entry->alias != NULL)
			s = entry->alias;
		else
			s = "";
		format_add(ft, "command_list_alias", "%s", s);
		if (entry->usage != NULL)
			s = entry->usage;
		else
			s = "";
		format_add(ft, "command_list_usage", "%s", s);

		line = format_expand(ft, template);
		if (*line != '\0')
			cmdq_print(item, "%s", line);
		free(line);
	}

	format_free(ft);
	return (CMD_RETURN_NORMAL);
}
