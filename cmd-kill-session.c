/* $OpenBSD: cmd-kill-session.c,v 1.31 2026/06/09 12:57:40 nicm Exp $ */

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

#include "tmux.h"

/*
 * Destroy session, detaching all clients attached to it and destroying any
 * windows linked only to this session.
 *
 * Note this deliberately has no alias to make it hard to hit by accident.
 */

static enum cmd_retval	cmd_kill_session_exec(struct cmd *, struct cmdq_item *);
static enum cmd_retval	cmd_kill_session_all(struct cmdq_item *, const char *);
static int		cmd_kill_session_filter(struct cmdq_item *,
			    struct session *, const char *);

const struct cmd_entry cmd_kill_session_entry = {
	.name = "kill-session",
	.alias = NULL,

	.args = { "aCgf:t:", 0, 0, NULL },
	.usage = "[-aCg] [-f filter] " CMD_TARGET_SESSION_USAGE,

	.target = { 't', CMD_FIND_SESSION, 0 },

	.flags = 0,
	.exec = cmd_kill_session_exec
};

static enum cmd_retval
cmd_kill_session_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct session		*s = target->s, *sloop, *stmp;
	struct session_group	*sg;
	struct winlink		*wl;
	const char		*filter = args_get(args, 'f');

	if (filter != NULL && (!args_has(args, 'a') || args_has(args, 'C'))) {
		cmdq_error(item, "-f only valid with -a");
		return (CMD_RETURN_ERROR);
	}

	if (args_has(args, 'C')) {
		RB_FOREACH(wl, winlinks, &s->windows) {
			wl->window->flags &= ~WINDOW_ALERTFLAGS;
			wl->flags &= ~WINLINK_ALERTFLAGS;
		}
		server_redraw_session(s);
	} else if (args_has(args, 'a'))
		return (cmd_kill_session_all(item, filter));
	else if (args_has(args, 'g') &&
	    (sg = session_group_contains(s)) != NULL) {
		TAILQ_FOREACH_SAFE(sloop, &sg->sessions, gentry, stmp) {
			server_destroy_session(sloop);
			session_destroy(sloop, 1, __func__);
		}
	} else {
		server_destroy_session(s);
		session_destroy(s, 1, __func__);
	}
	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_kill_session_all(struct cmdq_item *item, const char *filter)
{
	struct session	*s = cmdq_get_target(item)->s;
	struct session	*sloop, *stmp;

	RB_FOREACH_SAFE(sloop, sessions, &sessions, stmp) {
		if (sloop == s)
			continue;
		if (!cmd_kill_session_filter(item, sloop, filter))
			continue;
		server_destroy_session(sloop);
		session_destroy(sloop, 1, __func__);
	}
	return (CMD_RETURN_NORMAL);
}

static int
cmd_kill_session_filter(struct cmdq_item *item, struct session *s,
    const char *filter)
{
	struct format_tree	*ft;
	char			*expanded;
	int			 flag;

	if (filter == NULL)
		return (1);

	ft = format_create(cmdq_get_client(item), item, FORMAT_NONE, 0);
	format_defaults(ft, NULL, s, NULL, NULL);

	expanded = format_expand(ft, filter);
	flag = format_true(expanded);
	free(expanded);

	format_free(ft);
	return (flag);
}
