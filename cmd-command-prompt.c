/* $Id: cmd-command-prompt.c,v 1.23 2009-08-20 11:51:20 tcunha Exp $ */

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

void	 cmd_command_prompt_init(struct cmd *, int);
int	 cmd_command_prompt_parse(struct cmd *, int, char **, char **);
int	 cmd_command_prompt_exec(struct cmd *, struct cmd_ctx *);
void	 cmd_command_prompt_free(struct cmd *);
size_t	 cmd_command_prompt_print(struct cmd *, char *, size_t);

int	 cmd_command_prompt_callback(void *, const char *);
void	 cmd_command_prompt_cfree(void *);
char	*cmd_command_prompt_replace(char *, const char *, int);

const struct cmd_entry cmd_command_prompt_entry = {
	"command-prompt", NULL,
	CMD_TARGET_CLIENT_USAGE " [-p prompts] [template]",
	0, 0,
	cmd_command_prompt_init,
	cmd_command_prompt_parse,
	cmd_command_prompt_exec,
	cmd_command_prompt_free,
	cmd_command_prompt_print
};

struct cmd_command_prompt_data {
	char	*prompts;
	char	*target;
	char	*template;
};

struct cmd_command_prompt_cdata {
	struct client	*c;
	char		*next_prompt;
	char		*prompts;
	char		*template;
	int		 idx;
};

void
cmd_command_prompt_init(struct cmd *self, int key)
{
	struct cmd_command_prompt_data	*data;

	self->data = data = xmalloc(sizeof *data);
	data->prompts = NULL;
	data->target = NULL;
	data->template = NULL;

	switch (key) {
	case ',':
		data->template = xstrdup("rename-window '%%'");
		break;
	case '.':
		data->template = xstrdup("move-window -t '%%'");
		break;
	case 'f':
		data->template = xstrdup("find-window '%%'");
		break;
	}
}

int
cmd_command_prompt_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_command_prompt_data	*data;
	int				 opt;

	self->entry->init(self, 0);
	data = self->data;

	while ((opt = getopt(argc, argv, "p:t:")) != -1) {
		switch (opt) {
		case 'p':
			if (data->prompts == NULL)
				data->prompts = xstrdup(optarg);
			break;
		case 't':
			if (data->target == NULL)
				data->target = xstrdup(optarg);
			break;
		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 0 && argc != 1)
		goto usage;

	if (argc == 1)
		data->template = xstrdup(argv[0]);

	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

	self->entry->free(self);
	return (-1);
}

int
cmd_command_prompt_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_command_prompt_data	*data = self->data;
	struct cmd_command_prompt_cdata	*cdata;
	struct client			*c;
	char				*prompt, *ptr;
	size_t				 n;

	if ((c = cmd_find_client(ctx, data->target)) == NULL)
		return (-1);

	if (c->prompt_string != NULL)
		return (0);

	cdata = xmalloc(sizeof *cdata);
	cdata->c = c;
	cdata->idx = 1;
	cdata->next_prompt = NULL;
	cdata->prompts = NULL;
	cdata->template = NULL;

	if (data->template != NULL)
		cdata->template = xstrdup(data->template);
	else
		cdata->template = xstrdup("%1");
	if (data->prompts != NULL)
		cdata->prompts = xstrdup(data->prompts);
	else if (data->template != NULL) {
		n = strcspn(data->template, " ,");
		xasprintf(&cdata->prompts, "(%.*s) ", (int) n, data->template);
	} else
		cdata->prompts = xstrdup(":");

	cdata->next_prompt = cdata->prompts;
	ptr = strsep(&cdata->next_prompt, ",");
	if (data->prompts == NULL)
		prompt = xstrdup(ptr);
	else
		xasprintf(&prompt, "%s ", ptr);
	status_prompt_set(c, prompt, cmd_command_prompt_callback,
	    cmd_command_prompt_cfree, cdata, 0);
	xfree(prompt);

	return (0);
}

void
cmd_command_prompt_free(struct cmd *self)
{
	struct cmd_command_prompt_data	*data = self->data;

	if (data->prompts != NULL)
		xfree(data->prompts);
	if (data->target != NULL)
		xfree(data->target);
	if (data->template != NULL)
		xfree(data->template);
	xfree(data);
}

size_t
cmd_command_prompt_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_command_prompt_data	*data = self->data;
	size_t				 off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return (off);
	if (off < len && data->prompts != NULL)
		off += cmd_prarg(buf + off, len - off, " -p ", data->prompts);
	if (off < len && data->target != NULL)
		off += cmd_prarg(buf + off, len - off, " -t ", data->target);
	if (off < len && data->template != NULL)
		off += cmd_prarg(buf + off, len - off, " ", data->template);
	return (off);
}

int
cmd_command_prompt_callback(void *data, const char *s)
{
	struct cmd_command_prompt_cdata	*cdata = data;
	struct client			*c = cdata->c;
	struct cmd_list			*cmdlist;
	struct cmd_ctx			 ctx;
	char				*cause, *newtempl, *prompt, *ptr;

	if (s == NULL)
		return (0);

	newtempl = cmd_command_prompt_replace(cdata->template, s, cdata->idx);
	xfree(cdata->template);
	cdata->template = newtempl;

	if ((ptr = strsep(&cdata->next_prompt, ",")) != NULL) {
		xasprintf(&prompt, "%s ", ptr);
		status_prompt_update(c, prompt);
		xfree(prompt);
		cdata->idx++;
		return (1);
	}

	if (cmd_string_parse(newtempl, &cmdlist, &cause) != 0) {
		if (cause != NULL) {
			*cause = toupper((u_char) *cause);
			status_message_set(c, "%s", cause);
			xfree(cause);
		}
		return (0);
	}

	ctx.msgdata = NULL;
	ctx.cursession = c->session;
	ctx.curclient = c;

	ctx.error = key_bindings_error;
	ctx.print = key_bindings_print;
	ctx.info = key_bindings_info;

	ctx.cmdclient = NULL;

	cmd_list_exec(cmdlist, &ctx);
	cmd_list_free(cmdlist);

	if (c->prompt_callbackfn != (void *) &cmd_command_prompt_callback)
		return (1);
	return (0);
}

void
cmd_command_prompt_cfree(void *data)
{
	struct cmd_command_prompt_cdata	*cdata = data;

	if (cdata->prompts != NULL)
		xfree(cdata->prompts);
	if (cdata->template != NULL)
		xfree(cdata->template);
	xfree(cdata);
}

char *
cmd_command_prompt_replace(char *template, const char *s, int idx)
{
	char	 ch;
	char	*buf, *ptr;
	int	 replaced;
	size_t	 len;

	if (strstr(template, "%") == NULL)
		return (xstrdup(template));

	buf = xmalloc(1);
	*buf = '\0';
	len = 0;
	replaced = 0;

	ptr = template;
	while (*ptr != '\0') {
		switch (ch = *ptr++) {
		case '%':
			if (*ptr < '1' || *ptr > '9' || *ptr - '0' != idx) {
				if (*ptr != '%' || replaced)
					break;
				replaced = 1;
			}
			ptr++;

			len += strlen(s);
			buf = xrealloc(buf, 1, len + 1);
			strlcat(buf, s, len + 1);
			continue;
		}
		buf = xrealloc(buf, 1, len + 2);
		buf[len++] = ch;
		buf[len] = '\0';
	}

	return (buf);
}
