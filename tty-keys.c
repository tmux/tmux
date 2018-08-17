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

#include <limits.h>
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
static int	tty_keys_mouse(struct tty *, const char *, size_t, size_t *);
static int	tty_keys_device_attributes(struct tty *, const char *, size_t,
		    size_t *);

/* Default raw keys. */
struct tty_default_key_raw {
	const char	       *string;
	key_code	 	key;
};
static const struct tty_default_key_raw tty_default_raw_keys[] = {
	/*
	 * Numeric keypad. Just use the vt100 escape sequences here and always
	 * put the terminal into keypad_xmit mode. Translation of numbers
	 * mode/applications mode is done in input-keys.c.
	 */
	{ "\033Oo", KEYC_KP_SLASH },
	{ "\033Oj", KEYC_KP_STAR },
	{ "\033Om", KEYC_KP_MINUS },
	{ "\033Ow", KEYC_KP_SEVEN },
	{ "\033Ox", KEYC_KP_EIGHT },
	{ "\033Oy", KEYC_KP_NINE },
	{ "\033Ok", KEYC_KP_PLUS },
	{ "\033Ot", KEYC_KP_FOUR },
	{ "\033Ou", KEYC_KP_FIVE },
	{ "\033Ov", KEYC_KP_SIX },
	{ "\033Oq", KEYC_KP_ONE },
	{ "\033Or", KEYC_KP_TWO },
	{ "\033Os", KEYC_KP_THREE },
	{ "\033OM", KEYC_KP_ENTER },
	{ "\033Op", KEYC_KP_ZERO },
	{ "\033On", KEYC_KP_PERIOD },

	/* Arrow keys. */
	{ "\033OA", KEYC_UP },
	{ "\033OB", KEYC_DOWN },
	{ "\033OC", KEYC_RIGHT },
	{ "\033OD", KEYC_LEFT },

	{ "\033[A", KEYC_UP },
	{ "\033[B", KEYC_DOWN },
	{ "\033[C", KEYC_RIGHT },
	{ "\033[D", KEYC_LEFT },

	/* Other (xterm) "cursor" keys. */
	{ "\033OH", KEYC_HOME },
	{ "\033OF", KEYC_END },

	{ "\033[H", KEYC_HOME },
	{ "\033[F", KEYC_END },

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

/*
 * Default terminfo(5) keys. Any keys that have builtin modifiers
 * (that is, where the key itself contains the modifiers) has the
 * KEYC_XTERM flag set so a leading escape is not treated as meta (and
 * probably removed).
 */
struct tty_default_key_code {
	enum tty_code_code	code;
	key_code	 	key;
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

	{ TTYC_KF13, KEYC_F1|KEYC_SHIFT|KEYC_XTERM },
	{ TTYC_KF14, KEYC_F2|KEYC_SHIFT|KEYC_XTERM },
	{ TTYC_KF15, KEYC_F3|KEYC_SHIFT|KEYC_XTERM },
	{ TTYC_KF16, KEYC_F4|KEYC_SHIFT|KEYC_XTERM },
	{ TTYC_KF17, KEYC_F5|KEYC_SHIFT|KEYC_XTERM },
	{ TTYC_KF18, KEYC_F6|KEYC_SHIFT|KEYC_XTERM },
	{ TTYC_KF19, KEYC_F7|KEYC_SHIFT|KEYC_XTERM },
	{ TTYC_KF20, KEYC_F8|KEYC_SHIFT|KEYC_XTERM },
	{ TTYC_KF21, KEYC_F9|KEYC_SHIFT|KEYC_XTERM },
	{ TTYC_KF22, KEYC_F10|KEYC_SHIFT|KEYC_XTERM },
	{ TTYC_KF23, KEYC_F11|KEYC_SHIFT|KEYC_XTERM },
	{ TTYC_KF24, KEYC_F12|KEYC_SHIFT|KEYC_XTERM },

	{ TTYC_KF25, KEYC_F1|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KF26, KEYC_F2|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KF27, KEYC_F3|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KF28, KEYC_F4|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KF29, KEYC_F5|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KF30, KEYC_F6|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KF31, KEYC_F7|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KF32, KEYC_F8|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KF33, KEYC_F9|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KF34, KEYC_F10|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KF35, KEYC_F11|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KF36, KEYC_F12|KEYC_CTRL|KEYC_XTERM },

	{ TTYC_KF37, KEYC_F1|KEYC_SHIFT|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KF38, KEYC_F2|KEYC_SHIFT|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KF39, KEYC_F3|KEYC_SHIFT|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KF40, KEYC_F4|KEYC_SHIFT|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KF41, KEYC_F5|KEYC_SHIFT|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KF42, KEYC_F6|KEYC_SHIFT|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KF43, KEYC_F7|KEYC_SHIFT|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KF44, KEYC_F8|KEYC_SHIFT|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KF45, KEYC_F9|KEYC_SHIFT|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KF46, KEYC_F10|KEYC_SHIFT|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KF47, KEYC_F11|KEYC_SHIFT|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KF48, KEYC_F12|KEYC_SHIFT|KEYC_CTRL|KEYC_XTERM },

	{ TTYC_KF49, KEYC_F1|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KF50, KEYC_F2|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KF51, KEYC_F3|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KF52, KEYC_F4|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KF53, KEYC_F5|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KF54, KEYC_F6|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KF55, KEYC_F7|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KF56, KEYC_F8|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KF57, KEYC_F9|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KF58, KEYC_F10|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KF59, KEYC_F11|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KF60, KEYC_F12|KEYC_ESCAPE|KEYC_XTERM },

	{ TTYC_KF61, KEYC_F1|KEYC_ESCAPE|KEYC_SHIFT|KEYC_XTERM },
	{ TTYC_KF62, KEYC_F2|KEYC_ESCAPE|KEYC_SHIFT|KEYC_XTERM },
	{ TTYC_KF63, KEYC_F3|KEYC_ESCAPE|KEYC_SHIFT|KEYC_XTERM },

	{ TTYC_KICH1, KEYC_IC },
	{ TTYC_KDCH1, KEYC_DC },
	{ TTYC_KHOME, KEYC_HOME },
	{ TTYC_KEND, KEYC_END },
	{ TTYC_KNP, KEYC_NPAGE },
	{ TTYC_KPP, KEYC_PPAGE },
	{ TTYC_KCBT, KEYC_BTAB },

	/* Arrow keys from terminfo. */
	{ TTYC_KCUU1, KEYC_UP },
	{ TTYC_KCUD1, KEYC_DOWN },
	{ TTYC_KCUB1, KEYC_LEFT },
	{ TTYC_KCUF1, KEYC_RIGHT },

	/* Key and modifier capabilities. */
	{ TTYC_KDC2, KEYC_DC|KEYC_SHIFT|KEYC_XTERM },
	{ TTYC_KDC3, KEYC_DC|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KDC4, KEYC_DC|KEYC_SHIFT|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KDC5, KEYC_DC|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KDC6, KEYC_DC|KEYC_SHIFT|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KDC7, KEYC_DC|KEYC_ESCAPE|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KIND, KEYC_DOWN|KEYC_SHIFT|KEYC_XTERM },
	{ TTYC_KDN2, KEYC_DOWN|KEYC_SHIFT|KEYC_XTERM },
	{ TTYC_KDN3, KEYC_DOWN|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KDN4, KEYC_DOWN|KEYC_SHIFT|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KDN5, KEYC_DOWN|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KDN6, KEYC_DOWN|KEYC_SHIFT|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KDN7, KEYC_DOWN|KEYC_ESCAPE|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KEND2, KEYC_END|KEYC_SHIFT|KEYC_XTERM },
	{ TTYC_KEND3, KEYC_END|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KEND4, KEYC_END|KEYC_SHIFT|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KEND5, KEYC_END|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KEND6, KEYC_END|KEYC_SHIFT|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KEND7, KEYC_END|KEYC_ESCAPE|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KHOM2, KEYC_HOME|KEYC_SHIFT|KEYC_XTERM },
	{ TTYC_KHOM3, KEYC_HOME|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KHOM4, KEYC_HOME|KEYC_SHIFT|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KHOM5, KEYC_HOME|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KHOM6, KEYC_HOME|KEYC_SHIFT|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KHOM7, KEYC_HOME|KEYC_ESCAPE|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KIC2, KEYC_IC|KEYC_SHIFT|KEYC_XTERM },
	{ TTYC_KIC3, KEYC_IC|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KIC4, KEYC_IC|KEYC_SHIFT|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KIC5, KEYC_IC|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KIC6, KEYC_IC|KEYC_SHIFT|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KIC7, KEYC_IC|KEYC_ESCAPE|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KLFT2, KEYC_LEFT|KEYC_SHIFT|KEYC_XTERM },
	{ TTYC_KLFT3, KEYC_LEFT|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KLFT4, KEYC_LEFT|KEYC_SHIFT|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KLFT5, KEYC_LEFT|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KLFT6, KEYC_LEFT|KEYC_SHIFT|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KLFT7, KEYC_LEFT|KEYC_ESCAPE|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KNXT2, KEYC_NPAGE|KEYC_SHIFT|KEYC_XTERM },
	{ TTYC_KNXT3, KEYC_NPAGE|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KNXT4, KEYC_NPAGE|KEYC_SHIFT|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KNXT5, KEYC_NPAGE|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KNXT6, KEYC_NPAGE|KEYC_SHIFT|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KNXT7, KEYC_NPAGE|KEYC_ESCAPE|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KPRV2, KEYC_PPAGE|KEYC_SHIFT|KEYC_XTERM },
	{ TTYC_KPRV3, KEYC_PPAGE|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KPRV4, KEYC_PPAGE|KEYC_SHIFT|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KPRV5, KEYC_PPAGE|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KPRV6, KEYC_PPAGE|KEYC_SHIFT|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KPRV7, KEYC_PPAGE|KEYC_ESCAPE|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KRIT2, KEYC_RIGHT|KEYC_SHIFT|KEYC_XTERM },
	{ TTYC_KRIT3, KEYC_RIGHT|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KRIT4, KEYC_RIGHT|KEYC_SHIFT|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KRIT5, KEYC_RIGHT|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KRIT6, KEYC_RIGHT|KEYC_SHIFT|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KRIT7, KEYC_RIGHT|KEYC_ESCAPE|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KRI, KEYC_UP|KEYC_SHIFT|KEYC_XTERM },
	{ TTYC_KUP2, KEYC_UP|KEYC_SHIFT|KEYC_XTERM },
	{ TTYC_KUP3, KEYC_UP|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KUP4, KEYC_UP|KEYC_SHIFT|KEYC_ESCAPE|KEYC_XTERM },
	{ TTYC_KUP5, KEYC_UP|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KUP6, KEYC_UP|KEYC_SHIFT|KEYC_CTRL|KEYC_XTERM },
	{ TTYC_KUP7, KEYC_UP|KEYC_ESCAPE|KEYC_CTRL|KEYC_XTERM },
};

/* Add key to tree. */
static void
tty_keys_add(struct tty *tty, const char *s, key_code key)
{
	struct tty_key	*tk;
	size_t		 size;
	const char     	*keystr;

	keystr = key_string_lookup_key(key);
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
	const struct tty_default_key_code	*tdkc;
	u_int		 			 i, size;
	const char				*s, *value;
	struct options_entry			*o;

	if (tty->key_tree != NULL)
		tty_keys_free(tty);
	tty->key_tree = NULL;

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
	if (o != NULL && options_array_size(o, &size) != -1) {
		for (i = 0; i < size; i++) {
			value = options_array_get(o, i);
			if (value != NULL)
				tty_keys_add(tty, value, KEYC_USER + i);
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
	u_int			 i;
	wchar_t			 wc;
	int			 n;

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

	/* Is this an an xterm(1) key? */
	n = xterm_keys_find(buf, len, size, key);
	if (n == 0)
		return (0);
	if (n == 1 && !expired)
		return (1);

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

		if (utf8_combine(&ud, &wc) != UTF8_DONE)
			return (-1);
		*key = wc;

		log_debug("%s: UTF-8 key %.*s %#llx", c->name, (int)ud.size,
		    buf, *key);
		return (0);
	}

	return (-1);
}

/*
 * Process at least one key in the buffer and invoke tty->key_callback. Return
 * 0 if there are no further keys, or 1 if there could be more in the buffer.
 */
key_code
tty_keys_next(struct tty *tty)
{
	struct client	*c = tty->client;
	struct timeval	 tv;
	const char	*buf;
	size_t		 len, size;
	cc_t		 bspace;
	int		 delay, expired = 0, n;
	key_code	 key;

	/* Get key buffer. */
	buf = EVBUFFER_DATA(tty->in);
	len = EVBUFFER_LENGTH(tty->in);

	if (len == 0)
		return (0);
	log_debug("%s: keys are %zu (%.*s)", c->name, len, (int)len, buf);

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

	/* Is this a mouse key press? */
	switch (tty_keys_mouse(tty, buf, len, &size)) {
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
	if (*buf == '\033') {
		/* Look for a key without the escape. */
		n = tty_keys_next1(tty, buf + 1, len - 1, &key, &size, expired);
		if (n == 0) {	/* found */
			if (key & KEYC_XTERM) {
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
			key |= KEYC_ESCAPE;
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
		key = (u_char)buf[1] | KEYC_ESCAPE;
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
		key = (key & KEYC_MASK_MOD) | KEYC_BSPACE;

	/* Remove data from buffer. */
	evbuffer_drain(tty->in, size);

	/* Remove key timer. */
	if (event_initialized(&tty->key_timer))
		evtimer_del(&tty->key_timer);
	tty->flags &= ~TTY_TIMER;

	/* Check for focus events. */
	if (key == KEYC_FOCUS_OUT) {
		tty->client->flags &= ~CLIENT_FOCUSED;
		return (1);
	} else if (key == KEYC_FOCUS_IN) {
		tty->client->flags |= CLIENT_FOCUSED;
		return (1);
	}

	/* Fire the key. */
	if (key != KEYC_UNKNOWN)
		server_client_handle_key(tty->client, key);

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
 * Handle mouse key input. Returns 0 for success, -1 for failure, 1 for partial
 * (probably a mouse sequence but need more data).
 */
static int
tty_keys_mouse(struct tty *tty, const char *buf, size_t len, size_t *size)
{
	struct client		*c = tty->client;
	struct mouse_event	*m = &tty->mouse;
	u_int			 i, x, y, b, sgr_b;
	u_char			 sgr_type, ch;

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
	m->lx = m->x;
	m->x = x;
	m->ly = m->y;
	m->y = y;
	m->lb = m->b;
	m->b = b;
	m->sgr_type = sgr_type;
	m->sgr_b = sgr_b;

	return (0);
}

/*
 * Handle device attributes input. Returns 0 for success, -1 for failure, 1 for
 * partial.
 */
static int
tty_keys_device_attributes(struct tty *tty, const char *buf, size_t len,
    size_t *size)
{
	struct client		*c = tty->client;
	u_int			 i, a, b;
	char			 tmp[64], *endptr;
	static const char	*types[] = TTY_TYPES;
	int			 type;

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

	/* Copy the rest up to a 'c'. */
	for (i = 0; i < (sizeof tmp) - 1 && buf[3 + i] != 'c'; i++) {
		if (3 + i == len)
			return (1);
		tmp[i] = buf[3 + i];
	}
	if (i == (sizeof tmp) - 1)
		return (-1);
	tmp[i] = '\0';
	*size = 4 + i;

	/* Convert version numbers. */
	a = strtoul(tmp, &endptr, 10);
	if (*endptr == ';') {
		b = strtoul(endptr + 1, &endptr, 10);
		if (*endptr != '\0' && *endptr != ';')
			b = 0;
	} else
		a = b = 0;

	/* Store terminal type. */
	type = TTY_UNKNOWN;
	switch (a) {
	case 1:
		if (b == 2)
			type = TTY_VT100;
		else if (b == 0)
			type = TTY_VT101;
		break;
	case 6:
		type = TTY_VT102;
		break;
	case 62:
		type = TTY_VT220;
		break;
	case 63:
		type = TTY_VT320;
		break;
	case 64:
		type = TTY_VT420;
		break;
	}
	tty_set_type(tty, type);

	log_debug("%s: received DA %.*s (%s)", c->name, (int)*size, buf,
	    types[type]);
	return (0);
}
