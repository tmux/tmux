/* $Id$ */

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

int	xterm_keys_match(const char *, const char *, size_t);
int	xterm_keys_modifiers(const char *, const char *, size_t);

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
	{ KEYC_F13,	"\033[25;_~" },
	{ KEYC_F14,	"\033[26;_~" },
	{ KEYC_F15,	"\033[28;_~" },
	{ KEYC_F16,	"\033[29;_~" },
	{ KEYC_F17,	"\033[31;_~" },
	{ KEYC_F18,	"\033[32;_~" },
	{ KEYC_F19,	"\033[33;_~" },
	{ KEYC_F20,	"\033[34;_~" },
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
};

/*
 * Match key against buffer, treating _ as a wildcard. Return -1 for no match,
 * 0 for match, 1 if the end of the buffer is reached (need more data).
 */
int
xterm_keys_match(const char *template, const char *buf, size_t len)
{
	size_t	pos;

	if (len == 0)
		return (0);

	pos = 0;
	do {
		if (*template != '_' && buf[pos] != *template)
			return (-1);
	} while (pos++ != len && *++template != '\0');

	if (*template != '\0')	/* partial */
		return (1);

	return (0);
}

/* Find modifiers based on template. */
int
xterm_keys_modifiers(const char *template, const char *buf, size_t len)
{
	size_t	idx;
	int     param, modifiers;

	idx = strcspn(template, "_");
	if (idx >= len)
		return (0);
	param = buf[idx] - '1';

	modifiers = 0;
	if (param & 1)
		modifiers |= KEYC_SHIFT;
	if (param & 2)
		modifiers |= KEYC_ESCAPE;
	if (param & 4)
		modifiers |= KEYC_CTRL;
	if (param & 8)
		modifiers |= KEYC_ESCAPE;
	return (modifiers);
}

/*
 * Lookup key from a buffer against the table. Returns 0 for found (and the
 * key), -1 for not found, 1 for partial match.
 */
int
xterm_keys_find(const char *buf, size_t len, size_t *size, int *key)
{
	const struct xterm_keys_entry	*entry;
	u_int				 i;

	for (i = 0; i < nitems(xterm_keys_table); i++) {
		entry = &xterm_keys_table[i];
		switch (xterm_keys_match(entry->template, buf, len)) {
		case 0:
			*size = strlen(entry->template);
			*key = entry->key;
			*key |= xterm_keys_modifiers(entry->template, buf, len);
			return (0);
		case 1:
			return (1);
		}
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
