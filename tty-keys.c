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

#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Handle keys input from the outside terminal.
 */

void	tty_keys_add(struct tty *, const char *, int, int);
int	tty_keys_mouse(char *, size_t, size_t *, struct mouse_event *);

struct tty_key_ent {
	enum tty_code_code	code;
	const char	       *string;

	int	 	 	key;
	int		 	flags;
};

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

	/*
	 * Arrow keys. There are several variants of these so just accept them.
	 * We always put the terminal into application keys mode so ctrl should
	 * swap between SS3 and CSI.
	 */
	{ 0,		"\033OA",	KEYC_UP,		TTYKEY_RAW },
	{ 0,		"\033OB",	KEYC_DOWN,		TTYKEY_RAW },
	{ 0,		"\033OC",	KEYC_RIGHT,		TTYKEY_RAW },
	{ 0,		"\033OD",	KEYC_LEFT,		TTYKEY_RAW },

	{ 0,		"\033[A",	KEYC_UP|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,		"\033[B",	KEYC_DOWN|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,		"\033[C",	KEYC_RIGHT|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,		"\033[D",	KEYC_LEFT|KEYC_CTRL,	TTYKEY_RAW },

	{ 0,		"\033Oa",	KEYC_UP|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,		"\033Ob",	KEYC_DOWN|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,		"\033Oc",	KEYC_RIGHT|KEYC_CTRL,	TTYKEY_RAW },
	{ 0,		"\033Od",	KEYC_LEFT|KEYC_CTRL,	TTYKEY_RAW },

	{ 0,		"\033[a",	KEYC_UP|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,		"\033[b",	KEYC_DOWN|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,		"\033[c",	KEYC_RIGHT|KEYC_SHIFT,	TTYKEY_RAW },
	{ 0,		"\033[d",	KEYC_LEFT|KEYC_SHIFT,	TTYKEY_RAW },

	{ TTYC_KCUU1,	NULL,		KEYC_UP,		TTYKEY_CTRL },
	{ TTYC_KCUD1,	NULL,		KEYC_DOWN,		TTYKEY_CTRL },
	{ TTYC_KCUB1,	NULL,		KEYC_LEFT,		TTYKEY_CTRL },
	{ TTYC_KCUF1,	NULL,		KEYC_RIGHT,		TTYKEY_CTRL },

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

RB_GENERATE(tty_keys, tty_key, entry, tty_keys_cmp);

struct tty_key *tty_keys_find(struct tty *, char *, size_t, size_t *);

int
tty_keys_cmp(struct tty_key *k1, struct tty_key *k2)
{
	return (strcmp(k1->string, k2->string));
}

void
tty_keys_add(struct tty *tty, const char *s, int key, int flags)
{
	struct tty_key	*tk, *tl;

	tk = xmalloc(sizeof *tk);
	tk->string = xstrdup(s);
	tk->key = key;
	tk->flags = flags;

	if ((tl = RB_INSERT(tty_keys, &tty->ktree, tk)) != NULL) {
		xfree(tk->string);
		xfree(tk);
		log_debug("key exists: %s (old %x, new %x)", s, tl->key, key);
 		return;
	}

	if (strlen(tk->string) > tty->ksize)
		tty->ksize = strlen(tk->string);
	log_debug("new key %x: size now %zu (%s)", key, tty->ksize, tk->string);
}

void
tty_keys_init(struct tty *tty)
{
	struct tty_key_ent	*tke;
	u_int		 	 i;
	const char		*s;
	char			 tmp[64];

	RB_INIT(&tty->ktree);

	tty->ksize = 0;
	for (i = 0; i < nitems(tty_keys); i++) {
		tke = &tty_keys[i];

		if (tke->flags & TTYKEY_RAW)
			s = tke->string;
		else {
			if (!tty_term_has(tty->term, tke->code))
				continue;
			s = tty_term_string(tty->term, tke->code);
			if (s[0] != '\033' || s[1] == '\0')
				continue;
		}

		tty_keys_add(tty, s + 1, tke->key, tke->flags);
		if (tke->flags & TTYKEY_CTRL) {
			if (strlcpy(tmp, s, sizeof tmp) >= sizeof tmp)
				continue;
			tmp[strlen(tmp) - 1] ^= 0x20;
			tty_keys_add(tty, tmp + 1, tke->key | KEYC_CTRL, 0);
		}
	}
}

