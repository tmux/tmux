/* $OpenBSD$ */

/*
 * Copyright (c) 2023 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "tmux.h"

static const wchar_t utf8_modifier_table[] = {
	0x1F1E6,
	0x1F1E7,
	0x1F1E8,
	0x1F1E9,
	0x1F1EA,
	0x1F1EB,
	0x1F1EC,
	0x1F1ED,
	0x1F1EE,
	0x1F1EF,
	0x1F1F0,
	0x1F1F1,
	0x1F1F2,
	0x1F1F3,
	0x1F1F4,
	0x1F1F5,
	0x1F1F6,
	0x1F1F7,
	0x1F1F8,
	0x1F1F9,
	0x1F1FA,
	0x1F1FB,
	0x1F1FC,
	0x1F1FD,
	0x1F1FE,
	0x1F1FF,
	0x1F3FB,
	0x1F3FC,
	0x1F3FD,
	0x1F3FE,
	0x1F3FF
};

/* Has this got a zero width joiner at the end? */
int
utf8_has_zwj(const struct utf8_data *ud)
{
	if (ud->size < 3)
		return (0);
	return (memcmp(ud->data + ud->size - 3, "\342\200\215", 3) == 0);
}

/* Is this a zero width joiner? */
int
utf8_is_zwj(const struct utf8_data *ud)
{
	if (ud->size != 3)
		return (0);
	return (memcmp(ud->data, "\342\200\215", 3) == 0);
}

/* Is this a variation selector? */
int
utf8_is_vs(const struct utf8_data *ud)
{
	if (ud->size != 3)
		return (0);
	return (memcmp(ud->data, "\357\270\217", 3) == 0);
}

/* Is this in the modifier table? */
int
utf8_is_modifier(const struct utf8_data *ud)
{
	wchar_t	wc;

	if (utf8_towc(ud, &wc) != UTF8_DONE)
		return (0);
	if (!utf8_in_table(wc, utf8_modifier_table,
	    nitems(utf8_modifier_table)))
		return (0);
	return (1);
}
