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
#include <sys/time.h>

#include <netinet/in.h>

#include <ctype.h>
#include <limits.h>
#include <resolv.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Handle keys input from the outside terminal. tty_default_*_keys[] are a base
 * table of supported keys which are looked up in terminfo(5) and translated
 * into a ternary tree.
 */

static void	tty_keys_add1(struct tty_key **, const char *, key_code);
static void	tty_keys_add(struct tty *, const char *, key_code);
static void	tty_keys_free1(struct tty_key *);
static struct tty_key *tty_keys_find1(struct tty_key *, const char *, size_t,
		    size_t *);
static struct tty_key *tty_keys_find(struct tty *, const char *, size_t,
		    size_t *);
static int	tty_keys_next1(struct tty *, const char *, size_t, key_code *,
		    size_t *, int);
static void	tty_keys_callback(int, short, void *);
static int	tty_keys_extended_key(struct tty *, const char *, size_t,
		    size_t *, key_code *);
static int	tty_keys_mouse(struct tty *, const char *, size_t, size_t *,
		    struct mouse_event *);
static int	tty_keys_clipboard(struct tty *, const char *, size_t,
		    size_t *);
static int	tty_keys_device_attributes(struct tty *, const char *, size_t,
		    size_t *);
static int	tty_keys_extended_device_attributes(struct tty *, const char *,
		    size_t, size_t *);

/* Default raw keys. */
struct tty_default_key_raw {
	const char	       *string;
	key_code		key;
};
static const struct tty_default_key_raw tty_default_raw_keys[] = {
	/* Application escape. */
	{ "\033O[", '\033' },

	/*
	 * Numeric keypad. Just use the vt100 escape sequences here and always
	 * put the terminal into keypad_xmit mode. Translation of numbers
	 * mode/applications mode is done in input-keys.c.
	 */
	{ "\033Oo", KEYC_KP_SLASH|KEYC_KEYPAD },
	{ "\033Oj", KEYC_KP_STAR|KEYC_KEYPAD },
	{ "\033Om", KEYC_KP_MINUS|KEYC_KEYPAD },
	{ "\033Ow", KEYC_KP_SEVEN|KEYC_KEYPAD },
	{ "\033Ox", KEYC_KP_EIGHT|KEYC_KEYPAD },
	{ "\033Oy", KEYC_KP_NINE|KEYC_KEYPAD },
	{ "\033Ok", KEYC_KP_PLUS|KEYC_KEYPAD },
	{ "\033Ot", KEYC_KP_FOUR|KEYC_KEYPAD },
	{ "\033Ou", KEYC_KP_FIVE|KEYC_KEYPAD },
	{ "\033Ov", KEYC_KP_SIX|KEYC_KEYPAD },
	{ "\033Oq", KEYC_KP_ONE|KEYC_KEYPAD },
	{ "\033Or", KEYC_KP_TWO|KEYC_KEYPAD },
	{ "\033Os", KEYC_KP_THREE|KEYC_KEYPAD },
	{ "\033OM", KEYC_KP_ENTER|KEYC_KEYPAD },
	{ "\033Op", KEYC_KP_ZERO|KEYC_KEYPAD },
	{ "\033On", KEYC_KP_PERIOD|KEYC_KEYPAD },

	/* Arrow keys. */
	{ "\033OA", KEYC_UP|KEYC_CURSOR },
	{ "\033OB", KEYC_DOWN|KEYC_CURSOR },
	{ "\033OC", KEYC_RIGHT|KEYC_CURSOR },
	{ "\033OD", KEYC_LEFT|KEYC_CURSOR },

	{ "\033[A", KEYC_UP|KEYC_CURSOR },
	{ "\033[B", KEYC_DOWN|KEYC_CURSOR },
	{ "\033[C", KEYC_RIGHT|KEYC_CURSOR },
	{ "\033[D", KEYC_LEFT|KEYC_CURSOR },

	/*
	 * Meta arrow keys. These do not get the IMPLIED_META flag so they
	 * don't match the xterm-style meta keys in the output tree - Escape+Up
	 * should stay as Escape+Up and not become M-Up.
	 */
	{ "\033\033OA", KEYC_UP|KEYC_CURSOR|KEYC_META },
	{ "\033\033OB", KEYC_DOWN|KEYC_CURSOR|KEYC_META },
	{ "\033\033OC", KEYC_RIGHT|KEYC_CURSOR|KEYC_META },
	{ "\033\033OD", KEYC_LEFT|KEYC_CURSOR|KEYC_META },

	{ "\033\033[A", KEYC_UP|KEYC_CURSOR|KEYC_META },
	{ "\033\033[B", KEYC_DOWN|KEYC_CURSOR|KEYC_META },
	{ "\033\033[C", KEYC_RIGHT|KEYC_CURSOR|KEYC_META },
	{ "\033\033[D", KEYC_LEFT|KEYC_CURSOR|KEYC_META },

	/* Other (xterm) "cursor" keys. */
	{ "\033OH", KEYC_HOME },
	{ "\033OF", KEYC_END },

	{ "\033\033OH", KEYC_HOME|KEYC_META|KEYC_IMPLIED_META },
	{ "\033\033OF", KEYC_END|KEYC_META|KEYC_IMPLIED_META },

	{ "\033[H", KEYC_HOME },
	{ "\033[F", KEYC_END },

	{ "\033\033[H", KEYC_HOME|KEYC_META|KEYC_IMPLIED_META },
	{ "\033\033[F", KEYC_END|KEYC_META|KEYC_IMPLIED_META },

	/* rxvt-style arrow + modifier keys. */
	{ "\033Oa", KEYC_UP|KEYC_CTRL },
	{ "\033Ob", KEYC_DOWN|KEYC_CTRL },
	{ "\033Oc", KEYC_RIGHT|KEYC_CTRL },
	{ "\033Od", KEYC_LEFT|KEYC_CTRL },

	{ "\033[a", KEYC_UP|KEYC_SHIFT },
	{ "\033[b", KEYC_DOWN|KEYC_SHIFT },
	{ "\033[c", KEYC_RIGHT|KEYC_SHIFT },
	{ "\033[d", KEYC_LEFT|KEYC_SHIFT },

	/* rxvt-style function + modifier keys (C = ^, S = $, C-S = @). */
	{ "\033[11^", KEYC_F1|KEYC_CTRL },
	{ "\033[12^", KEYC_F2|KEYC_CTRL },
	{ "\033[13^", KEYC_F3|KEYC_CTRL },
	{ "\033[14^", KEYC_F4|KEYC_CTRL },
	{ "\033[15^", KEYC_F5|KEYC_CTRL },
	{ "\033[17^", KEYC_F6|KEYC_CTRL },
	{ "\033[18^", KEYC_F7|KEYC_CTRL },
	{ "\033[19^", KEYC_F8|KEYC_CTRL },
	{ "\033[20^", KEYC_F9|KEYC_CTRL },
	{ "\033[21^", KEYC_F10|KEYC_CTRL },
	{ "\033[23^", KEYC_F11|KEYC_CTRL },
	{ "\033[24^", KEYC_F12|KEYC_CTRL },
	{ "\033[2^", KEYC_IC|KEYC_CTRL },
	{ "\033[3^", KEYC_DC|KEYC_CTRL },
	{ "\033[7^", KEYC_HOME|KEYC_CTRL },
	{ "\033[8^", KEYC_END|KEYC_CTRL },
	{ "\033[6^", KEYC_NPAGE|KEYC_CTRL },
	{ "\033[5^", KEYC_PPAGE|KEYC_CTRL },

	{ "\033[11$", KEYC_F1|KEYC_SHIFT },
	{ "\033[12$", KEYC_F2|KEYC_SHIFT },
	{ "\033[13$", KEYC_F3|KEYC_SHIFT },
	{ "\033[14$", KEYC_F4|KEYC_SHIFT },
	{ "\033[15$", KEYC_F5|KEYC_SHIFT },
	{ "\033[17$", KEYC_F6|KEYC_SHIFT },
	{ "\033[18$", KEYC_F7|KEYC_SHIFT },
	{ "\033[19$", KEYC_F8|KEYC_SHIFT },
	{ "\033[20$", KEYC_F9|KEYC_SHIFT },
	{ "\033[21$", KEYC_F10|KEYC_SHIFT },
	{ "\033[23$", KEYC_F11|KEYC_SHIFT },
	{ "\033[24$", KEYC_F12|KEYC_SHIFT },
	{ "\033[2$", KEYC_IC|KEYC_SHIFT },
	{ "\033[3$", KEYC_DC|KEYC_SHIFT },
	{ "\033[7$", KEYC_HOME|KEYC_SHIFT },
	{ "\033[8$", KEYC_END|KEYC_SHIFT },
	{ "\033[6$", KEYC_NPAGE|KEYC_SHIFT },
	{ "\033[5$", KEYC_PPAGE|KEYC_SHIFT },

	{ "\033[11@", KEYC_F1|KEYC_CTRL|KEYC_SHIFT },
	{ "\033[12@", KEYC_F2|KEYC_CTRL|KEYC_SHIFT },
	{ "\033[13@", KEYC_F3|KEYC_CTRL|KEYC_SHIFT },
	{ "\033[14@", KEYC_F4|KEYC_CTRL|KEYC_SHIFT },
	{ "\033[15@", KEYC_F5|KEYC_CTRL|KEYC_SHIFT },
	{ "\033[17@", KEYC_F6|KEYC_CTRL|KEYC_SHIFT },
	{ "\033[18@", KEYC_F7|KEYC_CTRL|KEYC_SHIFT },
	{ "\033[19@", KEYC_F8|KEYC_CTRL|KEYC_SHIFT },
	{ "\033[20@", KEYC_F9|KEYC_CTRL|KEYC_SHIFT },
	{ "\033[21@", KEYC_F10|KEYC_CTRL|KEYC_SHIFT },
	{ "\033[23@", KEYC_F11|KEYC_CTRL|KEYC_SHIFT },
	{ "\033[24@", KEYC_F12|KEYC_CTRL|KEYC_SHIFT },
	{ "\033[2@", KEYC_IC|KEYC_CTRL|KEYC_SHIFT },
	{ "\033[3@", KEYC_DC|KEYC_CTRL|KEYC_SHIFT },
	{ "\033[7@", KEYC_HOME|KEYC_CTRL|KEYC_SHIFT },
	{ "\033[8@", KEYC_END|KEYC_CTRL|KEYC_SHIFT },
	{ "\033[6@", KEYC_NPAGE|KEYC_CTRL|KEYC_SHIFT },
	{ "\033[5@", KEYC_PPAGE|KEYC_CTRL|KEYC_SHIFT },

	/* Focus tracking. */
	{ "\033[I", KEYC_FOCUS_IN },
	{ "\033[O", KEYC_FOCUS_OUT },

	/* Paste keys. */
	{ "\033[200~", KEYC_PASTE_START },
	{ "\033[201~", KEYC_PASTE_END },
};

