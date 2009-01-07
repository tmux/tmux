/* $Id: input-keys.c,v 1.16 2009-01-07 22:52:33 nicm Exp $ */

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

#define INPUTKEY_KEYPAD 0x1
#define INPUTKEY_CURSOR 0x2
struct {
	int		 key;
	const char	*data;
	int		 flags;
} input_keys[] = {
	{ KEYC_F1,     "\033OP", 0 },
	{ KEYC_F2,     "\033OQ", 0 },
	{ KEYC_F3,     "\033OR", 0 },
	{ KEYC_F4,     "\033OS", 0 },
	{ KEYC_F5,     "\033[15~", 0 },
	{ KEYC_F6,     "\033[17~", 0 },
	{ KEYC_F7,     "\033[18~", 0 },
	{ KEYC_F8,     "\033[19~", 0 },
	{ KEYC_F9,     "\033[20~", 0 },
	{ KEYC_F10,    "\033[21~", 0 },
	{ KEYC_F11,    "\033[23~", 0 },
	{ KEYC_F12,    "\033[24~", 0 },
	{ KEYC_DC,     "\033[3~", 0 },
	{ KEYC_IC,     "\033[2~", 0 },
	{ KEYC_NPAGE,  "\033[6~", 0 },
	{ KEYC_PPAGE,  "\033[5~", 0 },
	{ KEYC_HOME,   "\033[1~", 0 },
	{ KEYC_END,    "\033[4~", 0 },

	/* Arrow keys. Cursor versions must come first. */
	{ KEYC_UP,     "\033OA", INPUTKEY_CURSOR },
	{ KEYC_DOWN,   "\033OB", INPUTKEY_CURSOR },
	{ KEYC_LEFT,   "\033OD", INPUTKEY_CURSOR },
	{ KEYC_RIGHT,  "\033OC", INPUTKEY_CURSOR },
	{ KEYC_UP,     "\033[A", 0 },
	{ KEYC_DOWN,   "\033[B", 0 },
	{ KEYC_LEFT,   "\033[D", 0 },
	{ KEYC_RIGHT,  "\033[C", 0 },

	/* Keypad keys. Keypad versions must come first. */
	{ KEYC_KP0_1,  "/", INPUTKEY_KEYPAD },
	{ KEYC_KP0_2,  "*", INPUTKEY_KEYPAD },
	{ KEYC_KP0_3,  "-", INPUTKEY_KEYPAD },
	{ KEYC_KP1_0,  "7", INPUTKEY_KEYPAD },
	{ KEYC_KP1_1,  "8", INPUTKEY_KEYPAD },
	{ KEYC_KP1_2,  "9", INPUTKEY_KEYPAD },
	{ KEYC_KP1_3,  "+", INPUTKEY_KEYPAD },
	{ KEYC_KP2_0,  "4", INPUTKEY_KEYPAD },
	{ KEYC_KP2_1,  "5", INPUTKEY_KEYPAD },
	{ KEYC_KP2_2,  "6", INPUTKEY_KEYPAD },
	{ KEYC_KP3_0,  "1", INPUTKEY_KEYPAD },
	{ KEYC_KP3_1,  "2", INPUTKEY_KEYPAD },
	{ KEYC_KP3_2,  "3", INPUTKEY_KEYPAD },
	{ KEYC_KP3_3,  "\n", INPUTKEY_KEYPAD }, /* this can be CRLF too? */
	{ KEYC_KP4_0,  "0", INPUTKEY_KEYPAD },
	{ KEYC_KP4_2,  ".", INPUTKEY_KEYPAD },
	{ KEYC_KP0_1,  "\033Oo", 0 },
	{ KEYC_KP0_2,  "\033Oj", 0 },
	{ KEYC_KP0_3,  "\033Om", 0 },
	{ KEYC_KP1_0,  "\033Ow", 0 },
	{ KEYC_KP1_1,  "\033Ox", 0 },
	{ KEYC_KP1_2,  "\033Oy", 0 },
	{ KEYC_KP1_3,  "\033Ok", 0 },
	{ KEYC_KP2_0,  "\033Ot", 0 },
	{ KEYC_KP2_1,  "\033Ou", 0 },
	{ KEYC_KP2_2,  "\033Ov", 0 },
	{ KEYC_KP3_0,  "\033Oq", 0 },
	{ KEYC_KP3_1,  "\033Or", 0 },
	{ KEYC_KP3_2,  "\033Os", 0 },
	{ KEYC_KP3_3,  "\033OM", 0 },
	{ KEYC_KP4_0,  "\033Op", 0 },
	{ KEYC_KP4_2,  "\033On", 0 },
};

/* Translate a key code from client into an output key sequence. */
void
input_key(struct window *w, int key)
{
	u_int	i;

	log_debug2("writing key 0x%x", key);

	if (KEYC_ISESCAPE(key)) {
		buffer_write8(w->out, '\033');
		key = KEYC_REMOVEESCAPE(key);
	}

	if (key != KEYC_NONE && key < KEYC_OFFSET) {
		buffer_write8(w->out, (uint8_t) key);
		return;
	}

	for (i = 0; i < nitems(input_keys); i++) {
		if ((input_keys[i].flags & INPUTKEY_KEYPAD) &&
		    !(w->screen->mode & MODE_KKEYPAD))
			continue;
		if ((input_keys[i].flags & INPUTKEY_CURSOR) &&
		    !(w->screen->mode & MODE_KCURSOR))
			continue;
		if (input_keys[i].key == key)
			break;
	}
	if (i == nitems(input_keys)) {
		log_debug2("key 0x%x missing", key);
		return;
	}

	log_debug2("found key 0x%x: \"%s\"", key, input_keys[i].data);
	buffer_write(w->out, input_keys[i].data, strlen(input_keys[i].data));
}
