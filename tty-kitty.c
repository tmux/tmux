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

#include "tmux.h"

/* Kitty function key number to KEYC_* mapping. */
static const struct {
	u_int		number;
	key_code	key;
} tty_keys_kitty_function[] = {
	{ 27,     27 },                   /* ESC */
	{ 13,     13 },                   /* Enter */
	{ 9,      9 },                    /* Tab */
	{ 127,    KEYC_BSPACE },
	{ 57358,  KEYC_CAPS_LOCK_KEY },
	{ 57359,  KEYC_SCROLL_LOCK },
	{ 57360,  KEYC_NUM_LOCK_KEY },
	{ 57361,  KEYC_PRINT },
	{ 57362,  KEYC_PAUSE },
	{ 57363,  KEYC_MENU },
	/* F13-F35 (57376-57398) */
	{ 57376,  KEYC_F13 },
	{ 57377,  KEYC_F14 },
	{ 57378,  KEYC_F15 },
	{ 57379,  KEYC_F16 },
	{ 57380,  KEYC_F17 },
	{ 57381,  KEYC_F18 },
	{ 57382,  KEYC_F19 },
	{ 57383,  KEYC_F20 },
	{ 57384,  KEYC_F21 },
	{ 57385,  KEYC_F22 },
	{ 57386,  KEYC_F23 },
	{ 57387,  KEYC_F24 },
	{ 57388,  KEYC_F25 },
	{ 57389,  KEYC_F26 },
	{ 57390,  KEYC_F27 },
	{ 57391,  KEYC_F28 },
	{ 57392,  KEYC_F29 },
	{ 57393,  KEYC_F30 },
	{ 57394,  KEYC_F31 },
	{ 57395,  KEYC_F32 },
	{ 57396,  KEYC_F33 },
	{ 57397,  KEYC_F34 },
	{ 57398,  KEYC_F35 },
	/* Keypad keys (57399-57427) */
	{ 57399,  KEYC_KP_ZERO },
	{ 57400,  KEYC_KP_ONE },
	{ 57401,  KEYC_KP_TWO },
	{ 57402,  KEYC_KP_THREE },
	{ 57403,  KEYC_KP_FOUR },
	{ 57404,  KEYC_KP_FIVE },
	{ 57405,  KEYC_KP_SIX },
	{ 57406,  KEYC_KP_SEVEN },
	{ 57407,  KEYC_KP_EIGHT },
	{ 57408,  KEYC_KP_NINE },
	{ 57409,  KEYC_KP_PERIOD },
	{ 57410,  KEYC_KP_SLASH },
	{ 57411,  KEYC_KP_STAR },
	{ 57412,  KEYC_KP_MINUS },
	{ 57413,  KEYC_KP_PLUS },
	{ 57414,  KEYC_KP_ENTER },
	{ 57415,  KEYC_KP_EQUAL },
	{ 57416,  KEYC_KP_SEPARATOR },
	{ 57417,  KEYC_KP_LEFT },
	{ 57418,  KEYC_KP_RIGHT },
	{ 57419,  KEYC_KP_UP },
	{ 57420,  KEYC_KP_DOWN },
	{ 57421,  KEYC_KP_PAGE_UP },
	{ 57422,  KEYC_KP_PAGE_DOWN },
	{ 57423,  KEYC_KP_HOME },
	{ 57424,  KEYC_KP_END },
	{ 57425,  KEYC_KP_INSERT },
	{ 57426,  KEYC_KP_DELETE },
	{ 57427,  KEYC_KP_BEGIN },
	/* Media keys (57428-57440) */
	{ 57428,  KEYC_MEDIA_PLAY },
	{ 57429,  KEYC_MEDIA_PAUSE },
	{ 57430,  KEYC_MEDIA_PLAY_PAUSE },
	{ 57431,  KEYC_MEDIA_REVERSE },
	{ 57432,  KEYC_MEDIA_STOP },
	{ 57433,  KEYC_MEDIA_FAST_FORWARD },
	{ 57434,  KEYC_MEDIA_REWIND },
	{ 57435,  KEYC_MEDIA_NEXT },
	{ 57436,  KEYC_MEDIA_PREVIOUS },
	{ 57437,  KEYC_MEDIA_RECORD },
	{ 57438,  KEYC_VOLUME_DOWN },
	{ 57439,  KEYC_VOLUME_UP },
	{ 57440,  KEYC_VOLUME_MUTE },
	/* Modifier keys (57441-57454) */
	{ 57441,  KEYC_LEFT_SHIFT },
	{ 57442,  KEYC_LEFT_CONTROL },
	{ 57443,  KEYC_LEFT_ALT },
	{ 57444,  KEYC_LEFT_SUPER },
	{ 57445,  KEYC_LEFT_HYPER },
	{ 57446,  KEYC_LEFT_META },
	{ 57447,  KEYC_RIGHT_SHIFT },
	{ 57448,  KEYC_RIGHT_CONTROL },
	{ 57449,  KEYC_RIGHT_ALT },
	{ 57450,  KEYC_RIGHT_SUPER },
	{ 57451,  KEYC_RIGHT_HYPER },
	{ 57452,  KEYC_RIGHT_META },
	{ 57453,  KEYC_ISO_LEVEL3_SHIFT },
	{ 57454,  KEYC_ISO_LEVEL5_SHIFT },
};

