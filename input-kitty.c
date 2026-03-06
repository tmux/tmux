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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/* Kitty function keys (CSI u encoding). */
static const struct {
	key_code	key;
	u_int		number;
} input_key_kitty_function[] = {
	{ KEYC_BSPACE,			127 },
	{ KEYC_CAPS_LOCK_KEY,		57358 },
	{ KEYC_SCROLL_LOCK,		57359 },
	{ KEYC_NUM_LOCK_KEY,		57360 },
	{ KEYC_PRINT,			57361 },
	{ KEYC_PAUSE,			57362 },
	{ KEYC_MENU,			57363 },
	{ KEYC_F13,			57376 },
	{ KEYC_F14,			57377 },
	{ KEYC_F15,			57378 },
	{ KEYC_F16,			57379 },
	{ KEYC_F17,			57380 },
	{ KEYC_F18,			57381 },
	{ KEYC_F19,			57382 },
	{ KEYC_F20,			57383 },
	{ KEYC_F21,			57384 },
	{ KEYC_F22,			57385 },
	{ KEYC_F23,			57386 },
	{ KEYC_F24,			57387 },
	{ KEYC_F25,			57388 },
	{ KEYC_F26,			57389 },
	{ KEYC_F27,			57390 },
	{ KEYC_F28,			57391 },
	{ KEYC_F29,			57392 },
	{ KEYC_F30,			57393 },
	{ KEYC_F31,			57394 },
	{ KEYC_F32,			57395 },
	{ KEYC_F33,			57396 },
	{ KEYC_F34,			57397 },
	{ KEYC_F35,			57398 },
	{ KEYC_KP_ZERO,			57399 },
	{ KEYC_KP_ONE,			57400 },
	{ KEYC_KP_TWO,			57401 },
	{ KEYC_KP_THREE,		57402 },
	{ KEYC_KP_FOUR,			57403 },
	{ KEYC_KP_FIVE,			57404 },
	{ KEYC_KP_SIX,			57405 },
	{ KEYC_KP_SEVEN,		57406 },
	{ KEYC_KP_EIGHT,		57407 },
	{ KEYC_KP_NINE,			57408 },
	{ KEYC_KP_PERIOD,		57409 },
	{ KEYC_KP_SLASH,		57410 },
	{ KEYC_KP_STAR,			57411 },
	{ KEYC_KP_MINUS,		57412 },
	{ KEYC_KP_PLUS,			57413 },
	{ KEYC_KP_ENTER,		57414 },
	{ KEYC_KP_EQUAL,		57415 },
	{ KEYC_KP_SEPARATOR,		57416 },
	{ KEYC_KP_LEFT,			57417 },
	{ KEYC_KP_RIGHT,		57418 },
	{ KEYC_KP_UP,			57419 },
	{ KEYC_KP_DOWN,			57420 },
	{ KEYC_KP_PAGE_UP,		57421 },
	{ KEYC_KP_PAGE_DOWN,		57422 },
	{ KEYC_KP_HOME,			57423 },
	{ KEYC_KP_END,			57424 },
	{ KEYC_KP_INSERT,		57425 },
	{ KEYC_KP_DELETE,		57426 },
	{ KEYC_KP_BEGIN,		57427 },
	{ KEYC_MEDIA_PLAY,		57428 },
	{ KEYC_MEDIA_PAUSE,		57429 },
	{ KEYC_MEDIA_PLAY_PAUSE,	57430 },
	{ KEYC_MEDIA_REVERSE,		57431 },
	{ KEYC_MEDIA_STOP,		57432 },
	{ KEYC_MEDIA_FAST_FORWARD,	57433 },
	{ KEYC_MEDIA_REWIND,		57434 },
	{ KEYC_MEDIA_NEXT,		57435 },
	{ KEYC_MEDIA_PREVIOUS,		57436 },
	{ KEYC_MEDIA_RECORD,		57437 },
	{ KEYC_VOLUME_DOWN,		57438 },
	{ KEYC_VOLUME_UP,		57439 },
	{ KEYC_VOLUME_MUTE,		57440 },
	{ KEYC_LEFT_SHIFT,		57441 },
	{ KEYC_LEFT_CONTROL,		57442 },
	{ KEYC_LEFT_ALT,		57443 },
	{ KEYC_LEFT_SUPER,		57444 },
	{ KEYC_LEFT_HYPER,		57445 },
	{ KEYC_LEFT_META,		57446 },
	{ KEYC_RIGHT_SHIFT,		57447 },
	{ KEYC_RIGHT_CONTROL,		57448 },
	{ KEYC_RIGHT_ALT,		57449 },
	{ KEYC_RIGHT_SUPER,		57450 },
	{ KEYC_RIGHT_HYPER,		57451 },
	{ KEYC_RIGHT_META,		57452 },
	{ KEYC_ISO_LEVEL3_SHIFT,	57453 },
	{ KEYC_ISO_LEVEL5_SHIFT,	57454 },
};

