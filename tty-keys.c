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

void		tty_keys_add1(struct tty_key **, const char *, int);
void		tty_keys_add(struct tty *, const char *, int);
void		tty_keys_free1(struct tty_key *);
struct tty_key *tty_keys_find1(
		    struct tty_key *, const char *, size_t, size_t *);
struct tty_key *tty_keys_find(struct tty *, const char *, size_t, size_t *);
void		tty_keys_callback(int, short, void *);
int		tty_keys_mouse(struct tty *, const char *, size_t, size_t *);

/* Default raw keys. */
struct tty_default_key_raw {
	const char	       *string;
	int	 	 	key;
};
const struct tty_default_key_raw tty_default_raw_keys[] = {
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
};

/* Default terminfo(5) keys. */
struct tty_default_key_code {
	enum tty_code_code	code;
	int	 	 	key;
};
const struct tty_default_key_code tty_default_code_keys[] = {
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

	{ TTYC_KF49, KEYC_F1|KEYC_ESCAPE },
	{ TTYC_KF50, KEYC_F2|KEYC_ESCAPE },
	{ TTYC_KF51, KEYC_F3|KEYC_ESCAPE },
	{ TTYC_KF52, KEYC_F4|KEYC_ESCAPE },
	{ TTYC_KF53, KEYC_F5|KEYC_ESCAPE },
	{ TTYC_KF54, KEYC_F6|KEYC_ESCAPE },
	{ TTYC_KF55, KEYC_F7|KEYC_ESCAPE },
	{ TTYC_KF56, KEYC_F8|KEYC_ESCAPE },
	{ TTYC_KF57, KEYC_F9|KEYC_ESCAPE },
	{ TTYC_KF58, KEYC_F10|KEYC_ESCAPE },
	{ TTYC_KF59, KEYC_F11|KEYC_ESCAPE },
	{ TTYC_KF60, KEYC_F12|KEYC_ESCAPE },

	{ TTYC_KF61, KEYC_F1|KEYC_ESCAPE|KEYC_SHIFT },
	{ TTYC_KF62, KEYC_F2|KEYC_ESCAPE|KEYC_SHIFT },
	{ TTYC_KF63, KEYC_F3|KEYC_ESCAPE|KEYC_SHIFT },

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
	{ TTYC_KDC2, KEYC_DC|KEYC_SHIFT },
	{ TTYC_KDC3, KEYC_DC|KEYC_ESCAPE },
	{ TTYC_KDC4, KEYC_DC|KEYC_SHIFT|KEYC_ESCAPE },
	{ TTYC_KDC5, KEYC_DC|KEYC_CTRL },
	{ TTYC_KDC6, KEYC_DC|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KDC7, KEYC_DC|KEYC_ESCAPE|KEYC_CTRL },
	{ TTYC_KDN2, KEYC_DOWN|KEYC_SHIFT },
	{ TTYC_KDN3, KEYC_DOWN|KEYC_ESCAPE },
	{ TTYC_KDN4, KEYC_DOWN|KEYC_SHIFT|KEYC_ESCAPE },
	{ TTYC_KDN5, KEYC_DOWN|KEYC_CTRL },
	{ TTYC_KDN6, KEYC_DOWN|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KDN7, KEYC_DOWN|KEYC_ESCAPE|KEYC_CTRL },
	{ TTYC_KEND2, KEYC_END|KEYC_SHIFT },
	{ TTYC_KEND3, KEYC_END|KEYC_ESCAPE },
	{ TTYC_KEND4, KEYC_END|KEYC_SHIFT|KEYC_ESCAPE },
	{ TTYC_KEND5, KEYC_END|KEYC_CTRL },
	{ TTYC_KEND6, KEYC_END|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KEND7, KEYC_END|KEYC_ESCAPE|KEYC_CTRL },
	{ TTYC_KHOM2, KEYC_HOME|KEYC_SHIFT },
	{ TTYC_KHOM3, KEYC_HOME|KEYC_ESCAPE },
	{ TTYC_KHOM4, KEYC_HOME|KEYC_SHIFT|KEYC_ESCAPE },
	{ TTYC_KHOM5, KEYC_HOME|KEYC_CTRL },
	{ TTYC_KHOM6, KEYC_HOME|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KHOM7, KEYC_HOME|KEYC_ESCAPE|KEYC_CTRL },
	{ TTYC_KIC2, KEYC_IC|KEYC_SHIFT },
	{ TTYC_KIC3, KEYC_IC|KEYC_ESCAPE },
	{ TTYC_KIC4, KEYC_IC|KEYC_SHIFT|KEYC_ESCAPE },
	{ TTYC_KIC5, KEYC_IC|KEYC_CTRL },
	{ TTYC_KIC6, KEYC_IC|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KIC7, KEYC_IC|KEYC_ESCAPE|KEYC_CTRL },
	{ TTYC_KLFT2, KEYC_LEFT|KEYC_SHIFT },
	{ TTYC_KLFT3, KEYC_LEFT|KEYC_ESCAPE },
	{ TTYC_KLFT4, KEYC_LEFT|KEYC_SHIFT|KEYC_ESCAPE },
	{ TTYC_KLFT5, KEYC_LEFT|KEYC_CTRL },
	{ TTYC_KLFT6, KEYC_LEFT|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KLFT7, KEYC_LEFT|KEYC_ESCAPE|KEYC_CTRL },
	{ TTYC_KNXT2, KEYC_NPAGE|KEYC_SHIFT },
	{ TTYC_KNXT3, KEYC_NPAGE|KEYC_ESCAPE },
	{ TTYC_KNXT4, KEYC_NPAGE|KEYC_SHIFT|KEYC_ESCAPE },
	{ TTYC_KNXT5, KEYC_NPAGE|KEYC_CTRL },
	{ TTYC_KNXT6, KEYC_NPAGE|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KNXT7, KEYC_NPAGE|KEYC_ESCAPE|KEYC_CTRL },
	{ TTYC_KPRV2, KEYC_PPAGE|KEYC_SHIFT },
	{ TTYC_KPRV3, KEYC_PPAGE|KEYC_ESCAPE },
	{ TTYC_KPRV4, KEYC_PPAGE|KEYC_SHIFT|KEYC_ESCAPE },
	{ TTYC_KPRV5, KEYC_PPAGE|KEYC_CTRL },
	{ TTYC_KPRV6, KEYC_PPAGE|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KPRV7, KEYC_PPAGE|KEYC_ESCAPE|KEYC_CTRL },
	{ TTYC_KRIT2, KEYC_RIGHT|KEYC_SHIFT },
	{ TTYC_KRIT3, KEYC_RIGHT|KEYC_ESCAPE },
	{ TTYC_KRIT4, KEYC_RIGHT|KEYC_SHIFT|KEYC_ESCAPE },
	{ TTYC_KRIT5, KEYC_RIGHT|KEYC_CTRL },
	{ TTYC_KRIT6, KEYC_RIGHT|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KRIT7, KEYC_RIGHT|KEYC_ESCAPE|KEYC_CTRL },
	{ TTYC_KUP2, KEYC_UP|KEYC_SHIFT },
	{ TTYC_KUP3, KEYC_UP|KEYC_ESCAPE },
	{ TTYC_KUP4, KEYC_UP|KEYC_SHIFT|KEYC_ESCAPE },
	{ TTYC_KUP5, KEYC_UP|KEYC_CTRL },
	{ TTYC_KUP6, KEYC_UP|KEYC_SHIFT|KEYC_CTRL },
	{ TTYC_KUP7, KEYC_UP|KEYC_ESCAPE|KEYC_CTRL },
};