/* Kitty tilde-terminated function keys. */
static const struct {
	u_int		number;
	key_code	key;
} tty_keys_kitty_tilde[] = {
	{ 2,   KEYC_IC },
	{ 3,   KEYC_DC },
	{ 5,   KEYC_PPAGE },
	{ 6,   KEYC_NPAGE },
	{ 7,   KEYC_HOME },
	{ 8,   KEYC_END },
	{ 11,  KEYC_F1 },
	{ 12,  KEYC_F2 },
	{ 13,  KEYC_F3 },
	{ 14,  KEYC_F4 },
	{ 15,  KEYC_F5 },
	{ 17,  KEYC_F6 },
	{ 18,  KEYC_F7 },
	{ 19,  KEYC_F8 },
	{ 20,  KEYC_F9 },
	{ 21,  KEYC_F10 },
	{ 23,  KEYC_F11 },
	{ 24,  KEYC_F12 },
	{ 57427, KEYC_KP_BEGIN }
};

/* Map kitty modifier encoding to KEYC_* modifier bits. */
static key_code
tty_keys_kitty_modifiers(u_int mods)
{
	key_code	key = 0;

	if (mods > 0) {
		mods--;
		if (mods & 0x01) key |= KEYC_SHIFT;
		if (mods & 0x02) key |= (KEYC_META | KEYC_IMPLIED_META);
		if (mods & 0x04) key |= KEYC_CTRL;
		if (mods & 0x08) key |= KEYC_SUPER;
		if (mods & 0x10) key |= KEYC_HYPER;
		/*
		 * Caps Lock and Num Lock are only meaningful when the
		 * terminal is in report-all-keys mode (flag 8+).  We
		 * only push disambiguate (flag 1), so strip these to
		 * avoid injecting noise modifiers into key codes.
		 */
	}
	return (key);
}

