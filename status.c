/* $Id: status.c,v 1.2 2007-10-03 23:32:26 nicm Exp $ */

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
	struct screen	*s = &c->session->window->screen;
	struct buffer	*b = c->out;
	struct window	*w;
	size_t		 size;
	u_int		 i;

	input_store_zero(b, CODE_CURSOROFF);
	input_store_two(b, CODE_CURSORMOVE, c->sy - status_lines + 1, 1);
	input_store_two(b, CODE_ATTRIBUTES, ATTR_REVERSE, 0x20);

	size = s->sx;
	for (i = 0; i < ARRAY_LENGTH(&c->session->windows); i++) {
		w = ARRAY_ITEM(&c->session->windows, i);
		if (w == NULL)
			continue;
		status_print(b, &size,
		    "%u:%s%s ", i, w->name, w == c->session->window ? "*" : "");
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
