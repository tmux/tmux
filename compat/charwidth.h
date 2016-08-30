#ifndef _CHARWIDTH_H_
#define _CHARWIDTH_H_

#include <utf8proc.h>

int
wcwidth(wchar_t wc)
{
	const int cat = utf8proc_category(wc);
	if (cat == UTF8PROC_CATEGORY_CO)
		// private use category is where powerline and
		// similar codepoints are stored, they have
		// "ambiguous" widths, so use 1
		return (1);

	if (cat == UTF8PROC_CATEGORY_SO)
		// symbols, like emoji, should always use width 1
		return (1);

	return utf8proc_charwidth(wc);
}

int
mbtowc(wchar_t *pwc, const char *s, size_t n)
{
	if (s == NULL)
		return (0);

	const utf8proc_ssize_t slen = utf8proc_iterate(s, n, pwc);
	// *pwc == -1 indicates invalid codepoint
	// slen < 0 indicates an error
	if (*pwc == -1 || slen < 0)
		return (-1);

	return slen;
}

int
wctomb(char *s, wchar_t wchar)
{
	if (s == NULL)
		return (0);

	if (!utf8proc_codepoint_valid(wchar))
		return (-1);

	return utf8proc_encode_char(wchar, s);
}
#endif
