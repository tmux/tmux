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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

RB_GENERATE(key_bindings, key_binding, entry, key_bindings_cmp);
RB_GENERATE(key_tables, key_table, entry, key_table_cmp);
struct key_tables key_tables = RB_INITIALIZER(&key_tables);

int
key_table_cmp(struct key_table *e1, struct key_table *e2)
{
	return (strcmp(e1->name, e2->name));
}

int
key_bindings_cmp(struct key_binding *bd1, struct key_binding *bd2)
{
	if (bd1->key < bd2->key)
		return (-1);
	if (bd1->key > bd2->key)
		return (1);
	return (0);
}

struct key_table *
key_bindings_get_table(const char *name, int create)
{
	struct key_table	table_find, *table;

	table_find.name = name;
	table = RB_FIND(key_tables, &key_tables, &table_find);
	if (table != NULL || !create)
		return (table);

	table = xmalloc(sizeof *table);
	table->name = xstrdup(name);
	RB_INIT(&table->key_bindings);

	table->references = 1; /* one reference in key_tables */
	RB_INSERT(key_tables, &key_tables, table);

	return (table);
}

void
key_bindings_unref_table(struct key_table *table)
{
	struct key_binding	*bd;
	struct key_binding	*bd1;

	if (--table->references != 0)
		return;

	RB_FOREACH_SAFE(bd, key_bindings, &table->key_bindings, bd1) {
		RB_REMOVE(key_bindings, &table->key_bindings, bd);
		cmd_list_free(bd->cmdlist);
		free(bd);
	}

	free((void *)table->name);
	free(table);
}

void
key_bindings_add(const char *name, key_code key, int can_repeat,
    struct cmd_list *cmdlist)
{
	struct key_table	*table;
	struct key_binding	 bd_find, *bd;

	table = key_bindings_get_table(name, 1);

	bd_find.key = key;
	bd = RB_FIND(key_bindings, &table->key_bindings, &bd_find);
	if (bd != NULL) {
		RB_REMOVE(key_bindings, &table->key_bindings, bd);
		cmd_list_free(bd->cmdlist);
		free(bd);
	}

	bd = xmalloc(sizeof *bd);
	bd->key = key;
	RB_INSERT(key_bindings, &table->key_bindings, bd);

	bd->can_repeat = can_repeat;
	bd->cmdlist = cmdlist;
}

void
key_bindings_remove(const char *name, key_code key)
{
	struct key_table	*table;
	struct key_binding	 bd_find, *bd;

	table = key_bindings_get_table(name, 0);
	if (table == NULL)
		return;

	bd_find.key = key;
	bd = RB_FIND(key_bindings, &table->key_bindings, &bd_find);
	if (bd == NULL)
		return;

	RB_REMOVE(key_bindings, &table->key_bindings, bd);
	cmd_list_free(bd->cmdlist);
	free(bd);

	if (RB_EMPTY(&table->key_bindings)) {
		RB_REMOVE(key_tables, &key_tables, table);
		key_bindings_unref_table(table);
	}
}

void
key_bindings_remove_table(const char *name)
{
	struct key_table	*table;

	table = key_bindings_get_table(name, 0);
	if (table != NULL) {
		RB_REMOVE(key_tables, &key_tables, table);
		key_bindings_unref_table(table);
	}
}

