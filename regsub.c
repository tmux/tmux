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
regsub_copy(char **buf, ssize_t *len, const char *text, size_t start, size_t end)
{
	size_t	add = end - start;

	*buf = xrealloc(*buf, (*len) + add + 1);
	memcpy((*buf) + *len, text + start, add);
	(*len) += add;
}

static void
regsub_expand(char **buf, ssize_t *len, const char *with, const char *text,
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
	ssize_t		 start, end, last, len = 0;
	int		 empty = 0;
	char		*buf = NULL;

	if (*text == '\0')
		return (xstrdup(""));
	if (regcomp(&r, pattern, flags) != 0)
		return (NULL);

	start = 0;
	last = 0;
	end = strlen(text);

	while (start <= end) {
		if (regexec(&r, text + start, nitems(m), m, 0) != 0) {
			regsub_copy(&buf, &len, text, start, end);
			break;
		}

		/*
		 * Append any text not part of this match (from the end of the
		 * last match).
		 */
		regsub_copy(&buf, &len, text, last, m[0].rm_so + start);

		/*
		 * If the last match was empty and this one isn't (it is either
		 * later or has matched text), expand this match. If it is
		 * empty, move on one character and try again from there.
		 */
		if (empty ||
		    start + m[0].rm_so != last ||
		    m[0].rm_so != m[0].rm_eo) {
			regsub_expand(&buf, &len, with, text + start, m,
			    nitems(m));

			last = start + m[0].rm_eo;
			start += m[0].rm_eo;
			empty = 0;
		} else {
			last = start + m[0].rm_eo;
			start += m[0].rm_eo + 1;
			empty = 1;
		}

		/* Stop now if anchored to start. */
		if (*pattern == '^') {
			regsub_copy(&buf, &len, text, start, end);
			break;
		}
	}
	buf[len] = '\0';

	regfree(&r);
	return (buf);
}
