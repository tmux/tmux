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
 * Fuzz the tmux command parser (cmd_parse_from_buffer).
 *
 * This exercises:
 *   - cmd-parse.y (yacc grammar, lexer, command building)
 *   - cmd.c (command lookup and validation)
 *   - arguments.c (argument parsing and flag handling)
 *   - cmd-find.c (target resolution)
 *   - options.c (option name lookups during parsing)
 */

#include <stddef.h>
#include <string.h>

#include "tmux.h"

struct event_base *libevent;

int
LLVMFuzzerTestOneInput(const u_char *data, size_t size)
{
	struct cmd_parse_input	 pi;
	struct cmd_parse_result	*pr;

	if (size > 2048 || size == 0)
		return 0;

	memset(&pi, 0, sizeof pi);
	pi.flags = CMD_PARSE_QUIET;

	pr = cmd_parse_from_buffer(data, size, &pi);
	switch (pr->status) {
	case CMD_PARSE_SUCCESS:
		cmd_list_free(pr->cmdlist);
		break;
	case CMD_PARSE_ERROR:
		free(pr->error);
		break;
	default:
		break;
	}

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
