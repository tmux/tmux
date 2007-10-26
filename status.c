/* $Id: status.c,v 1.7 2007-10-26 12:29:07 nicm Exp $ */

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

#include "tmux.h"

void	status_print(struct buffer *, size_t *, const char *, ...);

void
status_write(struct client *c)
{
	struct screen	*s = &c->session->curw->window->screen;
	struct buffer	*b = c->out;
	struct winlink	*wl;
	size_t		 size;
	char		 flag;

	input_store_zero(b, CODE_CURSOROFF);
	input_store_two(b, CODE_CURSORMOVE, c->sy - status_lines + 1, 1);
	input_store_two(b, CODE_ATTRIBUTES, 0, status_colour);

	size = c->sx;
	RB_FOREACH(wl, winlinks, &c->session->windows) {
		flag = ' ';
		if (wl == c->session->lastw)
			flag = '-';
		if (wl == c->session->curw)
			flag = '*';
		if (session_hasbell(c->session, wl))
			flag = '!';
		status_print(
		    b, &size, "%u:%s%c ", wl->idx, wl->window->name, flag);

		if (size == 0)
			break;
	}
	while (size-- > 0)
		input_store8(b, ' ');

	input_store_two(b, CODE_ATTRIBUTES, s->attr, s->colr);
	input_store_two(b, CODE_CURSORMOVE, s->cy + 1, s->cx + 1);
	if (s->mode & MODE_CURSOR)
		input_store_zero(b, CODE_CURSORON);
}

void
status_print(struct buffer *b, size_t *size, const char *fmt, ...)
{
	va_list	ap;
	char   *msg, *ptr;
	int	n;

	va_start(ap, fmt);
	n = xvasprintf(&msg, fmt, ap);
	va_end(ap);

	if ((size_t) n > *size) {
		msg[*size] = '\0';
		n = *size;
	}

	for (ptr = msg; *ptr != '\0'; ptr++)
		input_store8(b, *ptr);
	(*size) -= n;

	xfree(msg);
}
