/* $OpenBSD$ */

/*
 * Copyright (c) 2012 Thomas Adam <thomas@xteddy.org>
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

#define CMD_CHOOSE_TREE_WINDOW_ACTION "select-window -t '%%'"
#define CMD_CHOOSE_TREE_SESSION_ACTION "switch-client -t '%%'"

/*
 * Enter choice mode to choose a session and/or window.
 */

#define CHOOSE_TREE_SESSION_TEMPLATE				\
	"#{session_name}: #{session_windows} windows"		\
	"#{?session_grouped, (group ,}"				\
	"#{session_group}#{?session_grouped,),}"		\
	"#{?session_attached, (attached),}"
#define CHOOSE_TREE_WINDOW_TEMPLATE				\
	"#{window_index}: #{window_name}#{window_flags} "	\
	"\"#{pane_title}\""

enum cmd_retval	cmd_choose_tree_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_choose_tree_entry = {
	"choose-tree", NULL,
	"S:W:swub:c:t:", 0, 1,
	"[-suw] [-b session-template] [-c window template] [-S format] " \
	"[-W format] " CMD_TARGET_WINDOW_USAGE,
	0,
	cmd_choose_tree_exec
};

const struct cmd_entry cmd_choose_session_entry = {
	"choose-session", NULL,
	"F:t:", 0, 1,
	CMD_TARGET_WINDOW_USAGE " [-F format] [template]",
	0,
	cmd_choose_tree_exec
};

const struct cmd_entry cmd_choose_window_entry = {
	"choose-window", NULL,
	"F:t:", 0, 1,
	CMD_TARGET_WINDOW_USAGE "[-F format] [template]",
	0,
	cmd_choose_tree_exec
};

enum cmd_retval
cmd_choose_tree_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args			*args = self->args;
	struct winlink			*wl, *wm;
	struct session			*s, *s2;
	struct client			*c;
	struct window_choose_data	*wcd = NULL;
	const char			*ses_template, *win_template;
	char				*final_win_action, *cur_win_template;
	char				*final_win_template_middle;
	char				*final_win_template_last;
	const char			*ses_action, *win_action;
	u_int				 cur_win, idx_ses, win_ses, win_max;
	u_int				 wflag, sflag;

	ses_template = win_template = NULL;
	ses_action = win_action = NULL;

	if ((c = cmd_find_client(cmdq, NULL, 1)) == NULL) {
		cmdq_error(cmdq, "no client available");
		return (CMD_RETURN_ERROR);
	}

	if ((wl = cmd_find_window(cmdq, args_get(args, 't'), &s)) == NULL)
		return (CMD_RETURN_ERROR);

	if (window_pane_set_mode(wl->window->active, &window_choose_mode) != 0)
		return (CMD_RETURN_NORMAL);

	/* Sort out which command this is. */
	wflag = sflag = 0;
	if (self->entry == &cmd_choose_session_entry) {
		sflag = 1;
		if ((ses_template = args_get(args, 'F')) == NULL)
			ses_template = CHOOSE_TREE_SESSION_TEMPLATE;

		if (args->argc != 0)
			ses_action = args->argv[0];
		else
			ses_action = CMD_CHOOSE_TREE_SESSION_ACTION;
	} else if (self->entry == &cmd_choose_window_entry) {
		wflag = 1;
		if ((win_template = args_get(args, 'F')) == NULL)
			win_template = CHOOSE_TREE_WINDOW_TEMPLATE;

		if (args->argc != 0)
			win_action = args->argv[0];
		else
			win_action = CMD_CHOOSE_TREE_WINDOW_ACTION;
	} else {
		wflag = args_has(args, 'w');
		sflag = args_has(args, 's');

		if ((ses_action = args_get(args, 'b')) == NULL)
			ses_action = CMD_CHOOSE_TREE_SESSION_ACTION;

		if ((win_action = args_get(args, 'c')) == NULL)
			win_action = CMD_CHOOSE_TREE_WINDOW_ACTION;

		if ((ses_template = args_get(args, 'S')) == NULL)
			ses_template = CHOOSE_TREE_SESSION_TEMPLATE;

		if ((win_template = args_get(args, 'W')) == NULL)
			win_template = CHOOSE_TREE_WINDOW_TEMPLATE;
	}

	/*
	 * If not asking for windows and sessions, assume no "-ws" given and
	 * hence display the entire tree outright.
	 */
	if (!wflag && !sflag)
		wflag = sflag = 1;

	/*
	 * If we're drawing in tree mode, including sessions, then pad the
	 * window template, otherwise just render the windows as a flat list
	 * without any padding.
	 */
	if (wflag && sflag) {
		xasprintf(&final_win_template_middle,
		    " \001tq\001> %s", win_template);
		xasprintf(&final_win_template_last,
		    " \001mq\001> %s", win_template);
	} else if (wflag) {
		final_win_template_middle = xstrdup(win_template);
		final_win_template_last = xstrdup(win_template);
	} else
		final_win_template_middle = final_win_template_last = NULL;

	idx_ses = cur_win = -1;
	RB_FOREACH(s2, sessions, &sessions) {
		idx_ses++;

		/*
		 * If we're just choosing windows, jump straight there. Note
		 * that this implies the current session, so only choose
		 * windows when the session matches this one.
		 */
		if (wflag && !sflag) {
			if (s != s2)
				continue;
			goto windows_only;
		}

		wcd = window_choose_add_session(wl->window->active,
		    c, s2, ses_template, ses_action, idx_ses);

		/* If we're just choosing sessions, skip choosing windows. */
		if (sflag && !wflag) {
			if (s == s2)
				cur_win = idx_ses;
			continue;
		}
windows_only:
		win_ses = win_max = -1;
		RB_FOREACH(wm, winlinks, &s2->windows)
			win_max++;
		RB_FOREACH(wm, winlinks, &s2->windows) {
			win_ses++;
			if (sflag && wflag)
				idx_ses++;

			if (wm == s2->curw && s == s2) {
				if (wflag && !sflag) {
					/*
					 * Then we're only counting windows.
					 * So remember which is the current
					 * window in the list.
					 */
					cur_win = win_ses;
				} else
					cur_win = idx_ses;
			}

			xasprintf(&final_win_action, "%s %s %s",
			    wcd != NULL ? wcd->command : "",
			    wcd != NULL ? ";" : "", win_action);

			if (win_ses != win_max)
				cur_win_template = final_win_template_middle;
			else
				cur_win_template = final_win_template_last;

			window_choose_add_window(wl->window->active,
			    c, s2, wm, cur_win_template,
			    final_win_action,
			    (wflag && !sflag) ? win_ses : idx_ses);

			free(final_win_action);
		}

		/*
		 * If we're just drawing windows, don't consider moving on to
		 * other sessions as we only list windows in this session.
		 */
		if (wflag && !sflag)
			break;
	}
	free(final_win_template_middle);
	free(final_win_template_last);

	window_choose_ready(wl->window->active, cur_win, NULL);

	if (args_has(args, 'u')) {
		window_choose_expand_all(wl->window->active);
		window_choose_set_current(wl->window->active, cur_win);
	}

	return (CMD_RETURN_NORMAL);
}
