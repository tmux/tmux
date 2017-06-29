/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
 * Copyright (c) 2017 Frank Hebold <frank.hebld@chelnok.de>
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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

RB_GENERATE(macro_table, macro, entry, macro_cmp);
struct macro_table macro_table = RB_INITIALIZER(&macro_table);

int
macro_cmp(struct macro *bd1, struct macro *bd2)
{
	return (strcmp(bd1->name, bd2->name));
}

void
macro_unref_table(struct macro_table *table)
{
	struct macro	*bd;
	struct macro	*bd1;

	RB_FOREACH_SAFE(bd, macro_table, table, bd1) {
		RB_REMOVE(macro_table, table, bd);
		cmd_list_free(bd->cmdlist);
		free(bd);
	}

	free(table);
}

void
macro_add(const char *name, struct cmd_list *cmdlist)
{
	struct macro_table	*table;
	struct macro	 bd_find, *bd;

	table = &macro_table;

	bd_find.name = name;
	bd = RB_FIND(macro_table, table, &bd_find);
	if (bd != NULL) {
		RB_REMOVE(macro_table, table, bd);
		cmd_list_free(bd->cmdlist);
		free(bd);
	}

	bd = xcalloc(1, sizeof *bd);
    bd->name = name;
	RB_INSERT(macro_table, table, bd);

	bd->cmdlist = cmdlist;
}

void
macro_remove(const char *name)
{
	struct macro_table	*table = &macro_table;
	struct macro	 bd_find, *bd;

	bd_find.name = name;
	bd = RB_FIND(macro_table, table, &bd_find);
	if (bd == NULL)
		return;

	RB_REMOVE(macro_table, table, bd);
	cmd_list_free(bd->cmdlist);
	free(bd);

	if (RB_EMPTY(table)) {
		macro_unref_table(table);
	}
}

void
macro_init(void) { }

static enum cmd_retval
macro_read_only(struct cmdq_item *item, __unused void *data)
{
	cmdq_error(item, "client is read-only");
	return (CMD_RETURN_ERROR);
}

void
macro_dispatch(struct macro *bd, struct cmdq_item *item,
    struct client *c, struct mouse_event *m, struct cmd_find_state *fs)
{
	struct cmd		*cmd;
	struct cmdq_item	*new_item;
	int			 readonly;

	readonly = 1;
	TAILQ_FOREACH(cmd, &bd->cmdlist->list, qentry) {
		if (!(cmd->entry->flags & CMD_READONLY))
			readonly = 0;
	}
	if (!readonly && (c->flags & CLIENT_READONLY))
		new_item = cmdq_get_callback(macro_read_only, NULL);
	else {
		new_item = cmdq_get_command(bd->cmdlist, fs, m, 0);
		if (bd->flags & KEY_BINDING_REPEAT)
			new_item->shared->flags |= CMDQ_SHARED_REPEAT;
	}
	if (item != NULL)
		cmdq_insert_after(item, new_item);
	else
		cmdq_append(c, new_item);
}
