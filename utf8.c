/* $Id: utf8.c,v 1.1 2008-09-09 22:16:37 nicm Exp $ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicm@users.sourceforge.net>
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
 * UTF8 data structures. Just crappy array + linear search for now.
 */

/* Pack UTF8 index into attr, data. */
void
utf8_pack(int idx, u_char *data, u_short *attr)
{
	*data = idx & 0xff;

	*attr &= ~(ATTR_UTF8b8|ATTR_UTF8b9);
	if (idx & 0x100)
		*attr |= ATTR_UTF8b8;
	if (idx & 0x200)
		*attr |= ATTR_UTF8b9;
	if (idx & 0x400)
		*attr |= ATTR_UTF8b10;
	if (idx & 0x800)
		*attr |= ATTR_UTF8b11;
}

/* Unpack UTF8 index from attr, data. */
int
utf8_unpack(u_char data, u_short attr)
{
	int	idx;

	idx = data;
	if (attr & ATTR_UTF8b8)
		idx |= 0x100;
	if (attr & ATTR_UTF8b9)
		idx |= 0x200;
	if (attr & ATTR_UTF8b10)
		idx |= 0x400;
	if (attr & ATTR_UTF8b11)
		idx |= 0x800;

	return (idx);
}

void
utf8_init(struct utf8_table *utab, int limit)
{
	utab->limit = limit;
	ARRAY_INIT(&utab->array);
}

void
utf8_free(struct utf8_table *utab)
{
	ARRAY_FREE(&utab->array);
}

struct utf8_data *
utf8_lookup(struct utf8_table *utab, int idx)
{
	if (idx < 0 || idx >= (int) ARRAY_LENGTH(&utab->array))
		return (NULL);
	return (&ARRAY_ITEM(&utab->array, idx));
}

int
utf8_search(struct utf8_table *utab, struct utf8_data *udat)
{
	u_int	idx;

	for (idx = 0; idx < ARRAY_LENGTH(&utab->array); idx++) {
		if (memcmp(udat->data,
		    ARRAY_ITEM(&utab->array, idx).data, sizeof udat->data) == 0)
			return (idx);
	}
	return (-1);
}

int
utf8_add(struct utf8_table *utab, struct utf8_data *udat)
{
	int	idx;

	if (ARRAY_LENGTH(&utab->array) == utab->limit)
		return (-1);

	if ((idx = utf8_search(utab, udat)) != -1)
		return (idx);

	ARRAY_EXPAND(&utab->array, 1);
	memcpy(
	    &ARRAY_LAST(&utab->array), udat, sizeof ARRAY_LAST(&utab->array));
	return (ARRAY_LENGTH(&utab->array) - 1);
}
