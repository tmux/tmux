/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicholas.marriott@gmail.com>
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
 * Display panes on a client.
 */

static enum args_parse_type	cmd_display_panes_args_parse(struct args *,
				    u_int, char **);
static enum cmd_retval		cmd_display_panes_exec(struct cmd *,
				    struct cmdq_item *);

const struct cmd_entry cmd_display_panes_entry = {
	.name = "display-panes",
	.alias = "displayp",

	.args = { "bd:Nt:", 0, 1, cmd_display_panes_args_parse },
	.usage = "[-bN] [-d duration] " CMD_TARGET_CLIENT_USAGE " [template]",

	.flags = CMD_AFTERHOOK|CMD_CLIENT_TFLAG,
	.exec = cmd_display_panes_exec
};

struct cmd_display_panes_data {
	struct cmdq_item		*item;
	struct args_command_state	*state;
};

static enum args_parse_type
cmd_display_panes_args_parse(__unused struct args *args, __unused u_int idx,
    __unused char **cause)
{
	return (ARGS_PARSE_COMMANDS_OR_STRING);
}

static void
cmd_display_panes_draw_pane(struct screen_redraw_ctx *ctx,
    struct window_pane *wp)
{
	struct client		*c = ctx->c;
	struct tty		*tty = &c->tty;
	struct session		*s = c->session;
	struct options		*oo = s->options;
	struct window		*w = wp->window;
	struct grid_cell	 fgc, bgc;
	u_int			 pane, idx, px, py, i, j, xoff, yoff, sx, sy;
	int			 colour, active_colour;
	char			 buf[16], lbuf[16], rbuf[16], *ptr;
	size_t			 len, llen, rlen;

	if (wp->xoff + wp->sx <= ctx->ox ||
	    wp->xoff >= ctx->ox + ctx->sx ||
	    wp->yoff + wp->sy <= ctx->oy ||
	    wp->yoff >= ctx->oy + ctx->sy)
		return;

	if (wp->xoff >= ctx->ox && wp->xoff + wp->sx <= ctx->ox + ctx->sx) {
		/* All visible. */
		xoff = wp->xoff - ctx->ox;
		sx = wp->sx;
	} else if (wp->xoff < ctx->ox &&
	    wp->xoff + wp->sx > ctx->ox + ctx->sx) {
		/* Both left and right not visible. */
		xoff = 0;
		sx = ctx->sx;
	} else if (wp->xoff < ctx->ox) {
		/* Left not visible. */
		xoff = 0;
		sx = wp->sx - (ctx->ox - wp->xoff);
	} else {
		/* Right not visible. */
		xoff = wp->xoff - ctx->ox;
		sx = wp->sx - xoff;
	}
	if (wp->yoff >= ctx->oy && wp->yoff + wp->sy <= ctx->oy + ctx->sy) {
		/* All visible. */
		yoff = wp->yoff - ctx->oy;
		sy = wp->sy;
	} else if (wp->yoff < ctx->oy &&
	    wp->yoff + wp->sy > ctx->oy + ctx->sy) {
		/* Both top and bottom not visible. */
		yoff = 0;
		sy = ctx->sy;
	} else if (wp->yoff < ctx->oy) {
		/* Top not visible. */
		yoff = 0;
		sy = wp->sy - (ctx->oy - wp->yoff);
	} else {
		/* Bottom not visible. */
		yoff = wp->yoff - ctx->oy;
		sy = wp->sy - yoff;
	}

	if (ctx->statustop)
		yoff += ctx->statuslines;
	px = sx / 2;
	py = sy / 2;

	if (window_pane_index(wp, &pane) != 0)
		fatalx("index not found");
	len = xsnprintf(buf, sizeof buf, "%u", pane);

	if (sx < len)
		return;
	colour = options_get_number(oo, "display-panes-colour");
	active_colour = options_get_number(oo, "display-panes-active-colour");

	memcpy(&fgc, &grid_default_cell, sizeof fgc);
	memcpy(&bgc, &grid_default_cell, sizeof bgc);
	if (w->active == wp) {
		fgc.fg = active_colour;
		bgc.bg = active_colour;
	} else {
		fgc.fg = colour;
		bgc.bg = colour;
	}

	rlen = xsnprintf(rbuf, sizeof rbuf, "%ux%u", wp->sx, wp->sy);
	if (pane > 9 && pane < 35)
		llen = xsnprintf(lbuf, sizeof lbuf, "%c", 'a' + (pane - 10));
	else
		llen = 0;

	if (sx < len * 6 || sy < 5) {
		tty_attributes(tty, &fgc, &grid_default_cell, NULL);
		if (sx >= len + llen + 1) {
			len += llen + 1;
			tty_cursor(tty, xoff + px - len / 2, yoff + py);
			tty_putn(tty, buf, len,	 len);
			tty_putn(tty, " ", 1, 1);
			tty_putn(tty, lbuf, llen, llen);
		} else {
			tty_cursor(tty, xoff + px - len / 2, yoff + py);
			tty_putn(tty, buf, len, len);
		}
		goto out;
	}

