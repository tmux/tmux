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

static enum cmd_retval	cmd_display_panes_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_display_panes_entry = {
	.name = "display-panes",
	.alias = "displayp",

	.args = { "bd:t:", 0, 1 },
	.usage = "[-b] [-d duration] " CMD_TARGET_CLIENT_USAGE " [template]",

	.flags = CMD_AFTERHOOK,
	.exec = cmd_display_panes_exec
};

struct cmd_display_panes_data {
	struct cmdq_item	*item;
	char			*command;
};

static void
cmd_display_panes_draw_pane(struct screen_redraw_ctx *ctx,
    struct window_pane *wp)
{
	struct client		*c = ctx->c;
	struct tty		*tty = &c->tty;
	struct session		*s = c->session;
	struct options		*oo = s->options;
	struct window		*w = wp->window;
	struct grid_cell	 gc;
	u_int			 idx, px, py, i, j, xoff, yoff, sx, sy;
	int			 colour, active_colour;
	char			 buf[16], *ptr;
	size_t			 len;

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

	if (window_pane_index(wp, &idx) != 0)
		fatalx("index not found");
	len = xsnprintf(buf, sizeof buf, "%u", idx);

	if (sx < len)
		return;
	colour = options_get_number(oo, "display-panes-colour");
	active_colour = options_get_number(oo, "display-panes-active-colour");

	if (sx < len * 6 || sy < 5) {
		tty_cursor(tty, xoff + px - len / 2, yoff + py);
		goto draw_text;
	}

	px -= len * 3;
	py -= 2;

	memcpy(&gc, &grid_default_cell, sizeof gc);
	if (w->active == wp)
		gc.bg = active_colour;
	else
		gc.bg = colour;
	gc.flags |= GRID_FLAG_NOPALETTE;

	tty_attributes(tty, &gc, wp);
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

	len = xsnprintf(buf, sizeof buf, "%ux%u", wp->sx, wp->sy);
	if (sx < len || sy < 6)
		return;
	tty_cursor(tty, xoff + sx - len, yoff);

draw_text:
	memcpy(&gc, &grid_default_cell, sizeof gc);
	if (w->active == wp)
		gc.fg = active_colour;
	else
		gc.fg = colour;
	gc.flags |= GRID_FLAG_NOPALETTE;

	tty_attributes(tty, &gc, wp);
	tty_puts(tty, buf);

	tty_cursor(tty, 0, 0);
}

static void
cmd_display_panes_draw(struct client *c, struct screen_redraw_ctx *ctx)
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
cmd_display_panes_free(struct client *c)
{
	struct cmd_display_panes_data	*cdata = c->overlay_data;

	if (cdata->item != NULL)
		cmdq_continue(cdata->item);
	free(cdata->command);
	free(cdata);
}

static int
cmd_display_panes_key(struct client *c, struct key_event *event)
{
	struct cmd_display_panes_data	*cdata = c->overlay_data;
	struct cmdq_item		*new_item;
	char				*cmd, *expanded;
	struct window			*w = c->session->curw->window;
	struct window_pane		*wp;
	struct cmd_parse_result		*pr;

	if (event->key < '0' || event->key > '9')
		return (-1);

	wp = window_pane_at_index(w, event->key - '0');
	if (wp == NULL)
		return (1);
	window_unzoom(w);

	xasprintf(&expanded, "%%%u", wp->id);
	cmd = cmd_template_replace(cdata->command, expanded, 1);

	pr = cmd_parse_from_string(cmd, NULL);
	switch (pr->status) {
	case CMD_PARSE_EMPTY:
		new_item = NULL;
		break;
	case CMD_PARSE_ERROR:
		new_item = cmdq_get_error(pr->error);
		free(pr->error);
		cmdq_append(c, new_item);
		break;
	case CMD_PARSE_SUCCESS:
		new_item = cmdq_get_command(pr->cmdlist, NULL, NULL, 0);
		cmd_list_free(pr->cmdlist);
		cmdq_append(c, new_item);
		break;
	}

	free(cmd);
	free(expanded);
	return (1);
}

static enum cmd_retval
cmd_display_panes_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = cmd_get_args(self);
	struct client			*c;
	struct session			*s;
	u_int		 		 delay;
	char				*cause;
	struct cmd_display_panes_data	*cdata;

	if ((c = cmd_find_client(item, args_get(args, 't'), 0)) == NULL)
		return (CMD_RETURN_ERROR);
	s = c->session;

	if (c->overlay_draw != NULL)
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

	cdata = xmalloc(sizeof *cdata);
	if (args->argc != 0)
		cdata->command = xstrdup(args->argv[0]);
	else
		cdata->command = xstrdup("select-pane -t '%%'");
	if (args_has(args, 'b'))
		cdata->item = NULL;
	else
		cdata->item = item;

	server_client_set_overlay(c, delay, NULL, NULL, cmd_display_panes_draw,
	    cmd_display_panes_key, cmd_display_panes_free, cdata);

	if (args_has(args, 'b'))
		return (CMD_RETURN_NORMAL);
	return (CMD_RETURN_WAIT);
}
