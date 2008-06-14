/* $Id: status.c,v 1.25 2008-06-14 16:47:20 nicm Exp $ */

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

#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "tmux.h"

/* Draw status for client on the last lines of given context. */
void
status_redraw(struct client *c)
{
	struct screen_redraw_ctx	ctx;
	struct winlink		       *wl;
	char		 		flag, *left, *right;
	char				lbuf[BUFSIZ], rbuf[BUFSIZ];
	size_t				llen, rlen;
	u_char		 		scolour;
	u_int		 		slines;

	scolour = options_get_number(&c->session->options, "status-colour");
	slines = options_get_number(&c->session->options, "status-lines");
	if (slines == 0 || c->sy <= slines)
		return;

	if (clock_gettime(CLOCK_REALTIME, &c->status_ts) != 0)
		fatal("clock_gettime failed");

	left = options_get_string(&c->session->options, "status-left");
	strftime(lbuf, sizeof lbuf, left, localtime(&(c->status_ts.tv_sec)));
	llen = strlen(lbuf) + 1;
	right = options_get_string(&c->session->options, "status-right");
	strftime(rbuf, sizeof rbuf, right, localtime(&(c->status_ts.tv_sec)));
	rlen = strlen(rbuf) + 1;

	screen_redraw_start_client(&ctx, c);
	screen_redraw_move_cursor(&ctx, llen, c->sy - slines);
	screen_redraw_set_attributes(&ctx, 0, scolour);

	RB_FOREACH(wl, winlinks, &c->session->windows) {
		flag = ' ';
		if (wl == c->session->lastw)
			flag = '-';
		if (wl == c->session->curw)
			flag = '*';
		if (session_alert_has(c->session, wl, WINDOW_ACTIVITY)) {
			flag = '#';
			screen_redraw_set_attributes(
			    &ctx, ATTR_REVERSE, scolour);
		}
		if (session_alert_has(c->session, wl, WINDOW_BELL)) {
			flag = '!';
			screen_redraw_set_attributes(
			    &ctx, ATTR_REVERSE, scolour);
		}
		screen_redraw_write_string(
		    &ctx, "%d:%s%c", wl->idx, wl->window->name, flag);
		if (flag == '!' || flag == '#')
			screen_redraw_set_attributes(&ctx, 0, scolour);
		screen_redraw_write_string(&ctx, " ");
		
		if (ctx.s->cx > c->sx - rlen)
			break;
	}
	while (ctx.s->cx < c->sx - rlen) {
		ctx.write(ctx.data, TTY_CHARACTER, ' ');
		ctx.s->cx++;
	}

	screen_redraw_move_cursor(&ctx, 0, c->sy - slines);
	screen_redraw_write_string(&ctx, "%s ", lbuf);

	screen_redraw_move_cursor(&ctx, c->sx - rlen, c->sy - slines);
	screen_redraw_write_string(&ctx, " %s", rbuf);

	screen_redraw_stop(&ctx);
}
