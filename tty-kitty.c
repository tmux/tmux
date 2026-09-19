/* $OpenBSD$ */

/*
 * Copyright (c) 2026 Michael Grant <mgrant@grant.org>
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
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/* Detect support in the outside terminal and decode its key sequences. */

struct tty_kitty_key {
	u_int		 number;
	key_code	 key;
};

static const struct tty_kitty_key tty_kitty_keys[] = {
	{ 57358, KEYC_CAPSLOCK },
	{ 57359, KEYC_SCROLLLOCK },
	{ 57360, KEYC_NUMLOCK },
	{ 57361, KEYC_PRINTSCREEN },
	{ 57362, KEYC_PAUSE },
	{ 57363, KEYC_MENU },
	{ 57376, KEYC_F13 },
	{ 57377, KEYC_F14 },
	{ 57378, KEYC_F15 },
	{ 57379, KEYC_F16 },
	{ 57380, KEYC_F17 },
	{ 57381, KEYC_F18 },
	{ 57382, KEYC_F19 },
	{ 57383, KEYC_F20 },
	{ 57384, KEYC_F21 },
	{ 57385, KEYC_F22 },
	{ 57386, KEYC_F23 },
	{ 57387, KEYC_F24 },
	{ 57388, KEYC_F25 },
	{ 57389, KEYC_F26 },
	{ 57390, KEYC_F27 },
	{ 57391, KEYC_F28 },
	{ 57392, KEYC_F29 },
	{ 57393, KEYC_F30 },
	{ 57394, KEYC_F31 },
	{ 57395, KEYC_F32 },
	{ 57396, KEYC_F33 },
	{ 57397, KEYC_F34 },
	{ 57398, KEYC_F35 },
	{ 57399, KEYC_KP_ZERO|KEYC_KEYPAD },
	{ 57400, KEYC_KP_ONE|KEYC_KEYPAD },
	{ 57401, KEYC_KP_TWO|KEYC_KEYPAD },
	{ 57402, KEYC_KP_THREE|KEYC_KEYPAD },
	{ 57403, KEYC_KP_FOUR|KEYC_KEYPAD },
	{ 57404, KEYC_KP_FIVE|KEYC_KEYPAD },
	{ 57405, KEYC_KP_SIX|KEYC_KEYPAD },
	{ 57406, KEYC_KP_SEVEN|KEYC_KEYPAD },
	{ 57407, KEYC_KP_EIGHT|KEYC_KEYPAD },
	{ 57408, KEYC_KP_NINE|KEYC_KEYPAD },
	{ 57409, KEYC_KP_PERIOD|KEYC_KEYPAD },
	{ 57410, KEYC_KP_SLASH|KEYC_KEYPAD },
	{ 57411, KEYC_KP_STAR|KEYC_KEYPAD },
	{ 57412, KEYC_KP_MINUS|KEYC_KEYPAD },
	{ 57413, KEYC_KP_PLUS|KEYC_KEYPAD },
	{ 57414, KEYC_KP_ENTER|KEYC_KEYPAD },
	{ 57415, KEYC_KP_EQUAL|KEYC_KEYPAD },
	{ 57416, KEYC_KP_SEPARATOR|KEYC_KEYPAD },
	{ 57417, KEYC_KP_LEFT|KEYC_KEYPAD },
	{ 57418, KEYC_KP_RIGHT|KEYC_KEYPAD },
	{ 57419, KEYC_KP_UP|KEYC_KEYPAD },
	{ 57420, KEYC_KP_DOWN|KEYC_KEYPAD },
	{ 57421, KEYC_KP_PPAGE|KEYC_KEYPAD },
	{ 57422, KEYC_KP_NPAGE|KEYC_KEYPAD },
	{ 57423, KEYC_KP_HOME|KEYC_KEYPAD },
	{ 57424, KEYC_KP_END|KEYC_KEYPAD },
	{ 57425, KEYC_KP_IC|KEYC_KEYPAD },
	{ 57426, KEYC_KP_DC|KEYC_KEYPAD },
	{ 57427, KEYC_KP_BEGIN|KEYC_KEYPAD },
	{ 57428, KEYC_MEDIA_PLAY },
	{ 57429, KEYC_MEDIA_PAUSE },
	{ 57430, KEYC_MEDIA_PLAYPAUSE },
	{ 57431, KEYC_MEDIA_REVERSE },
	{ 57432, KEYC_MEDIA_STOP },
	{ 57433, KEYC_MEDIA_FASTFORWARD },
	{ 57434, KEYC_MEDIA_REWIND },
	{ 57435, KEYC_MEDIA_NEXT },
	{ 57436, KEYC_MEDIA_PREVIOUS },
	{ 57437, KEYC_MEDIA_RECORD },
	{ 57438, KEYC_VOLUME_DOWN },
	{ 57439, KEYC_VOLUME_UP },
	{ 57440, KEYC_VOLUME_MUTE },
	{ 57441, KEYC_LEFT_SHIFT },
	{ 57442, KEYC_LEFT_CTRL },
	{ 57443, KEYC_LEFT_ALT },
	{ 57444, KEYC_LEFT_SUPER },
	{ 57445, KEYC_LEFT_HYPER },
	{ 57446, KEYC_LEFT_META },
	{ 57447, KEYC_RIGHT_SHIFT },
	{ 57448, KEYC_RIGHT_CTRL },
	{ 57449, KEYC_RIGHT_ALT },
	{ 57450, KEYC_RIGHT_SUPER },
	{ 57451, KEYC_RIGHT_HYPER },
	{ 57452, KEYC_RIGHT_META },
	{ 57453, KEYC_ISO_LEVEL3_SHIFT },
	{ 57454, KEYC_ISO_LEVEL5_SHIFT }
};

