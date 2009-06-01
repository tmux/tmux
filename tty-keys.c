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

#include "tmux.h"

void	tty_keys_add(struct tty *, const char *, int, int);
int	tty_keys_parse_xterm(struct tty *, char *, size_t, size_t *);
int	tty_keys_parse_mouse(struct tty *, char *, size_t, size_t *, u_char *);

struct tty_key_ent {
	enum tty_code_code	code;
	const char	       *string;

	int	 	 	key;
	int		 	flags;
};

struct tty_key_ent tty_keys[] = {
	/* Function keys. */
	{ TTYC_KF1,   NULL,     KEYC_F1,    TTYKEY_CTRL },
	{ TTYC_KF2,   NULL,     KEYC_F2,    TTYKEY_CTRL },
	{ TTYC_KF3,   NULL,     KEYC_F3,    TTYKEY_CTRL },
	{ TTYC_KF4,   NULL,     KEYC_F4,    TTYKEY_CTRL },
	{ TTYC_KF5,   NULL,     KEYC_F5,    TTYKEY_CTRL },
	{ TTYC_KF6,   NULL,     KEYC_F6,    TTYKEY_CTRL },
	{ TTYC_KF7,   NULL,     KEYC_F7,    TTYKEY_CTRL },
	{ TTYC_KF8,   NULL,     KEYC_F8,    TTYKEY_CTRL },
	{ TTYC_KF9,   NULL,     KEYC_F9,    TTYKEY_CTRL },
	{ TTYC_KF10,  NULL,     KEYC_F10,   TTYKEY_CTRL },
	{ TTYC_KF11,  NULL,     KEYC_F11,   TTYKEY_CTRL },
	{ TTYC_KF12,  NULL,     KEYC_F12,   TTYKEY_CTRL },
	{ TTYC_KF13,  NULL,     KEYC_F13,   TTYKEY_CTRL },
	{ TTYC_KF14,  NULL,     KEYC_F14,   TTYKEY_CTRL },
	{ TTYC_KF15,  NULL,     KEYC_F15,   TTYKEY_CTRL },
	{ TTYC_KF16,  NULL,     KEYC_F16,   TTYKEY_CTRL },
	{ TTYC_KF17,  NULL,     KEYC_F17,   TTYKEY_CTRL },
	{ TTYC_KF18,  NULL,     KEYC_F18,   TTYKEY_CTRL },
	{ TTYC_KF19,  NULL,     KEYC_F19,   TTYKEY_CTRL },
	{ TTYC_KF20,  NULL,     KEYC_F20,   TTYKEY_CTRL },
	{ TTYC_KICH1, NULL,     KEYC_IC,    TTYKEY_CTRL },
	{ TTYC_KDCH1, NULL,     KEYC_DC,    TTYKEY_CTRL },
	{ TTYC_KHOME, NULL,     KEYC_HOME,  TTYKEY_CTRL },
	{ TTYC_KEND,  NULL,     KEYC_END,   TTYKEY_CTRL },
	{ TTYC_KNP,   NULL,     KEYC_NPAGE, TTYKEY_CTRL },
	{ TTYC_KPP,   NULL,     KEYC_PPAGE, TTYKEY_CTRL },
	{ TTYC_KCBT,  NULL,	KEYC_BTAB,  TTYKEY_CTRL },

	/* Arrow keys. */
	{ 0,          "\033OA", KEYC_UP,    TTYKEY_RAW },
	{ 0,          "\033OB", KEYC_DOWN,  TTYKEY_RAW },
	{ 0,          "\033OC", KEYC_RIGHT, TTYKEY_RAW },
	{ 0,          "\033OD", KEYC_LEFT,  TTYKEY_RAW },

	{ 0,          "\033[A", KEYC_UP,    TTYKEY_RAW },
	{ 0,          "\033[B", KEYC_DOWN,  TTYKEY_RAW },
	{ 0,          "\033[C", KEYC_RIGHT, TTYKEY_RAW },
	{ 0,          "\033[D", KEYC_LEFT,  TTYKEY_RAW },

