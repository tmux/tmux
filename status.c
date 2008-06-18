/* $Id: status.c,v 1.30 2008-06-18 22:21:51 nicm Exp $ */

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

size_t	status_width(struct winlink *);
char   *status_print(struct session *, struct winlink *, u_char *);

/* Draw status for client on the last lines of given context. */
void
status_redraw(struct client *c)
{
	struct screen_redraw_ctx	ctx;
	struct session		       *s = c->session;
	struct winlink		       *wl;
	char		 		*left, *right, *text, *ptr;
	char				lbuf[BUFSIZ], rbuf[BUFSIZ];
	size_t				llen, rlen, offset, xx, yy;
	size_t				size, start, width;
	u_char		 		attr, colr;
	int				larrow, rarrow;

	yy = options_get_number(&s->options, "status-lines");
	if (c->sy == 0 || yy == 0)
		return;
	larrow = rarrow = 0;

	if (clock_gettime(CLOCK_REALTIME, &c->status_ts) != 0)
		fatal("clock_gettime failed");
	colr = options_get_colours(&s->options, "status-colour");

	yy = c->sy - yy;
	if (yy == 0)
		goto blank;

	left = options_get_string(&s->options, "status-left");
	strftime(lbuf, sizeof lbuf, left, localtime(&(c->status_ts.tv_sec)));
	llen = strlen(lbuf);
	right = options_get_string(&s->options, "status-right");
	strftime(rbuf, sizeof rbuf, right, localtime(&(c->status_ts.tv_sec)));
	rlen = strlen(rbuf);

	/*
	 * Figure out how much space we have for the window list. If there isn't
	 * enough space, just wimp out.
	 */
	xx = 0;
	if (llen != 0)
		xx += llen + 1;
	if (rlen != 0)
		xx += rlen + 1;
	if (c->sx == 0 || c->sx <= xx)
		goto blank;
	xx = c->sx - xx;

	/*
	 * Right. We have xx characters to fill. Find out how much is to go in
	 * them and the offset of the current window (it must be on screen).
	 */
	width = offset = 0;
	RB_FOREACH(wl, winlinks, &s->windows) {
		size = status_width(wl) + 1;
		if (wl == s->curw)
			offset = width;
		width += size;
	}
	start = 0;

	/* If there is enough space for the total width, all is gravy. */
	if (width <= xx)
		goto draw;

	/* Find size of current window text. */
	size = status_width(s->curw);

	/*
	 * If the offset is already on screen, we're good to draw from the
	 * start and just leave off the end.
	 */
	if (offset + size < xx) {
		if (xx > 0) {
			rarrow = 1;
			xx--;
		}

		width = xx;
		goto draw;
	}

	/*
	 * Work out how many characters we need to omit from the start. There
	 * are xx characters to fill, and offset + size must be the last. So,
	 * the start character is offset + size - xx.
	 */
	if (xx > 0) {
		larrow = 1;
		xx--;
	}

	start = offset + size - xx;
 	if (xx > 0 && width > start + xx + 1) { /* + 1, eh? */
 		rarrow = 1;
 		start++;
 		xx--;
 	}
 	width = xx;

draw:
	/* Bail here if anything is too small too. XXX. */
	if (width == 0 || xx == 0)
		goto blank;

 	/* Begin drawing and move to the starting position. */
	screen_redraw_start_client(&ctx, c);
	screen_redraw_set_attributes(&ctx, 0, colr);
	if (llen != 0) {
 		screen_redraw_move_cursor(&ctx, 0, yy);
		screen_redraw_write_string(&ctx, "%s ", lbuf);
		if (larrow)
			ctx.write(ctx.data, TTY_CHARACTER, ' ');
	} else {
		if (larrow)
			screen_redraw_move_cursor(&ctx, 1, yy);
		else
			screen_redraw_move_cursor(&ctx, 0, yy);
	}

	/* Draw each character in succession. */
	offset = 0;
	RB_FOREACH(wl, winlinks, &s->windows) {
		text = status_print(s, wl, &attr);
		screen_redraw_set_attributes(&ctx, attr, colr);

		if (larrow == 1 && offset < start) {
			if (session_alert_has(s, wl, WINDOW_ACTIVITY))
				larrow = -1;
			if (session_alert_has(s, wl, WINDOW_BELL))
				larrow = -1;
		}

 		for (ptr = text; *ptr != '\0'; ptr++) {
			if (offset >= start && offset < start + width)
				ctx.write(ctx.data, TTY_CHARACTER, *ptr);
			offset++;
		}

		if (rarrow == 1 && offset > start + width) {
			if (session_alert_has(s, wl, WINDOW_ACTIVITY))
				rarrow = -1;
			if (session_alert_has(s, wl, WINDOW_BELL))
				rarrow = -1;
		}

		if (offset < start + width) {
			if (offset >= start) {
				screen_redraw_set_attributes(&ctx, 0, colr);
				ctx.write(ctx.data, TTY_CHARACTER, ' ');
			}
			offset++;
		}

		xfree(text);
	}

	/* Fill the remaining space if any. */
	screen_redraw_set_attributes(&ctx, 0, colr);
 	while (offset++ < xx)
		ctx.write(ctx.data, TTY_CHARACTER, ' ');

	/* Draw the last item. */
	if (rlen != 0) {
		screen_redraw_move_cursor(&ctx, c->sx - rlen - 1, yy);
		screen_redraw_write_string(&ctx, " %s", rbuf);
	}

	/* Draw the arrows. */
	if (larrow != 0) {
		if (larrow == -1)
			screen_redraw_set_attributes(&ctx, ATTR_REVERSE, colr);
		else
			screen_redraw_set_attributes(&ctx, 0, colr);
		if (llen != 0)
			screen_redraw_move_cursor(&ctx, llen + 1, yy);
		else
			screen_redraw_move_cursor(&ctx, 0, yy);
 		ctx.write(ctx.data, TTY_CHARACTER, '<');
	}
	if (rarrow != 0) {
		if (rarrow == -1)
			screen_redraw_set_attributes(&ctx, ATTR_REVERSE, colr);
		else
			screen_redraw_set_attributes(&ctx, 0, colr);
		if (rlen != 0)
			screen_redraw_move_cursor(&ctx, c->sx - rlen - 2, yy);
		else
			screen_redraw_move_cursor(&ctx, c->sx - 1, yy);
 		ctx.write(ctx.data, TTY_CHARACTER, '>');
	}

	screen_redraw_stop(&ctx);
	return;

blank:
 	/* Just draw the whole line as blank. */
	screen_redraw_start_client(&ctx, c);
	screen_redraw_set_attributes(&ctx, 0, colr);
	screen_redraw_move_cursor(&ctx, 0, yy);
	for (offset = 0; offset < c->sx; offset++)
		ctx.write(ctx.data, TTY_CHARACTER, ' ');
	screen_redraw_stop(&ctx);
}

size_t
status_width(struct winlink *wl)
{
	return (xsnprintf(NULL, 0, "%d:%s ", wl->idx, wl->window->name));
}

char *
status_print(struct session *s, struct winlink *wl, u_char *attr)
{
	char   *text, flag;

	flag = ' ';
 	if (wl == s->lastw)
		flag = '-';
	if (wl == s->curw)
		flag = '*';

	*attr = 0;
	if (session_alert_has(s, wl, WINDOW_ACTIVITY)) {
		flag = '#';
		*attr = ATTR_REVERSE;
	}
	if (session_alert_has(s, wl, WINDOW_BELL)) {
		flag = '!';
		*attr = ATTR_REVERSE;
	}

	xasprintf(&text, "%d:%s%c", wl->idx, wl->window->name, flag);
	return (text);
}
