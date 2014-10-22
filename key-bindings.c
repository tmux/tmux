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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

RB_GENERATE(key_bindings, key_binding, entry, key_bindings_cmp);

struct key_bindings	key_bindings;

int
key_bindings_cmp(struct key_binding *bd1, struct key_binding *bd2)
{
	int	key1, key2;

	key1 = bd1->key & ~KEYC_PREFIX;
	key2 = bd2->key & ~KEYC_PREFIX;
	if (key1 != key2)
		return (key1 - key2);

	if (bd1->key & KEYC_PREFIX && !(bd2->key & KEYC_PREFIX))
		return (-1);
	if (bd2->key & KEYC_PREFIX && !(bd1->key & KEYC_PREFIX))
		return (1);
	return (0);
}

struct key_binding *
key_bindings_lookup(int key)
{
	struct key_binding	bd;

	bd.key = key;
	return (RB_FIND(key_bindings, &key_bindings, &bd));
}

void
key_bindings_add(int key, int can_repeat, struct cmd_list *cmdlist)
{
	struct key_binding	*bd;

	key_bindings_remove(key);

	bd = xmalloc(sizeof *bd);
	bd->key = key;
	RB_INSERT(key_bindings, &key_bindings, bd);

	bd->can_repeat = can_repeat;
	bd->cmdlist = cmdlist;
}

void
key_bindings_remove(int key)
{
	struct key_binding	*bd;

	if ((bd = key_bindings_lookup(key)) == NULL)
		return;
	RB_REMOVE(key_bindings, &key_bindings, bd);
	cmd_list_free(bd->cmdlist);
	free(bd);
}

void
key_bindings_init(void)
{
	static const char* defaults[] = {
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
		"bind 0 select-window -t:0",
		"bind 1 select-window -t:1",
		"bind 2 select-window -t:2",
		"bind 3 select-window -t:3",
		"bind 4 select-window -t:4",
		"bind 5 select-window -t:5",
		"bind 6 select-window -t:6",
		"bind 7 select-window -t:7",
		"bind 8 select-window -t:8",
		"bind 9 select-window -t:9",
		"bind : command-prompt",
		"bind \\; last-pane",
		"bind = choose-buffer",
		"bind ? list-keys",
		"bind D choose-client",
		"bind L switch-client -l",
		"bind [ copy-mode",
		"bind ] paste-buffer",
		"bind c new-window",
		"bind d detach-client",
		"bind f command-prompt \"find-window '%%'\"",
		"bind i display-message",
		"bind l last-window",
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
	};
	u_int		 i;
	struct cmd_list	*cmdlist;
	char*            cause;
	int		 error;
	struct cmd_q	*cmdq;

	RB_INIT(&key_bindings);

	cmdq = cmdq_new(NULL);
	for (i = 0; i < nitems(defaults); i++) {
		error = cmd_string_parse(defaults[i], &cmdlist,
		    "<default-keys>", i, &cause);
		if (error != 0)
			fatalx("bad default key");
		cmdq_run(cmdq, cmdlist);
		cmd_list_free(cmdlist);
	}
	cmdq_free(cmdq);
}

void
key_bindings_dispatch(struct key_binding *bd, struct client *c)
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

	cmdq_run(c->cmdq, bd->cmdlist);
}