/* Default xterm keys. */
struct tty_default_key_xterm {
	const char	*template;
	key_code	 key;
};
static const struct tty_default_key_xterm tty_default_xterm_keys[] = {
	{ "\033[1;_P", KEYC_F1 },
	{ "\033O1;_P", KEYC_F1 },
	{ "\033O_P", KEYC_F1 },
	{ "\033[1;_Q", KEYC_F2 },
	{ "\033O1;_Q", KEYC_F2 },
	{ "\033O_Q", KEYC_F2 },
	{ "\033[1;_R", KEYC_F3 },
	{ "\033O1;_R", KEYC_F3 },
	{ "\033O_R", KEYC_F3 },
	{ "\033[1;_S", KEYC_F4 },
	{ "\033O1;_S", KEYC_F4 },
	{ "\033O_S", KEYC_F4 },
	{ "\033[15;_~", KEYC_F5 },
	{ "\033[17;_~", KEYC_F6 },
	{ "\033[18;_~", KEYC_F7 },
	{ "\033[19;_~", KEYC_F8 },
	{ "\033[20;_~", KEYC_F9 },
	{ "\033[21;_~", KEYC_F10 },
	{ "\033[23;_~", KEYC_F11 },
	{ "\033[24;_~", KEYC_F12 },
	{ "\033[1;_A", KEYC_UP },
	{ "\033[1;_B", KEYC_DOWN },
	{ "\033[1;_C", KEYC_RIGHT },
	{ "\033[1;_D", KEYC_LEFT },
	{ "\033[1;_H", KEYC_HOME },
	{ "\033[1;_F", KEYC_END },
	{ "\033[5;_~", KEYC_PPAGE },
	{ "\033[6;_~", KEYC_NPAGE },
	{ "\033[2;_~", KEYC_IC },
	{ "\033[3;_~", KEYC_DC },
};
static const key_code tty_default_xterm_modifiers[] = {
	0,
	0,
	KEYC_SHIFT,
	KEYC_META|KEYC_IMPLIED_META,
	KEYC_SHIFT|KEYC_META|KEYC_IMPLIED_META,
	KEYC_CTRL,
	KEYC_SHIFT|KEYC_CTRL,
	KEYC_META|KEYC_IMPLIED_META|KEYC_CTRL,
	KEYC_SHIFT|KEYC_META|KEYC_IMPLIED_META|KEYC_CTRL,
	KEYC_META|KEYC_IMPLIED_META
};