	{ 0,          "\033Oa", KEYC_ADDCTL(KEYC_UP),    TTYKEY_RAW },
	{ 0,          "\033Ob", KEYC_ADDCTL(KEYC_DOWN),  TTYKEY_RAW },
	{ 0,          "\033Oc", KEYC_ADDCTL(KEYC_RIGHT), TTYKEY_RAW },
	{ 0,          "\033Od", KEYC_ADDCTL(KEYC_LEFT),  TTYKEY_RAW },
	{ 0,          "\033[a", KEYC_ADDSFT(KEYC_UP),    TTYKEY_RAW },
	{ 0,          "\033[b", KEYC_ADDSFT(KEYC_DOWN),  TTYKEY_RAW },
	{ 0,          "\033[c", KEYC_ADDSFT(KEYC_RIGHT), TTYKEY_RAW },
	{ 0,          "\033[d", KEYC_ADDSFT(KEYC_LEFT),  TTYKEY_RAW },

	{ TTYC_KCUU1, NULL,     KEYC_UP,    TTYKEY_CTRL },
	{ TTYC_KCUD1, NULL,     KEYC_DOWN,  TTYKEY_CTRL },
	{ TTYC_KCUB1, NULL,     KEYC_LEFT,  TTYKEY_CTRL },
	{ TTYC_KCUF1, NULL,     KEYC_RIGHT, TTYKEY_CTRL },

