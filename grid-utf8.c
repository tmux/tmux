/* $Id$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicm@users.sourceforge.net>
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

/*
 * Grid UTF-8 utility functions.
 */

/* Calculate UTF-8 grid cell size. Data is terminated by 0xff. */
size_t
grid_utf8_size(const struct grid_utf8 *gu)
{
	size_t	size;

	for (size = 0; size < sizeof gu->data; size++) {
		if (gu->data[size] == 0xff)
			break;
	}
	return (size);
}

/* Copy UTF-8 out into a buffer. */
size_t
grid_utf8_copy(const struct grid_utf8 *gu, char *buf, size_t len)
{
	size_t	size;

	size = grid_utf8_size(gu);
	if (size > len)
		fatalx("UTF-8 copy overflow");
	memcpy(buf, gu->data, size);
	return (size);
}

/* Set UTF-8 grid data from input UTF-8. */
void
grid_utf8_set(struct grid_utf8 *gu, const struct utf8_data *utf8data)
{
	if (utf8data->size == 0)
		fatalx("UTF-8 data empty");
	if (utf8data->size > sizeof gu->data)
		fatalx("UTF-8 data too long");
	memcpy(gu->data, utf8data->data, utf8data->size);
	if (utf8data->size != sizeof gu->data)
		gu->data[utf8data->size] = 0xff;
	gu->width = utf8data->width;
}

/* Append UTF-8 character onto the cell data (for combined characters). */
int
grid_utf8_append(struct grid_utf8 *gu, const struct utf8_data *utf8data)
{
	size_t	old_size;

	old_size = grid_utf8_size(gu);
	if (old_size + utf8data->size > sizeof gu->data)
		return (-1);
	memcpy(gu->data + old_size, utf8data->data, utf8data->size);
	if (old_size + utf8data->size != sizeof gu->data)
		gu->data[old_size + utf8data->size] = 0xff;
	return (0);
}

/* Compare two UTF-8 cells. */
int
grid_utf8_compare(const struct grid_utf8 *gu1, const struct grid_utf8 *gu2)
{
	size_t	size;

	size = grid_utf8_size(gu1);
	if (size != grid_utf8_size(gu2))
		return (0);
	if (memcmp(gu1->data, gu2->data, size) != 0)
		return (0);
	return (1);
}
