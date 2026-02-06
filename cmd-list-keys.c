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

#define LIST_KEYS_TEMPLATE			\
	"bind-key"				\
	"#{p3:?key_binding_repeat, -r,} "	\
	"-T #{p%u:key_binding_tablename}"	\
	"#{p%u:key_binding_key}"		\
	"#{key_binding_command}"		\

#define LIST_KEYS_N_FLAG_TEMPLATE	\
	"#{key_binding_prefix}"		\
	"#{p%u:key_binding_key} "	\
	"#{key_binding_note}"

static enum cmd_retval cmd_list_keys_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_list_keys_entry = {
	.name = "list-keys",
	.alias = "lsk",

	.args = { "1aF:NO:P:rT:", 0, 1, NULL },
	.usage = "[-1aNr] [-F format] [-O order] [-P prefix-string]"
		 "[-T key-table] [key]",

	.flags = CMD_STARTSERVER|CMD_AFTERHOOK,
	.exec = cmd_list_keys_exec
};

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

static u_int
cmd_list_keys_get_width(struct key_binding **l, u_int n, key_code only)
{
	struct key_binding	*bd;
	u_int			 i, width, keywidth = 0;

	for (i = 0; i < n; i++) {
		bd = l[i];
		if ((only != KEYC_UNKNOWN && bd->key != only))
			continue;

		width = utf8_cstrwidth(key_string_lookup_key(bd->key, 0));
		if (width > keywidth)
			keywidth = width;
	}

	return (keywidth + 1);
}

static u_int
cmd_list_keys_get_table_width(struct key_binding **l, u_int n, key_code only)
{
	struct key_binding	*bd;
	u_int			 i, width, tablewidth = 0;

	for (i = 0; i < n; i++) {
		bd = l[i];
		if ((only != KEYC_UNKNOWN && bd->key != only))
			continue;

		width = utf8_cstrwidth(bd->tablename);
		if (width > tablewidth)
			tablewidth = width;
	}

	return (tablewidth + 1);
}

static char *
cmd_list_single_key_binding(const struct key_binding *bd, const char *prefix, 
    struct format_tree *ft, const char *template, struct cmdq_item *item)
{
	char	*tmp;

	if (bd->flags & KEY_BINDING_REPEAT)
		tmp = xstrdup("1");
	else
		tmp = xstrdup("0");
	format_add(ft, "key_binding_repeat", "%s", tmp);

	tmp = xstrdup(prefix);
	format_add(ft, "key_binding_prefix", "%s", tmp);

	tmp = xstrdup(bd->tablename);
	format_add(ft, "key_binding_tablename", "%s", tmp);

	tmp = args_escape(key_string_lookup_key(bd->key, 0));
	format_add(ft, "key_binding_key", "%s", tmp);

	tmp = cmd_list_print(bd->cmdlist,
	    CMD_LIST_PRINT_ESCAPED|CMD_LIST_PRINT_NO_GROUPS);
	format_add(ft, "key_binding_command", "%s", tmp);

	if (bd->note != NULL)
		tmp = xstrdup(bd->note);
	else
		tmp = xstrdup("");
	format_add(ft, "key_binding_note", "%s", tmp);


	return format_expand(ft, template);
}

static enum cmd_retval
cmd_list_keys_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct client		*tc = cmdq_get_target_client(item);
	struct format_tree	*ft;
	struct key_table	*table;
	struct key_binding	*bd, **l;
	const char		*tablename, *keystr, *line;
	char			*template;
	int			 free_template = 0;
	char			*prefixstr = NULL;
	key_code		 prefix, only = KEYC_UNKNOWN;
	u_int			 i, n, keywidth, tablewidth = 0;
	struct sort_criteria	 sort_crit;

	if ((keystr = args_string(args, 0)) != NULL) {
		only = key_string_lookup_string(keystr);
		if (only == KEYC_UNKNOWN) {
			cmdq_error(item, "invalid key: %s", keystr);
			return (CMD_RETURN_ERROR);
		}
		only &= (KEYC_MASK_KEY|KEYC_MASK_MODIFIERS);
	}

	sort_crit.order = sort_order_from_string(args_get(args, 'O'));
	sort_crit.reversed = args_has(args, 'r');

	if ((tablename = args_get(args, 'T')) != NULL) {
		table = key_bindings_get_table(tablename, 0);
		if (table == NULL) {
			cmdq_error(item, "table %s doesn't exist", tablename);
			return (CMD_RETURN_ERROR);
		}
		l = sort_get_key_bindings_table(table, &n, &sort_crit);
	} else
		l = sort_get_key_bindings(&n, &sort_crit);

	if ((template = args_get(args, 'F')) == NULL) {
		free_template = 1;
		keywidth = cmd_list_keys_get_width(l, n, only);
		tablewidth = cmd_list_keys_get_table_width(l, n, only);
		if (args_has(args, 'N'))
			xasprintf(&template, LIST_KEYS_N_FLAG_TEMPLATE,
			    keywidth);
		else
			xasprintf(&template, LIST_KEYS_TEMPLATE, tablewidth,
			    keywidth);
	}

	prefixstr = cmd_list_keys_get_prefix(args, &prefix);

	ft = format_create(cmdq_get_client(item), item, FORMAT_NONE, 0);
	format_defaults(ft, NULL, NULL, NULL, NULL);

	for (i = 0; i < n; i++) {
		bd = l[i];
		if (only != KEYC_UNKNOWN && bd->key != only)
			continue;

		if (args_has(args, 'N') && !args_has(args, 'a')
		    && bd->note == NULL && only == KEYC_UNKNOWN)
			continue;

		line = cmd_list_single_key_binding(bd, prefixstr, ft, template, item);

		if (args_has(args, '1') && tc != NULL)
			status_message_set(tc, -1, 1, 0, 0, "%s", line);
		else {
			if (*line != '\0')
				cmdq_print(item, "%s", line);
		}
		free(line);

		if (args_has(args, '1'))
			break;
	}
	format_free(ft);
	free(prefixstr);
	if (free_template)
		free(template);

	return (CMD_RETURN_NORMAL);
}