/* Parse kitty keyboard protocol key sequences. */
int
tty_keys_kitty(struct tty *tty, const char *buf, size_t len,
    size_t *size, key_code *key)
{
	char		tmp[128];
	char		*params, *field1, *field2;
	char		*subfield, *endptr, *mf, *kf;
	size_t		 end, consumed;
	u_int		 number, modifiers, event_type, i, kitty_flags;
	key_code	 result;

	*size = 0;
	kitty_flags = tty->kitty_flags & KITTY_KBD_SUPPORTED;

	/* Must start with CSI (\033[). */
	if (buf[0] != '\033')
		return (-1);
	if (len == 1)
		return (1);
	if (buf[1] != '[')
		return (-1);
	if (len == 2)
		return (1);

	/* Scan for the terminating character: u, ~, or A-S. */
	for (end = 2; end < len && end < sizeof tmp; end++) {
		if (buf[end] == 'u' || buf[end] == '~' ||
		    (buf[end] >= 'A' && buf[end] <= 'S'))
			break;
		/* Only digits, semicolons, colons, and ? are valid. */
		if (!isdigit((u_char)buf[end]) && buf[end] != ';' &&
		    buf[end] != ':' && buf[end] != '?')
			return (-1);
	}
	if (end == len || end == sizeof tmp)
		return (1); /* partial */

	/* Skip CSI ? u (that's the query response, handled elsewhere). */
	if (buf[2] == '?')
		return (-1);

	/* Copy sequence parameters to tmp. */
	memcpy(tmp, buf + 2, end - 2);
	tmp[end - 2] = '\0';
	consumed = end + 1;

	/* Parse based on terminator. */
	if (buf[end] == 'u') {
		/* CSI key-code[:alternates] [; mods[:event] [; text]] u */
		params = tmp;

		/* Split by semicolons (up to 3 fields). */
		field1 = strsep(&params, ";"); /* key code field */
		field2 = strsep(&params, ";"); /* modifier field */
		/* third field (text-as-codepoints) */

		/* Parse key code (first sub-field before ':'). */
		kf = field1;
		subfield = strsep(&kf, ":");
		number = (u_int)strtoul(subfield, &endptr, 10);
		if (*endptr != '\0')
			return (-1);

		if (kf != NULL && *kf != '\0' &&
		    (~kitty_flags & KITTY_KBD_REPORT_ALTERNATES)) {
			*size = consumed;
			return (-2);
		}
		if (params != NULL && *params != '\0' &&
		    (~kitty_flags & KITTY_KBD_REPORT_TEXT)) {
			*size = consumed;
			return (-2);
		}

		/* Parse modifiers and event type. */
		modifiers = 0;
		event_type = 1;
		if (field2 != NULL && *field2 != '\0') {
			mf = field2;
			subfield = strsep(&mf, ":");
			modifiers = (u_int)strtoul(subfield, &endptr, 10);
			if (*endptr != '\0')
				modifiers = 0;
			if (mf != NULL && *mf != '\0') {
				event_type = (u_int)strtoul(mf, &endptr, 10);
				if (*endptr != '\0')
					event_type = 1;
			}
		}

		if (event_type != 1 && (~kitty_flags & KITTY_KBD_REPORT_EVENTS)) {
			*size = consumed;
			return (-2);
		}
		if (event_type == 3) {
			*size = consumed;
			return (-2);
		}

		/* Map key number to key_code. */
		result = tty_keys_kitty_modifiers(modifiers);

		/* Check function key table first. */
		for (i = 0; i < nitems(tty_keys_kitty_function); i++) {
			if (tty_keys_kitty_function[i].number == number) {
				result |= tty_keys_kitty_function[i].key;
				*key = result;
				*size = consumed;
				return (0);
			}
		}

		/* Not in function table, treat as Unicode codepoint. */
		result |= (key_code)number;
		*key = result;
		*size = consumed;
		return (0);

	} else if (buf[end] == '~') {
		/* CSI number ; modifiers ~ */
		params = tmp;

		field1 = strsep(&params, ";");
		field2 = strsep(&params, ";");

		/* Parse number. */
		number = (u_int)strtoul(field1, &endptr, 10);
		if (*endptr != '\0' && *endptr != ':')
			return (-1);

		/* Parse modifiers and event type. */
		modifiers = 0;
		event_type = 1;
		if (field2 != NULL && *field2 != '\0') {
			mf = field2;
			subfield = strsep(&mf, ":");
			modifiers = (u_int)strtoul(subfield, &endptr, 10);
			if (*endptr != '\0')
				modifiers = 0;
			if (mf != NULL && *mf != '\0') {
				event_type = (u_int)strtoul(mf, &endptr, 10);
				if (*endptr != '\0')
					event_type = 1;
			}
		}

		if (event_type != 1 && (~kitty_flags & KITTY_KBD_REPORT_EVENTS)) {
			*size = consumed;
			return (-2);
		}
		if (event_type == 3) {
			*size = consumed;
			return (-2);
		}

		result = tty_keys_kitty_modifiers(modifiers);

		/* Look up in tilde table. */
		for (i = 0; i < nitems(tty_keys_kitty_tilde); i++) {
			if (tty_keys_kitty_tilde[i].number == number) {
				result |= tty_keys_kitty_tilde[i].key;
				*key = result;
				*size = consumed;
				return (0);
			}
		}

		/* Also check function table for high numbers. */
		for (i = 0; i < nitems(tty_keys_kitty_function); i++) {
			if (tty_keys_kitty_function[i].number == number) {
				result |= tty_keys_kitty_function[i].key;
				*key = result;
				*size = consumed;
				return (0);
			}
		}

		/* Unknown tilde key - fall through to other parsers. */
		return (-1);

	} else {
		/* CSI [1 ;] modifiers [A-S] */
		modifiers = 0;
		event_type = 1;

		if (tmp[0] == '\0') {
			/* No parameters: CSI A */
			modifiers = 0;
		} else {
			params = tmp;
			field1 = strsep(&params, ";");
			field2 = strsep(&params, ";");

			if (field2 == NULL) {
				/* Only one field before letter. */
				modifiers = (u_int)strtoul(field1,
				    &endptr, 10);
				if (*endptr != '\0' && *endptr != ':')
					modifiers = 0;
			} else {
				/* CSI number ; modifiers[:event] letter */
				mf = field2;
				subfield = strsep(&mf, ":");
				modifiers = (u_int)strtoul(subfield,
				    &endptr, 10);
				if (*endptr != '\0')
					modifiers = 0;
				if (mf != NULL && *mf != '\0') {
					event_type = (u_int)strtoul(mf,
					    &endptr, 10);
					if (*endptr != '\0')
						event_type = 1;
				}
			}
		}

		if (event_type != 1 && (~kitty_flags & KITTY_KBD_REPORT_EVENTS)) {
			*size = consumed;
			return (-2);
		}
		if (event_type == 3) {
			*size = consumed;
			return (-2);
		}

		result = tty_keys_kitty_modifiers(modifiers);

		switch (buf[end]) {
		case 'A':
			result |= KEYC_UP|KEYC_CURSOR;
			break;
		case 'B':
			result |= KEYC_DOWN|KEYC_CURSOR;
			break;
		case 'C':
			result |= KEYC_RIGHT|KEYC_CURSOR;
			break;
		case 'D':
			result |= KEYC_LEFT|KEYC_CURSOR;
			break;
		case 'E':
			result |= KEYC_KP_BEGIN;
			break;
		case 'F':
			result |= KEYC_END;
			break;
		case 'H':
			result |= KEYC_HOME;
			break;
		case 'P':
			result |= KEYC_F1;
			break;
		case 'Q':
			result |= KEYC_F2;
			break;
		case 'S':
			result |= KEYC_F4;
			break;
		default:
			return (-1);
		}
		*key = result;
		*size = consumed;
		return (0);
	}
}