	/*
	 * Numeric keypad. termcap and terminfo are totally confusing for this.
	 * There are definitions for some keypad keys and for function keys,
	 * but these seem to now be used for the real function keys rather than
	 * for the keypad keys in application mode (which is different from
	 * what it says in the termcap file). So, we just hardcode the vt100
	 * escape sequences here and always put the terminal into keypad_xmit
	 * mode. Translation of numbers mode/applications mode is done in
	 * input-keys.c.
	 */
	{ 0,          "\033Oo", KEYC_KP0_1, TTYKEY_RAW },
	{ 0,          "\033Oj", KEYC_KP0_2, TTYKEY_RAW },
	{ 0,          "\033Om", KEYC_KP0_3, TTYKEY_RAW },
	{ 0,          "\033Ow", KEYC_KP1_0, TTYKEY_RAW },
	{ 0,          "\033Ox", KEYC_KP1_1, TTYKEY_RAW },
	{ 0,          "\033Oy", KEYC_KP1_2, TTYKEY_RAW },
	{ 0,          "\033Ok", KEYC_KP1_3, TTYKEY_RAW },
	{ 0,          "\033Ot", KEYC_KP2_0, TTYKEY_RAW },
	{ 0,          "\033Ou", KEYC_KP2_1, TTYKEY_RAW },
	{ 0,          "\033Ov", KEYC_KP2_2, TTYKEY_RAW },
	{ 0,          "\033Oq", KEYC_KP3_0, TTYKEY_RAW },
	{ 0,          "\033Or", KEYC_KP3_1, TTYKEY_RAW },
	{ 0,          "\033Os", KEYC_KP3_2, TTYKEY_RAW },
	{ 0,          "\033OM", KEYC_KP3_3, TTYKEY_RAW },
	{ 0,          "\033Op", KEYC_KP4_0, TTYKEY_RAW },
	{ 0,          "\033On", KEYC_KP4_2, TTYKEY_RAW },
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
			tty_keys_add(tty, tmp + 1, KEYC_ADDCTL(tke->key), 0);
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
tty_keys_next(struct tty *tty, int *key, u_char *mouse)
{
	struct tty_key	*tk;
	struct timeval	 tv;
	char		*buf;
	size_t		 len, size;

	buf = BUFFER_OUT(tty->in);
	len = BUFFER_USED(tty->in);
	if (len == 0)
		return (1);
	log_debug("keys are %zu (%.*s)", len, (int) len, buf);

	/* If a normal key, return it. */
	if (*buf != '\033') {
		*key = buffer_read8(tty->in);
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
	*key = tty_keys_parse_mouse(tty, buf, len, &size, mouse);
	if (*key != KEYC_NONE) {
		buffer_remove(tty->in, size);
		goto found;
	}

	/* Not found. Try to parse xterm-type arguments. */
	*key = tty_keys_parse_xterm(tty, buf, len, &size);
	if (*key != KEYC_NONE) {
		buffer_remove(tty->in, size);
		goto found;
	}

	/* Escape but no key string. If the timer isn't started, start it. */
	if (!(tty->flags & TTY_ESCAPE)) {
		tv.tv_sec = 0;
		tv.tv_usec = ESCAPE_PERIOD * 1000L;
		if (gettimeofday(&tty->key_timer, NULL) != 0)
			fatal("gettimeofday");
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
		*key = KEYC_ADDESC(buffer_read8(tty->in));
		goto found;
	}

	/* Or a key string? */
	if (len > 1) {
		tk = tty_keys_find(tty, buf + 1, len - 1, &size);
		if (tk != NULL) {
			buffer_remove(tty->in, size + 2);
			*key = KEYC_ADDESC(tk->key);
			goto found;
		}
	}

	/* If the timer hasn't expired, keep waiting. */
	if (gettimeofday(&tv, NULL) != 0)
		fatal("gettimeofday");
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
tty_keys_parse_mouse(
    unused struct tty *tty, char *buf, size_t len, size_t *size, u_char *mouse)
{
	/*
	 * Mouse sequences are \033[M followed by three characters indicating
	 * buttons, X and Y, all based at 32 with 1,1 top-left.
	 */

	log_debug("mouse input is: %.*s", (int) len, buf);
	if (len != 6 || memcmp(buf, "\033[M", 3) != 0)
		return (KEYC_NONE);
	*size = 6;

	if (buf[3] < 32 || buf[4] < 33 || buf[5] < 33)
		return (KEYC_NONE);

	mouse[0] = buf[3] - 32;
	mouse[1] = buf[4] - 33;
	mouse[2] = buf[5] - 33;
	return (KEYC_MOUSE);
}

int
tty_keys_parse_xterm(struct tty *tty, char *buf, size_t len, size_t *size)
{
	struct tty_key	*tk;
	char		 tmp[5];
	size_t		 tmplen;
	int		 key;

	/*
	 * xterm sequences with modifier keys are of the form:
	 *
	 * ^[[1;xD becomes ^[[D
	 * ^[[5;x~ becomes ^[[5~
	 *
	 * This function is a bit of a hack. Need to figure out what exact
	 * format and meaning xterm outputs and fix it. XXX
	 */

	log_debug("xterm input is: %.*s", (int) len, buf);
	if (len != 6 || memcmp(buf, "\033[1;", 4) != 0)
		return (KEYC_NONE);
	*size = 6;

	tmplen = 0;
	tmp[tmplen++] = '[';
	if (buf[5] == '~') {
		tmp[tmplen++] = buf[2];
		tmp[tmplen++] = '~';
	} else
		tmp[tmplen++] = buf[5];
	log_debug("xterm output is: %.*s", (int) tmplen, tmp);

	tk = tty_keys_find(tty, tmp, tmplen, size);
	if (tk == NULL)
		return (KEYC_NONE);
	key = tk->key;

	switch (buf[4]) {
	case '8':
		key = KEYC_ADDSFT(key);
		key = KEYC_ADDESC(key);
		key = KEYC_ADDCTL(key);
		break;
	case '7':
		key = KEYC_ADDESC(key);
		key = KEYC_ADDCTL(key);
		break;
	case '6':
		key = KEYC_ADDSFT(key);
		key = KEYC_ADDCTL(key);
		break;
	case '5':
		key = KEYC_ADDCTL(key);
		break;
	case '4':
		key = KEYC_ADDSFT(key);
		key = KEYC_ADDESC(key);
		break;
	case '3':
		key = KEYC_ADDESC(key);
		break;
	case '2':
		key = KEYC_ADDSFT(key);
		break;
	}

	*size = 6;
	return (key);
}