void
key_bindings_init(void)
{
	static const char *defaults[] = {
		"bind C-b send-prefix",
		"bind C-o rotate-window",
		"bind C-z suspend-client",
		"bind Space next-layout",
		"bind ! break-pane",
		"bind '\"' split-window",
		"bind '#' list-buffers",
		"bind '$' command-prompt -I'#S' \"rename-session '%%'\"",
		"bind % split-window -h",
		"bind & confirm-before -p\"kill-window #W? (y/n)\" kill-window",
		"bind \"'\" command-prompt -pindex \"select-window -t ':%%'\"",
		"bind ( switch-client -p",
		"bind ) switch-client -n",
		"bind , command-prompt -I'#W' \"rename-window '%%'\"",
		"bind - delete-buffer",
		"bind . command-prompt \"move-window -t '%%'\"",
		"bind 0 select-window -t:=0",
		"bind 1 select-window -t:=1",
		"bind 2 select-window -t:=2",
		"bind 3 select-window -t:=3",
		"bind 4 select-window -t:=4",
		"bind 5 select-window -t:=5",
		"bind 6 select-window -t:=6",
		"bind 7 select-window -t:=7",
		"bind 8 select-window -t:=8",
		"bind 9 select-window -t:=9",
		"bind : command-prompt",
		"bind \\; last-pane",
		"bind = choose-buffer",
		"bind ? list-keys",
		"bind D choose-client",
		"bind L switch-client -l",
		"bind M select-pane -M",
		"bind [ copy-mode",
		"bind ] paste-buffer",
		"bind c new-window",
		"bind d detach-client",
		"bind f command-prompt \"find-window '%%'\"",
		"bind i display-message",
		"bind l last-window",
		"bind m select-pane -m",
		"bind n next-window",
		"bind o select-pane -t:.+",
		"bind p previous-window",
		"bind q display-panes",
		"bind r refresh-client",
		"bind s choose-tree",
		"bind t clock-mode",
		"bind w choose-window",
		"bind x confirm-before -p\"kill-pane #P? (y/n)\" kill-pane",
		"bind z resize-pane -Z",
		"bind { swap-pane -U",
		"bind } swap-pane -D",
		"bind '~' show-messages",
		"bind PPage copy-mode -u",
		"bind -r Up select-pane -U",
		"bind -r Down select-pane -D",
		"bind -r Left select-pane -L",
		"bind -r Right select-pane -R",
		"bind M-1 select-layout even-horizontal",
		"bind M-2 select-layout even-vertical",
		"bind M-3 select-layout main-horizontal",
		"bind M-4 select-layout main-vertical",
		"bind M-5 select-layout tiled",
		"bind M-n next-window -a",
		"bind M-o rotate-window -D",
		"bind M-p previous-window -a",
		"bind -r M-Up resize-pane -U 5",
		"bind -r M-Down resize-pane -D 5",
		"bind -r M-Left resize-pane -L 5",
		"bind -r M-Right resize-pane -R 5",
		"bind -r C-Up resize-pane -U",
		"bind -r C-Down resize-pane -D",
		"bind -r C-Left resize-pane -L",
		"bind -r C-Right resize-pane -R",
		"bind -n MouseDown1Pane select-pane -t=\\; send-keys -M",
		"bind -n MouseDrag1Border resize-pane -M",
		"bind -n MouseDown1Status select-window -t=",
		"bind -n WheelDownStatus next-window",
		"bind -n WheelUpStatus previous-window",
		"bind -n MouseDrag1Pane if -Ft= '#{mouse_any_flag}' 'if -Ft= \"#{pane_in_mode}\" \"copy-mode -M\" \"send-keys -M\"' 'copy-mode -M'",
		"bind -n MouseDown3Pane if-shell -Ft= '#{mouse_any_flag}' 'select-pane -t=; send-keys -M' 'select-pane -mt='",
		"bind -n WheelUpPane if-shell -Ft= '#{mouse_any_flag}' 'send-keys -M' 'if -Ft= \"#{pane_in_mode}\" \"send-keys -M\" \"copy-mode -et=\"'",
	};
	u_int		 i;
	struct cmd_list	*cmdlist;
	char		*cause;
	int		 error;
	struct cmd_q	*cmdq;

	cmdq = cmdq_new(NULL);
	for (i = 0; i < nitems(defaults); i++) {
		error = cmd_string_parse(defaults[i], &cmdlist,
		    "<default-keys>", i, &cause);
		if (error != 0)
			fatalx("bad default key");
		cmdq_run(cmdq, cmdlist, NULL);
		cmd_list_free(cmdlist);
	}
	cmdq_free(cmdq);
}

void
key_bindings_dispatch(struct key_binding *bd, struct client *c,
    struct mouse_event *m)
{
	struct cmd	*cmd;
	int		 readonly;

	readonly = 1;
	TAILQ_FOREACH(cmd, &bd->cmdlist->list, qentry) {
		if (!(cmd->entry->flags & CMD_READONLY))
			readonly = 0;
	}
	if (!readonly && (c->flags & CLIENT_READONLY)) {
		cmdq_error(c->cmdq, "client is read-only");
		return;
	}

	cmdq_run(c->cmdq, bd->cmdlist, m);
}
