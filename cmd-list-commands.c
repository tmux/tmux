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
 * List all commands.
 */

#define LIST_COMMANDS_TEMPLATE					\
	"#{command_list_name}"					\
	"#{?command_list_alias, (#{command_list_alias}),} "	\
	"#{command_list_usage}"

static enum cmd_retval cmd_list_commands(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_list_commands_entry = {
	.name = "list-commands",
	.alias = "lscm",

	.args = { "F:", 0, 1, NULL },
	.usage = "[-F format] [command]",

	.flags = CMD_STARTSERVER|CMD_AFTERHOOK,
	.exec = cmd_list_commands
};

static void
cmd_list_single_command(const struct cmd_entry *entry, struct format_tree *ft,
    const char *template, struct cmdq_item *item)
{
	const char	*s;
	char		*line;

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

static enum cmd_retval
cmd_list_commands(struct cmd *self, struct cmdq_item *item)
{
	struct args		 *args = cmd_get_args(self);
	const struct cmd_entry	**entryp;
	const struct cmd_entry	 *entry;
	struct format_tree	 *ft;
	const char		 *template,  *command;
	char			 *cause;

	if ((template = args_get(args, 'F')) == NULL)
		template = LIST_COMMANDS_TEMPLATE;

	ft = format_create(cmdq_get_client(item), item, FORMAT_NONE, 0);
	format_defaults(ft, NULL, NULL, NULL, NULL);

	command = args_string(args, 0);
	if (command == NULL) {
		for (entryp = cmd_table; *entryp != NULL; entryp++)
			cmd_list_single_command(*entryp, ft, template, item);
	} else {
		entry = cmd_find(command, &cause);
		if (entry != NULL)
			cmd_list_single_command(entry, ft, template, item);
		else {
			cmdq_error(item, "%s", cause);
			free(cause);
			format_free(ft);
			return (CMD_RETURN_ERROR);
		}
	}

	format_free(ft);
	return (CMD_RETURN_NORMAL);
}
