/* $Id: input-keys.c,v 1.10 2008-06-25 07:30:08 nicm Exp $ */

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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

struct {
	int		 key;
	const char	*data;
} input_keys[] = {
/*	{ KEYC_BACKSPACE, "\010" }, */

	{ KEYC_DC,     "\033[3~" },
	{ KEYC_F1,     "\033OP" },
	{ KEYC_F10,    "\033[21~" },
	{ KEYC_F11,    "\033[23~" },
	{ KEYC_F12,    "\033[24~" },
	{ KEYC_F2,     "\033OQ" },
	{ KEYC_F3,     "\033OR" },
	{ KEYC_F4,     "\033OS" },
	{ KEYC_F5,     "\033[15~" },
	{ KEYC_F6,     "\033[17~" },
	{ KEYC_F7,     "\033[18~" },
	{ KEYC_F8,     "\033[19~" },
	{ KEYC_F9,     "\033[20~" },
	{ KEYC_FIND,   "\033[1~" },
	{ KEYC_IC,     "\033[2~" },
	{ KEYC_NPAGE,  "\033[6~" },
	{ KEYC_PPAGE,  "\033[5~" },
	{ KEYC_SELECT, "\033[4~" },

	{ KEYC_UP,     "\033OA" },
	{ KEYC_DOWN,   "\033OB" },
	{ KEYC_LEFT,   "\033OD" },
	{ KEYC_RIGHT,  "\033OC" },

	{ KEYC_A1,     "\033Ow" },
	{ KEYC_A3,     "\033Oy" },
	{ KEYC_B2,     "\033Ou" },
	{ KEYC_C1,     "\033Oq" },
	{ KEYC_C3,     "\033Os" }
};
#define NINPUTKEYS (sizeof input_keys / sizeof input_keys[0])

/* Translate a key code from client into an output key sequence. */
void
input_key(struct window *w, int key)
{
	u_int	i;

	log_debug2("writing key %x", key);

	if (KEYC_ISESCAPE(key)) {
		buffer_write8(w->out, '\033');
		key = KEYC_REMOVEESCAPE(key);
	}

	if (key != KEYC_NONE && key < KEYC_OFFSET) {
		buffer_write8(w->out, (uint8_t) key);
		return;
	}

#ifdef notyetifever
/* XXX can't we just pass the keypad changes through to tty? */
	if (!(w->screen->mode & MODE_KKEYPAD)) {
		switch (key) {
		case KEYC_A1:
			buffer_write8(w->out, '9');
			return;
		case KEYC_UP:
			buffer_write8(w->out, '8');
			return;
		case KEYC_A3:
			buffer_write8(w->out, '7');
			return;
		case KEYC_LEFT:
			buffer_write8(w->out, '6');
			return;
		case KEYC_B2:
			buffer_write8(w->out, '5');
			return;
		case KEYC_RIGHT:
			buffer_write8(w->out, '4');
			return;
		case KEYC_C1:
			buffer_write8(w->out, '3');
			return;
		case KEYC_DOWN:
			buffer_write8(w->out, '2');
			return;
		case KEYC_C3:
			buffer_write8(w->out, '1');
			return;
		}
	}
#endif

	for (i = 0; i < NINPUTKEYS; i++) {
		if (input_keys[i].key == key)
			break;
	}
	if (i == NINPUTKEYS) {
		log_debug2("key %d missing", key);
		return;
	}

	log_debug2("found key %d: \"%s\"", key, input_keys[i].data);
	buffer_write(w->out, input_keys[i].data, strlen(input_keys[i].data));
}