static int
tty_kitty_parse_number(const char *s, u_int *value, int empty)
{
	char		*end;
	unsigned long	 number;

	if (*s == '\0') {
		if (!empty)
			return (-1);
		return (0);
	}
	if (!isdigit((u_char)*s))
		return (-1);
	number = strtoul(s, &end, 10);
	if (*end != '\0' || number > UINT_MAX)
		return (-1);
	*value = number;
	return (0);
}

static int
tty_kitty_parse_list(char *s, u_int *first, u_int *second, int empty,
    u_int maximum)
{
	char	*copy, *item;
	u_int	 n = 0, value;
	int	 result = 0;

	copy = s;
	while ((item = strsep(&copy, ":")) != NULL) {
		if (maximum != 0 && n == maximum)
			return (-1);
		value = 0;
		if (tty_kitty_parse_number(item, &value, empty) != 0) {
			result = -1;
			break;
		}
		if (n == 0 && *item != '\0')
			*first = value;
		else if (n == 1 && *item != '\0')
			*second = value;
		n++;
	}
	return (result);
}

static int
tty_kitty_find_end(const char *buf, size_t len, size_t *end)
{
	size_t	i;

	for (i = 2; i < len; i++) {
		if (buf[i] == 'u' || buf[i] == '~' || buf[i] == 'A' ||
		    buf[i] == 'B' || buf[i] == 'C' || buf[i] == 'D' ||
		    buf[i] == 'E' || buf[i] == 'F' || buf[i] == 'H' ||
		    buf[i] == 'P' || buf[i] == 'Q' || buf[i] == 'S') {
			*end = i;
			return (0);
		}
		if (!isdigit((u_char)buf[i]) && buf[i] != ';' && buf[i] != ':')
			return (-1);
	}
	return (1);
}

static key_code
tty_kitty_function(u_int number, char final)
{
	u_int	i;

	if (final == 'u') {
		switch (number) {
		case 9:
			return (C0_HT);
		case 13:
			return (C0_CR);
		case 27:
			return (C0_ESC);
		case 127:
			return (KEYC_BSPACE);
		}
		for (i = 0; i < nitems(tty_kitty_keys); i++) {
			if (tty_kitty_keys[i].number == number)
				return (tty_kitty_keys[i].key);
		}
		return (KEYC_NONE);
	}

	if (final == '~') {
		switch (number) {
		case 2: return (KEYC_IC);
		case 3: return (KEYC_DC);
		case 5: return (KEYC_PPAGE);
		case 6: return (KEYC_NPAGE);
		case 7: return (KEYC_HOME);
		case 8: return (KEYC_END);
		case 11: return (KEYC_F1);
		case 12: return (KEYC_F2);
		case 13: return (KEYC_F3);
		case 14: return (KEYC_F4);
		case 15: return (KEYC_F5);
		case 17: return (KEYC_F6);
		case 18: return (KEYC_F7);
		case 19: return (KEYC_F8);
		case 20: return (KEYC_F9);
		case 21: return (KEYC_F10);
		case 23: return (KEYC_F11);
		case 24: return (KEYC_F12);
		case 29: return (KEYC_MENU);
		case 57427: return (KEYC_KP_BEGIN|KEYC_KEYPAD);
		}
		return (KEYC_UNKNOWN);
	}
	if (number != 1)
		return (KEYC_UNKNOWN);
	switch (final) {
	case 'A': return (KEYC_UP|KEYC_CURSOR);
	case 'B': return (KEYC_DOWN|KEYC_CURSOR);
	case 'C': return (KEYC_RIGHT|KEYC_CURSOR);
	case 'D': return (KEYC_LEFT|KEYC_CURSOR);
	case 'E': return (KEYC_KP_BEGIN|KEYC_KEYPAD);
	case 'F': return (KEYC_END);
	case 'H': return (KEYC_HOME);
	case 'P': return (KEYC_F1);
	case 'Q': return (KEYC_F2);
	case 'S': return (KEYC_F4);
	}
	return (KEYC_UNKNOWN);
}

