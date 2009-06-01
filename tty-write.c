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

#include "tmux.h"

void	tty_vwrite_cmd(struct window_pane *, enum tty_cmd, va_list);

void
tty_write_cmd(struct window_pane *wp, enum tty_cmd cmd, ...)
{
	va_list	ap;

	va_start(ap, cmd);
	tty_vwrite_cmd(wp, cmd, ap);
	va_end(ap);
}

void
tty_vwrite_cmd(struct window_pane *wp, enum tty_cmd cmd, va_list ap)
{
	struct client	*c;
	va_list		 aq;
	u_int		 i;

	if (wp == NULL)
		return;

	if (wp->window->flags & WINDOW_REDRAW || wp->flags & PANE_REDRAW)
		return;
	if (wp->window->flags & WINDOW_HIDDEN || wp->flags & PANE_HIDDEN)
		return;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		if (c->flags & CLIENT_SUSPENDED)
			continue;

		if (c->session->curw->window == wp->window) {
			tty_update_mode(&c->tty, c->tty.mode & ~MODE_CURSOR);

			va_copy(aq, ap);
			tty_vwrite(&c->tty, wp, cmd, aq);
			va_end(aq);
		}
	}
}

void
tty_write_mode(struct window_pane *wp, int mode)
{
	struct client	*c;
	u_int		 i;

	if (wp == NULL)
		return;

	if (wp->window->flags & WINDOW_REDRAW || wp->flags & PANE_REDRAW)
		return;
	if (wp->window->flags & WINDOW_HIDDEN || wp->flags & PANE_HIDDEN)
		return;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		if (c->flags & CLIENT_SUSPENDED)
			continue;

		tty_update_mode(&c->tty, mode);
	}
}

