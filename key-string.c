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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "tmux.h"

static key_code	key_string_search_table(const char *);
static key_code	key_string_get_modifiers(const char **);

static const struct {
	const char     *string;
	key_code	key;
} key_string_table[] = {
	/* Function keys. */
	{ "F1",		KEYC_F1|KEYC_IMPLIED_META },
	{ "F2",		KEYC_F2|KEYC_IMPLIED_META },
	{ "F3",		KEYC_F3|KEYC_IMPLIED_META },
	{ "F4",		KEYC_F4|KEYC_IMPLIED_META },
	{ "F5",		KEYC_F5|KEYC_IMPLIED_META },
	{ "F6",		KEYC_F6|KEYC_IMPLIED_META },
	{ "F7",		KEYC_F7|KEYC_IMPLIED_META },
	{ "F8",		KEYC_F8|KEYC_IMPLIED_META },
	{ "F9",		KEYC_F9|KEYC_IMPLIED_META },
	{ "F10",	KEYC_F10|KEYC_IMPLIED_META },
	{ "F11",	KEYC_F11|KEYC_IMPLIED_META },
	{ "F12",	KEYC_F12|KEYC_IMPLIED_META },
	{ "IC",		KEYC_IC|KEYC_IMPLIED_META },
	{ "Insert",	KEYC_IC|KEYC_IMPLIED_META },
	{ "DC",		KEYC_DC|KEYC_IMPLIED_META },
	{ "Delete",	KEYC_DC|KEYC_IMPLIED_META },
	{ "Home",	KEYC_HOME|KEYC_IMPLIED_META },
	{ "End",	KEYC_END|KEYC_IMPLIED_META },
	{ "NPage",	KEYC_NPAGE|KEYC_IMPLIED_META },
	{ "PageDown",	KEYC_NPAGE|KEYC_IMPLIED_META },
	{ "PgDn",	KEYC_NPAGE|KEYC_IMPLIED_META },
	{ "PPage",	KEYC_PPAGE|KEYC_IMPLIED_META },
	{ "PageUp",	KEYC_PPAGE|KEYC_IMPLIED_META },
	{ "PgUp",	KEYC_PPAGE|KEYC_IMPLIED_META },
	{ "BTab",	KEYC_BTAB },
	{ "Space",	' ' },
	{ "BSpace",	KEYC_BSPACE },

	/*
	 * C0 control characters, with the exception of Tab, Enter,
	 * and Esc, should never appear as keys. We still render them,
	 * so to be able to spot them in logs in case of an abnormality.
	 */
	{ "[NUL]",	C0_NUL },
	{ "[SOH]",	C0_SOH },
	{ "[STX]",	C0_STX },
	{ "[ETX]",	C0_ETX },
	{ "[EOT]",	C0_EOT },
	{ "[ENQ]",	C0_ENQ },
	{ "[ASC]",	C0_ASC },
	{ "[BEL]",	C0_BEL },
	{ "[BS]",	C0_BS },
	{ "Tab",	C0_HT },
	{ "[LF]",	C0_LF },
	{ "[VT]",	C0_VT },
	{ "[FF]",	C0_FF },
	{ "Enter",	C0_CR },
	{ "[SO]",	C0_SO },
	{ "[SI]",	C0_SI },
	{ "[DLE]",	C0_DLE },
	{ "[DC1]",	C0_DC1 },
	{ "[DC2]",	C0_DC2 },
	{ "[DC3]",	C0_DC3 },
	{ "[DC4]",	C0_DC4 },
	{ "[NAK]",	C0_NAK },
	{ "[SYN]",	C0_SYN },
	{ "[ETB]",	C0_ETB },
	{ "[CAN]",	C0_CAN },
	{ "[EM]",	C0_EM },
	{ "[SUB]",	C0_SUB },
	{ "Escape",	C0_ESC },
	{ "[FS]",	C0_FS },
	{ "[GS]",	C0_GS },
	{ "[RS]",	C0_RS },
	{ "[US]",	C0_US },

	/* Arrow keys. */
	{ "Up",		KEYC_UP|KEYC_CURSOR|KEYC_IMPLIED_META },
	{ "Down",	KEYC_DOWN|KEYC_CURSOR|KEYC_IMPLIED_META },
	{ "Left",	KEYC_LEFT|KEYC_CURSOR|KEYC_IMPLIED_META },
	{ "Right",	KEYC_RIGHT|KEYC_CURSOR|KEYC_IMPLIED_META },

	/* Numeric keypad. */
	{ "KP/",	KEYC_KP_SLASH|KEYC_KEYPAD },
	{ "KP*",	KEYC_KP_STAR|KEYC_KEYPAD },
	{ "KP-",	KEYC_KP_MINUS|KEYC_KEYPAD },
	{ "KP7",	KEYC_KP_SEVEN|KEYC_KEYPAD },
	{ "KP8",	KEYC_KP_EIGHT|KEYC_KEYPAD },
	{ "KP9",	KEYC_KP_NINE|KEYC_KEYPAD },
	{ "KP+",	KEYC_KP_PLUS|KEYC_KEYPAD },
	{ "KP4",	KEYC_KP_FOUR|KEYC_KEYPAD },
	{ "KP5",	KEYC_KP_FIVE|KEYC_KEYPAD },
	{ "KP6",	KEYC_KP_SIX|KEYC_KEYPAD },
	{ "KP1",	KEYC_KP_ONE|KEYC_KEYPAD },
	{ "KP2",	KEYC_KP_TWO|KEYC_KEYPAD },
	{ "KP3",	KEYC_KP_THREE|KEYC_KEYPAD },
	{ "KPEnter",	KEYC_KP_ENTER|KEYC_KEYPAD },
	{ "KP0",	KEYC_KP_ZERO|KEYC_KEYPAD },
	{ "KP.",	KEYC_KP_PERIOD|KEYC_KEYPAD },

	/* Mouse keys. */
	KEYC_MOUSE_STRING(MOUSEDOWN1, MouseDown1),
	KEYC_MOUSE_STRING(MOUSEDOWN2, MouseDown2),
	KEYC_MOUSE_STRING(MOUSEDOWN3, MouseDown3),
	KEYC_MOUSE_STRING(MOUSEDOWN6, MouseDown6),
	KEYC_MOUSE_STRING(MOUSEDOWN7, MouseDown7),
	KEYC_MOUSE_STRING(MOUSEDOWN8, MouseDown8),
	KEYC_MOUSE_STRING(MOUSEDOWN9, MouseDown9),
	KEYC_MOUSE_STRING(MOUSEDOWN10, MouseDown10),
	KEYC_MOUSE_STRING(MOUSEDOWN11, MouseDown11),
	KEYC_MOUSE_STRING(MOUSEUP1, MouseUp1),
	KEYC_MOUSE_STRING(MOUSEUP2, MouseUp2),
	KEYC_MOUSE_STRING(MOUSEUP3, MouseUp3),
	KEYC_MOUSE_STRING(MOUSEUP6, MouseUp6),
	KEYC_MOUSE_STRING(MOUSEUP7, MouseUp7),
	KEYC_MOUSE_STRING(MOUSEUP8, MouseUp8),
	KEYC_MOUSE_STRING(MOUSEUP9, MouseUp9),
	KEYC_MOUSE_STRING(MOUSEUP10, MouseUp10),
	KEYC_MOUSE_STRING(MOUSEUP11, MouseUp11),
	KEYC_MOUSE_STRING(MOUSEDRAG1, MouseDrag1),
	KEYC_MOUSE_STRING(MOUSEDRAG2, MouseDrag2),
	KEYC_MOUSE_STRING(MOUSEDRAG3, MouseDrag3),
	KEYC_MOUSE_STRING(MOUSEDRAG6, MouseDrag6),
	KEYC_MOUSE_STRING(MOUSEDRAG7, MouseDrag7),
	KEYC_MOUSE_STRING(MOUSEDRAG8, MouseDrag8),
	KEYC_MOUSE_STRING(MOUSEDRAG9, MouseDrag9),
	KEYC_MOUSE_STRING(MOUSEDRAG10, MouseDrag10),
	KEYC_MOUSE_STRING(MOUSEDRAG11, MouseDrag11),
	KEYC_MOUSE_STRING(MOUSEDRAGEND1, MouseDragEnd1),
	KEYC_MOUSE_STRING(MOUSEDRAGEND2, MouseDragEnd2),
	KEYC_MOUSE_STRING(MOUSEDRAGEND3, MouseDragEnd3),
	KEYC_MOUSE_STRING(MOUSEDRAGEND6, MouseDragEnd6),
	KEYC_MOUSE_STRING(MOUSEDRAGEND7, MouseDragEnd7),
	KEYC_MOUSE_STRING(MOUSEDRAGEND8, MouseDragEnd8),
	KEYC_MOUSE_STRING(MOUSEDRAGEND9, MouseDragEnd9),
	KEYC_MOUSE_STRING(MOUSEDRAGEND10, MouseDragEnd10),
	KEYC_MOUSE_STRING(MOUSEDRAGEND11, MouseDragEnd11),
	KEYC_MOUSE_STRING(WHEELUP, WheelUp),
	KEYC_MOUSE_STRING(WHEELDOWN, WheelDown),
	KEYC_MOUSE_STRING(SECONDCLICK1, SecondClick1),
	KEYC_MOUSE_STRING(SECONDCLICK2, SecondClick2),
	KEYC_MOUSE_STRING(SECONDCLICK3, SecondClick3),
	KEYC_MOUSE_STRING(SECONDCLICK6, SecondClick6),
	KEYC_MOUSE_STRING(SECONDCLICK7, SecondClick7),
	KEYC_MOUSE_STRING(SECONDCLICK8, SecondClick8),
	KEYC_MOUSE_STRING(SECONDCLICK9, SecondClick9),
	KEYC_MOUSE_STRING(SECONDCLICK10, SecondClick10),
	KEYC_MOUSE_STRING(SECONDCLICK11, SecondClick11),
	KEYC_MOUSE_STRING(DOUBLECLICK1, DoubleClick1),
	KEYC_MOUSE_STRING(DOUBLECLICK2, DoubleClick2),
	KEYC_MOUSE_STRING(DOUBLECLICK3, DoubleClick3),
	KEYC_MOUSE_STRING(DOUBLECLICK6, DoubleClick6),
	KEYC_MOUSE_STRING(DOUBLECLICK7, DoubleClick7),
	KEYC_MOUSE_STRING(DOUBLECLICK8, DoubleClick8),
	KEYC_MOUSE_STRING(DOUBLECLICK9, DoubleClick9),
	KEYC_MOUSE_STRING(DOUBLECLICK10, DoubleClick10),
	KEYC_MOUSE_STRING(DOUBLECLICK11, DoubleClick11),
	KEYC_MOUSE_STRING(TRIPLECLICK1, TripleClick1),
	KEYC_MOUSE_STRING(TRIPLECLICK2, TripleClick2),
	KEYC_MOUSE_STRING(TRIPLECLICK3, TripleClick3),
	KEYC_MOUSE_STRING(TRIPLECLICK6, TripleClick6),
	KEYC_MOUSE_STRING(TRIPLECLICK7, TripleClick7),
	KEYC_MOUSE_STRING(TRIPLECLICK8, TripleClick8),
	KEYC_MOUSE_STRING(TRIPLECLICK9, TripleClick9),
	KEYC_MOUSE_STRING(TRIPLECLICK10, TripleClick10),
	KEYC_MOUSE_STRING(TRIPLECLICK11, TripleClick11)
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
			modifiers |= KEYC_META;
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
	key_code		 key, modifiers = 0;
	u_int			 u, i;
	struct utf8_data	 ud, *udp;
	enum utf8_state		 more;
	utf8_char		 uc;
	char			 m[MB_LEN_MAX + 1];
	int			 mlen;

	/* Is this no key or any key? */
	if (strcasecmp(string, "None") == 0)
		return (KEYC_NONE);
	if (strcasecmp(string, "Any") == 0)
		return (KEYC_ANY);

	/* Is this a hexadecimal value? */
	if (string[0] == '0' && string[1] == 'x') {
		if (sscanf(string + 2, "%x", &u) != 1)
			return (KEYC_UNKNOWN);
		if (u < 32)
			return (u);
		mlen = wctomb(m, u);
		if (mlen <= 0 || mlen > MB_LEN_MAX)
			return (KEYC_UNKNOWN);
		m[mlen] = '\0';

		udp = utf8_fromcstr(m);
		if (udp == NULL ||
		    udp[0].size == 0 ||
		    udp[1].size != 0 ||
		    utf8_from_data(&udp[0], &uc) != UTF8_DONE) {
			free(udp);
			return (KEYC_UNKNOWN);
		}
		free(udp);
		return (uc);
	}

	/* Check for short Ctrl key. */
	if (string[0] == '^' && string[1] != '\0') {
		if (string[2] == '\0')
			return (tolower((u_char)string[1])|KEYC_CTRL);
		modifiers |= KEYC_CTRL;
		string++;
	}

	/* Check for modifiers. */
	modifiers |= key_string_get_modifiers(&string);
	if (string == NULL || string[0] == '\0')
		return (KEYC_UNKNOWN);

	/* Is this a standard ASCII key? */
	if (string[1] == '\0' && (u_char)string[0] <= 127) {
		key = (u_char)string[0];
		if (key < 32)
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
			if (utf8_from_data(&ud, &uc) != UTF8_DONE)
				return (KEYC_UNKNOWN);
			return (uc|modifiers);
		}

		/* Otherwise look the key up in the table. */
		key = key_string_search_table(string);
		if (key == KEYC_UNKNOWN)
			return (KEYC_UNKNOWN);
		if (~modifiers & KEYC_META)
			key &= ~KEYC_IMPLIED_META;
	}

	return (key|modifiers);
}

