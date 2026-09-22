/*
 * Copyright (c) 2026 Arthur Chan <arthur.chan@adalogics.com>
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

/*
 * Fuzz the custom layout parser.
 *
 * A layout string such as "bb62,80x24,0,0{40x24,0,0,1,39x24,41,0,2}" is
 * accepted by select-layout and is what tmux stores and restores for a
 * window, so it is parsed from configuration and from commands. It drives
 * layout-custom.c (the string parser and the checksum), then layout.c, which
 * resizes and assigns the cells.
 *
 * layout_parse() refuses a window with no panes, and refuses a layout whose
 * cell count is smaller than the pane count, so the window is given a fixed
 * number of panes. The count is fixed rather than derived from the input, so
 * a mutation always means a different layout string.
 */

#include <sys/types.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

#define FUZZER_MAXLEN 1024
#define FUZZER_PANES 4

struct event_base	*libevent;

int
LLVMFuzzerTestOneInput(const u_char *data, size_t size)
{
	struct window		*w;
	struct window_pane	*wp;
	char			*buf, *cause = NULL, *dump;
	u_int			 i;

	if (size == 0 || size > FUZZER_MAXLEN)
		return 0;

	/* layout_parse() takes a C string. */
	buf = malloc(size + 1);
	if (buf == NULL)
		return 0;
	memcpy(buf, data, size);
	buf[size] = '\0';

	w = window_create(80, 24, 0, 0);
	if (w == NULL) {
		free(buf);
		return 0;
	}
	window_add_ref(w, __func__);

	for (i = 0; i < FUZZER_PANES; i++) {
		wp = window_add_pane(w, NULL, 0, 0);
		if (w->active == NULL)
			w->active = wp;
	}

	if (layout_parse(w, buf, &cause) == 0) {
		dump = layout_dump(w, w->layout_root, 1);
		free(dump);
	}
	free(cause);

	window_remove_ref(w, __func__);

	free(buf);
	return 0;
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

	socket_path = xstrdup("dummy");

	return 0;
}