int
tty_keys_kitty_keyboard(struct tty *tty, const char *buf, size_t len,
    size_t *size)
{
	struct client	*c = tty->client;
	u_int		 i, n;
	char		 tmp[64];

	*size = 0;

	/* First three bytes are always \033[?. */
	if (buf[0] != '\033')
		return (-1);
	if (len == 1)
		return (1);
	if (buf[1] != '[')
		return (-1);
	if (len == 2)
		return (1);
	if (buf[2] != '?')
		return (-1);
	if (len == 3)
		return (1);

	/* Copy the rest until we find 'u'. */
	for (i = 3; i < len && i < sizeof tmp; i++) {
		if (buf[i] == 'u')
			break;
		if (!isdigit((u_char)buf[i]))
			return (-1);
	}
	if (i == len || i == sizeof tmp)
		return (1);

	/* Copy to tmp and parse. */
	memcpy(tmp, buf + 3, i - 3);
	tmp[i - 3] = '\0';
	*size = i + 1;

	n = 0;
	sscanf(tmp, "%u", &n);

	log_debug("%s: kitty keyboard query response: flags=%u", c->name, n);

	tty->kitty_flags = n & KITTY_KBD_SUPPORTED;
	if ((n & ~KITTY_KBD_SUPPORTED) != 0)
		log_debug("%s: dropping unsupported kitty keyboard flags %#x",
		    c->name, n & ~KITTY_KBD_SUPPORTED);
	tty->flags |= TTY_HAVEDA_KITTY;

	/* Add kitkeys terminal feature. */
	tty_add_features(&c->term_features, "kitkeys", ",");

	tty_update_features(tty);
	if (tty->flags & TTY_KITTY_PUSHED)
		tty->kitty_flags = KITTY_KBD_DISAMBIGUATE;

	return (0);
}
