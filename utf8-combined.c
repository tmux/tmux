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
	switch (wc) {
	case 0x1F1E6:
	case 0x1F1E7:
	case 0x1F1E8:
	case 0x1F1E9:
	case 0x1F1EA:
	case 0x1F1EB:
	case 0x1F1EC:
	case 0x1F1ED:
	case 0x1F1EE:
	case 0x1F1EF:
	case 0x1F1F0:
	case 0x1F1F1:
	case 0x1F1F2:
	case 0x1F1F3:
	case 0x1F1F4:
	case 0x1F1F5:
	case 0x1F1F6:
	case 0x1F1F7:
	case 0x1F1F8:
	case 0x1F1F9:
	case 0x1F1FA:
	case 0x1F1FB:
	case 0x1F1FC:
	case 0x1F1FD:
	case 0x1F1FE:
	case 0x1F1FF:
	case 0x1F3FB:
	case 0x1F3FC:
	case 0x1F3FD:
	case 0x1F3FE:
	case 0x1F3FF:
		return (1);
	}
	return (0);
}