/*
 * Default terminfo(5) keys. Any keys that have builtin modifiers (that is,
 * where the key itself contains the modifiers) has the KEYC_XTERM flag set so
 * a leading escape is not treated as meta (and probably removed).
 */
struct tty_default_key_code {
	enum tty_code_code	code;
	key_code		key;
};
static const struct tty_default_key_code tty_default_code_keys[] = {
	/* Function keys. */
	{ TTYC_KF1, KEYC_F1 },
	{ TTYC_KF2, KEYC_F2 },
	{ TTYC_KF3, KEYC_F3 },
	{ TTYC_KF4, KEYC_F4 },
	{ TTYC_KF5, KEYC_F5 },
	{ TTYC_KF6, KEYC_F6 },
	{ TTYC_KF7, KEYC_F7 },
	{ TTYC_KF8, KEYC_F8 },
	{ TTYC_KF9, KEYC_F9 },
	{ TTYC_KF10, KEYC_F10 },
	{ TTYC_KF11, KEYC_F11 },
	{ TTYC_KF12, KEYC_F12 },

	{ TTYC_KF13, KEYC_F1|KEYC_SHIFT },
	{ TTYC_KF14, KEYC_F2|KEYC_SHIFT },
	{ TTYC_KF15, KEYC_F3|KEYC_SHIFT },
	{ TTYC_KF16, KEYC_F4|KEYC_SHIFT },
	{ TTYC_KF17, KEYC_F5|KEYC_SHIFT },
	{ TTYC_KF18, KEYC_F6|KEYC_SHIFT },
	{ TTYC_KF19, KEYC_F7|KEYC_SHIFT },
	{ TTYC_KF20, KEYC_F8|KEYC_SHIFT },
	{ TTYC_KF21, KEYC_F9|KEYC_SHIFT },
	{ TTYC_KF22, KEYC_F10|KEYC_SHIFT },
	{ TTYC_KF23, KEYC_F11|KEYC_SHIFT },
	{ TTYC_KF24, KEYC_F12|KEYC_SHIFT },

	{ TTYC_KF25, KEYC_F1|KEYC_CTRL },
	{ TTYC_KF26, KEYC_F2|KEYC_CTRL },
	{ TTYC_KF27, KEYC_F3|KEYC_CTRL },
	{ TTYC_KF28, KEYC_F4|KEYC_CTRL },
	{ TTYC_KF29, KEYC_F5|KEYC_CTRL },
	{ TTYC_KF30, KEYC_F6|KEYC_CTRL },
	{ TTYC_KF31, KEYC_F7|KEYC_CTRL },
	{ TTYC_KF32, KEYC_F8|KEYC_CTRL },
	{ TTYC_KF33, KEYC_F9|KEYC_CTRL },
	{ TTYC_KF34, KEYC_F10|KEYC_CTRL },
	{ TTYC_KF35, KEYC_F11|KEYC_CTRL },
	{ TTYC_KF36, KEYC_F12|KEYC_CTRL },

	{ TTYC_KF37, KEYC_F1|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KF38, KEYC_F2|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KF39, KEYC_F3|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KF40, KEYC_F4|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KF41, KEYC_F5|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KF42, KEYC_F6|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KF43, KEYC_F7|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KF44, KEYC_F8|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KF45, KEYC_F9|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KF46, KEYC_F10|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KF47, KEYC_F11|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KF48, KEYC_F12|KEYC_SHIFT|KEYC_CTRL },

	{ TTYC_KF49, KEYC_F1|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KF50, KEYC_F2|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KF51, KEYC_F3|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KF52, KEYC_F4|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KF53, KEYC_F5|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KF54, KEYC_F6|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KF55, KEYC_F7|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KF56, KEYC_F8|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KF57, KEYC_F9|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KF58, KEYC_F10|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KF59, KEYC_F11|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KF60, KEYC_F12|KEYC_META|KEYC_IMPLIED_META },

	{ TTYC_KF61, KEYC_F1|KEYC_META|KEYC_IMPLIED_META|KEYC_SHIFT },
	{ TTYC_KF62, KEYC_F2|KEYC_META|KEYC_IMPLIED_META|KEYC_SHIFT },
	{ TTYC_KF63, KEYC_F3|KEYC_META|KEYC_IMPLIED_META|KEYC_SHIFT },

	{ TTYC_KICH1, KEYC_IC },
	{ TTYC_KDCH1, KEYC_DC },
	{ TTYC_KHOME, KEYC_HOME },
	{ TTYC_KEND, KEYC_END },
	{ TTYC_KNP, KEYC_NPAGE },
	{ TTYC_KPP, KEYC_PPAGE },
	{ TTYC_KCBT, KEYC_BTAB },

	/* Arrow keys from terminfo. */
	{ TTYC_KCUU1, KEYC_UP|KEYC_CURSOR },
	{ TTYC_KCUD1, KEYC_DOWN|KEYC_CURSOR },
	{ TTYC_KCUB1, KEYC_LEFT|KEYC_CURSOR },
	{ TTYC_KCUF1, KEYC_RIGHT|KEYC_CURSOR },

	/* Key and modifier capabilities. */
	{ TTYC_KDC2, KEYC_DC|KEYC_SHIFT },
	{ TTYC_KDC3, KEYC_DC|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KDC4, KEYC_DC|KEYC_SHIFT|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KDC5, KEYC_DC|KEYC_CTRL },
	{ TTYC_KDC6, KEYC_DC|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KDC7, KEYC_DC|KEYC_META|KEYC_IMPLIED_META|KEYC_CTRL },
	{ TTYC_KIND, KEYC_DOWN|KEYC_SHIFT },
	{ TTYC_KDN2, KEYC_DOWN|KEYC_SHIFT },
	{ TTYC_KDN3, KEYC_DOWN|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KDN4, KEYC_DOWN|KEYC_SHIFT|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KDN5, KEYC_DOWN|KEYC_CTRL },
	{ TTYC_KDN6, KEYC_DOWN|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KDN7, KEYC_DOWN|KEYC_META|KEYC_IMPLIED_META|KEYC_CTRL },
	{ TTYC_KEND2, KEYC_END|KEYC_SHIFT },
	{ TTYC_KEND3, KEYC_END|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KEND4, KEYC_END|KEYC_SHIFT|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KEND5, KEYC_END|KEYC_CTRL },
	{ TTYC_KEND6, KEYC_END|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KEND7, KEYC_END|KEYC_META|KEYC_IMPLIED_META|KEYC_CTRL },
	{ TTYC_KHOM2, KEYC_HOME|KEYC_SHIFT },
	{ TTYC_KHOM3, KEYC_HOME|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KHOM4, KEYC_HOME|KEYC_SHIFT|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KHOM5, KEYC_HOME|KEYC_CTRL },
	{ TTYC_KHOM6, KEYC_HOME|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KHOM7, KEYC_HOME|KEYC_META|KEYC_IMPLIED_META|KEYC_CTRL },
	{ TTYC_KIC2, KEYC_IC|KEYC_SHIFT },
	{ TTYC_KIC3, KEYC_IC|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KIC4, KEYC_IC|KEYC_SHIFT|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KIC5, KEYC_IC|KEYC_CTRL },
	{ TTYC_KIC6, KEYC_IC|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KIC7, KEYC_IC|KEYC_META|KEYC_IMPLIED_META|KEYC_CTRL },
	{ TTYC_KLFT2, KEYC_LEFT|KEYC_SHIFT },
	{ TTYC_KLFT3, KEYC_LEFT|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KLFT4, KEYC_LEFT|KEYC_SHIFT|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KLFT5, KEYC_LEFT|KEYC_CTRL },
	{ TTYC_KLFT6, KEYC_LEFT|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KLFT7, KEYC_LEFT|KEYC_META|KEYC_IMPLIED_META|KEYC_CTRL },
	{ TTYC_KNXT2, KEYC_NPAGE|KEYC_SHIFT },
	{ TTYC_KNXT3, KEYC_NPAGE|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KNXT4, KEYC_NPAGE|KEYC_SHIFT|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KNXT5, KEYC_NPAGE|KEYC_CTRL },
	{ TTYC_KNXT6, KEYC_NPAGE|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KNXT7, KEYC_NPAGE|KEYC_META|KEYC_IMPLIED_META|KEYC_CTRL },
	{ TTYC_KPRV2, KEYC_PPAGE|KEYC_SHIFT },
	{ TTYC_KPRV3, KEYC_PPAGE|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KPRV4, KEYC_PPAGE|KEYC_SHIFT|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KPRV5, KEYC_PPAGE|KEYC_CTRL },
	{ TTYC_KPRV6, KEYC_PPAGE|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KPRV7, KEYC_PPAGE|KEYC_META|KEYC_IMPLIED_META|KEYC_CTRL },
	{ TTYC_KRIT2, KEYC_RIGHT|KEYC_SHIFT },
	{ TTYC_KRIT3, KEYC_RIGHT|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KRIT4, KEYC_RIGHT|KEYC_SHIFT|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KRIT5, KEYC_RIGHT|KEYC_CTRL },
	{ TTYC_KRIT6, KEYC_RIGHT|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KRIT7, KEYC_RIGHT|KEYC_META|KEYC_IMPLIED_META|KEYC_CTRL },
	{ TTYC_KRI, KEYC_UP|KEYC_SHIFT },
	{ TTYC_KUP2, KEYC_UP|KEYC_SHIFT },
	{ TTYC_KUP3, KEYC_UP|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KUP4, KEYC_UP|KEYC_SHIFT|KEYC_META|KEYC_IMPLIED_META },
	{ TTYC_KUP5, KEYC_UP|KEYC_CTRL },
	{ TTYC_KUP6, KEYC_UP|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KUP7, KEYC_UP|KEYC_META|KEYC_IMPLIED_META|KEYC_CTRL },
};