void
tty_keys_free(struct tty *tty)
{
	struct tty_key	*tk;

	while (!RB_EMPTY(&tty->ktree)) {
		tk = RB_ROOT(&tty->ktree);
		RB_REMOVE(tty_keys, &tty->ktree, tk);
		xfree(tk->string);
		xfree(tk);
	}
}

struct tty_key *
tty_keys_find(struct tty *tty, char *buf, size_t len, size_t *size)
{
	struct tty_key	*tk, tl;
	char		*s;

	if (len == 0)
		return (NULL);

	s = xmalloc(tty->ksize + 1);
	for (*size = tty->ksize; (*size) > 0; (*size)--) {
		if ((*size) > len)
			continue;
		memcpy(s, buf, *size);
		s[*size] = '\0';

		log_debug2("looking for key: %s", s);

		tl.string = s;
		tk = RB_FIND(tty_keys, &tty->ktree, &tl);
		if (tk != NULL) {
			log_debug2("got key: 0x%x", tk->key);
			xfree(s);
			return (tk);
		}
	}
	xfree(s);

	return (NULL);
}

int
tty_keys_next(struct tty *tty, int *key, struct mouse_event *mouse)
{
	struct tty_key	*tk;
	struct timeval	 tv;
	char		*buf;
	size_t		 len, size;
	cc_t		 bspace;

	buf = BUFFER_OUT(tty->in);
	len = BUFFER_USED(tty->in);
	if (len == 0)
		return (1);
	log_debug("keys are %zu (%.*s)", len, (int) len, buf);

	/* If a normal key, return it. */
	if (*buf != '\033') {
		*key = buffer_read8(tty->in);

		/*
		 * Check for backspace key using termios VERASE - the terminfo
		 * kbs entry is extremely unreliable, so cannot be safely
		 * used. termios should have a better idea.
		 */
		bspace = tty->tio.c_cc[VERASE];
		if (bspace != _POSIX_VDISABLE && *key == bspace)
			*key = KEYC_BSPACE;
		goto found;
	}

	/* Look for matching key string and return if found. */
	tk = tty_keys_find(tty, buf + 1, len - 1, &size);
	if (tk != NULL) {
		buffer_remove(tty->in, size + 1);
		*key = tk->key;
		goto found;
	}

	/* Not found. Is this a mouse key press? */
	*key = tty_keys_mouse(buf, len, &size, mouse);
	if (*key != KEYC_NONE) {
		buffer_remove(tty->in, size);
		goto found;
	}

	/* Not found. Try to parse a key with an xterm-style modifier. */
	*key = xterm_keys_find(buf, len, &size);
	if (*key != KEYC_NONE) {
		buffer_remove(tty->in, size);
		goto found;
	}

	/* Escape but no key string. If the timer isn't started, start it. */
	if (!(tty->flags & TTY_ESCAPE)) {
		tv.tv_sec = 0;
		tv.tv_usec = ESCAPE_PERIOD * 1000L;
		if (gettimeofday(&tty->key_timer, NULL) != 0)
			fatal("gettimeofday failed");
		timeradd(&tty->key_timer, &tv, &tty->key_timer);

		tty->flags |= TTY_ESCAPE;
		return (1);
	}

	/* Skip the escape. */
	buf++;
	len--;

	/* Is there a normal key following? */
	if (len != 0 && *buf != '\033') {
		buffer_remove(tty->in, 1);
		*key = buffer_read8(tty->in) | KEYC_ESCAPE;
		goto found;
	}

	/* Or a key string? */
	if (len > 1) {
		tk = tty_keys_find(tty, buf + 1, len - 1, &size);
		if (tk != NULL) {
			buffer_remove(tty->in, size + 2);
			*key = tk->key | KEYC_ESCAPE;
			goto found;
		}
	}

	/* If the timer hasn't expired, keep waiting. */
	if (gettimeofday(&tv, NULL) != 0)
		fatal("gettimeofday failed");
	if (timercmp(&tty->key_timer, &tv, >))
		return (1);

	/* Give up and return the escape. */
	buffer_remove(tty->in, 1);
	*key = '\033';

found:
	tty->flags &= ~TTY_ESCAPE;
	return (0);
}

int
tty_keys_mouse(char *buf, size_t len, size_t *size, struct mouse_event *m)
{
	/*
	 * Mouse sequences are \033[M followed by three characters indicating
	 * buttons, X and Y, all based at 32 with 1,1 top-left.
	 */

	log_debug("mouse input is: %.*s", (int) len, buf);
	if (len != 6 || memcmp(buf, "\033[M", 3) != 0)
		return (KEYC_NONE);
	*size = 6;

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
