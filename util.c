/* $OpenBSD: util.c,v 1.2 2009/06/03 19:37:27 nicm Exp $ */

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

#include "tmux.h"

/* Return a section of a string around a point. */
char *
section_string(char *buf, size_t len, size_t sectoff, size_t sectlen)
{
	char	*s;
	size_t	 first, last;

	if (len <= sectlen) {
		first = 0;
		last = len;
	} else if (sectoff < sectlen / 2) {
		first = 0;
		last = sectlen;
	} else if (sectoff + sectlen / 2 > len) {
		last = len;
		first = last - sectlen;
	} else {
		first = sectoff - sectlen / 2;
		last = first + sectlen;
	}

	if (last - first > 3 && first != 0)
		first += 3;
	if (last - first > 3 && last != len)
		last -= 3;

	xasprintf(&s, "%s%.*s%s", first == 0 ? "" : "...",
	    (int) (last - first), buf + first, last == len ? "" : "...");
	return (s);
}
