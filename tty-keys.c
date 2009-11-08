/* $Id: tty-keys.c,v 1.43 2009-11-08 23:33:17 tcunha Exp $ */

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
int		tty_keys_mouse(char *, size_t, size_t *, struct mouse_event *);

struct tty_key_ent {
	enum tty_code_code	code;
	const char	       *string;

	int	 	 	key;
	int		 	flags;
#define TTYKEY_CTRL 0x1
#define TTYKEY_RAW 0x2
};

/* 
 * Default key tables. Those flagged with TTYKEY_RAW are inserted directly,
 * otherwise they are looked up in terminfo(5). Any keys marked TTYKEY_CTRL
 * have their last byte twiddled and are inserted as a Ctrl key as well.
 */
struct tty_key_ent tty_keys[] = {
	/* Function keys. */
	{ TTYC_KF1,	NULL,		KEYC_F1,		TTYKEY_CTRL },
	{ TTYC_KF1,	NULL,		KEYC_F1,		TTYKEY_CTRL },
	{ TTYC_KF2,	NULL,		KEYC_F2,		TTYKEY_CTRL },
	{ TTYC_KF3,	NULL,		KEYC_F3,		TTYKEY_CTRL },
	{ TTYC_KF4,	NULL,		KEYC_F4,		TTYKEY_CTRL },
	{ TTYC_KF5,	NULL,		KEYC_F5,		TTYKEY_CTRL },
	{ TTYC_KF6,	NULL,		KEYC_F6,		TTYKEY_CTRL },
	{ TTYC_KF7,	NULL,		KEYC_F7,		TTYKEY_CTRL },
	{ TTYC_KF8,	NULL,		KEYC_F8,		TTYKEY_CTRL },
	{ TTYC_KF9,	NULL,		KEYC_F9,		TTYKEY_CTRL },
	{ TTYC_KF10,	NULL,		KEYC_F10,		TTYKEY_CTRL },
	{ TTYC_KF11,	NULL,		KEYC_F11,		TTYKEY_CTRL },
	{ TTYC_KF12,	NULL,		KEYC_F12,		TTYKEY_CTRL },
	{ TTYC_KF13,	NULL,		KEYC_F13,		TTYKEY_CTRL },
	{ TTYC_KF14,	NULL,		KEYC_F14,		TTYKEY_CTRL },
	{ TTYC_KF15,	NULL,		KEYC_F15,		TTYKEY_CTRL },
	{ TTYC_KF16,	NULL,		KEYC_F16,		TTYKEY_CTRL },
	{ TTYC_KF17,	NULL,		KEYC_F17,		TTYKEY_CTRL },
	{ TTYC_KF18,	NULL,		KEYC_F18,		TTYKEY_CTRL },
	{ TTYC_KF19,	NULL,		KEYC_F19,		TTYKEY_CTRL },
	{ TTYC_KF20,	NULL,		KEYC_F20,		TTYKEY_CTRL },
	{ TTYC_KICH1,	NULL,		KEYC_IC,		TTYKEY_CTRL },
	{ TTYC_KDCH1,	NULL,		KEYC_DC,		TTYKEY_CTRL },
	{ TTYC_KHOME,	NULL,		KEYC_HOME,		TTYKEY_CTRL },
	{ TTYC_KEND,	NULL,		KEYC_END,		TTYKEY_CTRL },
	{ TTYC_KNP,	NULL,		KEYC_NPAGE,		TTYKEY_CTRL },
	{ TTYC_KPP,	NULL,		KEYC_PPAGE,		TTYKEY_CTRL },
	{ TTYC_KCBT,	NULL,		KEYC_BTAB,		TTYKEY_CTRL },

	/* Arrow keys. */
	{ 0,		"\033OA",	KEYC_UP,		TTYKEY_RAW },
	{ 0,		"\033OB",	KEYC_DOWN,		TTYKEY_RAW },
	{ 0,		"\033OC",	KEYC_RIGHT,		TTYKEY_RAW },
	{ 0,		"\033OD",	KEYC_LEFT,		TTYKEY_RAW },

	{ 0,		"\033[A",	KEYC_UP,		TTYKEY_RAW },
	{ 0,		"\033[B",	KEYC_DOWN,		TTYKEY_RAW },
	{ 0,		"\033[C",	KEYC_RIGHT,		TTYKEY_RAW },
	{ 0,		"\033[D",	KEYC_LEFT,		TTYKEY_RAW },

	{ TTYC_KCUU1,	NULL,		KEYC_UP,		TTYKEY_CTRL },
	{ TTYC_KCUD1,	NULL,		KEYC_DOWN,		TTYKEY_CTRL },
	{ TTYC_KCUB1,	NULL,		KEYC_LEFT,		TTYKEY_CTRL },
	{ TTYC_KCUF1,	NULL,		KEYC_RIGHT,		TTYKEY_CTRL },

	/* Special-case arrow keys for rxvt until terminfo has kRIT5 etc. */
	{ 0,		"\033Oa",	KEYC_UP|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,		"\033Ob",	KEYC_DOWN|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,		"\033Oc",	KEYC_RIGHT|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,		"\033Od",	KEYC_LEFT|KEYC_CTRL,	TTYKEY_RAW },

