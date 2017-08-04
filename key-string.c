/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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

static key_code	key_string_search_table(const char *);
static key_code	key_string_get_modifiers(const char **);

static const struct {
	const char     *string;
	key_code	key;
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
	KEYC_MOUSE_STRING(MOUSEDRAGEND1, MouseDragEnd1),
	KEYC_MOUSE_STRING(MOUSEDRAGEND2, MouseDragEnd2),
	KEYC_MOUSE_STRING(MOUSEDRAGEND3, MouseDragEnd3),
	KEYC_MOUSE_STRING(WHEELUP, WheelUp),
	KEYC_MOUSE_STRING(WHEELDOWN, WheelDown),
	KEYC_MOUSE_STRING(DOUBLECLICK1, DoubleClick1),
	KEYC_MOUSE_STRING(DOUBLECLICK2, DoubleClick2),
	KEYC_MOUSE_STRING(DOUBLECLICK3, DoubleClick3),
	KEYC_MOUSE_STRING(TRIPLECLICK1, TripleClick1),
	KEYC_MOUSE_STRING(TRIPLECLICK2, TripleClick2),
	KEYC_MOUSE_STRING(TRIPLECLICK3, TripleClick3),
};

/* Find key string in table. */
static key_code
key_string_search_table(const char *string)
{
	u_int	i, user;

	for (i = 0; i < nitems(key_string_table); i++) {
		if (strcasecmp(string, key_string_table[i].string) == 0)
			return (key_string_table[i].key);
	}

	if (sscanf(string, "User%u", &user) == 1 && user < KEYC_NUSER)
		return (KEYC_USER + user);

	return (KEYC_UNKNOWN);
}

/* Find modifiers. */
static key_code
key_string_get_modifiers(const char **string)
{
	key_code	modifiers;

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
		default:
			*string = NULL;
			return (0);
		}
		*string += 2;
	}
	return (modifiers);
}

/* Lookup a string and convert to a key value. */
key_code
key_string_lookup_string(const char *string)
{
	static const char	*other = "!#()+,-.0123456789:;<=>?'\r\t";
	key_code		 key;
	u_int			 u;
	key_code		 modifiers;
	struct utf8_data	 ud;
	u_int			 i;
	enum utf8_state		 more;
	wchar_t			 wc;

	/* Is this no key? */
	if (strcasecmp(string, "None") == 0)
		return (KEYC_NONE);

	/* Is this a hexadecimal value? */
	if (string[0] == '0' && string[1] == 'x') {
	        if (sscanf(string + 2, "%x", &u) != 1)
	                return (KEYC_UNKNOWN);
		if (u > 0x1fffff)
	                return (KEYC_UNKNOWN);
	        return (u);
	}

	/* Check for modifiers. */
	modifiers = 0;
	if (string[0] == '^' && string[1] != '\0') {
		modifiers |= KEYC_CTRL;
		string++;
	}
	modifiers |= key_string_get_modifiers(&string);
	if (string == NULL || string[0] == '\0')
		return (KEYC_UNKNOWN);

	/* Is this a standard ASCII key? */
	if (string[1] == '\0' && (u_char)string[0] <= 127) {
		key = (u_char)string[0];
		if (key < 32 || key == 127)
			return (KEYC_UNKNOWN);
	} else {
		/* Try as a UTF-8 key. */
		if ((more = utf8_open(&ud, (u_char)*string)) == UTF8_MORE) {
			if (strlen(string) != ud.size)
				return (KEYC_UNKNOWN);
			for (i = 1; i < ud.size; i++)
				more = utf8_append(&ud, (u_char)string[i]);
			if (more != UTF8_DONE)
				return (KEYC_UNKNOWN);
			if (utf8_combine(&ud, &wc) != UTF8_DONE)
				return (KEYC_UNKNOWN);
			return (wc | modifiers);
		}

		/* Otherwise look the key up in the table. */
		key = key_string_search_table(string);
		if (key == KEYC_UNKNOWN)
			return (KEYC_UNKNOWN);
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
			return (KEYC_UNKNOWN);
		modifiers &= ~KEYC_CTRL;
	}

	return (key | modifiers);
}

/* Convert a key code into string format, with prefix if necessary. */
const char *
key_string_lookup_key(key_code key)
{
	static char		out[32];
	char			tmp[8];
	u_int			i;
	struct utf8_data	ud;
	size_t			off;

	*out = '\0';

	/* Handle no key. */
	if (key == KEYC_NONE)
		return ("None");

	/* Handle special keys. */
	if (key == KEYC_UNKNOWN)
		return ("Unknown");
	if (key == KEYC_FOCUS_IN)
		return ("FocusIn");
	if (key == KEYC_FOCUS_OUT)
		return ("FocusOut");
	if (key == KEYC_PASTE_START)
		return ("PasteStart");
	if (key == KEYC_PASTE_END)
		return ("PasteEnd");
	if (key == KEYC_MOUSE)
		return ("Mouse");
	if (key == KEYC_DRAGGING)
		return ("Dragging");
	if (key == KEYC_MOUSEMOVE_PANE)
		return ("MouseMovePane");
	if (key == KEYC_MOUSEMOVE_STATUS)
		return ("MouseMoveStatus");
	if (key == KEYC_MOUSEMOVE_BORDER)
		return ("MouseMoveBorder");
	if (key >= KEYC_USER && key < KEYC_USER + KEYC_NUSER) {
		snprintf(out, sizeof out, "User%u", (u_int)(key - KEYC_USER));
		return (out);
	}

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

	/* Is this a UTF-8 key? */
	if (key > 127 && key < KEYC_BASE) {
		if (utf8_split(key, &ud) == UTF8_DONE) {
			off = strlen(out);
			memcpy(out + off, ud.data, ud.size);
			out[off + ud.size] = '\0';
			return (out);
		}
	}

	/* Invalid keys are errors. */
	if (key == 127 || key > 255) {
		snprintf(out, sizeof out, "Invalid#%llx", key);
		return (out);
	}

	/* Check for standard or control key. */
	if (key <= 32) {
		if (key == 0 || key > 26)
			xsnprintf(tmp, sizeof tmp, "C-%c", (int)(64 + key));
		else
			xsnprintf(tmp, sizeof tmp, "C-%c", (int)(96 + key));
	} else if (key >= 32 && key <= 126) {
		tmp[0] = key;
		tmp[1] = '\0';
	} else if (key >= 128)
		xsnprintf(tmp, sizeof tmp, "\\%llo", key);

	strlcat(out, tmp, sizeof out);
	return (out);
}
