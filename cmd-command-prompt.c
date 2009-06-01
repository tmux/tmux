/* $OpenBSD$ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicm@users.sourceforge.net>
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

#include <ctype.h>
#include <string.h>

#include "tmux.h"

/*
 * Prompt for command in client.
 */

void	cmd_command_prompt_init(struct cmd *, int);
int	cmd_command_prompt_exec(struct cmd *, struct cmd_ctx *);

int	cmd_command_prompt_callback(void *, const char *);

const struct cmd_entry cmd_command_prompt_entry = {
	"command-prompt", NULL,
	CMD_TARGET_CLIENT_USAGE " [template]",
	CMD_ARG01,
	cmd_command_prompt_init,
	cmd_target_parse,
	cmd_command_prompt_exec,
	cmd_target_send,
	cmd_target_recv,
	cmd_target_free,
	cmd_target_print
};

struct cmd_command_prompt_data {
	struct client	*c;
	char		*template;
};

void
cmd_command_prompt_init(struct cmd *self, int key)
{
	struct cmd_target_data	*data;

	cmd_target_init(self, key);
	data = self->data;

	switch (key) {
	case ',':
		data->arg = xstrdup("rename-window '%%'");
		break;
	case '.':
		data->arg = xstrdup("move-window -t '%%'");
		break;
	case 'f':
		data->arg = xstrdup("find-window '%%'");
		break;
	}
}

int
cmd_command_prompt_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data		*data = self->data;
	struct cmd_command_prompt_data	*cdata;
	struct client			*c;
	char				*hdr, *ptr;

	if ((c = cmd_find_client(ctx, data->target)) == NULL)
		return (-1);

	if (c->prompt_string != NULL)
		return (0);

	cdata = xmalloc(sizeof *cdata);
	cdata->c = c;
	if (data->arg != NULL) {
		cdata->template = xstrdup(data->arg);
		if ((ptr = strchr(data->arg, ' ')) == NULL)
			ptr = strchr(data->arg, '\0');
		xasprintf(&hdr, "(%.*s) ", (int) (ptr - data->arg), data->arg);
	} else {
		cdata->template = NULL;
		hdr = xstrdup(":");
	}
	status_prompt_set(c, hdr, cmd_command_prompt_callback, cdata, 0);
	xfree(hdr);

	return (0);
}

int
cmd_command_prompt_callback(void *data, const char *s)
{
	struct cmd_command_prompt_data	*cdata = data;
	struct client			*c = cdata->c;
	struct cmd_list			*cmdlist;
	struct cmd_ctx	 		 ctx;
	char				*cause, *ptr, *buf, ch;
	size_t				 len, slen;

	if (s == NULL) {
		xfree(cdata);
		return (0);
	}
	slen = strlen(s);

	len = 0;
	buf = NULL;
	if (cdata->template != NULL) {
		ptr = cdata->template;
		while (*ptr != '\0') {
			switch (ch = *ptr++) {
			case '%':
				if (*ptr != '%')
					break;
				ptr++;

				buf = xrealloc(buf, 1, len + slen + 1);
				memcpy(buf + len, s, slen);
				len += slen;
				break;
			default:
				buf = xrealloc(buf, 1, len + 2);
				buf[len++] = ch;
				break;
			}
		}
		xfree(cdata->template);

		buf[len] = '\0';
		s = buf;
	}
	xfree(cdata);

	if (cmd_string_parse(s, &cmdlist, &cause) != 0) {
		if (cause == NULL)
			return (0);
		*cause = toupper((u_char) *cause);
		status_message_set(c, cause);
		xfree(cause);
		cmdlist = NULL;
	}
	if (buf != NULL)
		xfree(buf);
	if (cmdlist == NULL)
		return (0);

	ctx.msgdata = NULL;
	ctx.cursession = c->session;
	ctx.curclient = c;

	ctx.error = key_bindings_error;
	ctx.print = key_bindings_print;
	ctx.info = key_bindings_info;

	ctx.cmdclient = NULL;

	cmd_list_exec(cmdlist, &ctx);
	cmd_list_free(cmdlist);

	if (c->prompt_callback != (void *) &cmd_command_prompt_callback)
		return (1);
	return (0);
}
