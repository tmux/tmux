/* $Id: input-keys.c,v 1.11 2008-07-23 23:44:50 nicm Exp $ */

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
	{ KEYC_F1,     "\033OP" },
	{ KEYC_F2,     "\033OQ" },
	{ KEYC_F3,     "\033OR" },
	{ KEYC_F4,     "\033OS" },
	{ KEYC_F5,     "\033[15~" },
	{ KEYC_F6,     "\033[17~" },
	{ KEYC_F7,     "\033[18~" },
	{ KEYC_F8,     "\033[19~" },
	{ KEYC_F9,     "\033[20~" },
	{ KEYC_F10,    "\033[21~" },
	{ KEYC_F11,    "\033[23~" },
	{ KEYC_F12,    "\033[24~" },
	{ KEYC_FIND,   "\033[1~" },
	{ KEYC_DC,     "\033[3~" },
	{ KEYC_IC,     "\033[2~" },
	{ KEYC_NPAGE,  "\033[6~" },
	{ KEYC_PPAGE,  "\033[5~" },
	{ KEYC_SELECT, "\033[4~" },

	{ KEYC_UP,     "\033[A" },
	{ KEYC_DOWN,   "\033[B" },
	{ KEYC_LEFT,   "\033[D" },
	{ KEYC_RIGHT,  "\033[C" },

	{ KEYC_KP0_1,  "\033Oo" },
	{ KEYC_KP0_2,  "\033Oj" },
	{ KEYC_KP0_3,  "\033Om" },
	{ KEYC_KP1_0,  "\033Ow" },
	{ KEYC_KP1_1,  "\033Ox" },
	{ KEYC_KP1_2,  "\033Oy" },
	{ KEYC_KP1_3,  "\033Ok" },
	{ KEYC_KP2_0,  "\033Ot" },
	{ KEYC_KP2_1,  "\033Ou" },
	{ KEYC_KP2_2,  "\033Ov" },
	{ KEYC_KP3_0,  "\033Oq" },
	{ KEYC_KP3_1,  "\033Or" },
	{ KEYC_KP3_2,  "\033Os" },
	{ KEYC_KP3_3,  "\033OM" },
	{ KEYC_KP4_0,  "\033Op" },
	{ KEYC_KP4_2,  "\033On" },
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
