/* $Id: utf8.c,v 1.2 2008-09-25 20:08:56 nicm Exp $ */

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

u_int
utf8_combine(const u_char data[4])
{
	u_int	uv;

	if (data[1] == 0xff)
		uv = data[0];
	else if (data[2] == 0xff) {
		uv = data[1] & 0x3f;
		uv |= (data[0] & 0x1f) << 6;
	} else if (data[3] == 0xff) {
		uv = data[2] & 0x3f;
		uv |= (data[1] & 0x3f) << 6;
		uv |= (data[0] & 0x0f) << 12;
	} else {
		uv = data[3] & 0x3f;
		uv |= (data[2] & 0x3f) << 6;
		uv |= (data[1] & 0x3f) << 12;
		uv |= (data[0] & 0x3f) << 18;
	}
	return (uv);
}

void
utf8_split(u_int uv, u_char data[4])
{
	memset(data, 0xff, sizeof data);

	if (uv <= 0x7f)
		data[0] = uv;
	else if (uv > 0x7f && uv <= 0x7ff) {
		data[0] = (uv >> 6) | 0xc0;
		data[1] = (uv & 0x3f) | 0x80;
	} else if (uv > 0x7ff && uv <= 0xffff) {
		data[0] = (uv >> 12) | 0xe0;
		data[1] = ((uv >> 6) & 0x3f) | 0x80;
		data[2] = (uv & 0x3f) | 0x80;
	} else if (uv > 0xffff && uv <= 0x10ffff) {
		data[0] = (uv >> 18) | 0xf0;
		data[1] = ((uv >> 12) & 0x3f) | 0x80;
		data[2] = ((uv >> 6) & 0x3f) | 0x80;
		data[3] = (uv & 0x3f) | 0x80;
	}
}

int
utf8_width(u_int uv)
{
	if ((uv >= 0x1100 && uv <= 0x115f) ||
	    uv == 0x2329 ||
	    uv == 0x232a ||
	    (uv >= 0x2e80 && uv <= 0xa4cf && uv != 0x303f) ||
	    (uv >= 0xac00 && uv <= 0xd7a3) ||
	    (uv >= 0xf900 && uv <= 0xfaff) ||
	    (uv >= 0xfe10 && uv <= 0xfe19) ||
	    (uv >= 0xfe30 && uv <= 0xfe6f) ||
	    (uv >= 0xff00 && uv <= 0xff60) ||
	    (uv >= 0xffe0 && uv <= 0xffe6) ||
	    (uv >= 0x20000 && uv <= 0x2fffd) ||
	    (uv >= 0x30000 && uv <= 0x3fffd))
		return (2);
	return (1);
}