int
tty_keys_kitty_query(struct tty *tty, const char *buf, size_t len,
    size_t *size)
{
	struct client	*c = tty->client;
	size_t		 i;
	u_int		 flags = 0;

	*size = 0;
	if (tty->flags & TTY_HAVEKKB)
		return (-1);

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

	for (i = 3; i < len && isdigit((u_char)buf[i]); i++) {
		if (flags > (UINT_MAX - (buf[i] - '0')) / 10)
			return (-1);
		flags = (flags * 10) + (buf[i] - '0');
	}
	if (i == len)
		return (1);
	if (i == 3 || buf[i] != 'u')
		return (-1);

	*size = i + 1;
	tty->kitty_keys = flags;
	tty->flags |= TTY_HAVEKKB;
	log_debug("%s: received Kitty keyboard flags %u", c->name, flags);
	tty_parse_client_features(c, "kittykeys", ",");
	tty_update_features(tty);
	return (0);
}

void
tty_update_kitty(struct tty *tty, struct screen *s)
{
	u_int	flags;
	char	buf[32];

	if (options_get_number(global_options, "extended-keys") == 0 ||
	    (~tty->term->flags & TERM_KITTYKEYS)) {
		if (tty->flags & TTY_KKBPUSHED) {
			tty_puts(tty, "\033[<u");
			tty->flags &= ~TTY_KKBPUSHED;
		}
		return;
	}
	flags = KITTY_KEY_DISAMBIGUATE;
	if (s != NULL && (s->kitty_keys.flags & KITTY_KEY_SUPPORTED) != 0)
		flags = s->kitty_keys.flags & KITTY_KEY_SUPPORTED;
	if ((tty->flags & TTY_KKBPUSHED) == 0) {
		xsnprintf(buf, sizeof buf, "\033[>%uu", flags);
		tty_puts(tty, buf);
		tty->flags |= TTY_KKBPUSHED;
	} else if (tty->kitty_keys != flags) {
		xsnprintf(buf, sizeof buf, "\033[=%uu", flags);
		tty_puts(tty, buf);
	}
	tty->kitty_keys = flags;
}

int
tty_keys_kitty(struct tty *tty, const char *buf, size_t len, size_t *size,
    key_code *key)
{
	struct client	*c = tty->client;
	struct utf8_data ud;
	utf8_char	 uc;
	size_t		 end;
	char		 tmp[128], *copy, *field, final;
	u_int		 number = 0, modifiers = 1, event = 1, value;
	u_int		 fields = 0;
	key_code	 nkey, onlykey;
	int		 result;

	*size = 0;
	if ((tty->flags & TTY_KKBPUSHED) == 0)
		return (-1);
	if (len == 0 || buf[0] != '\033')
		return (-1);
	if (len == 1)
		return (1);
	if (buf[1] != '[')
		return (-1);
	if (len == 2)
		return (1);
	if (buf[2] == '?')
		return (-1);

	result = tty_kitty_find_end(buf, len, &end);
	if (result != 0)
		return (result);
	if (end - 2 >= sizeof tmp)
		return (-1);
	memcpy(tmp, buf + 2, end - 2);
	tmp[end - 2] = '\0';
	final = buf[end];
	*size = end + 1;

	copy = tmp;
	while ((field = strsep(&copy, ";")) != NULL) {
		if (fields == 3)
			return (-1);
		value = 0;
		if (fields == 0) {
			if (*field == '\0') {
				if (final == 'u')
					return (-1);
				number = 1;
			} else if (tty_kitty_parse_list(field, &number, &value,
			    1, 3) != 0)
				return (-1);
		} else if (fields == 1) {
			if (tty_kitty_parse_list(field, &modifiers, &event, 1,
			    2) != 0)
				return (-1);
		} else if (tty_kitty_parse_list(field, &value, &value, 1, 0) != 0)
			return (-1);
		fields++;
	}
	if (fields == 0 || number == 0 || modifiers == 0)
		return (-1);
	if (event == 0 || event == 3 || event > 3)
		return (-2);

	modifiers--;
	if (modifiers & 32)
		return (-2);
	nkey = tty_kitty_function(number, final);
	if (nkey == KEYC_NONE) {
		if (final != 'u')
			return (-1);
		if (number >= 57344 && number <= 63743)
			return (-2);
		if (number <= 0x7f)
			nkey = number;
		else if (utf8_fromwc(number, &ud) != UTF8_DONE ||
		    utf8_from_data(&ud, &uc) != UTF8_DONE)
			return (-2);
		else
			nkey = uc;
	} else if (nkey == KEYC_UNKNOWN)
		return (-1);

	if (modifiers & 1)
		nkey |= KEYC_SHIFT;
	if (modifiers & 2)
		nkey |= KEYC_META|KEYC_IMPLIED_META;
	if (modifiers & 4)
		nkey |= KEYC_CTRL;
	if (modifiers & 8)
		nkey |= KEYC_SUPER;
	if (modifiers & 16)
		nkey |= KEYC_HYPER;

	/* Shift-Tab has a dedicated tmux key code. */
	onlykey = nkey & KEYC_MASK_KEY;
	if (onlykey == C0_HT && (nkey & KEYC_SHIFT))
		nkey = KEYC_BTAB|(nkey & ~KEYC_MASK_KEY & ~KEYC_SHIFT);

	if (log_get_level() != 0) {
		log_debug("%s: Kitty key %.*s is %llx (%s)", c->name,
		    (int)*size, buf, nkey, key_string_lookup_key(nkey, 1));
	}
	*key = nkey;
	return (0);
}
