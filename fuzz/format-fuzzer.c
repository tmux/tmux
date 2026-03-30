/*
 * Copyright (c) 2026 David Korczynski <david@adalogics.com>
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
 * Fuzz the tmux format string expander (format_expand).
 *
 * This exercises:
 *   - format.c (format parsing, modifier chains, conditionals, math, regex)
 *   - colour.c (colour name and RGB parsing within formats)
 *   - utf8.c (UTF-8 width calculations in format padding)
 */

#include <stddef.h>
#include <string.h>

#include "tmux.h"

struct event_base *libevent;

int
LLVMFuzzerTestOneInput(const u_char *data, size_t size)
{
	struct format_tree	*ft;
	char			*buf, *expanded;

	if (size > 2048 || size == 0)
		return 0;

	/* Null-terminate the input for format_expand. */
	buf = malloc(size + 1);
	if (buf == NULL)
		return 0;
	memcpy(buf, data, size);
	buf[size] = '\0';

	ft = format_create(NULL, NULL, 0, FORMAT_NOJOBS);
	format_add(ft, "session_name", "%s", "fuzz-session");
	format_add(ft, "window_index", "%d", 0);
	format_add(ft, "window_name", "%s", "fuzz-window");
	format_add(ft, "pane_index", "%d", 0);
	format_add(ft, "pane_id", "%s", "%%0");
	format_add(ft, "host", "%s", "fuzzhost");
	format_add(ft, "pane_width", "%d", 80);
	format_add(ft, "pane_height", "%d", 25);

	expanded = format_expand(ft, buf);
	free(expanded);
	format_free(ft);

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
