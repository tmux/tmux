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
int	key_string_get_modifiers(const char **);

const struct {
	const char     *string;
	int	 	key;
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
	{ "IC",		KEYC_IC },
	{ "DC",		KEYC_DC },
	{ "Home",	KEYC_HOME },
	{ "End",	KEYC_END },
	{ "NPage",	KEYC_NPAGE },
	{ "PageDown",	KEYC_NPAGE },
	{ "PgDn",	KEYC_NPAGE },
	{ "PPage",	KEYC_PPAGE },
	{ "PageUp",	KEYC_PPAGE },
	{ "PgUp",	KEYC_PPAGE },
	{ "Tab",	'\011' },
	{ "BTab",	KEYC_BTAB },
	{ "Space",	' ' },
	{ "BSpace",	KEYC_BSPACE },
	{ "Enter",	'\r' },
	{ "Escape",	'\033' },

	/* Arrow keys. */
	{ "Up",		KEYC_UP },
	{ "Down",	KEYC_DOWN },
	{ "Left",	KEYC_LEFT },
	{ "Right",	KEYC_RIGHT },

	/* Numeric keypad. */
	{ "KP/", 	KEYC_KP_SLASH },
	{ "KP*",	KEYC_KP_STAR },
	{ "KP-",	KEYC_KP_MINUS },
	{ "KP7",	KEYC_KP_SEVEN },
	{ "KP8",	KEYC_KP_EIGHT },
	{ "KP9",	KEYC_KP_NINE },
	{ "KP+",	KEYC_KP_PLUS },
	{ "KP4",	KEYC_KP_FOUR },
	{ "KP5",	KEYC_KP_FIVE },
	{ "KP6",	KEYC_KP_SIX },
	{ "KP1",	KEYC_KP_ONE },
	{ "KP2",	KEYC_KP_TWO },
	{ "KP3",	KEYC_KP_THREE },
	{ "KPEnter",	KEYC_KP_ENTER },
	{ "KP0",	KEYC_KP_ZERO },
	{ "KP.",	KEYC_KP_PERIOD },

	/* Mouse keys. */
	KEYC_MOUSE_STRING(MOUSEDOWN1, MouseDown1),
	KEYC_MOUSE_STRING(MOUSEDOWN2, MouseDown2),
	KEYC_MOUSE_STRING(MOUSEDOWN3, MouseDown3),
	KEYC_MOUSE_STRING(MOUSEUP1, MouseUp1),
	KEYC_MOUSE_STRING(MOUSEUP2, MouseUp2),
	KEYC_MOUSE_STRING(MOUSEUP3, MouseUp3),
	KEYC_MOUSE_STRING(MOUSEDRAG1, MouseDrag1),
	KEYC_MOUSE_STRING(MOUSEDRAG2, MouseDrag2),
	KEYC_MOUSE_STRING(MOUSEDRAG3, MouseDrag3),
	KEYC_MOUSE_STRING(WHEELUP, WheelUp),
	KEYC_MOUSE_STRING(WHEELDOWN, WheelDown),
};

/* Find key string in table. */
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

/* Find modifiers. */
int
key_string_get_modifiers(const char **string)
{
	int	modifiers;

	modifiers = 0;
	while (((*string)[0] != '\0') && (*string)[1] == '-') {
		switch ((*string)[0]) {
		case 'C':
		case 'c':
			modifiers |= KEYC_CTRL;
			break;
		case 'M':
		case 'm':
			modifiers |= KEYC_ESCAPE;
			break;
		case 'S':
		case 's':
			modifiers |= KEYC_SHIFT;
			break;
		}
		*string += 2;
	}
	return (modifiers);
}

/* Lookup a string and convert to a key value. */
int
key_string_lookup_string(const char *string)
{
	static const char	*other = "!#()+,-.0123456789:;<=>?'\r\t";
	int			 key, modifiers;
	u_short			 u;
	int			 size;

	/* Is this a hexadecimal value? */
	if (string[0] == '0' && string[1] == 'x') {
	        if (sscanf(string + 2, "%hx%n", &u, &size) != 1 || size > 4)
	                return (KEYC_NONE);
	        return (u);
	}

	/* Check for modifiers. */
	modifiers = 0;
	if (string[0] == '^' && string[1] != '\0') {
		modifiers |= KEYC_CTRL;
		string++;
	}
	modifiers |= key_string_get_modifiers(&string);
	if (string[0] == '\0')
		return (KEYC_NONE);

	/* Is this a standard ASCII key? */
	if (string[1] == '\0') {
		key = (u_char) string[0];
		if (key < 32 || key == 127 || key > 255)
			return (KEYC_NONE);
	} else {
		/* Otherwise look the key up in the table. */
		key = key_string_search_table(string);
		if (key == KEYC_NONE)
			return (KEYC_NONE);
	}

	/* Convert the standard control keys. */
	if (key < KEYC_BASE && (modifiers & KEYC_CTRL) && !strchr(other, key)) {
		if (key >= 97 && key <= 122)
			key -= 96;
		else if (key >= 64 && key <= 95)
			key -= 64;
		else if (key == 32)
			key = 0;
		else if (key == 63)
			key = KEYC_BSPACE;
		else
			return (KEYC_NONE);
		modifiers &= ~KEYC_CTRL;
	}

	return (key | modifiers);
}

/* Convert a key code into string format, with prefix if necessary. */
const char *
key_string_lookup_key(int key)
{
	static char	out[24];
	char		tmp[8];
	u_int		i;

	*out = '\0';

	/* Handle no key. */
	if (key == KEYC_NONE)
		return ("<NONE>");
	if (key == KEYC_MOUSE)
		return ("<MOUSE>");

	/*
	 * Special case: display C-@ as C-Space. Could do this below in
	 * the (key >= 0 && key <= 32), but this way we let it be found
	 * in key_string_table, for the unlikely chance that we might
	 * change its name.
	 */
	if ((key & KEYC_MASK_KEY) == 0)
	    key = ' ' | KEYC_CTRL | (key & KEYC_MASK_MOD);

	/* Fill in the modifiers. */
	if (key & KEYC_CTRL)
		strlcat(out, "C-", sizeof out);
	if (key & KEYC_ESCAPE)
		strlcat(out, "M-", sizeof out);
	if (key & KEYC_SHIFT)
		strlcat(out, "S-", sizeof out);
	key &= KEYC_MASK_KEY;

	/* Try the key against the string table. */
	for (i = 0; i < nitems(key_string_table); i++) {
		if (key == key_string_table[i].key)
			break;
	}
	if (i != nitems(key_string_table)) {
		strlcat(out, key_string_table[i].string, sizeof out);
		return (out);
	}

	/* Invalid keys are errors. */
	if (key == 127 || key > 255)
		return (NULL);

	/* Check for standard or control key. */
	if (key >= 0 && key <= 32) {
		if (key == 0 || key > 26)
			xsnprintf(tmp, sizeof tmp, "C-%c", 64 + key);
		else
			xsnprintf(tmp, sizeof tmp, "C-%c", 96 + key);
	} else if (key >= 32 && key <= 126) {
		tmp[0] = key;
		tmp[1] = '\0';
	} else if (key >= 128)
		xsnprintf(tmp, sizeof tmp, "\\%o", key);

	strlcat(out, tmp, sizeof out);
	return (out);
}
