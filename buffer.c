/* $OpenBSD: buffer.c,v 1.2 2009/06/25 06:05:47 nicm Exp $ */

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

#include <string.h>

#include "tmux.h"

/* Create a buffer. */
struct buffer *
buffer_create(size_t size)
{
	struct buffer	*b;

	if (size == 0)
		fatalx("zero size");

	b = xcalloc(1, sizeof *b);

	b->base = xmalloc(size);
	b->space = size;

	return (b);
}

/* Destroy a buffer. */
void
buffer_destroy(struct buffer *b)
{
	xfree(b->base);
	xfree(b);
}

/* Ensure free space for size in buffer. */
void
buffer_ensure(struct buffer *b, size_t size)
{
	if (size == 0)
		fatalx("zero size");

	if (BUFFER_FREE(b) >= size)
		return;

	if (b->off > 0) {
		if (b->size > 0)
			memmove(b->base, b->base + b->off, b->size);
		b->off = 0;
	}

	if (SIZE_MAX - b->size < size)
		fatalx("size too big");
	while (b->space < b->size + size) {
		b->base = xrealloc(b->base, 2, b->space);
		b->space *= 2;
	}
}

/* Adjust buffer after data appended. */
void
buffer_add(struct buffer *b, size_t size)
{
	if (size == 0)
		fatalx("zero size");
	if (size > b->space - b->size)
		fatalx("overflow");

	b->size += size;
}

/* Adjust buffer after data removed. */
void
buffer_remove(struct buffer *b, size_t size)
{
	if (size == 0)
		fatalx("zero size");
	if (size > b->size)
		fatalx("underflow");

	b->size -= size;
	b->off += size;
}

/* Copy data into a buffer. */
void
buffer_write(struct buffer *b, const void *data, size_t size)
{
	if (size == 0)
		fatalx("zero size");

	buffer_ensure(b, size);
	memcpy(BUFFER_IN(b), data, size);
	buffer_add(b, size);
}

/* Copy data out of a buffer. */
void
buffer_read(struct buffer *b, void *data, size_t size)
{
	if (size == 0)
		fatalx("zero size");
	if (size > b->size)
		fatalx("underflow");

	memcpy(data, BUFFER_OUT(b), size);
	buffer_remove(b, size);
}

/* Store an 8-bit value. */
void
buffer_write8(struct buffer *b, uint8_t n)
{
	buffer_ensure(b, 1);
	BUFFER_IN(b)[0] = n;
	buffer_add(b, 1);
}

/* Extract an 8-bit value. */
uint8_t
buffer_read8(struct buffer *b)
{
	uint8_t	n;

	n = BUFFER_OUT(b)[0];
	buffer_remove(b, 1);
	return (n);
}
