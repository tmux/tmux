/*
 * Copyright (c) 2016 Joshua Rubin <joshua@rubixconsulting.com>
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

#include <utf8proc.h>

#include "compat.h"

int
utf8proc_wcwidth(wchar_t wc)
{
	int	cat;

	cat = utf8proc_category(wc);
	if (cat == UTF8PROC_CATEGORY_CO) {
		/*
		 * The private use category is where powerline and similar
		 * codepoints are stored, they have "ambiguous" width - use 1.
		 */
		return (1);
	}
	return (utf8proc_charwidth(wc));
}

int
utf8proc_mbtowc(wchar_t *pwc, const char *s, size_t n)
{
	utf8proc_ssize_t	slen;

	if (s == NULL)
		return (0);

	/*
	 * *pwc == -1 indicates invalid codepoint
	 * slen < 0 indicates an error
	 */
	slen = utf8proc_iterate(s, n, pwc);
	if (*pwc == (wchar_t)-1 || slen < 0)
		return (-1);
	return (slen);
}

int
utf8proc_wctomb(char *s, wchar_t wc)
{
	if (s == NULL)
		return (0);

	if (!utf8proc_codepoint_valid(wc))
		return (-1);
	return (utf8proc_encode_char(wc, s));
}
