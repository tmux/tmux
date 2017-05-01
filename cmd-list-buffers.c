/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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
 * List paste buffers.
 */

#define LIST_BUFFERS_TEMPLATE						\
	"#{buffer_name}: #{buffer_size} bytes: \"#{buffer_sample}\""

static enum cmd_retval	cmd_list_buffers_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_list_buffers_entry = {
	.name = "list-buffers",
	.alias = "lsb",

	.args = { "F:", 0, 0 },
	.usage = "[-F format]",

	.flags = CMD_AFTERHOOK,
	.exec = cmd_list_buffers_exec
};

static enum cmd_retval
cmd_list_buffers_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = self->args;
	struct paste_buffer	*pb;
	struct format_tree	*ft;
	char			*line;
	const char		*template;

	if ((template = args_get(args, 'F')) == NULL)
		template = LIST_BUFFERS_TEMPLATE;

	pb = NULL;
	while ((pb = paste_walk(pb)) != NULL) {
		ft = format_create(item->client, item, FORMAT_NONE, 0);
		format_defaults_paste_buffer(ft, pb);

		line = format_expand(ft, template);
		cmdq_print(item, "%s", line);
		free(line);

		format_free(ft);
	}

	return (CMD_RETURN_NORMAL);
}
