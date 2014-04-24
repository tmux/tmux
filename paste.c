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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Stack of paste buffers. Note that paste buffer data is not necessarily a C
 * string!
 */

ARRAY_DECL(, struct paste_buffer *) paste_buffers =  ARRAY_INITIALIZER;

/* Return each item of the stack in turn. */
struct paste_buffer *
paste_walk_stack(u_int *idx)
{
	struct paste_buffer	*pb;

	pb = paste_get_index(*idx);
	(*idx)++;
	return (pb);
}

/* Get the top item on the stack. */
struct paste_buffer *
paste_get_top(void)
{
	if (ARRAY_LENGTH(&paste_buffers) == 0)
		return (NULL);
	return (ARRAY_FIRST(&paste_buffers));
}

/* Get an item by its index. */
struct paste_buffer *
paste_get_index(u_int idx)
{
	if (idx >= ARRAY_LENGTH(&paste_buffers))
		return (NULL);
	return (ARRAY_ITEM(&paste_buffers, idx));
}

/* Free the top item on the stack. */
int
paste_free_top(void)
{
	struct paste_buffer	*pb;

	if (ARRAY_LENGTH(&paste_buffers) == 0)
		return (-1);

	pb = ARRAY_FIRST(&paste_buffers);
	ARRAY_REMOVE(&paste_buffers, 0);

	free(pb->data);
	free(pb);

	return (0);
}

/* Free an item by index. */
int
paste_free_index(u_int idx)
{
	struct paste_buffer	*pb;

	if (idx >= ARRAY_LENGTH(&paste_buffers))
		return (-1);

	pb = ARRAY_ITEM(&paste_buffers, idx);
	ARRAY_REMOVE(&paste_buffers, idx);

	free(pb->data);
	free(pb);

	return (0);
}

/*
 * Add an item onto the top of the stack, freeing the bottom if at limit. Note
 * that the caller is responsible for allocating data.
 */
void
paste_add(char *data, size_t size, u_int limit)
{
	struct paste_buffer	*pb;

	if (size == 0)
		return;

	while (ARRAY_LENGTH(&paste_buffers) >= limit) {
		pb = ARRAY_LAST(&paste_buffers);
		free(pb->data);
		free(pb);
		ARRAY_TRUNC(&paste_buffers, 1);
	}

	pb = xmalloc(sizeof *pb);
	ARRAY_INSERT(&paste_buffers, 0, pb);

	pb->data = data;
	pb->size = size;
}


/*
 * Replace an item on the stack. Note that the caller is responsible for
 * allocating data.
 */
int
paste_replace(u_int idx, char *data, size_t size)
{
	struct paste_buffer	*pb;

	if (size == 0) {
		free(data);
		return (0);
	}

	if (idx >= ARRAY_LENGTH(&paste_buffers))
		return (-1);

	pb = ARRAY_ITEM(&paste_buffers, idx);
	free(pb->data);

	pb->data = data;
	pb->size = size;

	return (0);
}

/* Convert start of buffer into a nice string. */
char *
paste_make_sample(struct paste_buffer *pb, int utf8flag)
{
	char		*buf;
	size_t		 len, used;
	const int	 flags = VIS_OCTAL|VIS_TAB|VIS_NL;
	const size_t	 width = 200;

	len = pb->size;
	if (len > width)
		len = width;
	buf = xmalloc(len * 4 + 4);

	if (utf8flag)
		used = utf8_strvis(buf, pb->data, len, flags);
	else
		used = strvisx(buf, pb->data, len, flags);
	if (pb->size > width || used > width)
		strlcpy(buf + width, "...", 4);
	return (buf);
}

/* Paste into a window pane, filtering '\n' according to separator. */
void
paste_send_pane(struct paste_buffer *pb, struct window_pane *wp,
    const char *sep, int bracket)
{
	const char	*data = pb->data, *end = data + pb->size, *lf;
	size_t		 seplen;

	if (bracket)
		bufferevent_write(wp->event, "\033[200~", 6);

	seplen = strlen(sep);
	while ((lf = memchr(data, '\n', end - data)) != NULL) {
		if (lf != data)
			bufferevent_write(wp->event, data, lf - data);
		bufferevent_write(wp->event, sep, seplen);
		data = lf + 1;
	}

	if (end != data)
		bufferevent_write(wp->event, data, end - data);

	if (bracket)
		bufferevent_write(wp->event, "\033[201~", 6);
}