	px -= len * 3;
	py -= 2;

	tty_attributes(tty, &bgc, &grid_default_cell, NULL);
	for (ptr = buf; *ptr != '\0'; ptr++) {
		if (*ptr < '0' || *ptr > '9')
			continue;
		idx = *ptr - '0';

		for (j = 0; j < 5; j++) {
			for (i = px; i < px + 5; i++) {
				tty_cursor(tty, xoff + i, yoff + py + j);
				if (window_clock_table[idx][j][i - px])
					tty_putc(tty, ' ');
			}
		}
		px += 6;
	}

	if (sy <= 6)
		goto out;
	tty_attributes(tty, &fgc, &grid_default_cell, NULL);
	if (rlen != 0 && sx >= rlen) {
		tty_cursor(tty, xoff + sx - rlen, yoff);
		tty_putn(tty, rbuf, rlen, rlen);
	}
	if (llen != 0) {
		tty_cursor(tty, xoff + sx / 2 + len * 3 - llen - 1,
		    yoff + py + 5);
		tty_putn(tty, lbuf, llen, llen);
	}

out:
	tty_cursor(tty, 0, 0);
}

static void
cmd_display_panes_draw(struct client *c, __unused void *data,
    struct screen_redraw_ctx *ctx)
{
	struct window		*w = c->session->curw->window;
	struct window_pane	*wp;

	log_debug("%s: %s @%u", __func__, c->name, w->id);

	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (window_pane_visible(wp))
			cmd_display_panes_draw_pane(ctx, wp);
	}
}

static void
cmd_display_panes_free(__unused struct client *c, void *data)
{
	struct cmd_display_panes_data	*cdata = data;

	if (cdata->item != NULL)
		cmdq_continue(cdata->item);
	args_make_commands_free(cdata->state);
	free(cdata);
}

static int
cmd_display_panes_key(struct client *c, void *data, struct key_event *event)
{
	struct cmd_display_panes_data	*cdata = data;
	char				*expanded, *error;
	struct cmdq_item		*item = cdata->item, *new_item;
	struct cmd_list			*cmdlist;
	struct window			*w = c->session->curw->window;
	struct window_pane		*wp;
	u_int				 index;
	key_code			 key;

	if (event->key >= '0' && event->key <= '9')
		index = event->key - '0';
	else if ((event->key & KEYC_MASK_MODIFIERS) == 0) {
		key = (event->key & KEYC_MASK_KEY);
		if (key >= 'a' && key <= 'z')
			index = 10 + (key - 'a');
		else
			return (-1);
	} else
		return (-1);

	wp = window_pane_at_index(w, index);
	if (wp == NULL)
		return (1);
	window_unzoom(w);

	xasprintf(&expanded, "%%%u", wp->id);

	cmdlist = args_make_commands(cdata->state, 1, &expanded, &error);
	if (cmdlist == NULL) {
		cmdq_append(c, cmdq_get_error(error));
		free(error);
	} else if (item == NULL) {
		new_item = cmdq_get_command(cmdlist, NULL);
		cmdq_append(c, new_item);
	} else {
		new_item = cmdq_get_command(cmdlist, cmdq_get_state(item));
		cmdq_insert_after(item, new_item);
	}

	free(expanded);
	return (1);
}

static enum cmd_retval
cmd_display_panes_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = cmd_get_args(self);
	struct client			*tc = cmdq_get_target_client(item);
	struct session			*s = tc->session;
	u_int				 delay;
	char				*cause;
	struct cmd_display_panes_data	*cdata;
	int				 wait = !args_has(args, 'b');

	if (tc->overlay_draw != NULL)
		return (CMD_RETURN_NORMAL);

	if (args_has(args, 'd')) {
		delay = args_strtonum(args, 'd', 0, UINT_MAX, &cause);
		if (cause != NULL) {
			cmdq_error(item, "delay %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
	} else
		delay = options_get_number(s->options, "display-panes-time");

	cdata = xcalloc(1, sizeof *cdata);
	if (wait)
		cdata->item = item;
	cdata->state = args_make_commands_prepare(self, item, 0,
	    "select-pane -t \"%%%\"", wait, 0);

	if (args_has(args, 'N')) {
		server_client_set_overlay(tc, delay, NULL, NULL,
		    cmd_display_panes_draw, NULL, cmd_display_panes_free, NULL,
		    cdata);
	} else {
		server_client_set_overlay(tc, delay, NULL, NULL,
		    cmd_display_panes_draw, cmd_display_panes_key,
		    cmd_display_panes_free, NULL, cdata);
	}

	if (!wait)
		return (CMD_RETURN_NORMAL);
	return (CMD_RETURN_WAIT);
}