/* Add key to tree. */
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
tty_keys_build(struct tty *tty)
{
	const struct tty_default_key_raw	*tdkr;
	const struct tty_default_key_code	*tdkc;
	u_int		 			 i;
	const char				*s;

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
	free(tk);
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
	struct tty_key	*tk;
	struct timeval	 tv;
	const char	*buf;
	size_t		 len, size;
	cc_t		 bspace;
	int		 key, delay, expired = 0;

	/* Get key buffer. */
	buf = EVBUFFER_DATA(tty->event->input);
	len = EVBUFFER_LENGTH(tty->event->input);
	if (len == 0)
		return (0);
	log_debug("keys are %zu (%.*s)", len, (int) len, buf);

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

	/* Look for matching key string and return if found. */
	tk = tty_keys_find(tty, buf, len, &size);
	if (tk != NULL) {
		if (tk->next != NULL)
			goto partial_key;
		key = tk->key;
		goto complete_key;
	}

	/* Try to parse a key with an xterm-style modifier. */
	switch (xterm_keys_find(buf, len, &size, &key)) {
	case 0:		/* found */
		goto complete_key;
	case -1:	/* not found */
		break;
	case 1:
		goto partial_key;
	}

first_key:
	/* Is this a meta key? */
	if (len >= 2 && buf[0] == '\033') {
		if (buf[1] != '\033') {
			key = buf[1] | KEYC_ESCAPE;
			size = 2;
			goto complete_key;
		}

		tk = tty_keys_find(tty, buf + 1, len - 1, &size);
		if (tk != NULL && (!expired || tk->next == NULL)) {
			size++;	/* include escape */
			if (tk->next != NULL)
				goto partial_key;
			key = tk->key;
			if (key != KEYC_NONE)
				key |= KEYC_ESCAPE;
			goto complete_key;
		}
	}

	/* No key found, take first. */
	key = (u_char) *buf;
	size = 1;

	/*
	 * Check for backspace key using termios VERASE - the terminfo
	 * kbs entry is extremely unreliable, so cannot be safely
	 * used. termios should have a better idea.
	 */
	bspace = tty->tio.c_cc[VERASE];
	if (bspace != _POSIX_VDISABLE && key == bspace)
		key = KEYC_BSPACE;

	goto complete_key;

partial_key:
	log_debug("partial key %.*s", (int) len, buf);

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
	delay = options_get_number(&global_options, "escape-time");
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
	log_debug("complete key %.*s %#x", (int) size, buf, key);

	/* Remove data from buffer. */
	evbuffer_drain(tty->event->input, size);

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
	if (key != KEYC_NONE)
		server_client_handle_key(tty->client, key);

	return (1);

discard_key:
	log_debug("discard key %.*s %#x", (int) size, buf, key);

	/* Remove data from buffer. */
	evbuffer_drain(tty->event->input, size);

	return (1);
}

