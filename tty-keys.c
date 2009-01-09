/* $Id: tty-keys.c,v 1.16 2009-01-09 23:57:42 nicm Exp $ */

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

struct tty_key_ent {
	enum tty_code_code	code;
	const char	       *string;

	int	 	 	key;
	int		 	flags;
};

struct tty_key_ent tty_keys[] = {
	/* Function keys. */
	{ TTYC_KF1,   NULL,     KEYC_F1,    TTYKEY_MODIFIER },
	{ TTYC_KF2,   NULL,     KEYC_F2,    TTYKEY_MODIFIER },
	{ TTYC_KF3,   NULL,     KEYC_F3,    TTYKEY_MODIFIER },
	{ TTYC_KF4,   NULL,     KEYC_F4,    TTYKEY_MODIFIER },
	{ TTYC_KF5,   NULL,     KEYC_F5,    TTYKEY_MODIFIER },
	{ TTYC_KF6,   NULL,     KEYC_F6,    TTYKEY_MODIFIER },
	{ TTYC_KF7,   NULL,     KEYC_F7,    TTYKEY_MODIFIER },
	{ TTYC_KF8,   NULL,     KEYC_F8,    TTYKEY_MODIFIER },
	{ TTYC_KF9,   NULL,     KEYC_F9,    TTYKEY_MODIFIER },
	{ TTYC_KF10,  NULL,     KEYC_F10,   TTYKEY_MODIFIER },
	{ TTYC_KF11,  NULL,     KEYC_F11,   TTYKEY_MODIFIER },
	{ TTYC_KF12,  NULL,     KEYC_F12,   TTYKEY_MODIFIER },
	{ TTYC_KICH1, NULL,     KEYC_IC,    TTYKEY_MODIFIER },
	{ TTYC_KDCH1, NULL,     KEYC_DC,    TTYKEY_MODIFIER },
	{ TTYC_KHOME, NULL,     KEYC_HOME,  TTYKEY_MODIFIER },
	{ TTYC_KEND,  NULL,     KEYC_END,   TTYKEY_MODIFIER },
	{ TTYC_KNP,   NULL,     KEYC_NPAGE, TTYKEY_MODIFIER },
	{ TTYC_KPP,   NULL,     KEYC_PPAGE, TTYKEY_MODIFIER },

	/* Arrow keys. */ 
	{ TTYC_KCUU1, NULL,     KEYC_UP,    TTYKEY_MODIFIER },
	{ TTYC_KCUD1, NULL,     KEYC_DOWN,  TTYKEY_MODIFIER },
	{ TTYC_KCUB1, NULL,     KEYC_LEFT,  TTYKEY_MODIFIER },
	{ TTYC_KCUF1, NULL,     KEYC_RIGHT, TTYKEY_MODIFIER },
	{ 0,          "\033OA", KEYC_UP,    TTYKEY_RAW|TTYKEY_MODIFIER },
	{ 0,          "\033OB", KEYC_DOWN,  TTYKEY_RAW|TTYKEY_MODIFIER },
	{ 0,          "\033OD", KEYC_LEFT,  TTYKEY_RAW|TTYKEY_MODIFIER },
	{ 0,          "\033OC", KEYC_RIGHT, TTYKEY_RAW|TTYKEY_MODIFIER },
	{ 0,          "\033[A", KEYC_UP,    TTYKEY_RAW|TTYKEY_MODIFIER },
	{ 0,          "\033[B", KEYC_DOWN,  TTYKEY_RAW|TTYKEY_MODIFIER },
	{ 0,          "\033[D", KEYC_LEFT,  TTYKEY_RAW|TTYKEY_MODIFIER },
	{ 0,          "\033[C", KEYC_RIGHT, TTYKEY_RAW|TTYKEY_MODIFIER },

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
	struct tty_key	*tk;

	tk = xmalloc(sizeof *tk);
	tk->string = xstrdup(s);
	tk->key = key;
	tk->flags = flags;
	
