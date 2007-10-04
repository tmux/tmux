/* $Id: cmd.c,v 1.10 2007-10-04 10:54:21 nicm Exp $ */

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

#include <getopt.h>
#include <string.h>

#include "tmux.h"

const struct cmd_entry *cmd_table[] = {
	&cmd_bind_key_entry,
	&cmd_detach_session_entry,
	&cmd_last_window_entry,
	&cmd_list_keys_entry,
	&cmd_list_sessions_entry,
	&cmd_new_session_entry,
	&cmd_new_window_entry,
	&cmd_next_window_entry,
	&cmd_previous_window_entry,
	&cmd_rename_window_entry,
	&cmd_select_window_entry,
	&cmd_set_option_entry,
	&cmd_unbind_key_entry,
	NULL
};

struct cmd *
cmd_parse(int argc, char **argv, char **cause)
{
	const struct cmd_entry **this, *entry;
	struct cmd	        *cmd;
	int			 opt;

	*cause = NULL;
	if (argc == 0)
		return (NULL);

	entry = NULL;
	for (this = cmd_table; *this != NULL; this++) {
		if (strcmp((*this)->alias, argv[0]) == 0) {
			entry = *this;
			break;
		}

		if (strncmp((*this)->name, argv[0], strlen(argv[0])) != 0)
			continue;
		if (entry != NULL) {
			xasprintf(cause, "ambiguous command: %s", argv[0]);
			return (NULL);
		}
		entry = *this;
	}
	if (entry == NULL) {
		xasprintf(cause, "unknown command: %s", argv[0]);
		return (NULL);
	}

	optind = 1;
	if (entry->parse == NULL) {
		while ((opt = getopt(argc, argv, "")) != EOF) {
			switch (opt) {
			default:
				goto usage;
			}
		}
		argc -= optind;
		argv += optind;
		if (argc != 0)
			goto usage;
	}

	cmd = xmalloc(sizeof *cmd);
	cmd->entry = entry;
	if (entry->parse != NULL) {
		if (entry->parse(&cmd->data, argc, argv, cause) != 0) {
			xfree(cmd);
			return (NULL);
		}
	}
	return (cmd);

usage:
	if (entry->usage == NULL)
		usage(cause, "%s", entry->name);
	else
		usage(cause, "%s", entry->usage());
	return (NULL);
}

void
cmd_exec(struct cmd *cmd, struct cmd_ctx *ctx)
{
	return (cmd->entry->exec(cmd->data, ctx));
}

void
cmd_send(struct cmd *cmd, struct buffer *b)
{
	buffer_write(b, &cmd->entry->type, sizeof cmd->entry->type);

	if (cmd->entry->send == NULL)
		return;
	return (cmd->entry->send(cmd->data, b));
}

struct cmd *
cmd_recv(struct buffer *b)
{
	const struct cmd_entry **this, *entry;
	struct cmd   	        *cmd;
	enum cmd_type		 type;

	buffer_read(b, &type, sizeof type);
	
	entry = NULL;
	for (this = cmd_table; *this != NULL; this++) {
		if ((*this)->type == type) {
			entry = *this;
			break;
		}
	}
	if (*this == NULL)
		return (NULL);

	cmd = xmalloc(sizeof *cmd);
	cmd->entry = entry;

	if (cmd->entry->recv != NULL)
		cmd->entry->recv(&cmd->data, b);
	return (cmd);
}

void
cmd_free(struct cmd *cmd)
{
	if (cmd->data != NULL && cmd->entry->free != NULL)
		cmd->entry->free(cmd->data);
	xfree(cmd);
}

void
cmd_send_string(struct buffer *b, const char *s)
{
	size_t	n;
	
	if (s == NULL) {
		n = 0;
		buffer_write(b, &n, sizeof n);
		return;
	}

	n = strlen(s) + 1;
	buffer_write(b, &n, sizeof n);

	buffer_write(b, s, n);
}

char *
cmd_recv_string(struct buffer *b)
{
	char   *s;
	size_t	n;

	buffer_read(b, &n, sizeof n);

	if (n == 0)
		return (NULL);
	
	s = xmalloc(n);
	buffer_read(b, s, n);
	s[n - 1] = '\0';

	return (s);
}
