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
 * Refresh client.
 */

static enum cmd_retval	cmd_refresh_client_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_refresh_client_entry = {
	.name = "refresh-client",
	.alias = "refresh",

	.args = { "A:B:cC:Df:r:F:l::LRSt:U", 0, 1, NULL },
	.usage = "[-cDlLRSU] [-A pane:state] [-B name:what:format] "
		 "[-C XxY] [-f flags] [-r pane:report]" CMD_TARGET_CLIENT_USAGE
		 " [adjustment]",

	.flags = CMD_AFTERHOOK|CMD_CLIENT_TFLAG,
	.exec = cmd_refresh_client_exec
};

static void
cmd_refresh_client_update_subscription(struct client *tc, const char *value)
{
	char			*copy, *split, *name, *what;
	enum control_sub_type	 subtype;
	int			 subid = -1;

	copy = name = xstrdup(value);
	if ((split = strchr(copy, ':')) == NULL) {
		control_remove_sub(tc, copy);
		goto out;
	}
	*split++ = '\0';

	what = split;
	if ((split = strchr(what, ':')) == NULL)
		goto out;
	*split++ = '\0';

	if (strcmp(what, "%*") == 0)
		subtype = CONTROL_SUB_ALL_PANES;
	else if (sscanf(what, "%%%d", &subid) == 1 && subid >= 0)
		subtype = CONTROL_SUB_PANE;
	else if (strcmp(what, "@*") == 0)
		subtype = CONTROL_SUB_ALL_WINDOWS;
	else if (sscanf(what, "@%d", &subid) == 1 && subid >= 0)
		subtype = CONTROL_SUB_WINDOW;
	else
		subtype = CONTROL_SUB_SESSION;
	control_add_sub(tc, name, subtype, subid, split);

out:
	free(copy);
}

static enum cmd_retval
cmd_refresh_client_control_client_size(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct client		*tc = cmdq_get_target_client(item);
	const char		*size = args_get(args, 'C');
	u_int			 w, x, y;
	struct client_window	*cw;

	if (sscanf(size, "@%u:%ux%u", &w, &x, &y) == 3) {
		if (x < WINDOW_MINIMUM || x > WINDOW_MAXIMUM ||
		    y < WINDOW_MINIMUM || y > WINDOW_MAXIMUM) {
			cmdq_error(item, "size too small or too big");
			return (CMD_RETURN_ERROR);
		}
		log_debug("%s: client %s window @%u: size %ux%u", __func__,
		    tc->name, w, x, y);
		cw = server_client_add_client_window(tc, w);
		cw->sx = x;
		cw->sy = y;
		tc->flags |= CLIENT_WINDOWSIZECHANGED;
		recalculate_sizes_now(1);
		return (CMD_RETURN_NORMAL);
	}
	if (sscanf(size, "@%u:", &w) == 1) {
		cw = server_client_get_client_window(tc, w);
		if (cw != NULL) {
			log_debug("%s: client %s window @%u: no size", __func__,
			    tc->name, w);
			cw->sx = 0;
			cw->sy = 0;
			recalculate_sizes_now(1);
		}
		return (CMD_RETURN_NORMAL);
	}

	if (sscanf(size, "%u,%u", &x, &y) != 2 &&
	    sscanf(size, "%ux%u", &x, &y) != 2) {
		cmdq_error(item, "bad size argument");
		return (CMD_RETURN_ERROR);
	}
	if (x < WINDOW_MINIMUM || x > WINDOW_MAXIMUM ||
	    y < WINDOW_MINIMUM || y > WINDOW_MAXIMUM) {
		cmdq_error(item, "size too small or too big");
		return (CMD_RETURN_ERROR);
	}
	tty_set_size(&tc->tty, x, y, 0, 0);
	tc->flags |= CLIENT_SIZECHANGED;
	recalculate_sizes_now(1);
	return (CMD_RETURN_NORMAL);
}

static void
cmd_refresh_client_update_offset(struct client *tc, const char *value)
{
	struct window_pane	*wp;
	char			*copy, *split;
	u_int			 pane;

	if (*value != '%')
		return;
	copy = xstrdup(value);
	if ((split = strchr(copy, ':')) == NULL)
		goto out;
	*split++ = '\0';

	if (sscanf(copy, "%%%u", &pane) != 1)
		goto out;
	wp = window_pane_find_by_id(pane);
	if (wp == NULL)
		goto out;

	if (strcmp(split, "on") == 0)
		control_set_pane_on(tc, wp);
	else if (strcmp(split, "off") == 0)
		control_set_pane_off(tc, wp);
	else if (strcmp(split, "continue") == 0)
		control_continue_pane(tc, wp);
	else if (strcmp(split, "pause") == 0)
		control_pause_pane(tc, wp);

out:
	free(copy);
}