/* VT10x function keys (CSI ~ or CSI letter encoding). */
static const struct {
	key_code	key;
	u_int		number;
	char		letter;
} input_key_kitty_vt10x[] = {
	{ KEYC_UP,		0,	'A' },
	{ KEYC_DOWN,		0,	'B' },
	{ KEYC_RIGHT,		0,	'C' },
	{ KEYC_LEFT,		0,	'D' },
	{ KEYC_KP_BEGIN,	0,	'E' },
	{ KEYC_END,		0,	'F' },
	{ KEYC_HOME,		0,	'H' },
	{ KEYC_F1,		0,	'P' },
	{ KEYC_F2,		0,	'Q' },
	{ KEYC_F4,		0,	'S' },
	{ KEYC_IC,		2,	0 },
	{ KEYC_DC,		3,	0 },
	{ KEYC_PPAGE,		5,	0 },
	{ KEYC_NPAGE,		6,	0 },
	{ KEYC_F3,		13,	0 },
	{ KEYC_F5,		15,	0 },
	{ KEYC_F6,		17,	0 },
	{ KEYC_F7,		18,	0 },
	{ KEYC_F8,		19,	0 },
	{ KEYC_F9,		20,	0 },
	{ KEYC_F10,		21,	0 },
	{ KEYC_F11,		23,	0 },
	{ KEYC_F12,		24,	0 },
};

static void
input_key_kitty_write(const char *from, struct bufferevent *bev,
    const char *data, size_t size)
{
	log_debug("%s: %.*s", from, (int)size, data);
	bufferevent_write(bev, data, size);
}

/* Encode kitty keyboard protocol modifier bits. */
static u_int
input_key_kitty_modifiers(key_code key)
{
	u_int	mod = 0;

	if (key & KEYC_SHIFT)
		mod |= 0x01;
	if (key & KEYC_META)
		mod |= 0x02;
	if (key & KEYC_CTRL)
		mod |= 0x04;
	if (key & KEYC_SUPER)
		mod |= 0x08;
	if (key & KEYC_HYPER)
		mod |= 0x10;
	if (key & KEYC_CAPS_LOCK)
		mod |= 0x40;
	if (key & KEYC_NUM_LOCK)
		mod |= 0x80;
	return (mod == 0) ? 0 : mod + 1;
}

/* Encode a key using the kitty keyboard protocol. */
int
input_key_kitty(struct screen *s, struct bufferevent *bev, key_code key)
{
	char		tmp[64];
	key_code	onlykey;
	u_int		mod, number, i, flags;

	flags = s->kitty_kbd.flags & KITTY_KBD_SUPPORTED;
	if (flags == 0)
		return (-1);

	onlykey = key & KEYC_MASK_KEY;
	/* Only include caps/num lock if report-all-keys (flag 8+). */
	if (!(flags & KITTY_KBD_REPORT_ALL))
		key &= ~(KEYC_CAPS_LOCK|KEYC_NUM_LOCK);
	mod = input_key_kitty_modifiers(key);

	/*
	 * Without report-all-keys, Tab, Enter, and Backspace without
	 * modifiers use VT10x encoding (raw bytes 0x09, 0x0d, 0x7f).
	 */
	if (!(flags & KITTY_KBD_REPORT_ALL) && mod == 0) {
		if (onlykey == 9 || onlykey == 13 || onlykey == KEYC_BSPACE ||
		    onlykey == 127 || onlykey == 8)
			return (-1);
	}

	/* Check if it is in the VT10x table (arrows, F-keys, etc). */
	for (i = 0; i < nitems(input_key_kitty_vt10x); i++) {
		if (input_key_kitty_vt10x[i].key == onlykey) {
			if (input_key_kitty_vt10x[i].letter != 0) {
				if (mod > 0)
					xsnprintf(tmp, sizeof tmp,
					    "\033[1;%u%c", mod,
					    input_key_kitty_vt10x[i].letter);
				else
					xsnprintf(tmp, sizeof tmp,
					    "\033[%c",
					    input_key_kitty_vt10x[i].letter);
			} else {
				if (mod > 0)
					xsnprintf(tmp, sizeof tmp,
					    "\033[%u;%u~",
					    input_key_kitty_vt10x[i].number,
					    mod);
				else
					xsnprintf(tmp, sizeof tmp,
					    "\033[%u~",
					    input_key_kitty_vt10x[i].number);
			}
			input_key_kitty_write(__func__, bev, tmp, strlen(tmp));
			return (0);
		}
	}

	/* Check kitty function key table. */
	for (i = 0; i < nitems(input_key_kitty_function); i++) {
		if (input_key_kitty_function[i].key == onlykey) {
			number = input_key_kitty_function[i].number;
			if (mod > 0)
				xsnprintf(tmp, sizeof tmp,
				    "\033[%u;%uu", number, mod);
			else
				xsnprintf(tmp, sizeof tmp,
				    "\033[%uu", number);
			input_key_kitty_write(__func__, bev, tmp, strlen(tmp));
			return (0);
		}
	}

	/* ESC always uses CSI u in kitty mode. */
	if (onlykey == 27) {
		if (mod > 0)
			xsnprintf(tmp, sizeof tmp, "\033[27;%uu", mod);
		else
			xsnprintf(tmp, sizeof tmp, "\033[27u");
		input_key_kitty_write(__func__, bev, tmp, strlen(tmp));
		return (0);
	}

	/* Unicode/ASCII keys. */
	if (KEYC_IS_UNICODE(key) || onlykey <= 0x7f) {
		/*
		 * In disambiguate-only mode, unmodified printable keys:
		 * use VT10x encoding.
		 */
		if (!(flags & KITTY_KBD_REPORT_ALL) && mod == 0 &&
		    onlykey >= 0x20 && onlykey <= 0x7e)
			return (-1);

		number = (u_int)(onlykey & KEYC_MASK_KEY);
		if (mod > 0)
			xsnprintf(tmp, sizeof tmp, "\033[%u;%uu",
			    number, mod);
		else
			xsnprintf(tmp, sizeof tmp, "\033[%uu", number);
		input_key_kitty_write(__func__, bev, tmp, strlen(tmp));
		return (0);
	}

	/* Unhandled key - fall back to VT10x encoding. */
	return (-1);
}
