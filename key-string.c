/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
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

int	key_string_search_table(const char *);

struct {
	const char *string;
	int	 key;
} key_string_table[] = {
	/* Function keys. */
	{ "F1",		KEYC_F1 },
	{ "F2",		KEYC_F2 },
	{ "F3",		KEYC_F3 },
	{ "F4",		KEYC_F4 },
	{ "F5",		KEYC_F5 },
	{ "F6",		KEYC_F6 },
	{ "F7",		KEYC_F7 },
	{ "F8",		KEYC_F8 },
	{ "F9",		KEYC_F9 },
	{ "F10",	KEYC_F10 },
	{ "F11",	KEYC_F11 },
	{ "F12",	KEYC_F12 },
	{ "F13",	KEYC_F13 },
	{ "F14",	KEYC_F14 },
	{ "F15",	KEYC_F15 },
	{ "F16",	KEYC_F16 },
	{ "F17",	KEYC_F17 },
	{ "F18",	KEYC_F18 },
	{ "F19",	KEYC_F19 },
	{ "F20",	KEYC_F20 },
	{ "IC",		KEYC_IC },
	{ "DC",		KEYC_DC },
	{ "Home",	KEYC_HOME },
	{ "End",	KEYC_END },
	{ "NPage",	KEYC_NPAGE },
	{ "PPage",	KEYC_PPAGE },
	{ "Tab",	'\011' },
	{ "BTab",	KEYC_BTAB },

	/* Arrow keys. */
	{ "Up",		KEYC_UP },
	{ "Down",	KEYC_DOWN },
	{ "Left",	KEYC_LEFT },
	{ "Right",	KEYC_RIGHT },

	/* Numeric keypad. */
	{ "KP/", 	KEYC_KP0_1 },
	{ "KP*",	KEYC_KP0_2 },
	{ "KP-",	KEYC_KP0_3 },
	{ "KP7",	KEYC_KP1_0 },
	{ "KP8",	KEYC_KP1_1 },
	{ "KP9",	KEYC_KP1_2 },
	{ "KP+",	KEYC_KP1_3 },
	{ "KP4",	KEYC_KP2_0 },
	{ "KP5",	KEYC_KP2_1 },
	{ "KP6",	KEYC_KP2_2 },
	{ "KP1",	KEYC_KP3_0 },
	{ "KP2",	KEYC_KP3_1 },
	{ "KP3",	KEYC_KP3_2 },
	{ "KPEnter",	KEYC_KP3_3 },
	{ "KP0",	KEYC_KP4_0 },
	{ "KP.",	KEYC_KP4_2 },
};

int
key_string_search_table(const char *string)
{
	u_int	i;

	for (i = 0; i < nitems(key_string_table); i++) {
		if (strcasecmp(string, key_string_table[i].string) == 0)
			return (key_string_table[i].key);
	}
	return (KEYC_NONE);
}

int
key_string_lookup_string(const char *string)
{
	int	      	 key;
	const u_char	*ptr;

	if (string[0] == '\0')
		return (KEYC_NONE);
	if (string[1] == '\0')
		return (string[0]);

	ptr = NULL;
	if (string[0] == 'C' && string[1] == '-')
		ptr = string + 2;
	else if (string[0] == '^')
		ptr = string + 1;
	if (ptr != NULL) {
		if (ptr[0] == '\0')
			return (KEYC_NONE);
		if (ptr[1] == '\0') {
			if (ptr[0] == 32)
				return (0);
			if (ptr[0] >= 64 && ptr[0] <= 95)
				return (ptr[0] - 64);
			if (ptr[0] >= 97 && ptr[0] <= 122)
				return (ptr[0] - 96);
			return (KEYC_NONE);
		}
		key = key_string_search_table(ptr);
		if (key != KEYC_NONE)
			return (KEYC_ADDCTL(key));
		return (KEYC_NONE);
	}

	if (string[0] == 'M' && string[1] == '-') {
		ptr = string + 2;
		if (ptr[0] == '\0')
			return (KEYC_NONE);
		if (ptr[1] == '\0') {
			if (ptr[0] < 32 || ptr[0] > 127)
				return (KEYC_NONE);
			return (KEYC_ADDESC(ptr[0]));
		}
		key = key_string_lookup_string(ptr);
		if (key != KEYC_NONE)
			return (KEYC_ADDESC(key));
		return (KEYC_NONE);
	}

	return (key_string_search_table(string));
}

const char *
key_string_lookup_key(int key)
{
	static char tmp[24], tmp2[24];
	const char *s;
	u_int	    i;

	if (key == 127)
		return (NULL);

	if (KEYC_ISESC(key)) {
		if ((s = key_string_lookup_key(KEYC_REMOVEESC(key))) == NULL)
			return (NULL);
		xsnprintf(tmp2, sizeof tmp2, "M-%s", s);
		return (tmp2);
	}
	if (KEYC_ISCTL(key)) {
		if ((s = key_string_lookup_key(KEYC_REMOVECTL(key))) == NULL)
			return (NULL);
		xsnprintf(tmp2, sizeof tmp2, "C-%s", s);
		return (tmp2);
	}
	if (KEYC_ISSFT(key)) {
		if ((s = key_string_lookup_key(KEYC_REMOVESFT(key))) == NULL)
			return (NULL);
		xsnprintf(tmp2, sizeof tmp2, "S-%s", s);
		return (tmp2);
	}

	if (key >= 32 && key <= 255) {
		tmp[0] = key;
		tmp[1] = '\0';
		return (tmp);
	}

	if (key >= 0 && key <= 32) {
		if (key == 0 || key > 26)
			xsnprintf(tmp, sizeof tmp, "C-%c", 64 + key);
		else
			xsnprintf(tmp, sizeof tmp, "C-%c", 96 + key);
		return (tmp);
	}

	for (i = 0; i < nitems(key_string_table); i++) {
		if (key == key_string_table[i].key)
			return (key_string_table[i].string);
	}
	return (NULL);
}