static enum cmd_retval
cmd_refresh_client_clipboard(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct client		*tc = cmdq_get_target_client(item);
	const char		*p;
	u_int			 i;
	struct cmd_find_state	 fs;

	p = args_get(args, 'l');
	if (p == NULL) {
		if (tc->flags & CLIENT_CLIPBOARDBUFFER)
			return (CMD_RETURN_NORMAL);
		tc->flags |= CLIENT_CLIPBOARDBUFFER;
	} else {
		if (cmd_find_target(&fs, item, p, CMD_FIND_PANE, 0) != 0)
			return (CMD_RETURN_ERROR);
		for (i = 0; i < tc->clipboard_npanes; i++) {
			if (tc->clipboard_panes[i] == fs.wp->id)
				break;
		}
		if (i != tc->clipboard_npanes)
			return (CMD_RETURN_NORMAL);
		tc->clipboard_panes = xreallocarray(tc->clipboard_panes,
		    tc->clipboard_npanes + 1, sizeof *tc->clipboard_panes);
		tc->clipboard_panes[tc->clipboard_npanes++] = fs.wp->id;
	}
	tty_clipboard_query(&tc->tty);
	return (CMD_RETURN_NORMAL);
}

static void
cmd_refresh_report(struct tty *tty, const char *value)
{
	struct window_pane	*wp;
	u_int			 pane;
	size_t			 size = 0;
	char			*copy, *split;

	if (*value != '%')
		return;
	copy = xstrdup(value);
	if ((split = strchr(copy, ':')) == NULL)
		goto out;
	*split++ = '\0';

	if (sscanf(copy, "%%%u", &pane) != 1)
		goto out;
	wp = window_pane_find_by_id(pane);
	if (wp == NULL)
		goto out;

	tty_keys_colours(tty, split, strlen(split), &size, &wp->control_fg,
	    &wp->control_bg);

out:
	free(copy);
}

static enum cmd_retval
cmd_refresh_client_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct client		*tc = cmdq_get_target_client(item);
	struct tty		*tty = &tc->tty;
	struct window		*w;
	const char		*errstr;
	u_int			 adjust;
	struct args_value	*av;

	if (args_has(args, 'c') ||
	    args_has(args, 'L') ||
	    args_has(args, 'R') ||
	    args_has(args, 'U') ||
	    args_has(args, 'D'))
	{
		if (args_count(args) == 0)
			adjust = 1;
		else {
			adjust = strtonum(args_string(args, 0), 1, INT_MAX,
			    &errstr);
			if (errstr != NULL) {
				cmdq_error(item, "adjustment %s", errstr);
				return (CMD_RETURN_ERROR);
			}
		}

		if (args_has(args, 'c'))
			tc->pan_window = NULL;
		else {
			w = tc->session->curw->window;
			if (tc->pan_window != w) {
				tc->pan_window = w;
				tc->pan_ox = tty->oox;
				tc->pan_oy = tty->ooy;
			}
			if (args_has(args, 'L')) {
				if (tc->pan_ox > adjust)
					tc->pan_ox -= adjust;
				else
					tc->pan_ox = 0;
			} else if (args_has(args, 'R')) {
				tc->pan_ox += adjust;
				if (tc->pan_ox > w->sx - tty->osx)
					tc->pan_ox = w->sx - tty->osx;
			} else if (args_has(args, 'U')) {
				if (tc->pan_oy > adjust)
					tc->pan_oy -= adjust;
				else
					tc->pan_oy = 0;
			} else if (args_has(args, 'D')) {
				tc->pan_oy += adjust;
				if (tc->pan_oy > w->sy - tty->osy)
					tc->pan_oy = w->sy - tty->osy;
			}
		}
		tty_update_client_offset(tc);
		server_redraw_client(tc);
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'l'))
		return (cmd_refresh_client_clipboard(self, item));

	if (args_has(args, 'F')) /* -F is an alias for -f */
		server_client_set_flags(tc, args_get(args, 'F'));
	if (args_has(args, 'f'))
		server_client_set_flags(tc, args_get(args, 'f'));
	if (args_has(args, 'r'))
		cmd_refresh_report(tty, args_get(args, 'r'));

	if (args_has(args, 'A')) {
		if (~tc->flags & CLIENT_CONTROL)
			goto not_control_client;
		av = args_first_value(args, 'A');
		while (av != NULL) {
			cmd_refresh_client_update_offset(tc, av->string);
			av = args_next_value(av);
		}
		return (CMD_RETURN_NORMAL);
	}
	if (args_has(args, 'B')) {
		if (~tc->flags & CLIENT_CONTROL)
			goto not_control_client;
		av = args_first_value(args, 'B');
		while (av != NULL) {
			cmd_refresh_client_update_subscription(tc, av->string);
			av = args_next_value(av);
		}
		return (CMD_RETURN_NORMAL);
	}
	if (args_has(args, 'C')) {
		if (~tc->flags & CLIENT_CONTROL)
			goto not_control_client;
		return (cmd_refresh_client_control_client_size(self, item));
	}

	if (args_has(args, 'S')) {
		tc->flags |= CLIENT_STATUSFORCE;
		server_status_client(tc);
	} else {
		tc->flags |= CLIENT_STATUSFORCE;
		server_redraw_client(tc);
	}
	return (CMD_RETURN_NORMAL);

not_control_client:
	cmdq_error(item, "not a control client");
	return (CMD_RETURN_ERROR);
}
