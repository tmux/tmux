/* $Id: tty-keys.c,v 1.13 2009-01-08 22:28:02 nicm Exp $ */

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

struct {
	const char	*name;
	int	 	 code;
} tty_keys[] = {
	/* Function keys. */
	{ "kf1",   KEYC_F1 },
	{ "kf2",   KEYC_F2 },
	{ "kf3",   KEYC_F3 },
	{ "kf4",   KEYC_F4 },
	{ "kf5",   KEYC_F5 },
	{ "kf6",   KEYC_F6 },
	{ "kf7",   KEYC_F7 },
	{ "kf8",   KEYC_F8 },
	{ "kf9",   KEYC_F9 },
	{ "kf10",  KEYC_F10 },
	{ "kf11",  KEYC_F11 },
	{ "kf12",  KEYC_F12 },
	{ "kich1", KEYC_IC },
	{ "kdch1", KEYC_DC },
	{ "khome", KEYC_HOME },
	{ "kend",  KEYC_END },
	{ "knp",   KEYC_NPAGE },
	{ "kpp",   KEYC_PPAGE },

	/* Arrow keys. */
	{ "kcuu1", KEYC_UP },
	{ "kcud1", KEYC_DOWN },
	{ "kcub1", KEYC_LEFT },
	{ "kcuf1", KEYC_RIGHT },

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
	{ "-\033Oo", KEYC_KP0_1 },
	{ "-\033Oj", KEYC_KP0_2 },
	{ "-\033Om", KEYC_KP0_3 },
	{ "-\033Ow", KEYC_KP1_0 },
	{ "-\033Ox", KEYC_KP1_1 },
	{ "-\033Oy", KEYC_KP1_2 },
	{ "-\033Ok", KEYC_KP1_3 },
	{ "-\033Ot", KEYC_KP2_0 },
	{ "-\033Ou", KEYC_KP2_1 },
	{ "-\033Ov", KEYC_KP2_2 },
	{ "-\033Oq", KEYC_KP3_0 },
	{ "-\033Or", KEYC_KP3_1 },
	{ "-\033Os", KEYC_KP3_2 },
	{ "-\033OM", KEYC_KP3_3 },
	{ "-\033Op", KEYC_KP4_0 },
	{ "-\033On", KEYC_KP4_2 },
};

RB_GENERATE(tty_keys, tty_key, entry, tty_keys_cmp);

struct tty_key *tty_keys_find(struct tty *, char *, size_t, size_t *);

int
tty_keys_cmp(struct tty_key *k1, struct tty_key *k2)
{
	return (strcmp(k1->string, k2->string));
}

void
tty_keys_init(struct tty *tty)
{
	struct tty_key	*tk;
	u_int		 i;
	const char	*s;

	RB_INIT(&tty->ktree);

	tty->ksize = 0;
	for (i = 0; i < nitems(tty_keys); i++) {
		if (*tty_keys[i].name == '-')
			s = tty_keys[i].name + 1;
		else {
			s = tigetstr(tty_keys[i].name);
			if (s == (char *) -1 || s == (char *) 0)
				continue;
		}
		if (s[0] != '\033' || s[1] == '\0')
			continue;

		tk = xmalloc(sizeof *tk);
		tk->string = xstrdup(s + 1);
		tk->code = tty_keys[i].code;

		if (strlen(tk->string) > tty->ksize)
			tty->ksize = strlen(tk->string);
		RB_INSERT(tty_keys, &tty->ktree, tk);

		log_debug("found key %x: size now %zu (%s)",
		    tk->code, tty->ksize, tk->string);
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
tty_keys_next(struct tty *tty, int *code)
{
	struct tty_key	*tk;
	size_t		 size;
	struct timeval	 tv;

	size = BUFFER_USED(tty->in);
	if (size == 0)
		return (1);
	log_debug("keys are %zu (%.*s)", size, (int) size, BUFFER_OUT(tty->in));

	/* If a normal key, return it. */
	if (*BUFFER_OUT(tty->in) != '\033') {
		*code = buffer_read8(tty->in);
		return (0);
	}

	/* Look for matching key string and return if found. */
	tk = tty_keys_find(tty, BUFFER_OUT(tty->in) + 1, size - 1, &size);
	if (tk != NULL) {
		*code = tk->code;
		buffer_remove(tty->in, size + 1);

		tty->flags &= ~TTY_ESCAPE;
		return (0);
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
	size = BUFFER_USED(tty->in);

	/* If we have no following data, return escape. */
	if (size == 0) {
		*code = '\033';
		return (0);
	}

	/* If a normal key follows, return it. */
	if (*BUFFER_OUT(tty->in) != '\033') {
		*code = KEYC_ADDESCAPE(buffer_read8(tty->in));
		return (0);
	}

	/* Try to look up the key. */
	tk = tty_keys_find(tty, BUFFER_OUT(tty->in) + 1, size - 1, &size);
	if (tk != NULL) {
		*code = KEYC_ADDESCAPE(tk->code);
 		buffer_remove(tty->in, size + 1);
		return (0);
	}

	/* If not found, return escape-escape. */
	*code = KEYC_ADDESCAPE('\033');
	buffer_remove(tty->in, 1);
	return (0);
}
