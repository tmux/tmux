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

#include <string.h>
#include <wchar.h>

#include "tmux.h"

/* Track pane keyboard state and encode keys for applications. */

struct input_kitty_key {
	key_code	 key;
	u_int		 number;
	char		 final;
};

static const struct input_kitty_key input_kitty_keys[] = {
	{ C0_ESC, 27, 'u' },
	{ C0_CR, 13, 'u' },
	{ C0_HT, 9, 'u' },
	{ KEYC_BSPACE, 127, 'u' },
	{ KEYC_IC, 2, '~' },
	{ KEYC_DC, 3, '~' },
	{ KEYC_LEFT, 1, 'D' },
	{ KEYC_RIGHT, 1, 'C' },
	{ KEYC_UP, 1, 'A' },
	{ KEYC_DOWN, 1, 'B' },
	{ KEYC_PPAGE, 5, '~' },
	{ KEYC_NPAGE, 6, '~' },
	{ KEYC_HOME, 1, 'H' },
	{ KEYC_END, 1, 'F' },
	{ KEYC_F1, 1, 'P' },
	{ KEYC_F2, 1, 'Q' },
	{ KEYC_F3, 13, '~' },
	{ KEYC_F4, 1, 'S' },
	{ KEYC_F5, 15, '~' },
	{ KEYC_F6, 17, '~' },
	{ KEYC_F7, 18, '~' },
	{ KEYC_F8, 19, '~' },
	{ KEYC_F9, 20, '~' },
	{ KEYC_F10, 21, '~' },
	{ KEYC_F11, 23, '~' },
	{ KEYC_F12, 24, '~' },
	{ KEYC_CAPSLOCK, 57358, 'u' },
	{ KEYC_SCROLLLOCK, 57359, 'u' },
	{ KEYC_NUMLOCK, 57360, 'u' },
	{ KEYC_PRINTSCREEN, 57361, 'u' },
	{ KEYC_PAUSE, 57362, 'u' },
	{ KEYC_MENU, 57363, 'u' },
	{ KEYC_F13, 57376, 'u' },
	{ KEYC_F14, 57377, 'u' },
	{ KEYC_F15, 57378, 'u' },
	{ KEYC_F16, 57379, 'u' },
	{ KEYC_F17, 57380, 'u' },
	{ KEYC_F18, 57381, 'u' },
	{ KEYC_F19, 57382, 'u' },
	{ KEYC_F20, 57383, 'u' },
	{ KEYC_F21, 57384, 'u' },
	{ KEYC_F22, 57385, 'u' },
	{ KEYC_F23, 57386, 'u' },
	{ KEYC_F24, 57387, 'u' },
	{ KEYC_F25, 57388, 'u' },
	{ KEYC_F26, 57389, 'u' },
	{ KEYC_F27, 57390, 'u' },
	{ KEYC_F28, 57391, 'u' },
	{ KEYC_F29, 57392, 'u' },
	{ KEYC_F30, 57393, 'u' },
	{ KEYC_F31, 57394, 'u' },
	{ KEYC_F32, 57395, 'u' },
	{ KEYC_F33, 57396, 'u' },
	{ KEYC_F34, 57397, 'u' },
	{ KEYC_F35, 57398, 'u' },
	{ KEYC_KP_ZERO|KEYC_KEYPAD, 57399, 'u' },
	{ KEYC_KP_ONE|KEYC_KEYPAD, 57400, 'u' },
	{ KEYC_KP_TWO|KEYC_KEYPAD, 57401, 'u' },
	{ KEYC_KP_THREE|KEYC_KEYPAD, 57402, 'u' },
	{ KEYC_KP_FOUR|KEYC_KEYPAD, 57403, 'u' },
	{ KEYC_KP_FIVE|KEYC_KEYPAD, 57404, 'u' },
	{ KEYC_KP_SIX|KEYC_KEYPAD, 57405, 'u' },
	{ KEYC_KP_SEVEN|KEYC_KEYPAD, 57406, 'u' },
	{ KEYC_KP_EIGHT|KEYC_KEYPAD, 57407, 'u' },
	{ KEYC_KP_NINE|KEYC_KEYPAD, 57408, 'u' },
	{ KEYC_KP_PERIOD|KEYC_KEYPAD, 57409, 'u' },
	{ KEYC_KP_SLASH|KEYC_KEYPAD, 57410, 'u' },
	{ KEYC_KP_STAR|KEYC_KEYPAD, 57411, 'u' },
	{ KEYC_KP_MINUS|KEYC_KEYPAD, 57412, 'u' },
	{ KEYC_KP_PLUS|KEYC_KEYPAD, 57413, 'u' },
	{ KEYC_KP_ENTER|KEYC_KEYPAD, 57414, 'u' },
	{ KEYC_KP_EQUAL|KEYC_KEYPAD, 57415, 'u' },
	{ KEYC_KP_SEPARATOR|KEYC_KEYPAD, 57416, 'u' },
	{ KEYC_KP_LEFT|KEYC_KEYPAD, 57417, 'u' },
	{ KEYC_KP_RIGHT|KEYC_KEYPAD, 57418, 'u' },
	{ KEYC_KP_UP|KEYC_KEYPAD, 57419, 'u' },
	{ KEYC_KP_DOWN|KEYC_KEYPAD, 57420, 'u' },
	{ KEYC_KP_PPAGE|KEYC_KEYPAD, 57421, 'u' },
	{ KEYC_KP_NPAGE|KEYC_KEYPAD, 57422, 'u' },
	{ KEYC_KP_HOME|KEYC_KEYPAD, 57423, 'u' },
	{ KEYC_KP_END|KEYC_KEYPAD, 57424, 'u' },
	{ KEYC_KP_IC|KEYC_KEYPAD, 57425, 'u' },
	{ KEYC_KP_DC|KEYC_KEYPAD, 57426, 'u' },
	{ KEYC_KP_BEGIN|KEYC_KEYPAD, 57427, '~' },
	{ KEYC_MEDIA_PLAY, 57428, 'u' },
	{ KEYC_MEDIA_PAUSE, 57429, 'u' },
	{ KEYC_MEDIA_PLAYPAUSE, 57430, 'u' },
	{ KEYC_MEDIA_REVERSE, 57431, 'u' },
	{ KEYC_MEDIA_STOP, 57432, 'u' },
	{ KEYC_MEDIA_FASTFORWARD, 57433, 'u' },
	{ KEYC_MEDIA_REWIND, 57434, 'u' },
	{ KEYC_MEDIA_NEXT, 57435, 'u' },
	{ KEYC_MEDIA_PREVIOUS, 57436, 'u' },
	{ KEYC_MEDIA_RECORD, 57437, 'u' },
	{ KEYC_VOLUME_DOWN, 57438, 'u' },
	{ KEYC_VOLUME_UP, 57439, 'u' },
	{ KEYC_VOLUME_MUTE, 57440, 'u' },
	{ KEYC_LEFT_SHIFT, 57441, 'u' },
	{ KEYC_LEFT_CTRL, 57442, 'u' },
	{ KEYC_LEFT_ALT, 57443, 'u' },
	{ KEYC_LEFT_SUPER, 57444, 'u' },
	{ KEYC_LEFT_HYPER, 57445, 'u' },
	{ KEYC_LEFT_META, 57446, 'u' },
	{ KEYC_RIGHT_SHIFT, 57447, 'u' },
	{ KEYC_RIGHT_CTRL, 57448, 'u' },
	{ KEYC_RIGHT_ALT, 57449, 'u' },
	{ KEYC_RIGHT_SUPER, 57450, 'u' },
	{ KEYC_RIGHT_HYPER, 57451, 'u' },
	{ KEYC_RIGHT_META, 57452, 'u' },
	{ KEYC_ISO_LEVEL3_SHIFT, 57453, 'u' },
	{ KEYC_ISO_LEVEL5_SHIFT, 57454, 'u' }
};

