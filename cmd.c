/* $Id: cmd.c,v 1.29 2007-11-16 21:31:03 nicm Exp $ */

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
	&cmd_attach_session_entry,
	&cmd_bind_key_entry,
	&cmd_detach_client_entry,
	&cmd_has_session_entry,
	&cmd_kill_session_entry,
	&cmd_kill_window_entry,
	&cmd_last_window_entry,
	&cmd_link_window_entry,
	&cmd_list_clients_entry,
	&cmd_list_keys_entry,
	&cmd_list_sessions_entry,
	&cmd_list_windows_entry,
	&cmd_new_session_entry,
	&cmd_new_window_entry,
	&cmd_next_window_entry,
	&cmd_previous_window_entry,
	&cmd_refresh_client_entry,
	&cmd_rename_session_entry,
	&cmd_rename_window_entry,
	&cmd_select_window_entry,
	&cmd_send_prefix_entry,
	&cmd_set_option_entry,	
	&cmd_swap_window_entry,
	&cmd_switch_client_entry,
	&cmd_unbind_key_entry,
	&cmd_unlink_window_entry,
	NULL
};

struct cmd *
cmd_parse(int argc, char **argv, char **cause)
{
	const struct cmd_entry **entryp, *entry;
	struct cmd	        *cmd;
	char			 s[BUFSIZ];
	int			 opt;

	*cause = NULL;
	if (argc == 0)
		return (NULL);

	entry = NULL;
	for (entryp = cmd_table; *entryp != NULL; entryp++) {
		if ((*entryp)->alias != NULL && 
		    strcmp((*entryp)->alias, argv[0]) == 0) {
			entry = *entryp;
			break;
		}

		if (strncmp((*entryp)->name, argv[0], strlen(argv[0])) != 0)
			continue;
		if (entry != NULL)
			goto ambiguous;
		entry = *entryp;
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

ambiguous:
	*s = '\0';
	for (entryp = cmd_table; *entryp != NULL; entryp++) {
		if (strncmp((*entryp)->name, argv[0], strlen(argv[0])) != 0)
			continue;
		if (strlcat(s, (*entryp)->name, sizeof s) >= sizeof s)
			break;
		if (strlcat(s, ", ", sizeof s) >= sizeof s)
			break;
	}
	s[strlen(s) - 2] = '\0';
	xasprintf(cause, "ambiguous command: %s, could be: %s", argv[0], s);
	return (NULL);

usage:
	usage(cause, "%s %s", entry->name, entry->usage);
	return (NULL);
}

void
cmd_exec(struct cmd *cmd, struct cmd_ctx *ctx)
{
	cmd->entry->exec(cmd->data, ctx);
}

void
cmd_send(struct cmd *cmd, struct buffer *b)
{
	const struct cmd_entry **entryp;
	u_int			 n;

	n = 0;
	for (entryp = cmd_table; *entryp != NULL; entryp++) {
		if (*entryp == cmd->entry)
			break;
		n++;
	}
	if (*entryp == NULL)
		fatalx("command not found");

	buffer_write(b, &n, sizeof n);

	if (cmd->entry->send != NULL)
		cmd->entry->send(cmd->data, b);
}

struct cmd *
cmd_recv(struct buffer *b)
{
	const struct cmd_entry **entryp;
	struct cmd   	        *cmd;
	u_int			 m, n;

	buffer_read(b, &m, sizeof m);
	
	n = 0;
	for (entryp = cmd_table; *entryp != NULL; entryp++) {
		if (n == m)
			break;
		n++;
	}
	if (*entryp == NULL)
		fatalx("command not found");

	cmd = xmalloc(sizeof *cmd);
	cmd->entry = *entryp;

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
