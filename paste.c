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
#include <sys/time.h>

#include <string.h>

#include "tmux.h"

void
paste_init_stack(struct paste_stack *ps)
{
	ARRAY_INIT(ps);
}

void
paste_free_stack(struct paste_stack *ps)
{
	while (paste_free_top(ps) == 0)
		;
}

struct paste_buffer *
paste_walk_stack(struct paste_stack *ps, uint *idx)
{
	struct paste_buffer	*pb;

	pb = paste_get_index(ps, *idx);
	(*idx)++;
	return (pb);
}

struct paste_buffer *
paste_get_top(struct paste_stack *ps)
{
	if (ARRAY_LENGTH(ps) == 0)
		return (NULL);
	return (ARRAY_FIRST(ps));
}

struct paste_buffer *
paste_get_index(struct paste_stack *ps, u_int idx)
{
	if (idx >= ARRAY_LENGTH(ps))
		return (NULL);
	return (ARRAY_ITEM(ps, idx));
}

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

void
paste_add(struct paste_stack *ps, char *data, u_int limit)
{
	struct paste_buffer	*pb;

	while (ARRAY_LENGTH(ps) >= limit)
		ARRAY_TRUNC(ps, 1);

	pb = xmalloc(sizeof *pb);
	ARRAY_INSERT(ps, 0, pb);

	pb->data = data;
	if (gettimeofday(&pb->tv, NULL) != 0)
		fatal("gettimeofday");
}

int
paste_replace(struct paste_stack *ps, u_int idx, char *data)
{
	struct paste_buffer	*pb;

	if (idx >= ARRAY_LENGTH(ps))
		return (-1);

	pb = ARRAY_ITEM(ps, idx);
	xfree(pb->data);

	pb->data = data;
	if (gettimeofday(&pb->tv, NULL) != 0)
		fatal("gettimeofday");

	return (0);
}