/* Key timer callback. */
void
tty_keys_callback(unused int fd, unused short events, void *data)
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
int
tty_keys_mouse(struct tty *tty, const char *buf, size_t len, size_t *size)
{
	struct mouse_event	*m = &tty->mouse;
	struct utf8_data	 utf8data;
	u_int			 i, value, x, y, b, sgr_b;
	u_char			 sgr_type, c;

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
	 * Third byte is M in old standard and UTF-8 extension, < in SGR
	 * extension.
	 */
	if (buf[2] == 'M') {
		/* Read the three inputs. */
		*size = 3;
		for (i = 0; i < 3; i++) {
			if (len <= *size)
				return (1);

			if (tty->mode & MODE_MOUSE_UTF8) {
				if (utf8_open(&utf8data, buf[*size])) {
					if (utf8data.size != 2)
						return (-1);
					(*size)++;
					if (len <= *size)
						return (1);
					utf8_append(&utf8data, buf[*size]);
					value = utf8_combine(&utf8data);
				} else
					value = (u_char) buf[*size];
				(*size)++;
			} else {
				value = (u_char) buf[*size];
				(*size)++;
			}

			if (i == 0)
				b = value;
			else if (i == 1)
				x = value;
			else
				y = value;
		}
		log_debug("mouse input: %.*s", (int)*size, buf);

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
			c = (u_char)buf[(*size)++];
			if (c == ';')
				break;
			if (c < '0' || c > '9')
				return (-1);
			sgr_b = 10 * sgr_b + (c - '0');
		}
		while (1) {
			if (len <= *size)
				return (1);
			c = (u_char)buf[(*size)++];
			if (c == ';')
				break;
			if (c < '0' || c > '9')
				return (-1);
			x = 10 * x + (c - '0');
		}
		while (1) {
			if (len <= *size)
				return (1);
			c = (u_char)buf[(*size)++];
			if (c == 'M' || c == 'm')
				break;
			if (c < '0' || c > '9')
				return (-1);
			y = 10 * y + (c - '0');
		}
		log_debug("mouse input (SGR): %.*s", (int)*size, buf);

		/* Check and return the mouse input. */
		if (x < 1 || y < 1)
			return (-1);
		x--;
		y--;
		b = sgr_b;

		/* Type is M for press, m for release. */
		sgr_type = c;
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
