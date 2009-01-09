/* $Id: tty-write.c,v 1.3 2009-01-09 23:57:42 nicm Exp $ */

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

void
tty_write_client(void *ptr, enum tty_cmd cmd, ...)
{
	struct client	*c = ptr;
	va_list		 ap;

	va_start(ap, cmd);
	tty_vwrite_client(c, cmd, ap);
	va_end(ap);
}

void
tty_vwrite_client(void *ptr, enum tty_cmd cmd, va_list ap)
{
	struct client	*c = ptr;
	struct screen	*s = c->session->curw->window->screen;

	tty_vwrite(&c->tty, s, cmd, ap);
}

void
tty_write_window(void *ptr, enum tty_cmd cmd, ...)
{
	va_list	ap;

	va_start(ap, cmd);
	tty_vwrite_window(ptr, cmd, ap);
	va_end(ap);
}

void
tty_vwrite_window(void *ptr, enum tty_cmd cmd, va_list ap)
{
	struct window	*w = ptr;
	struct client	*c;
	va_list		 aq;
	u_int		 i;

	if (w->flags & WINDOW_HIDDEN)
		return;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		if (c->session->curw->window != w)
			continue;

		va_copy(aq, ap);
		tty_vwrite_client(c, cmd, aq);
		va_end(aq);
	}
}

void
tty_write_session(void *ptr, enum tty_cmd cmd, ...)
{
	va_list	ap;

	va_start(ap, cmd);
	tty_vwrite_session(ptr, cmd, ap);
	va_end(ap);
}

void
tty_vwrite_session(void *ptr, enum tty_cmd cmd, va_list ap)
{
	struct session	*s = ptr;
	struct client	*c;
	va_list		 aq;
	u_int		 i;

	if (s->flags & SESSION_UNATTACHED)
		return;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session != s)
			continue;

		va_copy(aq, ap);
		tty_vwrite_client(c, cmd, aq);
		va_end(aq);
	}
}
