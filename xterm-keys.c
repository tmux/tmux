/* $OpenBSD$ */

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
 * xterm-style function keys append one of the following values before the last
 * character:
 *
 * 2 Shift
 * 3 Alt
 * 4 Shift + Alt
 * 5 Ctrl
 * 6 Shift + Ctrl
 * 7 Alt + Ctrl
 * 8 Shift + Alt + Ctrl
 *
 * Rather than parsing them, just match against a table.
 *
 * There are three forms for F1-F4 (\\033O_P and \\033O1;_P and \\033[1;_P).
 * We accept any but always output the latter (it comes first in the table).
 */

int	xterm_keys_match(const char *, const char *, size_t, size_t *, u_int *);
int	xterm_keys_modifiers(const char *, size_t, size_t *, u_int *);

struct xterm_keys_entry {
	int		 key;
	const char	*template;
};

const struct xterm_keys_entry xterm_keys_table[] = {
	{ KEYC_F1,	"\033[1;_P" },
	{ KEYC_F1,	"\033O1;_P" },
	{ KEYC_F1,	"\033O_P" },
	{ KEYC_F2,	"\033[1;_Q" },
	{ KEYC_F2,	"\033O1;_Q" },
	{ KEYC_F2,	"\033O_Q" },
	{ KEYC_F3,	"\033[1;_R" },
	{ KEYC_F3,	"\033O1;_R" },
	{ KEYC_F3,	"\033O_R" },
	{ KEYC_F4,	"\033[1;_S" },
	{ KEYC_F4,	"\033O1;_S" },
	{ KEYC_F4,	"\033O_S" },
	{ KEYC_F5,	"\033[15;_~" },
	{ KEYC_F6,	"\033[17;_~" },
	{ KEYC_F7,	"\033[18;_~" },
	{ KEYC_F8,	"\033[19;_~" },
	{ KEYC_F9,	"\033[20;_~" },
	{ KEYC_F10,	"\033[21;_~" },
	{ KEYC_F11,	"\033[23;_~" },
	{ KEYC_F12,	"\033[24;_~" },
	{ KEYC_UP,	"\033[1;_A" },
	{ KEYC_DOWN,	"\033[1;_B" },
	{ KEYC_RIGHT,	"\033[1;_C" },
	{ KEYC_LEFT,	"\033[1;_D" },
	{ KEYC_HOME,	"\033[1;_H" },
	{ KEYC_END,	"\033[1;_F" },
	{ KEYC_PPAGE,	"\033[5;_~" },
	{ KEYC_NPAGE,	"\033[6;_~" },
	{ KEYC_IC,	"\033[2;_~" },
	{ KEYC_DC,	"\033[3;_~" },

	{ '!',          "\033[27;_;33~" },
	{ '#',		"\033[27;_;35~" },
	{ '(',		"\033[27;_;40~" },
	{ ')',		"\033[27;_;41~" },
	{ '+',		"\033[27;_;43~" },
	{ ',',		"\033[27;_;44~" },
	{ '-',		"\033[27;_;45~" },
	{ '.',		"\033[27;_;46~" },
	{ '0',		"\033[27;_;48~" },
	{ '1',		"\033[27;_;49~" },
	{ '2',		"\033[27;_;50~" },
	{ '3',		"\033[27;_;51~" },
	{ '4',		"\033[27;_;52~" },
	{ '5',		"\033[27;_;53~" },
	{ '6',		"\033[27;_;54~" },
	{ '7',		"\033[27;_;55~" },
	{ '8',		"\033[27;_;56~" },
	{ '9',		"\033[27;_;57~" },
	{ ':',		"\033[27;_;58~" },
	{ ';',		"\033[27;_;59~" },
	{ '<',		"\033[27;_;60~" },
	{ '=',		"\033[27;_;61~" },
	{ '>',		"\033[27;_;62~" },
	{ '?',		"\033[27;_;63~" },
	{ '\'',		"\033[27;_;39~" },
	{ '\r',		"\033[27;_;13~" },
	{ '\t',		"\033[27;_;9~" },
};

/*
 * Match key against buffer, treating _ as a wildcard. Return -1 for no match,
 * 0 for match, 1 if the end of the buffer is reached (need more data).
 */
int
xterm_keys_match(const char *template, const char *buf, size_t len,
    size_t *size, u_int *modifiers)
{
	size_t	pos;
	int	retval;

	*modifiers = 0;

	if (len == 0)
		return (0);

	pos = 0;
	do {
		if (*template == '_') {
			retval = xterm_keys_modifiers(buf, len, &pos,
			    modifiers);
			if (retval != 0)
				return (retval);
			continue;
		}
		if (buf[pos] != *template)
			return (-1);
		pos++;
	} while (*++template != '\0' && pos != len);

	if (*template != '\0')	/* partial */
		return (1);

	*size = pos;
	return (0);
}

/* Find modifiers from buffer. */
int
xterm_keys_modifiers(const char *buf, size_t len, size_t *pos, u_int *modifiers)
{
	u_int	flags;

	if (len - *pos < 2)
		return (1);

	if (buf[*pos] < '0' || buf[*pos] > '9')
		return (-1);
	flags = buf[(*pos)++] - '0';
	if (buf[*pos] >= '0' && buf[*pos] <= '9')
		flags = (flags * 10) + (buf[(*pos)++] - '0');
	flags -= 1;

	*modifiers = 0;
	if (flags & 1)
		*modifiers |= KEYC_SHIFT;
	if (flags & 2)
		*modifiers |= KEYC_ESCAPE;
	if (flags & 4)
		*modifiers |= KEYC_CTRL;
	if (flags & 8)
		*modifiers |= KEYC_ESCAPE;
	return (0);
}

/*
 * Lookup key from a buffer against the table. Returns 0 for found (and the
 * key), -1 for not found, 1 for partial match.
 */
int
xterm_keys_find(const char *buf, size_t len, size_t *size, int *key)
{
	const struct xterm_keys_entry	*entry;
	u_int				 i, modifiers;
	int				 matched;

	for (i = 0; i < nitems(xterm_keys_table); i++) {
		entry = &xterm_keys_table[i];

		matched = xterm_keys_match(entry->template, buf, len, size,
		    &modifiers);
		if (matched == -1)
			continue;
		if (matched == 0)
			*key = entry->key | modifiers;
		return (matched);
	}
	return (-1);
}

/* Lookup a key number from the table. */
char *
xterm_keys_lookup(int key)
{
	const struct xterm_keys_entry	*entry;
	u_int				 i;
	int				 modifiers;
	char				*out;

	modifiers = 1;
	if (key & KEYC_SHIFT)
		modifiers += 1;
	if (key & KEYC_ESCAPE)
		modifiers += 2;
	if (key & KEYC_CTRL)
		modifiers += 4;

	/*
	 * If the key has no modifiers, return NULL and let it fall through to
	 * the normal lookup.
	 */
	if (modifiers == 1)
		return (NULL);

	/* Otherwise, find the key in the table. */
	key &= ~(KEYC_SHIFT|KEYC_ESCAPE|KEYC_CTRL);
	for (i = 0; i < nitems(xterm_keys_table); i++) {
		entry = &xterm_keys_table[i];
		if (key == entry->key)
			break;
	}
	if (i == nitems(xterm_keys_table))
		return (NULL);

	/* Copy the template and replace the modifier. */
	out = xstrdup(entry->template);
	out[strcspn(out, "_")] = '0' + modifiers;
	return (out);
}