/* Add key to tree. */
static void
tty_keys_add(struct tty *tty, const char *s, key_code key)
{
	struct tty_key	*tk;
	size_t		 size;
	const char	*keystr;

	keystr = key_string_lookup_key(key, 1);
	if ((tk = tty_keys_find(tty, s, strlen(s), &size)) == NULL) {
		log_debug("new key %s: 0x%llx (%s)", s, key, keystr);
		tty_keys_add1(&tty->key_tree, s, key);
	} else {
		log_debug("replacing key %s: 0x%llx (%s)", s, key, keystr);
		tk->key = key;
	}
}

/* Add next node to the tree. */
static void
tty_keys_add1(struct tty_key **tkp, const char *s, key_code key)
{
	struct tty_key	*tk;

	/* Allocate a tree entry if there isn't one already. */
	tk = *tkp;
	if (tk == NULL) {
		tk = *tkp = xcalloc(1, sizeof *tk);
		tk->ch = *s;
		tk->key = KEYC_UNKNOWN;
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
tty_keys_build(struct tty *tty)
{
	const struct tty_default_key_raw	*tdkr;
	const struct tty_default_key_xterm	*tdkx;
	const struct tty_default_key_code	*tdkc;
	u_int					 i, j;
	const char				*s;
	struct options_entry			*o;
	struct options_array_item		*a;
	union options_value			*ov;
	char					 copy[16];
	key_code				 key;

	if (tty->key_tree != NULL)
		tty_keys_free(tty);
	tty->key_tree = NULL;

	for (i = 0; i < nitems(tty_default_xterm_keys); i++) {
		tdkx = &tty_default_xterm_keys[i];
		for (j = 2; j < nitems(tty_default_xterm_modifiers); j++) {
			strlcpy(copy, tdkx->template, sizeof copy);
			copy[strcspn(copy, "_")] = '0' + j;

			key = tdkx->key|tty_default_xterm_modifiers[j];
			tty_keys_add(tty, copy, key);
		}
	}
	for (i = 0; i < nitems(tty_default_raw_keys); i++) {
		tdkr = &tty_default_raw_keys[i];

		s = tdkr->string;
		if (*s != '\0')
			tty_keys_add(tty, s, tdkr->key);
	}
	for (i = 0; i < nitems(tty_default_code_keys); i++) {
		tdkc = &tty_default_code_keys[i];

		s = tty_term_string(tty->term, tdkc->code);
		if (*s != '\0')
			tty_keys_add(tty, s, tdkc->key);

	}

	o = options_get(global_options, "user-keys");
	if (o != NULL) {
		a = options_array_first(o);
		while (a != NULL) {
			i = options_array_item_index(a);
			ov = options_array_item_value(a);
			tty_keys_add(tty, ov->string, KEYC_USER + i);
			a = options_array_next(a);
		}
	}
}

/* Free the entire key tree. */
void
tty_keys_free(struct tty *tty)
{
	tty_keys_free1(tty->key_tree);
}

/* Free a single key. */
static void
tty_keys_free1(struct tty_key *tk)
{
	if (tk->next != NULL)
		tty_keys_free1(tk->next);
	if (tk->left != NULL)
		tty_keys_free1(tk->left);
	if (tk->right != NULL)
		tty_keys_free1(tk->right);
	free(tk);
}

/* Lookup a key in the tree. */
static struct tty_key *
tty_keys_find(struct tty *tty, const char *buf, size_t len, size_t *size)
{
	*size = 0;
	return (tty_keys_find1(tty->key_tree, buf, len, size));
}

/* Find the next node. */
static struct tty_key *
tty_keys_find1(struct tty_key *tk, const char *buf, size_t len, size_t *size)
{
	/* If no data, no match. */
	if (len == 0)
		return (NULL);

	/* If the node is NULL, this is the end of the tree. No match. */
	if (tk == NULL)
		return (NULL);

	/* Pick the next in the sequence. */
	if (tk->ch == *buf) {
		/* Move forward in the string. */
		buf++; len--;
		(*size)++;

		/* At the end of the string, return the current node. */
		if (len == 0 || (tk->next == NULL && tk->key != KEYC_UNKNOWN))
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

/* Look up part of the next key. */
static int
tty_keys_next1(struct tty *tty, const char *buf, size_t len, key_code *key,
    size_t *size, int expired)
{
	struct client		*c = tty->client;
	struct tty_key		*tk, *tk1;
	struct utf8_data	 ud;
	enum utf8_state		 more;
	utf8_char		 uc;
	u_int			 i;

	log_debug("%s: next key is %zu (%.*s) (expired=%d)", c->name, len,
	    (int)len, buf, expired);

	/* Is this a known key? */
	tk = tty_keys_find(tty, buf, len, size);
	if (tk != NULL && tk->key != KEYC_UNKNOWN) {
		tk1 = tk;
		do
			log_debug("%s: keys in list: %#llx", c->name, tk1->key);
		while ((tk1 = tk1->next) != NULL);
		if (tk->next != NULL && !expired)
			return (1);
		*key = tk->key;
		return (0);
	}

	/* Is this valid UTF-8? */
	more = utf8_open(&ud, (u_char)*buf);
	if (more == UTF8_MORE) {
		*size = ud.size;
		if (len < ud.size) {
			if (!expired)
				return (1);
			return (-1);
		}
		for (i = 1; i < ud.size; i++)
			more = utf8_append(&ud, (u_char)buf[i]);
		if (more != UTF8_DONE)
			return (-1);

		if (utf8_from_data(&ud, &uc) != UTF8_DONE)
			return (-1);
		*key = uc;

		log_debug("%s: UTF-8 key %.*s %#llx", c->name, (int)ud.size,
		    ud.data, *key);
		return (0);
	}

	return (-1);
}

/* Process at least one key in the buffer. Return 0 if no keys present. */
int
tty_keys_next(struct tty *tty)
{
	struct client		*c = tty->client;
	struct timeval		 tv;
	const char		*buf;
	size_t			 len, size;
	cc_t			 bspace;
	int			 delay, expired = 0, n;
	key_code		 key;
	struct mouse_event	 m = { 0 };
	struct key_event	*event;

	/* Get key buffer. */
	buf = EVBUFFER_DATA(tty->in);
	len = EVBUFFER_LENGTH(tty->in);
	if (len == 0)
		return (0);
	log_debug("%s: keys are %zu (%.*s)", c->name, len, (int)len, buf);

	/* Is this a clipboard response? */
	switch (tty_keys_clipboard(tty, buf, len, &size)) {
	case 0:		/* yes */
		key = KEYC_UNKNOWN;
		goto complete_key;
	case -1:	/* no, or not valid */
		break;
	case 1:		/* partial */
		goto partial_key;
	}

	/* Is this a device attributes response? */
	switch (tty_keys_device_attributes(tty, buf, len, &size)) {
	case 0:		/* yes */
		key = KEYC_UNKNOWN;
		goto complete_key;
	case -1:	/* no, or not valid */
		break;
	case 1:		/* partial */
		goto partial_key;
	}

	/* Is this an extended device attributes response? */
	switch (tty_keys_extended_device_attributes(tty, buf, len, &size)) {
	case 0:		/* yes */
		key = KEYC_UNKNOWN;
		goto complete_key;
	case -1:	/* no, or not valid */
		break;
	case 1:		/* partial */
		goto partial_key;
	}

	/* Is this a mouse key press? */
	switch (tty_keys_mouse(tty, buf, len, &size, &m)) {
	case 0:		/* yes */
		key = KEYC_MOUSE;
		goto complete_key;
	case -1:	/* no, or not valid */
		break;
	case -2:	/* yes, but we don't care. */
		key = KEYC_MOUSE;
		goto discard_key;
	case 1:		/* partial */
		goto partial_key;
	}

	/* Is this an extended key press? */
	switch (tty_keys_extended_key(tty, buf, len, &size, &key)) {
	case 0:		/* yes */
		goto complete_key;
	case -1:	/* no, or not valid */
		break;
	case 1:		/* partial */
		goto partial_key;
	}

first_key:
	/* Try to lookup complete key. */
	n = tty_keys_next1(tty, buf, len, &key, &size, expired);
	if (n == 0)	/* found */
		goto complete_key;
	if (n == 1)
		goto partial_key;

	/*
	 * If not a complete key, look for key with an escape prefix (meta
	 * modifier).
	 */
	if (*buf == '\033' && len > 1) {
		/* Look for a key without the escape. */
		n = tty_keys_next1(tty, buf + 1, len - 1, &key, &size, expired);
		if (n == 0) {	/* found */
			if (key & KEYC_IMPLIED_META) {
				/*
				 * We want the escape key as well as the xterm
				 * key, because the xterm sequence implicitly
				 * includes the escape (so if we see
				 * \033\033[1;3D we know it is an Escape
				 * followed by M-Left, not just M-Left).
				 */
				key = '\033';
				size = 1;
				goto complete_key;
			}
			key |= KEYC_META;
			size++;
			goto complete_key;
		}
		if (n == 1)	/* partial */
			goto partial_key;
	}

	/*
	 * At this point, we know the key is not partial (with or without
	 * escape). So pass it through even if the timer has not expired.
	 */
	if (*buf == '\033' && len >= 2) {
		key = (u_char)buf[1] | KEYC_META;
		size = 2;
	} else {
		key = (u_char)buf[0];
		size = 1;
	}
	goto complete_key;

partial_key:
	log_debug("%s: partial key %.*s", c->name, (int)len, buf);

	/* If timer is going, check for expiration. */
	if (tty->flags & TTY_TIMER) {
		if (evtimer_initialized(&tty->key_timer) &&
		    !evtimer_pending(&tty->key_timer, NULL)) {
			expired = 1;
			goto first_key;
		}
		return (0);
	}

	/* Get the time period. */
	delay = options_get_number(global_options, "escape-time");
	tv.tv_sec = delay / 1000;
	tv.tv_usec = (delay % 1000) * 1000L;

	/* Start the timer. */
	if (event_initialized(&tty->key_timer))
		evtimer_del(&tty->key_timer);
	evtimer_set(&tty->key_timer, tty_keys_callback, tty);
	evtimer_add(&tty->key_timer, &tv);

	tty->flags |= TTY_TIMER;
	return (0);

complete_key:
	log_debug("%s: complete key %.*s %#llx", c->name, (int)size, buf, key);

	/*
	 * Check for backspace key using termios VERASE - the terminfo
	 * kbs entry is extremely unreliable, so cannot be safely
	 * used. termios should have a better idea.
	 */
	bspace = tty->tio.c_cc[VERASE];
	if (bspace != _POSIX_VDISABLE && (key & KEYC_MASK_KEY) == bspace)
		key = (key & KEYC_MASK_MODIFIERS)|KEYC_BSPACE;

	/* Remove data from buffer. */
	evbuffer_drain(tty->in, size);

	/* Remove key timer. */
	if (event_initialized(&tty->key_timer))
		evtimer_del(&tty->key_timer);
	tty->flags &= ~TTY_TIMER;

	/* Check for focus events. */
	if (key == KEYC_FOCUS_OUT)
		tty->client->flags &= ~CLIENT_FOCUSED;
	else if (key == KEYC_FOCUS_IN)
		tty->client->flags |= CLIENT_FOCUSED;

	/* Fire the key. */
	if (key != KEYC_UNKNOWN) {
		event = xmalloc(sizeof *event);
		event->key = key;
		memcpy(&event->m, &m, sizeof event->m);
		if (!server_client_handle_key(c, event))
			free(event);
	}

	return (1);

discard_key:
	log_debug("%s: discard key %.*s %#llx", c->name, (int)size, buf, key);

	/* Remove data from buffer. */
	evbuffer_drain(tty->in, size);

	return (1);
}

/* Key timer callback. */
static void
tty_keys_callback(__unused int fd, __unused short events, void *data)
{
	struct tty	*tty = data;

	if (tty->flags & TTY_TIMER) {
		while (tty_keys_next(tty))
			;
	}
}

/*
 * Handle extended key input. This has two forms: \033[27;m;k~ and \033[k;mu,
 * where k is key as a number and m is a modifier. Returns 0 for success, -1
 * for failure, 1 for partial;
 */
static int
tty_keys_extended_key(struct tty *tty, const char *buf, size_t len,
    size_t *size, key_code *key)
{
	struct client	*c = tty->client;
	size_t		 end;
	u_int		 number, modifiers;
	char		 tmp[64];
	cc_t		 bspace;
	key_code	 nkey;
	key_code	 onlykey;

	*size = 0;

	/* First two bytes are always \033[. */
	if (buf[0] != '\033')
		return (-1);
	if (len == 1)
		return (1);
	if (buf[1] != '[')
		return (-1);
	if (len == 2)
		return (1);

	/*
	 * Look for a terminator. Stop at either '~' or anything that isn't a
	 * number or ';'.
	 */
	for (end = 2; end < len && end != sizeof tmp; end++) {
		if (buf[end] == '~')
			break;
		if (!isdigit((u_char)buf[end]) && buf[end] != ';')
			break;
	}
	if (end == len)
		return (1);
	if (end == sizeof tmp || (buf[end] != '~' && buf[end] != 'u'))
		return (-1);

	/* Copy to the buffer. */
	memcpy(tmp, buf + 2, end);
	tmp[end] = '\0';

	/* Try to parse either form of key. */
	if (buf[end] == '~') {
		if (sscanf(tmp, "27;%u;%u", &modifiers, &number) != 2)
			return (-1);
	} else {
		if (sscanf(tmp ,"%u;%u", &number, &modifiers) != 2)
			return (-1);
	}
	*size = end + 1;

	/* Store the key. */
	bspace = tty->tio.c_cc[VERASE];
	if (bspace != _POSIX_VDISABLE && number == bspace)
		nkey = KEYC_BSPACE;
	else
		nkey = number;

	/* Update the modifiers. */
	switch (modifiers) {
	case 2:
		nkey |= KEYC_SHIFT;
		break;
	case 3:
		nkey |= (KEYC_META|KEYC_IMPLIED_META);
		break;
	case 4:
		nkey |= (KEYC_SHIFT|KEYC_META|KEYC_IMPLIED_META);
		break;
	case 5:
		nkey |= KEYC_CTRL;
		break;
	case 6:
		nkey |= (KEYC_SHIFT|KEYC_CTRL);
		break;
	case 7:
		nkey |= (KEYC_META|KEYC_CTRL);
		break;
	case 8:
		nkey |= (KEYC_SHIFT|KEYC_META|KEYC_IMPLIED_META|KEYC_CTRL);
		break;
	case 9:
		nkey |= (KEYC_META|KEYC_IMPLIED_META);
		break;
	default:
		*key = KEYC_NONE;
		break;
	}

	/*
	 * Don't allow both KEYC_CTRL and as an implied modifier. Also convert
	 * C-X into C-x and so on.
	 */
	if (nkey & KEYC_CTRL) {
		onlykey = (nkey & KEYC_MASK_KEY);
		if (onlykey < 32 &&
		    onlykey != 9 &&
		    onlykey != 13 &&
		    onlykey != 27)
			/* nothing */;
		else if (onlykey >= 97 && onlykey <= 122)
			onlykey -= 96;
		else if (onlykey >= 64 && onlykey <= 95)
			onlykey -= 64;
		else if (onlykey == 32)
			onlykey = 0;
		else if (onlykey == 63)
			onlykey = 127;
		else
			onlykey |= KEYC_CTRL;
		nkey = onlykey|((nkey & KEYC_MASK_MODIFIERS) & ~KEYC_CTRL);
	}

	if (log_get_level() != 0) {
		log_debug("%s: extended key %.*s is %llx (%s)", c->name,
		    (int)*size, buf, nkey, key_string_lookup_key(nkey, 1));
	}
	*key = nkey;
	return (0);
}

/*
 * Handle mouse key input. Returns 0 for success, -1 for failure, 1 for partial
 * (probably a mouse sequence but need more data).
 */
static int
tty_keys_mouse(struct tty *tty, const char *buf, size_t len, size_t *size,
    struct mouse_event *m)
{
	struct client	*c = tty->client;
	u_int		 i, x, y, b, sgr_b;
	u_char		 sgr_type, ch;

	/*
	 * Standard mouse sequences are \033[M followed by three characters
	 * indicating button, X and Y, all based at 32 with 1,1 top-left.
	 *
	 * UTF-8 mouse sequences are similar but the three are expressed as
	 * UTF-8 characters.
	 *
	 * SGR extended mouse sequences are \033[< followed by three numbers in
	 * decimal and separated by semicolons indicating button, X and Y. A
	 * trailing 'M' is click or scroll and trailing 'm' release. All are
	 * based at 0 with 1,1 top-left.
	 */

	*size = 0;
	x = y = b = sgr_b = 0;
	sgr_type = ' ';

	/* First two bytes are always \033[. */
	if (buf[0] != '\033')
		return (-1);
	if (len == 1)
		return (1);
	if (buf[1] != '[')
		return (-1);
	if (len == 2)
		return (1);

	/*
	 * Third byte is M in old standard (and UTF-8 extension which we do not
	 * support), < in SGR extension.
	 */
	if (buf[2] == 'M') {
		/* Read the three inputs. */
		*size = 3;
		for (i = 0; i < 3; i++) {
			if (len <= *size)
				return (1);
			ch = (u_char)buf[(*size)++];
			if (i == 0)
				b = ch;
			else if (i == 1)
				x = ch;
			else
				y = ch;
		}
		log_debug("%s: mouse input: %.*s", c->name, (int)*size, buf);

		/* Check and return the mouse input. */
		if (b < 32)
			return (-1);
		b -= 32;
		if (x >= 33)
			x -= 33;
		else
			x = 256 - x;
		if (y >= 33)
			y -= 33;
		else
			y = 256 - y;
	} else if (buf[2] == '<') {
		/* Read the three inputs. */
		*size = 3;
		while (1) {
			if (len <= *size)
				return (1);
			ch = (u_char)buf[(*size)++];
			if (ch == ';')
				break;
			if (ch < '0' || ch > '9')
				return (-1);
			sgr_b = 10 * sgr_b + (ch - '0');
		}
		while (1) {
			if (len <= *size)
				return (1);
			ch = (u_char)buf[(*size)++];
			if (ch == ';')
				break;
			if (ch < '0' || ch > '9')
				return (-1);
			x = 10 * x + (ch - '0');
		}
		while (1) {
			if (len <= *size)
				return (1);
			ch = (u_char)buf[(*size)++];
			if (ch == 'M' || ch == 'm')
				break;
			if (ch < '0' || ch > '9')
				return (-1);
			y = 10 * y + (ch - '0');
		}
		log_debug("%s: mouse input (SGR): %.*s", c->name, (int)*size,
		    buf);

		/* Check and return the mouse input. */
		if (x < 1 || y < 1)
			return (-1);
		x--;
		y--;
		b = sgr_b;

		/* Type is M for press, m for release. */
		sgr_type = ch;
		if (sgr_type == 'm')
			b |= 3;

		/*
		 * Some terminals (like PuTTY 0.63) mistakenly send
		 * button-release events for scroll-wheel button-press event.
		 * Discard it before it reaches any program running inside
		 * tmux.
		 */
		if (sgr_type == 'm' && (sgr_b & 64))
		    return (-2);
	} else
		return (-1);

	/* Fill mouse event. */
	m->lx = tty->mouse_last_x;
	m->x = x;
	m->ly = tty->mouse_last_y;
	m->y = y;
	m->lb = tty->mouse_last_b;
	m->b = b;
	m->sgr_type = sgr_type;
	m->sgr_b = sgr_b;

	/* Update last mouse state. */
	tty->mouse_last_x = x;
	tty->mouse_last_y = y;
	tty->mouse_last_b = b;

	return (0);
}

/*
 * Handle OSC 52 clipboard input. Returns 0 for success, -1 for failure, 1 for
 * partial.
 */
static int
tty_keys_clipboard(__unused struct tty *tty, const char *buf, size_t len,
    size_t *size)
{
	size_t	 end, terminator, needed;
	char	*copy, *out;
	int	 outlen;

	*size = 0;

	/* First five bytes are always \033]52;. */
	if (buf[0] != '\033')
		return (-1);
	if (len == 1)
		return (1);
	if (buf[1] != ']')
		return (-1);
	if (len == 2)
		return (1);
	if (buf[2] != '5')
		return (-1);
	if (len == 3)
		return (1);
	if (buf[3] != '2')
		return (-1);
	if (len == 4)
		return (1);
	if (buf[4] != ';')
		return (-1);
	if (len == 5)
		return (1);

	/* Find the terminator if any. */
	for (end = 5; end < len; end++) {
		if (buf[end] == '\007') {
			terminator = 1;
			break;
		}
		if (end > 5 && buf[end - 1] == '\033' && buf[end] == '\\') {
			terminator = 2;
			break;
		}
	}
	if (end == len)
		return (1);
	*size = end + terminator;

	/* Skip the initial part. */
	buf += 5;
	end -= 5;

	/* Get the second argument. */
	while (end != 0 && *buf != ';') {
		buf++;
		end--;
	}
	if (end == 0 || end == 1)
		return (0);
	buf++;
	end--;

	/* It has to be a string so copy it. */
	copy = xmalloc(end + 1);
	memcpy(copy, buf, end);
	copy[end] = '\0';

	/* Convert from base64. */
	needed = (end / 4) * 3;
	out = xmalloc(needed);
	if ((outlen = b64_pton(copy, out, len)) == -1) {
		free(out);
		free(copy);
		return (0);
	}
	free(copy);

	/* Create a new paste buffer. */
	log_debug("%s: %.*s", __func__, outlen, out);
	paste_add(NULL, out, outlen);

	return (0);
}

/*
 * Handle secondary device attributes input. Returns 0 for success, -1 for
 * failure, 1 for partial.
 */
static int
tty_keys_device_attributes(struct tty *tty, const char *buf, size_t len,
    size_t *size)
{
	struct client	*c = tty->client;
	u_int		 i, n = 0;
	char		 tmp[64], *endptr, p[32] = { 0 }, *cp, *next;

	*size = 0;
	if (tty->flags & TTY_HAVEDA)
		return (-1);

	/*
	 * First three bytes are always \033[>. Some older Terminal.app
	 * versions respond as for DA (\033[?) so accept and ignore that.
	 */
	if (buf[0] != '\033')
		return (-1);
	if (len == 1)
		return (1);
	if (buf[1] != '[')
		return (-1);
	if (len == 2)
		return (1);
	if (buf[2] != '>' && buf[2] != '?')
		return (-1);
	if (len == 3)
		return (1);

	/* Copy the rest up to a 'c'. */
	for (i = 0; i < (sizeof tmp) - 1; i++) {
		if (3 + i == len)
			return (1);
		if (buf[3 + i] == 'c')
			break;
		tmp[i] = buf[3 + i];
	}
	if (i == (sizeof tmp) - 1)
		return (-1);
	tmp[i] = '\0';
	*size = 4 + i;

	/* Ignore DA response. */
	if (buf[2] == '?')
		return (0);

	/* Convert all arguments to numbers. */
	cp = tmp;
	while ((next = strsep(&cp, ";")) != NULL) {
		p[n] = strtoul(next, &endptr, 10);
		if (*endptr != '\0')
			p[n] = 0;
		n++;
	}

	/* Add terminal features. */
	switch (p[0]) {
	case 41: /* VT420 */
		tty_add_features(&c->term_features, "margins,rectfill", ",");
		break;
	case 'M': /* mintty */
		tty_default_features(&c->term_features, "mintty", 0);
		break;
	case 'T': /* tmux */
		tty_default_features(&c->term_features, "tmux", 0);
		break;
	case 'U': /* rxvt-unicode */
		tty_default_features(&c->term_features, "rxvt-unicode", 0);
		break;
	}
	log_debug("%s: received secondary DA %.*s", c->name, (int)*size, buf);

	tty_update_features(tty);
	tty->flags |= TTY_HAVEDA;

	return (0);
}

/*
 * Handle extended device attributes input. Returns 0 for success, -1 for
 * failure, 1 for partial.
 */
static int
tty_keys_extended_device_attributes(struct tty *tty, const char *buf,
    size_t len, size_t *size)
{
	struct client	*c = tty->client;
	u_int		 i;
	char		 tmp[128];

	*size = 0;
	if (tty->flags & TTY_HAVEXDA)
		return (-1);

	/* First four bytes are always \033P>|. */
	if (buf[0] != '\033')
		return (-1);
	if (len == 1)
		return (1);
	if (buf[1] != 'P')
		return (-1);
	if (len == 2)
		return (1);
	if (buf[2] != '>')
		return (-1);
	if (len == 3)
		return (1);
	if (buf[3] != '|')
		return (-1);
	if (len == 4)
		return (1);

	/* Copy the rest up to a '\033\\'. */
	for (i = 0; i < (sizeof tmp) - 1; i++) {
		if (4 + i == len)
			return (1);
		if (buf[4 + i - 1] == '\033' && buf[4 + i] == '\\')
			break;
		tmp[i] = buf[4 + i];
	}
	if (i == (sizeof tmp) - 1)
		return (-1);
	tmp[i - 1] = '\0';
	*size = 5 + i;

	/* Add terminal features. */
	if (strncmp(tmp, "iTerm2 ", 7) == 0)
		tty_default_features(&c->term_features, "iTerm2", 0);
	else if (strncmp(tmp, "tmux ", 5) == 0)
		tty_default_features(&c->term_features, "tmux", 0);
	else if (strncmp(tmp, "XTerm(", 6) == 0)
		tty_default_features(&c->term_features, "XTerm", 0);
	else if (strncmp(tmp, "mintty ", 7) == 0)
		tty_default_features(&c->term_features, "mintty", 0);
	log_debug("%s: received extended DA %.*s", c->name, (int)*size, buf);

	free(c->term_type);
	c->term_type = xstrdup(tmp);

	tty_update_features(tty);
	tty->flags |= TTY_HAVEXDA;

	return (0);
}
