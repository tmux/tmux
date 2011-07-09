/* $Id$ */

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
#include <sys/time.h>

#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Handle keys input from the outside terminal. tty_keys[] is a base table of
 * supported keys which are looked up in terminfo(5) and translated into a
 * ternary tree (a binary tree of binary trees).
 */

void		tty_keys_add1(struct tty_key **, const char *, int);
void		tty_keys_add(struct tty *, const char *, int);
void		tty_keys_free1(struct tty_key *);
struct tty_key *tty_keys_find1(
		    struct tty_key *, const char *, size_t, size_t *);
struct tty_key *tty_keys_find(struct tty *, const char *, size_t, size_t *);
void		tty_keys_callback(int, short, void *);
int		tty_keys_mouse(struct tty *,
		    const char *, size_t, size_t *, struct mouse_event *);

struct tty_key_ent {
	enum tty_code_code	code;
	const char	       *string;

	int	 	 	key;
	int		 	flags;
#define TTYKEY_RAW 0x1
};

/*
 * Default key tables. Those flagged with TTYKEY_RAW are inserted directly,
 * otherwise they are looked up in terminfo(5).
 */
const struct tty_key_ent tty_keys[] = {
	/*
	 * Numeric keypad. Just use the vt100 escape sequences here and always
	 * put the terminal into keypad_xmit mode. Translation of numbers
	 * mode/applications mode is done in input-keys.c.
	 */
	{ 0,	"\033Oo",	KEYC_KP_SLASH,		TTYKEY_RAW },
	{ 0,	"\033Oj",	KEYC_KP_STAR,		TTYKEY_RAW },
	{ 0,	"\033Om",	KEYC_KP_MINUS,		TTYKEY_RAW },
	{ 0,	"\033Ow",	KEYC_KP_SEVEN,		TTYKEY_RAW },
	{ 0,	"\033Ox",	KEYC_KP_EIGHT,		TTYKEY_RAW },
	{ 0,	"\033Oy",	KEYC_KP_NINE,		TTYKEY_RAW },
	{ 0,	"\033Ok",	KEYC_KP_PLUS,		TTYKEY_RAW },
	{ 0,	"\033Ot",	KEYC_KP_FOUR,		TTYKEY_RAW },
	{ 0,	"\033Ou",	KEYC_KP_FIVE,		TTYKEY_RAW },
	{ 0,	"\033Ov",	KEYC_KP_SIX,		TTYKEY_RAW },
	{ 0,	"\033Oq",	KEYC_KP_ONE,		TTYKEY_RAW },
	{ 0,	"\033Or",	KEYC_KP_TWO,		TTYKEY_RAW },
	{ 0,	"\033Os",	KEYC_KP_THREE,		TTYKEY_RAW },
	{ 0,	"\033OM",	KEYC_KP_ENTER,		TTYKEY_RAW },
	{ 0,	"\033Op",	KEYC_KP_ZERO,		TTYKEY_RAW },
	{ 0,	"\033On",	KEYC_KP_PERIOD,		TTYKEY_RAW },

	/* Arrow keys. */
	{ 0,	"\033OA",	KEYC_UP,		TTYKEY_RAW },
	{ 0,	"\033OB",	KEYC_DOWN,		TTYKEY_RAW },
	{ 0,	"\033OC",	KEYC_RIGHT,		TTYKEY_RAW },
	{ 0,	"\033OD",	KEYC_LEFT,		TTYKEY_RAW },

	{ 0,	"\033[A",	KEYC_UP,		TTYKEY_RAW },
	{ 0,	"\033[B",	KEYC_DOWN,		TTYKEY_RAW },
	{ 0,	"\033[C",	KEYC_RIGHT,		TTYKEY_RAW },
	{ 0,	"\033[D",	KEYC_LEFT,		TTYKEY_RAW },

	/* rxvt-style arrow + modifier keys. */
	{ 0,	"\033Oa",	KEYC_UP|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,	"\033Ob",	KEYC_DOWN|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,	"\033Oc",	KEYC_RIGHT|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,	"\033Od",	KEYC_LEFT|KEYC_CTRL,	TTYKEY_RAW },

	{ 0,	"\033[a",	KEYC_UP|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[b",	KEYC_DOWN|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[c",	KEYC_RIGHT|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[d",	KEYC_LEFT|KEYC_SHIFT,	TTYKEY_RAW },

	/*
	 * rxvt-style function + modifier keys:
	 *		Ctrl = ^, Shift = $, Ctrl+Shift = @
	 */
	{ 0,	"\033[11^",	KEYC_F1|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,	"\033[12^",	KEYC_F2|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,	"\033[13^",	KEYC_F3|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,	"\033[14^",	KEYC_F4|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,	"\033[15^",	KEYC_F5|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,	"\033[17^",	KEYC_F6|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,	"\033[18^",	KEYC_F7|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,	"\033[19^",	KEYC_F8|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,	"\033[20^",	KEYC_F9|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,	"\033[21^",	KEYC_F10|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,	"\033[23^",	KEYC_F11|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,	"\033[24^",	KEYC_F12|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,	"\033[25^",	KEYC_F13|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,	"\033[26^",	KEYC_F14|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,	"\033[28^",	KEYC_F15|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,	"\033[29^",	KEYC_F16|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,	"\033[31^",	KEYC_F17|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,	"\033[32^",	KEYC_F18|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,	"\033[33^",	KEYC_F19|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,	"\033[34^",	KEYC_F20|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,	"\033[2^",	KEYC_IC|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,	"\033[3^",	KEYC_DC|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,	"\033[7^",	KEYC_HOME|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,	"\033[8^",	KEYC_END|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,	"\033[6^",	KEYC_NPAGE|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,	"\033[5^",	KEYC_PPAGE|KEYC_CTRL,	TTYKEY_RAW },

	{ 0,	"\033[11$",	KEYC_F1|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[12$",	KEYC_F2|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[13$",	KEYC_F3|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[14$",	KEYC_F4|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[15$",	KEYC_F5|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[17$",	KEYC_F6|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[18$",	KEYC_F7|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[19$",	KEYC_F8|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[20$",	KEYC_F9|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[21$",	KEYC_F10|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[23$",	KEYC_F11|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[24$",	KEYC_F12|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[25$",	KEYC_F13|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[26$",	KEYC_F14|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[28$",	KEYC_F15|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[29$",	KEYC_F16|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[31$",	KEYC_F17|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[32$",	KEYC_F18|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[33$",	KEYC_F19|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[34$",	KEYC_F20|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[2$",	KEYC_IC|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[3$",	KEYC_DC|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[7$",	KEYC_HOME|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[8$",	KEYC_END|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[6$",	KEYC_NPAGE|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[5$",	KEYC_PPAGE|KEYC_SHIFT,	TTYKEY_RAW },

	{ 0,	"\033[11@",	KEYC_F1|KEYC_CTRL|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[12@",	KEYC_F2|KEYC_CTRL|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[13@",	KEYC_F3|KEYC_CTRL|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[14@",	KEYC_F4|KEYC_CTRL|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[15@",	KEYC_F5|KEYC_CTRL|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[17@",	KEYC_F6|KEYC_CTRL|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[18@",	KEYC_F7|KEYC_CTRL|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[19@",	KEYC_F8|KEYC_CTRL|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[20@",	KEYC_F9|KEYC_CTRL|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[21@",	KEYC_F10|KEYC_CTRL|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[23@",	KEYC_F11|KEYC_CTRL|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[24@",	KEYC_F12|KEYC_CTRL|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[25@",	KEYC_F13|KEYC_CTRL|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[26@",	KEYC_F14|KEYC_CTRL|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[28@",	KEYC_F15|KEYC_CTRL|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[29@",	KEYC_F16|KEYC_CTRL|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[31@",	KEYC_F17|KEYC_CTRL|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[32@",	KEYC_F18|KEYC_CTRL|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[33@",	KEYC_F19|KEYC_CTRL|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[34@",	KEYC_F20|KEYC_CTRL|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[2@",	KEYC_IC|KEYC_CTRL|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[3@",	KEYC_DC|KEYC_CTRL|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[7@",	KEYC_HOME|KEYC_CTRL|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[8@",	KEYC_END|KEYC_CTRL|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,	"\033[6@",	KEYC_NPAGE|KEYC_CTRL|KEYC_SHIFT,TTYKEY_RAW },
	{ 0,	"\033[5@",	KEYC_PPAGE|KEYC_CTRL|KEYC_SHIFT,TTYKEY_RAW },

	/* terminfo lookups below this line so they can override raw keys. */

	/* Function keys. */
	{ TTYC_KF1,	NULL,		KEYC_F1,		0 },
	{ TTYC_KF2,	NULL,		KEYC_F2,		0 },
	{ TTYC_KF3,	NULL,		KEYC_F3,		0 },
	{ TTYC_KF4,	NULL,		KEYC_F4,		0 },
	{ TTYC_KF5,	NULL,		KEYC_F5,		0 },
	{ TTYC_KF6,	NULL,		KEYC_F6,		0 },
	{ TTYC_KF7,	NULL,		KEYC_F7,		0 },
	{ TTYC_KF8,	NULL,		KEYC_F8,		0 },
	{ TTYC_KF9,	NULL,		KEYC_F9,		0 },
	{ TTYC_KF10,	NULL,		KEYC_F10,		0 },
	{ TTYC_KF11,	NULL,		KEYC_F11,		0 },
	{ TTYC_KF12,	NULL,		KEYC_F12,		0 },
	{ TTYC_KF13,	NULL,		KEYC_F13,		0 },
	{ TTYC_KF14,	NULL,		KEYC_F14,		0 },
	{ TTYC_KF15,	NULL,		KEYC_F15,		0 },
	{ TTYC_KF16,	NULL,		KEYC_F16,		0 },
	{ TTYC_KF17,	NULL,		KEYC_F17,		0 },
	{ TTYC_KF18,	NULL,		KEYC_F18,		0 },
	{ TTYC_KF19,	NULL,		KEYC_F19,		0 },
	{ TTYC_KF20,	NULL,		KEYC_F20,		0 },
	{ TTYC_KICH1,	NULL,		KEYC_IC,		0 },
	{ TTYC_KDCH1,	NULL,		KEYC_DC,		0 },
	{ TTYC_KHOME,	NULL,		KEYC_HOME,		0 },
	{ TTYC_KEND,	NULL,		KEYC_END,		0 },
	{ TTYC_KNP,	NULL,		KEYC_NPAGE,		0 },
	{ TTYC_KPP,	NULL,		KEYC_PPAGE,		0 },
	{ TTYC_KCBT,	NULL,		KEYC_BTAB,		0 },

	/* Arrow keys from terminfo. */
	{ TTYC_KCUU1,	NULL,		KEYC_UP,		0 },
	{ TTYC_KCUD1,	NULL,		KEYC_DOWN,		0 },
	{ TTYC_KCUB1,	NULL,		KEYC_LEFT,		0 },
	{ TTYC_KCUF1,	NULL,		KEYC_RIGHT,		0 },

	/* Key and modifier capabilities. */
	{ TTYC_KDC2,	NULL,		KEYC_DC|KEYC_SHIFT,	0 },
	{ TTYC_KDC3,	NULL,		KEYC_DC|KEYC_ESCAPE,	0 },
	{ TTYC_KDC4,	NULL,		KEYC_DC|KEYC_SHIFT|KEYC_ESCAPE, 0 },
	{ TTYC_KDC5,	NULL,		KEYC_DC|KEYC_CTRL,	0 },
	{ TTYC_KDC6,	NULL,		KEYC_DC|KEYC_SHIFT|KEYC_CTRL, 0 },
	{ TTYC_KDC7,	NULL,		KEYC_DC|KEYC_ESCAPE|KEYC_CTRL, 0 },
	{ TTYC_KDN2,	NULL,		KEYC_DOWN|KEYC_SHIFT,	0 },
	{ TTYC_KDN3,	NULL,		KEYC_DOWN|KEYC_ESCAPE,	0 },
	{ TTYC_KDN4,	NULL,		KEYC_DOWN|KEYC_SHIFT|KEYC_ESCAPE, 0 },
	{ TTYC_KDN5,	NULL,		KEYC_DOWN|KEYC_CTRL,	0 },
	{ TTYC_KDN6,	NULL,		KEYC_DOWN|KEYC_SHIFT|KEYC_CTRL, 0 },
	{ TTYC_KDN7,	NULL,		KEYC_DOWN|KEYC_ESCAPE|KEYC_CTRL, 0 },
	{ TTYC_KEND2,	NULL,		KEYC_END|KEYC_SHIFT,	0 },
	{ TTYC_KEND3,	NULL,		KEYC_END|KEYC_ESCAPE,	0 },
	{ TTYC_KEND4,	NULL,		KEYC_END|KEYC_SHIFT|KEYC_ESCAPE, 0 },
	{ TTYC_KEND5,	NULL,		KEYC_END|KEYC_CTRL,	0 },
	{ TTYC_KEND6,	NULL,		KEYC_END|KEYC_SHIFT|KEYC_CTRL, 0 },
	{ TTYC_KEND7,	NULL,		KEYC_END|KEYC_ESCAPE|KEYC_CTRL, 0 },
	{ TTYC_KHOM2,	NULL,		KEYC_HOME|KEYC_SHIFT,	0 },
	{ TTYC_KHOM3,	NULL,		KEYC_HOME|KEYC_ESCAPE,	0 },
	{ TTYC_KHOM4,	NULL,		KEYC_HOME|KEYC_SHIFT|KEYC_ESCAPE, 0 },
	{ TTYC_KHOM5,	NULL,		KEYC_HOME|KEYC_CTRL,	0 },
	{ TTYC_KHOM6,	NULL,		KEYC_HOME|KEYC_SHIFT|KEYC_CTRL, 0 },
	{ TTYC_KHOM7,	NULL,		KEYC_HOME|KEYC_ESCAPE|KEYC_CTRL, 0 },
	{ TTYC_KIC2,	NULL,		KEYC_IC|KEYC_SHIFT,	0 },
	{ TTYC_KIC3,	NULL,		KEYC_IC|KEYC_ESCAPE,	0 },
	{ TTYC_KIC4,	NULL,		KEYC_IC|KEYC_SHIFT|KEYC_ESCAPE,	0 },
	{ TTYC_KIC5,	NULL,		KEYC_IC|KEYC_CTRL,	0 },
	{ TTYC_KIC6,	NULL,		KEYC_IC|KEYC_SHIFT|KEYC_CTRL, 0 },
	{ TTYC_KIC7,	NULL,		KEYC_IC|KEYC_ESCAPE|KEYC_CTRL, 0 },
	{ TTYC_KLFT2,	NULL,		KEYC_LEFT|KEYC_SHIFT,	0 },
	{ TTYC_KLFT3,	NULL,		KEYC_LEFT|KEYC_ESCAPE,	0 },
	{ TTYC_KLFT4,	NULL,		KEYC_LEFT|KEYC_SHIFT|KEYC_ESCAPE, 0 },
	{ TTYC_KLFT5,	NULL,		KEYC_LEFT|KEYC_CTRL,	0 },
	{ TTYC_KLFT6,	NULL,		KEYC_LEFT|KEYC_SHIFT|KEYC_CTRL, 0 },
	{ TTYC_KLFT7,	NULL,		KEYC_LEFT|KEYC_ESCAPE|KEYC_CTRL, 0 },
	{ TTYC_KNXT2,	NULL,		KEYC_NPAGE|KEYC_SHIFT,	0 },
	{ TTYC_KNXT3,	NULL,		KEYC_NPAGE|KEYC_ESCAPE,	0 },
	{ TTYC_KNXT4,	NULL,		KEYC_NPAGE|KEYC_SHIFT|KEYC_ESCAPE, 0 },
	{ TTYC_KNXT5,	NULL,		KEYC_NPAGE|KEYC_CTRL,	0 },
	{ TTYC_KNXT6,	NULL,		KEYC_NPAGE|KEYC_SHIFT|KEYC_CTRL, 0 },
	{ TTYC_KNXT7,	NULL,		KEYC_NPAGE|KEYC_ESCAPE|KEYC_CTRL, 0 },
	{ TTYC_KPRV2,	NULL,		KEYC_PPAGE|KEYC_SHIFT,	0 },
	{ TTYC_KPRV3,	NULL,		KEYC_PPAGE|KEYC_ESCAPE,	0 },
	{ TTYC_KPRV4,	NULL,		KEYC_PPAGE|KEYC_SHIFT|KEYC_ESCAPE, 0 },
	{ TTYC_KPRV5,	NULL,		KEYC_PPAGE|KEYC_CTRL,	0 },
	{ TTYC_KPRV6,	NULL,		KEYC_PPAGE|KEYC_SHIFT|KEYC_CTRL, 0 },
	{ TTYC_KPRV7,	NULL,		KEYC_PPAGE|KEYC_ESCAPE|KEYC_CTRL, 0 },
	{ TTYC_KRIT2,	NULL,		KEYC_RIGHT|KEYC_SHIFT,	0 },
	{ TTYC_KRIT3,	NULL,		KEYC_RIGHT|KEYC_ESCAPE,	0 },
	{ TTYC_KRIT4,	NULL,		KEYC_RIGHT|KEYC_SHIFT|KEYC_ESCAPE, 0 },
	{ TTYC_KRIT5,	NULL,		KEYC_RIGHT|KEYC_CTRL,	0 },
	{ TTYC_KRIT6,	NULL,		KEYC_RIGHT|KEYC_SHIFT|KEYC_CTRL, 0 },
	{ TTYC_KRIT7,	NULL,		KEYC_RIGHT|KEYC_ESCAPE|KEYC_CTRL, 0 },
	{ TTYC_KUP2,	NULL,		KEYC_UP|KEYC_SHIFT,	0 },
	{ TTYC_KUP3,	NULL,		KEYC_UP|KEYC_ESCAPE,	0 },
	{ TTYC_KUP4,	NULL,		KEYC_UP|KEYC_SHIFT|KEYC_ESCAPE,	0 },
	{ TTYC_KUP5,	NULL,		KEYC_UP|KEYC_CTRL,	0 },
	{ TTYC_KUP6,	NULL,		KEYC_UP|KEYC_SHIFT|KEYC_CTRL, 0 },
	{ TTYC_KUP7,	NULL,		KEYC_UP|KEYC_ESCAPE|KEYC_CTRL, 0 },
};

void
tty_keys_add(struct tty *tty, const char *s, int key)
{
	struct tty_key	*tk;
	size_t		 size;
	const char     	*keystr;

	keystr = key_string_lookup_key(key);
	if ((tk = tty_keys_find(tty, s, strlen(s), &size)) == NULL) {
		log_debug("new key %s: 0x%x (%s)", s, key, keystr);
		tty_keys_add1(&tty->key_tree, s, key);
	} else {
		log_debug("replacing key %s: 0x%x (%s)", s, key, keystr);
		tk->key = key;
	}
}

/* Add next node to the tree. */
void
tty_keys_add1(struct tty_key **tkp, const char *s, int key)
{
	struct tty_key	*tk;

	/* Allocate a tree entry if there isn't one already. */
	tk = *tkp;
	if (tk == NULL) {
		tk = *tkp = xcalloc(1, sizeof *tk);
		tk->ch = *s;
		tk->key = KEYC_NONE;
	}

	/* Find the next entry. */
	if (*s == tk->ch) {
		/* Move forward in string. */
		s++;

		/* If this is the end of the string, no more is necessary. */
		if (*s == '\0') {
			tk->key = key;
			return;
		}

		/* Use the child tree for the next character. */
		tkp = &tk->next;
	} else {
		if (*s < tk->ch)
			tkp = &tk->left;
		else if (*s > tk->ch)
			tkp = &tk->right;
	}

	/* And recurse to add it. */
	tty_keys_add1(tkp, s, key);
}

/* Initialise a key tree from the table. */
void
tty_keys_init(struct tty *tty)
{
	const struct tty_key_ent	*tke;
	u_int		 		 i;
	const char			*s;

	tty->key_tree = NULL;
	for (i = 0; i < nitems(tty_keys); i++) {
		tke = &tty_keys[i];

		if (tke->flags & TTYKEY_RAW)
			s = tke->string;
		else {
			if (!tty_term_has(tty->term, tke->code))
				continue;
			s = tty_term_string(tty->term, tke->code);
		}
		if (s[0] != '\033' || s[1] == '\0')
			continue;

		tty_keys_add(tty, s + 1, tke->key);
	}
}

/* Free the entire key tree. */
void
tty_keys_free(struct tty *tty)
{
	tty_keys_free1(tty->key_tree);
}

/* Free a single key. */
void
tty_keys_free1(struct tty_key *tk)
{
	if (tk->next != NULL)
		tty_keys_free1(tk->next);
	if (tk->left != NULL)
		tty_keys_free1(tk->left);
	if (tk->right != NULL)
		tty_keys_free1(tk->right);
	xfree(tk);
}

/* Lookup a key in the tree. */
struct tty_key *
tty_keys_find(struct tty *tty, const char *buf, size_t len, size_t *size)
{
	*size = 0;
	return (tty_keys_find1(tty->key_tree, buf, len, size));
}

/* Find the next node. */
struct tty_key *
tty_keys_find1(struct tty_key *tk, const char *buf, size_t len, size_t *size)
{
	/* If the node is NULL, this is the end of the tree. No match. */
	if (tk == NULL)
		return (NULL);

	/* Pick the next in the sequence. */
	if (tk->ch == *buf) {
		/* Move forward in the string. */
		buf++; len--;
		(*size)++;

		/* At the end of the string, return the current node. */
		if (len == 0 || (tk->next == NULL && tk->key != KEYC_NONE))
			return (tk);

		/* Move into the next tree for the following character. */
		tk = tk->next;
	} else {
		if (*buf < tk->ch)
			tk = tk->left;
		else if (*buf > tk->ch)
			tk = tk->right;
	}

	/* Move to the next in the tree. */
	return (tty_keys_find1(tk, buf, len, size));
}

/*
 * Process at least one key in the buffer and invoke tty->key_callback. Return
 * 0 if there are no further keys, or 1 if there could be more in the buffer.
 */
int
tty_keys_next(struct tty *tty)
{
	struct tty_key		*tk;
	struct timeval		 tv;
	struct mouse_event	 mouse;
	const char		*buf;
	size_t			 len, size;
	cc_t			 bspace;
	int			 key, delay;

	buf = EVBUFFER_DATA(tty->event->input);
	len = EVBUFFER_LENGTH(tty->event->input);
	if (len == 0)
		return (0);
	log_debug("keys are %zu (%.*s)", len, (int) len, buf);

	/* If a normal key, return it. */
	if (*buf != '\033') {
		key = (u_char) *buf;
		evbuffer_drain(tty->event->input, 1);

		/*
		 * Check for backspace key using termios VERASE - the terminfo
		 * kbs entry is extremely unreliable, so cannot be safely
		 * used. termios should have a better idea.
		 */
		bspace = tty->tio.c_cc[VERASE];
		if (bspace != _POSIX_VDISABLE && key == bspace)
			key = KEYC_BSPACE;
		goto handle_key;
	}

	/* Is this a mouse key press? */
	switch (tty_keys_mouse(tty, buf, len, &size, &mouse)) {
	case 0:		/* yes */
		evbuffer_drain(tty->event->input, size);
		key = KEYC_MOUSE;
		goto handle_key;
	case -1:	/* no, or not valid */
		break;
	case 1:		/* partial */
		goto partial_key;
	}

	/* Try to parse a key with an xterm-style modifier. */
	switch (xterm_keys_find(buf, len, &size, &key)) {
	case 0:		/* found */
		evbuffer_drain(tty->event->input, size);
		goto handle_key;
	case -1:	/* not found */
		break;
	case 1:
		goto partial_key;
	}

	/* Look for matching key string and return if found. */
	tk = tty_keys_find(tty, buf + 1, len - 1, &size);
	if (tk != NULL) {
		key = tk->key;
		goto found_key;
	}

	/* Skip the escape. */
	buf++;
	len--;

	/* Is there a normal key following? */
	if (len != 0 && *buf != '\033') {
		key = *buf | KEYC_ESCAPE;
		evbuffer_drain(tty->event->input, 2);
		goto handle_key;
	}

	/* Or a key string? */
	if (len > 1) {
		tk = tty_keys_find(tty, buf + 1, len - 1, &size);
		if (tk != NULL) {
			key = tk->key | KEYC_ESCAPE;
			size++;	/* include escape */
			goto found_key;
		}
	}

	/* Escape and then nothing useful - fall through. */

partial_key:
	/*
	 * Escape but no key string. If have already seen an escape, then the
	 * timer must have expired, so give up waiting and send the escape.
	 */
	if (tty->flags & TTY_ESCAPE) {
		evbuffer_drain(tty->event->input, 1);
		key = '\033';
		goto handle_key;
	}

	/* Fall through to start the timer. */

start_timer:
	/* Start the timer and wait for expiry or more data. */
	delay = options_get_number(&global_options, "escape-time");
	tv.tv_sec = delay / 1000;
	tv.tv_usec = (delay % 1000) * 1000L;

	evtimer_del(&tty->key_timer);
	evtimer_set(&tty->key_timer, tty_keys_callback, tty);
	evtimer_add(&tty->key_timer, &tv);

	tty->flags |= TTY_ESCAPE;
	return (0);

found_key:
	if (tk->next != NULL) {
		/* Partial key. Start the timer if not already expired. */
		if (!(tty->flags & TTY_ESCAPE))
			goto start_timer;

		/* Otherwise, if no key, send the escape alone. */
		if (tk->key == KEYC_NONE)
			goto partial_key;

		/* Or fall through to send the partial key found. */
	}
	evbuffer_drain(tty->event->input, size + 1);

	goto handle_key;

handle_key:
	evtimer_del(&tty->key_timer);

	tty->key_callback(key, &mouse, tty->key_data);

	tty->flags &= ~TTY_ESCAPE;
	return (1);
}

/* Key timer callback. */
/* ARGSUSED */
void
tty_keys_callback(unused int fd, unused short events, void *data)
{
	struct tty	*tty = data;

	if (!(tty->flags & TTY_ESCAPE))
		return;

	while (tty_keys_next(tty))
		;
}

/*
 * Handle mouse key input. Returns 0 for success, -1 for failure, 1 for partial
 * (probably a mouse sequence but need more data).
 */
int
tty_keys_mouse(struct tty *tty,
    const char *buf, size_t len, size_t *size, struct mouse_event *m)
{
	struct utf8_data	utf8data;
	u_int			i, value;

	/*
	 * Standard mouse sequences are \033[M followed by three characters
	 * indicating buttons, X and Y, all based at 32 with 1,1 top-left.
	 *
	 * UTF-8 mouse sequences are similar but the three are expressed as
	 * UTF-8 characters.
	 */

	*size = 0;

	/* First three bytes are always \033[M. */
	if (buf[0] != '\033')
		return (-1);
	if (len == 1)
		return (1);
	if (buf[1] != '[')
		return (-1);
	if (len == 2)
		return (1);
	if (buf[2] != 'M')
		return (-1);
	if (len == 3)
		return (1);

	/* Read the three inputs. */
	*size = 3;
	for (i = 0; i < 3; i++) {
		if (len < *size)
			return (1);

		if (tty->mode & MODE_MOUSE_UTF8) {
			if (utf8_open(&utf8data, buf[*size])) {
				if (utf8data.size != 2)
					return (-1);
				(*size)++;
				if (len < *size)
					return (1);
				utf8_append(&utf8data, buf[*size]);
				value = utf8_combine(&utf8data);
			} else
				value = (unsigned char)buf[*size];
			(*size)++;
		} else {
			value = (unsigned char)buf[*size];
			(*size)++;
		}

		if (i == 0)
			m->b = value;
		else if (i == 1)
			m->x = value;
		else
			m->y = value;
	}
	log_debug("mouse input: %.*s", (int) *size, buf);

	/* Check and return the mouse input. */
	if (m->b < 32 || m->x < 33 || m->y < 33)
		return (-1);
	m->b -= 32;
	m->x -= 33;
	m->y -= 33;
	log_debug("mouse position: x=%u y=%u b=%u", m->x, m->y, m->b);
	return (0);
}