/* Convert a key code into string format, with prefix if necessary. */
const char *
key_string_lookup_key(key_code key, int with_flags)
{
	key_code		 saved = key;
	static char		 out[64];
	char			 tmp[8];
	const char		*s;
	u_int			 i;
	struct utf8_data	 ud;
	size_t			 off;

	*out = '\0';

	/* Literal keys are themselves. */
	if (key & KEYC_LITERAL) {
		snprintf(out, sizeof out, "%c", (int)(key & 0xff));
		goto out;
	}

	/* Fill in the modifiers. */
	if (key & KEYC_CTRL)
		strlcat(out, "C-", sizeof out);
	if (key & KEYC_META)
		strlcat(out, "M-", sizeof out);
	if (key & KEYC_SHIFT)
		strlcat(out, "S-", sizeof out);
	key &= KEYC_MASK_KEY;

	/* Handle no key. */
	if (key == KEYC_NONE) {
		s = "None";
		goto append;
	}

	/* Handle special keys. */
	if (key == KEYC_UNKNOWN) {
		s = "Unknown";
		goto append;
	}
	if (key == KEYC_ANY) {
		s = "Any";
		goto append;
	}
	if (key == KEYC_FOCUS_IN) {
		s = "FocusIn";
		goto append;
	}
	if (key == KEYC_FOCUS_OUT) {
		s = "FocusOut";
		goto append;
	}
	if (key == KEYC_PASTE_START) {
		s = "PasteStart";
		goto append;
	}
	if (key == KEYC_PASTE_END) {
		s = "PasteEnd";
		goto append;
	}
	if (key == KEYC_MOUSE) {
		s = "Mouse";
		goto append;
	}
	if (key == KEYC_DRAGGING) {
		s = "Dragging";
		goto append;
	}
	if (key == KEYC_MOUSEMOVE_PANE) {
		s = "MouseMovePane";
		goto append;
	}
	if (key == KEYC_MOUSEMOVE_STATUS) {
		s = "MouseMoveStatus";
		goto append;
	}
	if (key == KEYC_MOUSEMOVE_STATUS_LEFT) {
		s = "MouseMoveStatusLeft";
		goto append;
	}
	if (key == KEYC_MOUSEMOVE_STATUS_RIGHT) {
		s = "MouseMoveStatusRight";
		goto append;
	}
	if (key == KEYC_MOUSEMOVE_BORDER) {
		s = "MouseMoveBorder";
		goto append;
	}
	if (key >= KEYC_USER && key < KEYC_USER_END) {
		snprintf(tmp, sizeof tmp, "User%u", (u_int)(key - KEYC_USER));
		strlcat(out, tmp, sizeof out);
		goto out;
	}

	/* Try the key against the string table. */
	for (i = 0; i < nitems(key_string_table); i++) {
		if (key == (key_string_table[i].key & KEYC_MASK_KEY))
			break;
	}
	if (i != nitems(key_string_table)) {
		strlcat(out, key_string_table[i].string, sizeof out);
		goto out;
	}

	/* Is this a Unicode key? */
	if (KEYC_IS_UNICODE(key)) {
		utf8_to_data(key, &ud);
		off = strlen(out);
		memcpy(out + off, ud.data, ud.size);
		out[off + ud.size] = '\0';
		goto out;
	}

	/* Invalid keys are errors. */
	if (key > 255) {
		snprintf(out, sizeof out, "Invalid#%llx", saved);
		goto out;
	}

	/* Printable ASCII keys. */
	if (key > 32 && key <= 126) {
		tmp[0] = key;
		tmp[1] = '\0';
	} else if (key == 127)
		xsnprintf(tmp, sizeof tmp, "C-?");
	else if (key >= 128)
		xsnprintf(tmp, sizeof tmp, "\\%llo", key);

	strlcat(out, tmp, sizeof out);
	goto out;

append:
	strlcat(out, s, sizeof out);

out:
	if (with_flags && (saved & KEYC_MASK_FLAGS) != 0) {
		strlcat(out, "[", sizeof out);
		if (saved & KEYC_LITERAL)
			strlcat(out, "L", sizeof out);
		if (saved & KEYC_KEYPAD)
			strlcat(out, "K", sizeof out);
		if (saved & KEYC_CURSOR)
			strlcat(out, "C", sizeof out);
		if (saved & KEYC_IMPLIED_META)
			strlcat(out, "I", sizeof out);
		if (saved & KEYC_BUILD_MODIFIERS)
			strlcat(out, "B", sizeof out);
		if (saved & KEYC_SENT)
			strlcat(out, "S", sizeof out);
		strlcat(out, "]", sizeof out);
	}
	return (out);
}
