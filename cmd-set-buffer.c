/* $Id$ */

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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Add, set, or append to a paste buffer.
 */

enum cmd_retval	 cmd_set_buffer_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_set_buffer_entry = {
	"set-buffer", "setb",
	"ab:", 1, 1,
	"[-a] " CMD_BUFFER_USAGE " data",
	0,
	NULL,
	cmd_set_buffer_exec
};

enum cmd_retval
cmd_set_buffer_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct paste_buffer	*pb;
	u_int			 limit;
	char			*pdata, *cause;
	size_t			 psize, newsize;
	int			 buffer;

	limit = options_get_number(&global_options, "buffer-limit");

	psize = 0;
	pdata = NULL;

	pb = NULL;
	buffer = -1;

	if ((newsize = strlen(args->argv[0])) == 0)
		return (CMD_RETURN_NORMAL);

	if (args_has(args, 'b')) {
		buffer = args_strtonum(args, 'b', 0, INT_MAX, &cause);
		if (cause != NULL) {
			cmdq_error(cmdq, "buffer %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
		pb = paste_get_index(&global_buffers, buffer);
		if (pb == NULL) {
			cmdq_error(cmdq, "no buffer %d", buffer);
			return (CMD_RETURN_ERROR);
		}
	} else if (args_has(args, 'a')) {
		pb = paste_get_top(&global_buffers);
		if (pb != NULL)
			buffer = 0;
	}

	if (args_has(args, 'a') && pb != NULL) {
		psize = pb->size;
		pdata = xmalloc(psize);
		memcpy(pdata, pb->data, psize);
	}

	pdata = xrealloc(pdata, 1, psize + newsize);
	memcpy(pdata + psize, args->argv[0], newsize);
	psize += newsize;

	if (buffer == -1)
		paste_add(&global_buffers, pdata, psize, limit);
	else
		paste_replace(&global_buffers, buffer, pdata, psize);

	return (CMD_RETURN_NORMAL);
}