	if (strlen(tk->string) > tty->ksize)
		tty->ksize = strlen(tk->string);
	RB_INSERT(tty_keys, &tty->ktree, tk);

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
		if (tke->flags & TTYKEY_MODIFIER) {
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
	struct tty_key	*tk, *tl;

	for (tk = RB_MIN(tty_keys, &tty->ktree); tk != NULL; tk = tl) {
		tl = RB_NEXT(tty_keys, &tty->ktree, tk);
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
			xfree(s);
			return (tk);
		}
	}
	xfree(s);

	return (NULL);
}

int
tty_keys_next(struct tty *tty, int *key)
{
	struct tty_key	*tk;
	struct timeval	 tv;
	char		 arg, *buf, tmp[32];
	size_t		 len, size;

	buf = BUFFER_OUT(tty->in);
	len = BUFFER_USED(tty->in);
	if (len == 0)
		return (1);
	log_debug("keys are %zu (%.*s)", len, (int) len, buf);

	/* If a normal key, return it. */
	if (*buf != '\033') {
		*key = buffer_read8(tty->in);
		return (0);
	}

	/* Look for matching key string and return if found. */
	tk = tty_keys_find(tty, buf + 1, len - 1, &size);
	if (tk != NULL) {
		*key = tk->key;
		buffer_remove(tty->in, size + 1);

		tty->flags &= ~TTY_ESCAPE;
		return (0);
	}

	/* Not found. Look for an xterm argument and try again. */
	if (len < sizeof tmp && len > 4 && buf[len - 3] == ';') {
		memcpy(tmp, buf, len);
		arg = tmp[len - 2];
		tmp[len - 3] = tmp[len - 1];	/* restore last */
		log_debug("argument is: %c", arg);

		tk = tty_keys_find(tty, tmp + 1, len - 3, &size);
		if (tk != NULL) {
			*key = tk->key;
			buffer_remove(tty->in, size + 3);
			
			switch (arg) {
			case '8':
				*key = KEYC_ADDSFT(*key);
				*key = KEYC_ADDESC(*key);
				*key = KEYC_ADDCTL(*key);
				break;
			case '7':
				*key = KEYC_ADDESC(*key);
				*key = KEYC_ADDCTL(*key);
				break;
			case '6':
				*key = KEYC_ADDSFT(*key);
				*key = KEYC_ADDCTL(*key);
				break;
			case '5':
				*key = KEYC_ADDCTL(*key);
				break;
			case '4':
				*key = KEYC_ADDSFT(*key);
				*key = KEYC_ADDESC(*key);
				break;
			case '3':
				*key = KEYC_ADDESC(*key);
				break;
			case '2':
				*key = KEYC_ADDSFT(*key);
				break;
			}

			tty->flags &= ~TTY_ESCAPE;
			return (0);
		}
	}

	/* Escape but no key string. If the timer isn't started, start it. */
	if (!(tty->flags & TTY_ESCAPE)) {
		tv.tv_sec = 0;
		tv.tv_usec = 500 * 1000L;
		if (gettimeofday(&tty->key_timer, NULL) != 0)
			fatal("gettimeofday");
		timeradd(&tty->key_timer, &tv, &tty->key_timer);

		tty->flags |= TTY_ESCAPE;
		return (1);
	}

	/* Otherwise, if the timer hasn't expired, wait. */
	if (gettimeofday(&tv, NULL) != 0)
		fatal("gettimeofday");
	if (!timercmp(&tty->key_timer, &tv, >))
		return (1);
	tty->flags &= ~TTY_ESCAPE;

	/* Remove the leading escape. */
	buffer_remove(tty->in, 1);
	buf = BUFFER_OUT(tty->in);
	len = BUFFER_USED(tty->in);

	/* If we have no following data, return escape. */
	if (len == 0) {
		*key = '\033';
		return (0);
	}

	/* If a normal key follows, return it. */
	if (*buf != '\033') {
		*key = KEYC_ADDESC(buffer_read8(tty->in));
		return (0);
	}

	/* Try to look up the key. */
	tk = tty_keys_find(tty, buf + 1, len - 1, &size);
	if (tk != NULL) {
		*key = KEYC_ADDESC(tk->key);
 		buffer_remove(tty->in, size + 1);
		return (0);
	}

	/* If not found, return escape-escape. */
	*key = KEYC_ADDESC('\033');
	buffer_remove(tty->in, 1);
	return (0);
}
