/* $OpenBSD$ */

/*
 * Copyright (c) 2019 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <regex.h>
#include <string.h>

#include "tmux.h"

static void
regsub_copy(char **buf, size_t *len, const char *text, size_t start,
    size_t end)
{
	size_t	add = end - start;

	*buf = xrealloc(*buf, (*len) + add + 1);
	memcpy((*buf) + *len, text + start, add);
	(*len) += add;
}

static void
regsub_expand(char **buf, size_t *len, const char *with, const char *text,
    regmatch_t *m, u_int n)
{
	const char	*cp;
	u_int		 i;

	for (cp = with; *cp != '\0'; cp++) {
		if (*cp == '\\') {
			cp++;
			if (*cp >= '0' && *cp <= '9') {
				i = *cp - '0';
				if (i < n && m[i].rm_so != m[i].rm_eo) {
					regsub_copy(buf, len, text, m[i].rm_so,
					    m[i].rm_eo);
					continue;
				}
			}
		}
		*buf = xrealloc(*buf, (*len) + 2);
		(*buf)[(*len)++] = *cp;
	}
}

char *
regsub(const char *pattern, const char *with, const char *text, int flags)
{
	regex_t		 r;
	regmatch_t	 m[10];
	size_t		 start, end, len = 0;
	char		*buf = NULL;

	if (*text == '\0')
		return (xstrdup(""));
	if (regcomp(&r, pattern, flags) != 0)
		return (NULL);

	start = 0;
	end = strlen(text);

	while (start != end) {
		m[0].rm_so = start;
		m[0].rm_eo = end;

		if (regexec(&r, text, nitems(m), m, REG_STARTEND) != 0) {
			regsub_copy(&buf, &len, text, start, end);
			break;
		}
		if (m[0].rm_so == m[0].rm_eo) {
			regsub_copy(&buf, &len, text, start, end);
			break;
		}

		regsub_copy(&buf, &len, text, start, m[0].rm_so);
		regsub_expand(&buf, &len, with, text, m, nitems(m));
		start = m[0].rm_eo;
	}
	buf[len] = '\0';

	regfree(&r);
	return (buf);
}
