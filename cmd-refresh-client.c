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

#include "tmux.h"

/*
 * Refresh client.
 */

static enum cmd_retval	cmd_refresh_client_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_refresh_client_entry = {
	.name = "refresh-client",
	.alias = "refresh",

	.args = { "cC:DLRSt:U", 0, 1 },
	.usage = "[-cDLRSU] [-C size] " CMD_TARGET_CLIENT_USAGE " [adjustment]",

	.flags = CMD_AFTERHOOK,
	.exec = cmd_refresh_client_exec
};

static enum cmd_retval
cmd_refresh_client_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args	*args = self->args;
	struct client	*c;
	struct tty	*tty;
	struct window	*w;
	const char	*size, *errstr;
	u_int		 x, y, adjust;

	if ((c = cmd_find_client(item, args_get(args, 't'), 0)) == NULL)
		return (CMD_RETURN_ERROR);
	tty = &c->tty;

	if (args_has(args, 'c') ||
	    args_has(args, 'L') ||
	    args_has(args, 'R') ||
	    args_has(args, 'U') ||
	    args_has(args, 'D'))
	{
		if (args->argc == 0)
			adjust = 1;
		else {
			adjust = strtonum(args->argv[0], 1, INT_MAX, &errstr);
			if (errstr != NULL) {
				cmdq_error(item, "adjustment %s", errstr);
				return (CMD_RETURN_ERROR);
			}
		}

		if (args_has(args, 'c'))
		    c->pan_window = NULL;
		else {
			w = c->session->curw->window;
			if (c->pan_window != w) {
				c->pan_window = w;
				c->pan_ox = tty->oox;
				c->pan_oy = tty->ooy;
			}
			if (args_has(args, 'L')) {
				if (c->pan_ox > adjust)
					c->pan_ox -= adjust;
				else
					c->pan_ox = 0;
			} else if (args_has(args, 'R')) {
				c->pan_ox += adjust;
				if (c->pan_ox > w->sx - tty->osx)
					c->pan_ox = w->sx - tty->osx;
			} else if (args_has(args, 'U')) {
				if (c->pan_oy > adjust)
					c->pan_oy -= adjust;
				else
					c->pan_oy = 0;
			} else if (args_has(args, 'D')) {
				c->pan_oy += adjust;
				if (c->pan_oy > w->sy - tty->osy)
					c->pan_oy = w->sy - tty->osy;
			}
		}
		tty_update_client_offset(c);
		server_redraw_client(c);
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'C')) {
		if ((size = args_get(args, 'C')) == NULL) {
			cmdq_error(item, "missing size");
			return (CMD_RETURN_ERROR);
		}
		if (sscanf(size, "%u,%u", &x, &y) != 2 &&
		    sscanf(size, "%ux%u", &x, &y)) {
			cmdq_error(item, "bad size argument");
			return (CMD_RETURN_ERROR);
		}
		if (x < WINDOW_MINIMUM || x > WINDOW_MAXIMUM ||
		    y < WINDOW_MINIMUM || y > WINDOW_MAXIMUM) {
			cmdq_error(item, "size too small or too big");
			return (CMD_RETURN_ERROR);
		}
		if (!(c->flags & CLIENT_CONTROL)) {
			cmdq_error(item, "not a control client");
			return (CMD_RETURN_ERROR);
		}
		tty_set_size(&c->tty, x, y);
		c->flags |= CLIENT_SIZECHANGED;
		recalculate_sizes();
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'S')) {
		c->flags |= CLIENT_STATUSFORCE;
		server_status_client(c);
	} else {
		c->flags |= CLIENT_STATUSFORCE;
		server_redraw_client(c);
	}
	return (CMD_RETURN_NORMAL);
}