	{ 0,		"\033[a",	KEYC_UP|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,		"\033[b",	KEYC_DOWN|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,		"\033[c",	KEYC_RIGHT|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,		"\033[d",	KEYC_LEFT|KEYC_SHIFT,	TTYKEY_RAW },

	/*
	 * Numeric keypad. Just use the vt100 escape sequences here and always
	 * put the terminal into keypad_xmit mode. Translation of numbers
	 * mode/applications mode is done in input-keys.c.
	 */
	{ 0,		"\033Oo",	KEYC_KP_SLASH,		TTYKEY_RAW },
	{ 0,		"\033Oj",	KEYC_KP_STAR,		TTYKEY_RAW },
	{ 0,		"\033Om",	KEYC_KP_MINUS,		TTYKEY_RAW },
	{ 0,		"\033Ow",	KEYC_KP_SEVEN,		TTYKEY_RAW },
	{ 0,		"\033Ox",	KEYC_KP_EIGHT,		TTYKEY_RAW },
	{ 0,		"\033Oy",	KEYC_KP_NINE,		TTYKEY_RAW },
	{ 0,		"\033Ok",	KEYC_KP_PLUS,		TTYKEY_RAW },
	{ 0,		"\033Ot",	KEYC_KP_FOUR,		TTYKEY_RAW },
	{ 0,		"\033Ou",	KEYC_KP_FIVE,		TTYKEY_RAW },
	{ 0,		"\033Ov",	KEYC_KP_SIX,		TTYKEY_RAW },
	{ 0,		"\033Oq",	KEYC_KP_ONE,		TTYKEY_RAW },
	{ 0,		"\033Or",	KEYC_KP_TWO,		TTYKEY_RAW },
	{ 0,		"\033Os",	KEYC_KP_THREE,		TTYKEY_RAW },
	{ 0,		"\033OM",	KEYC_KP_ENTER,		TTYKEY_RAW },
	{ 0,		"\033Op",	KEYC_KP_ZERO,		TTYKEY_RAW },
	{ 0,		"\033On",	KEYC_KP_PERIOD,		TTYKEY_RAW },

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
	size_t	size;

	if (tty_keys_find(tty, s, strlen(s), &size) == NULL) {
		log_debug("new key 0x%x: %s", key, s);
		tty_keys_add1(&tty->key_tree, s, key);
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
	struct tty_key_ent	*tke;
	u_int		 	 i;
	const char		*s;
	char			 tmp[64];

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
		if (!(tke->flags & TTYKEY_CTRL)) {
			if (strlcpy(tmp, s, sizeof tmp) >= sizeof tmp)
				continue;
			tmp[strlen(tmp) - 1] ^= 0x20;
			tty_keys_add(tty, tmp + 1, tke->key | KEYC_CTRL);
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
		if (len == 0)
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
 * 1 if there are no further keys, or 0 if there is more in the buffer.
 */
int
tty_keys_next(struct tty *tty)
{
	struct tty_key		*tk;
	struct timeval		 tv;
	struct mouse_event	 mouse;
	char			*buf;
	size_t			 len, size;
	cc_t			 bspace;
	int			 key;

	buf = EVBUFFER_DATA(tty->event->input);
	len = EVBUFFER_LENGTH(tty->event->input);
	if (len == 0)
		return (0);
	log_debug("keys are %zu (%.*s)", len, (int) len, buf);

	/* If a normal key, return it. */
	if (*buf != '\033') {
		key = *buf;
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

	/* Look for matching key string and return if found. */
	tk = tty_keys_find(tty, buf + 1, len - 1, &size);
	if (tk != NULL) {
		key = tk->key;
		goto found_key;
	}

	/* Not found. Is this a mouse key press? */
	key = tty_keys_mouse(buf, len, &size, &mouse);
	if (key != KEYC_NONE) {
		evbuffer_drain(tty->event->input, size);
		goto handle_key;
	}

	/* Not found. Try to parse a key with an xterm-style modifier. */
	key = xterm_keys_find(buf, len, &size);
	if (key != KEYC_NONE) {
		evbuffer_drain(tty->event->input, size);
		goto handle_key;
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
	tv.tv_sec = 0;
	tv.tv_usec = ESCAPE_PERIOD * 1000L;
	
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
void
tty_keys_callback(unused int fd, unused short events, void *data)
{
	struct tty	*tty = data;

	if (!(tty->flags & TTY_ESCAPE))
		return;

	while (tty_keys_next(tty))
		;
}

/* Handle mouse key input. */
int
tty_keys_mouse(char *buf, size_t len, size_t *size, struct mouse_event *m)
{
	/*
	 * Mouse sequences are \033[M followed by three characters indicating
	 * buttons, X and Y, all based at 32 with 1,1 top-left.
	 */

	if (len != 6 || memcmp(buf, "\033[M", 3) != 0)
		return (KEYC_NONE);
	*size = 6;

	log_debug("mouse input is: %.*s", (int) len, buf);

	m->b = buf[3];
	m->x = buf[4];
	m->y = buf[5];
	if (m->b < 32 || m->x < 33 || m->y < 33)
		return (KEYC_NONE);
	m->b -= 32;
	m->x -= 33;
	m->y -= 33;
	return (KEYC_MOUSE);
}
