/*
 * Copyright (c) 2026 OSS-Fuzz contributors
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Fuzz tmux copy mode (window-copy.c).
 *
 * No existing harness reaches the interactive layer of tmux: window-copy.c
 * alone is ~5.1k lines at 0% coverage (plus grid-reader.c). This harness
 * fills a pane grid by feeding terminal bytes through the real input
 * parser, enters copy mode over that attacker-controlled grid, then drives
 * the mode with key events (cursor motion, selection, search prompts,
 * scrolling, history) and pane resizes.
 *
 * Input layout:
 *   data[0]       pane width selector
 *   data[1]       pane height selector
 *   data[2]       mode-keys vi/emacs bit + copy-mode -e/-H flag bits
 *   data[3]       fraction (in 255ths) of the remaining bytes used as the
 *                 terminal stream; the rest is the key event stream
 *   stream bytes  terminal escape-sequence stream (grid + history content)
 *   key bytes     key events; 0xff bytes resize the pane instead
 */

#include <assert.h>
#include <fcntl.h>
#include <stddef.h>

#include "tmux.h"

#define FUZZER_MAXLEN 1024
#define STREAM_MAXLEN 512

struct event_base *libevent;

/* Map one key-stream byte to a key code. */
static key_code
copy_mode_fuzzer_key(u_char b)
{
	switch (b) {
	case 0x80: return (KEYC_UP);
	case 0x81: return (KEYC_DOWN);
	case 0x82: return (KEYC_LEFT);
	case 0x83: return (KEYC_RIGHT);
	case 0x84: return (KEYC_NPAGE);
	case 0x85: return (KEYC_PPAGE);
	case 0x86: return (KEYC_HOME);
	case 0x87: return (KEYC_END);
	case 0x88: return (KEYC_IC);
	case 0x89: return (KEYC_DC);
	case 0x8a: return (KEYC_BTAB);
	case 0x8b: return (KEYC_BSPACE);
	}
	if (b < 0x20)
		return (b == 0 ? KEYC_CTRL | ' ' : b); /* control chars */
	if (b <= 0x7e)
		return (b); /* printable */
	return (KEYC_NONE);
}

int
LLVMFuzzerTestOneInput(const u_char *data, size_t size)
{
	struct window		*w;
	struct window_pane	*wp;
	struct args		*args;
	size_t			 keys_off, stream_len, i;
	u_int			 sx, sy;
	key_code		 key;
	int			 error;

	/*
	 * Since AFL doesn't support -max_len parameter we have to
	 * discard long inputs manually.
	 */
	if (size < 8 || size > FUZZER_MAXLEN)
		return (0);

	sx = 2 + (data[0] % 79); /* pane width 2..80 */
	sy = 2 + (data[1] % 59); /* pane height 2..60 */

	options_set_number(global_w_options, "mode-keys",
	    (data[2] & 1) ? MODEKEY_VI : MODEKEY_EMACS);

	/* Split the payload into a terminal stream and a key stream. */
	stream_len = ((size - 4) * (u_int)data[3]) / 255;
	if (stream_len > STREAM_MAXLEN)
		stream_len = STREAM_MAXLEN;
	if (4 + stream_len > size)
		stream_len = size - 4;
	keys_off = 4 + stream_len;

	args = args_create();
	if (data[2] & 2)
		args_set(args, 'e', NULL, 0);
	if (data[2] & 4)
		args_set(args, 'H', NULL, 0);

	w = window_create(sx, sy, 0, 0);
	wp = window_add_pane(w, NULL, 0, 0);
	wp->ictx = input_init(wp, NULL);
	window_add_ref(w, __func__);

	wp->fd = open("/dev/null", O_WRONLY);
	if (wp->fd == -1)
		errx(1, "open(\"/dev/null\") failed");
	wp->event = bufferevent_new(wp->fd, NULL, NULL, NULL, NULL);

	/* Build the pane grid (and scrollback history) from the stream. */
	if (stream_len != 0)
		input_parse_buffer(wp, data + 4, stream_len);

	/* Enter copy mode over the freshly built grid. */
	window_pane_set_mode(wp, wp, &window_copy_mode, NULL, NULL, args);
	args_free(args);

	/* Drive the mode with key events; 0xff resizes the pane. */
	for (i = keys_off; i < size; i++) {
		if (data[i] == 0xff) {
			window_pane_resize(wp, 2 + ((i * 37) % 79),
			    2 + ((i * 53) % 59));
			continue;
		}
		key = copy_mode_fuzzer_key(data[i]);
		if (key == KEYC_NONE)
			continue;
		window_pane_key(wp, NULL, NULL, NULL, key, NULL);
	}

	/* Drain anything the mode queued (copy and paste commands). */
	while (cmdq_next(NULL) != 0)
		;
	error = event_base_loop(libevent, EVLOOP_NONBLOCK);
	if (error == -1)
		errx(1, "event_base_loop failed");

	assert(w->references == 1);
	window_pane_reset_mode_all(wp);
	window_remove_ref(w, __func__);

	return (0);
}

int
LLVMFuzzerInitialize(__unused int *argc, __unused char ***argv)
{
	const struct options_table_entry	*oe;

	global_environ = environ_create();
	global_options = options_create(NULL);
	global_s_options = options_create(NULL);
	global_w_options = options_create(NULL);
	for (oe = options_table; oe->name != NULL; oe++) {
		if (oe->scope & OPTIONS_TABLE_SERVER)
			options_default(global_options, oe);
		if (oe->scope & OPTIONS_TABLE_SESSION)
			options_default(global_s_options, oe);
		if (oe->scope & OPTIONS_TABLE_WINDOW)
			options_default(global_w_options, oe);
	}
	libevent = osdep_event_init();

	options_set_number(global_w_options, "monitor-bell", 0);
	options_set_number(global_w_options, "allow-rename", 1);
	options_set_number(global_options, "set-clipboard", 2);
	socket_path = xstrdup("dummy");

	return (0);
}