static u_int
input_kitty_default_flags(void)
{
	if (options_get_number(global_options, "extended-keys") == 2 &&
	    options_get_number(global_options, "extended-keys-format") ==
	    EXTENDED_KEYS_KITTY)
		return (KITTY_KEY_DISAMBIGUATE);
	return (0);
}

void
input_kitty_reset(struct screen *s)
{
	memset(&s->kitty_keys, 0, sizeof s->kitty_keys);
	s->kitty_keys.flags = input_kitty_default_flags();
}

void
input_kitty_alternate_on(struct screen *s)
{
	s->saved_kitty_keys = s->kitty_keys;
	input_kitty_reset(s);
}

void
input_kitty_alternate_off(struct screen *s)
{
	s->kitty_keys = s->saved_kitty_keys;
	memset(&s->saved_kitty_keys, 0, sizeof s->saved_kitty_keys);
}

void
input_kitty_set(struct screen *s, u_int flags, int mode)
{
	flags &= KITTY_KEY_SUPPORTED;
	if (mode == 2)
		s->kitty_keys.flags |= flags;
	else if (mode == 3)
		s->kitty_keys.flags &= ~flags;
	else
		s->kitty_keys.flags = flags;
	s->kitty_keys.flags |= input_kitty_default_flags();
}

void
input_kitty_push(struct screen *s, u_int flags)
{
	s->kitty_keys.saved_flags = s->kitty_keys.flags;
	s->kitty_keys.have_saved = 1;
	s->kitty_keys.flags = (flags & KITTY_KEY_SUPPORTED)|
	    input_kitty_default_flags();
}

