/* $Id$ */

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
#include <sys/time.h>

#include <string.h>

#include "tmux.h"

/*
 * Stack of paste buffers. Note that paste buffer data is not necessarily a C
 * string!
 */

/* Return each item of the stack in turn. */
struct paste_buffer *
paste_walk_stack(struct paste_stack *ps, u_int *idx)
{
	struct paste_buffer	*pb;

	pb = paste_get_index(ps, *idx);
	(*idx)++;
	return (pb);
}

/* Get the top item on the stack. */
struct paste_buffer *
paste_get_top(struct paste_stack *ps)
{
	if (ARRAY_LENGTH(ps) == 0)
		return (NULL);
	return (ARRAY_FIRST(ps));
}

/* Get an item by its index. */
struct paste_buffer *
paste_get_index(struct paste_stack *ps, u_int idx)
{
	if (idx >= ARRAY_LENGTH(ps))
		return (NULL);
	return (ARRAY_ITEM(ps, idx));
}

/* Free the top item on the stack. */
int
paste_free_top(struct paste_stack *ps)
{
	struct paste_buffer	*pb;

	if (ARRAY_LENGTH(ps) == 0)
		return (-1);

	pb = ARRAY_FIRST(ps);
	ARRAY_REMOVE(ps, 0);

	xfree(pb->data);
	xfree(pb);

	return (0);
}

/* Free an item by index. */
int
paste_free_index(struct paste_stack *ps, u_int idx)
{
	struct paste_buffer	*pb;

	if (idx >= ARRAY_LENGTH(ps))
		return (-1);

	pb = ARRAY_ITEM(ps, idx);
	ARRAY_REMOVE(ps, idx);

	xfree(pb->data);
	xfree(pb);

	return (0);
}

/*
 * Add an item onto the top of the stack, freeing the bottom if at limit. Note
 * that the caller is responsible for allocating data.
 */
void
paste_add(struct paste_stack *ps, char *data, size_t size, u_int limit)
{
	struct paste_buffer	*pb;

	if (size == 0)
		return;

	while (ARRAY_LENGTH(ps) >= limit) {
		pb = ARRAY_LAST(ps);
		xfree(pb->data);
		xfree(pb);
		ARRAY_TRUNC(ps, 1);
	}

	pb = xmalloc(sizeof *pb);
	ARRAY_INSERT(ps, 0, pb);

	pb->data = data;
	pb->size = size;
}


/*
 * Replace an item on the stack. Note that the caller is responsible for
 * allocating data.
 */
int
paste_replace(struct paste_stack *ps, u_int idx, char *data, size_t size)
{
	struct paste_buffer	*pb;

	if (size == 0)
		return (0);

	if (idx >= ARRAY_LENGTH(ps))
		return (-1);

	pb = ARRAY_ITEM(ps, idx);
	xfree(pb->data);

	pb->data = data;
	pb->size = size;

	return (0);
}

/* Convert a buffer into a visible string. */
char *
paste_print(struct paste_buffer *pb, size_t width)
{
	char	*buf;
	size_t	 len, used;

	if (width < 3)
		width = 3;
	buf = xmalloc(width * 4 + 1);

	len = pb->size;
	if (len > width)
		len = width;

	used = strvisx(buf, pb->data, len, VIS_OCTAL|VIS_TAB|VIS_NL);
	if (pb->size > width || used > width) {
		buf[width - 3] = '\0';
		strlcat(buf, "...", width);
	}

	return (buf);
}
