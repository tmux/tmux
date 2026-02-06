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

#define LIST_KEYS_TEMPLATE					\
	"#{?notes_only,"					\
	"#{key_prefix} "					\
	"#{p|#{key_string_width}:key_string} "			\
	"#{?key_note,#{key_note},#{key_command}}"		\
	","							\
	"bind-key #{?key_has_repeat,#{?key_repeat,-r,  },} "	\
	"-T #{p|#{key_table_width}:key_table} "			\
	"#{p|#{key_string_width}:key_string} "			\
	"#{key_command}}"

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
cmd_list_keys_get_prefix(struct args *args)
{
	char		*s;
	key_code	 prefix;

	prefix = options_get_number(global_s_options, "prefix");
	if (!args_has(args, 'P')) {
		if (prefix != KEYC_NONE)
			xasprintf(&s, "%s", key_string_lookup_key(prefix, 0));
		else
			s = xstrdup("");
	} else
		s = xstrdup(args_get(args, 'P'));
	return (s);
}

static u_int
cmd_list_keys_get_width(struct key_binding **l, u_int n)
{
	struct key_binding	*bd;
	u_int			 i, width, keywidth = 0;

	for (i = 0; i < n; i++) {
		bd = l[i];
		width = utf8_cstrwidth(key_string_lookup_key(bd->key, 0));
		if (width > keywidth)
			keywidth = width;
	}
	return (keywidth);
}

static u_int
cmd_list_keys_get_table_width(struct key_binding **l, u_int n)
{
	struct key_binding	*bd;
	u_int			 i, width, tablewidth = 0;

	for (i = 0; i < n; i++) {
		bd = l[i];
		width = utf8_cstrwidth(bd->tablename);
		if (width > tablewidth)
			tablewidth = width;
	}
	return (tablewidth);
}

static struct key_binding **
cmd_get_root_and_prefix(u_int *n, struct sort_criteria *sort_crit)
{
	struct key_table		 *r, *p;
	struct key_binding		**lr, **lp;
	u_int				  ir, ip, i = 0;
	static struct key_binding	**l = NULL;
	static u_int			  lsz = 0;

	p = key_bindings_get_table("prefix", 0);
	lp = sort_get_key_bindings_table(p, &ip, sort_crit);
	i += ip;
	if (lsz <= i) {
		lsz = i + 100;
		l = xreallocarray(l, lsz, sizeof *l);
	}
	memcpy(l, lp, ip * sizeof *l);

	r = key_bindings_get_table("root", 0);
	lr = sort_get_key_bindings_table(r, &ir, sort_crit);
	i += ir;
	if (lsz <= i) {
		lsz = i + 100;
		l = xreallocarray(l, lsz, sizeof *l);
	}
	memcpy(l + ip, lr, ir * sizeof *l);

	*n = i;
	return (l);
}

static void
cmd_filter_key_list(int filter_notes, int filter_key, key_code only,
    struct key_binding **l, u_int *n)
{
	u_int			  i, j = 0;

	for (i = 0; i < *n; i++) {
		if (filter_key &&
		    only != (l[i]->key & (KEYC_MASK_KEY|KEYC_MASK_MODIFIERS)))
			continue;
		if (filter_notes && l[i]->note == NULL)
			continue;

		l[j++] = l[i];
	}
	*n = j;
}

static void
cmd_format_add_key_binding(struct format_tree *ft,
    const struct key_binding *bd, const char *prefix)
{
	char	*tmp;

	if (bd->flags & KEY_BINDING_REPEAT)
		tmp = xstrdup("1");
	else
		tmp = xstrdup("0");
	format_add(ft, "key_repeat", "%s", tmp);
	if (bd->note != NULL)
		tmp = xstrdup(bd->note);
	else
		tmp = xstrdup("");
	format_add(ft, "key_note", "%s", tmp);
	format_add(ft, "key_prefix", "%s", xstrdup(prefix));
	format_add(ft, "key_table", "%s", xstrdup(bd->tablename));
	format_add(ft, "key_string", "%s",
	    key_string_lookup_key(bd->key, 0));
	format_add(ft, "key_command", "%s",
	    cmd_list_print(bd->cmdlist,
	        CMD_LIST_PRINT_ESCAPED|CMD_LIST_PRINT_NO_GROUPS));
}

static enum cmd_retval
cmd_list_keys_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct client		*tc = cmdq_get_target_client(item);
	struct format_tree	*ft;
	struct key_table	*table = NULL;
	struct key_binding	*bd, **l;
	key_code		 only = KEYC_UNKNOWN;
	const char		*template, *tablename, *keystr;
	char			*line;
	char			*prefix = NULL;
	u_int			 i, n;
	int			 single, notes_only, filter_notes, filter_key;
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

	prefix = cmd_list_keys_get_prefix(args);
	single = args_has(args, '1');
	notes_only = args_has(args, 'N');
	if ((tablename = args_get(args, 'T')) != NULL) {
		table = key_bindings_get_table(tablename, 0);
		if (table == NULL) {
			cmdq_error(item, "table %s doesn't exist", tablename);
			return (CMD_RETURN_ERROR);
		}
	}
	if ((template = args_get(args, 'F')) == NULL)
		template = LIST_KEYS_TEMPLATE;

	if (table)
		l = sort_get_key_bindings_table(table, &n, &sort_crit);
	else if (notes_only)
		l = cmd_get_root_and_prefix(&n, &sort_crit);
	else
		l = sort_get_key_bindings(&n, &sort_crit);

	filter_notes = notes_only && !args_has(args, 'a');
	filter_key = only != KEYC_UNKNOWN;
	if (filter_notes || filter_key)
		cmd_filter_key_list(filter_notes, filter_key, only, l, &n);
	if (single)
		n = 1;

	ft = format_create(cmdq_get_client(item), item, FORMAT_NONE, 0);
	format_defaults(ft, NULL, NULL, NULL, NULL);
	format_add(ft, "notes_only", "%d", notes_only);
	format_add(ft, "key_has_repeat", "%d", key_bindings_has_repeat(l, n));
	format_add(ft, "key_string_width", "%u", cmd_list_keys_get_width(l, n));
	format_add(ft, "key_table_width", "%u",
	    cmd_list_keys_get_table_width(l, n));
	for (i = 0; i < n; i++) {
		bd = l[i];

		cmd_format_add_key_binding(ft, bd, prefix);
		line = format_expand(ft, template);

		if ((single && tc != NULL) || n == 1)
			status_message_set(tc, -1, 1, 0, 0, "%s", line);
		else {
			if (*line != '\0')
				cmdq_print(item, "%s", line);
		}
		free(line);

		if (single)
			break;
	}
	format_free(ft);
	free(prefix);

	return (CMD_RETURN_NORMAL);
}