void
input_kitty_pop(struct screen *s, u_int count)
{
	if (count == 0)
		return;
	if (s->kitty_keys.have_saved && count == 1)
		s->kitty_keys.flags = s->kitty_keys.saved_flags;
	else
		s->kitty_keys.flags = input_kitty_default_flags();
	s->kitty_keys.have_saved = 0;
	s->kitty_keys.flags |= input_kitty_default_flags();
}

static u_int
input_kitty_modifiers(key_code key)
{
	u_int	modifiers = 1;

	if (key & KEYC_SHIFT)
		modifiers += 1;
	if (key & KEYC_META)
		modifiers += 2;
	if (key & KEYC_CTRL)
		modifiers += 4;
	if (key & KEYC_SUPER)
		modifiers += 8;
	if (key & KEYC_HYPER)
		modifiers += 16;
	return (modifiers);
}

static const struct input_kitty_key *
input_kitty_lookup(key_code key)
{
	key_code	lookup;
	u_int		i;

	lookup = key & (KEYC_MASK_KEY|KEYC_KEYPAD);
	for (i = 0; i < nitems(input_kitty_keys); i++) {
		if (input_kitty_keys[i].key == lookup)
			return (&input_kitty_keys[i]);
	}
	return (NULL);
}

int
input_key_kitty(struct screen *s, struct bufferevent *bev, key_code key)
{
	const struct input_kitty_key	*ikk;
	struct utf8_data			 ud;
	wchar_t				 wc;
	key_code			 modifiers, onlykey;
	u_int				 flags, number, modifier;
	char				 tmp[64];
	size_t				 size;

	flags = s->kitty_keys.flags & KITTY_KEY_SUPPORTED;
	if (flags == 0)
		return (-1);
	if ((key & KEYC_MASK_KEY) == KEYC_BTAB)
		key = C0_HT|(key & ~KEYC_MASK_KEY)|KEYC_SHIFT;

	modifiers = key & (KEYC_SHIFT|KEYC_META|KEYC_CTRL|KEYC_SUPER|KEYC_HYPER);
	ikk = input_kitty_lookup(key);
	if (ikk != NULL) {
		if ((flags & KITTY_KEY_REPORT_ALL) == 0) {
			if (ikk->key == C0_CR || ikk->key == C0_HT ||
			    ikk->key == KEYC_BSPACE)
				return (-1);
			if (ikk->final != 'u' &&
			    (modifiers & (KEYC_SUPER|KEYC_HYPER)) == 0)
				return (-1);
		}
		number = ikk->number;
		modifier = input_kitty_modifiers(key);
		if (ikk->final == 'u') {
			if (modifier == 1)
				size = xsnprintf(tmp, sizeof tmp, "\033[%uu", number);
			else
				size = xsnprintf(tmp, sizeof tmp, "\033[%u;%uu",
				    number, modifier);
		} else if (ikk->final == '~') {
			if (modifier == 1)
				size = xsnprintf(tmp, sizeof tmp, "\033[%u~", number);
			else
				size = xsnprintf(tmp, sizeof tmp, "\033[%u;%u~",
				    number, modifier);
		} else {
			if (modifier == 1)
				size = xsnprintf(tmp, sizeof tmp, "\033[%c",
				    ikk->final);
			else
				size = xsnprintf(tmp, sizeof tmp, "\033[1;%u%c",
				    modifier, ikk->final);
		}
		goto write;
	}

	if ((flags & KITTY_KEY_REPORT_ALL) == 0 &&
	    (modifiers & (KEYC_META|KEYC_CTRL|KEYC_SUPER|KEYC_HYPER)) == 0)
		return (-1);

	onlykey = key & KEYC_MASK_KEY;
	if (KEYC_IS_UNICODE(key)) {
		utf8_to_data(onlykey, &ud);
		if (utf8_towc(&ud, &wc) != UTF8_DONE)
			return (-1);
		number = wc;
	} else if (onlykey <= 0x7f)
		number = onlykey;
	else
		return (-1);
	if ((key & KEYC_SHIFT) && number >= 'A' && number <= 'Z')
		number += 'a' - 'A';

	modifier = input_kitty_modifiers(key);
	if (modifier == 1)
		size = xsnprintf(tmp, sizeof tmp, "\033[%uu", number);
	else
		size = xsnprintf(tmp, sizeof tmp, "\033[%u;%uu", number,
		    modifier);

write:
	log_debug("%s: %.*s", __func__, (int)size, tmp);
	bufferevent_write(bev, tmp, size);
	return (0);
}
