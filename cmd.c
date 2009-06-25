/* $OpenBSD: cmd.c,v 1.2 2009/06/04 23:34:32 nicm Exp $ */

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
#include <sys/time.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

const struct cmd_entry *cmd_table[] = {
	&cmd_attach_session_entry,
	&cmd_bind_key_entry,
	&cmd_break_pane_entry,
	&cmd_choose_session_entry,
	&cmd_choose_window_entry,
	&cmd_clear_history_entry,
	&cmd_clock_mode_entry,
	&cmd_command_prompt_entry,
	&cmd_confirm_before_entry,
	&cmd_copy_buffer_entry,
	&cmd_copy_mode_entry,
	&cmd_delete_buffer_entry,
	&cmd_detach_client_entry,
	&cmd_down_pane_entry,
	&cmd_find_window_entry,
	&cmd_has_session_entry,
	&cmd_kill_pane_entry,
	&cmd_kill_server_entry,
	&cmd_kill_session_entry,
	&cmd_kill_window_entry,
	&cmd_last_window_entry,
	&cmd_link_window_entry,
	&cmd_list_buffers_entry,
	&cmd_list_clients_entry,
	&cmd_list_commands_entry,
	&cmd_list_keys_entry,
	&cmd_list_sessions_entry,
	&cmd_list_windows_entry,
	&cmd_load_buffer_entry,
	&cmd_lock_server_entry,
	&cmd_move_window_entry,
	&cmd_new_session_entry,
	&cmd_new_window_entry,
	&cmd_next_layout_entry,
	&cmd_next_window_entry,
	&cmd_paste_buffer_entry,
	&cmd_previous_layout_entry,
	&cmd_previous_window_entry,
	&cmd_refresh_client_entry,
	&cmd_rename_session_entry,
	&cmd_rename_window_entry,
	&cmd_resize_pane_entry,
	&cmd_respawn_window_entry,
	&cmd_rotate_window_entry,
	&cmd_save_buffer_entry,
	&cmd_scroll_mode_entry,
	&cmd_select_layout_entry,
	&cmd_select_pane_entry,
	&cmd_select_prompt_entry,
	&cmd_select_window_entry,
	&cmd_send_keys_entry,
	&cmd_send_prefix_entry,
	&cmd_server_info_entry,
	&cmd_set_buffer_entry,
	&cmd_set_option_entry,
	&cmd_set_password_entry,
	&cmd_set_window_option_entry,
	&cmd_show_buffer_entry,
	&cmd_show_options_entry,
	&cmd_show_window_options_entry,
	&cmd_source_file_entry,
	&cmd_split_window_entry,
	&cmd_start_server_entry,
	&cmd_suspend_client_entry,
	&cmd_swap_pane_entry,
	&cmd_swap_window_entry,
	&cmd_switch_client_entry,
	&cmd_unbind_key_entry,
	&cmd_unlink_window_entry,
	&cmd_up_pane_entry,
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
	if (argc == 0) {
		xasprintf(cause, "no command");
		return (NULL);
	}

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

		/* Bail now if an exact match. */
		if (strcmp(entry->name, argv[0]) == 0)
			break;
	}
	if (entry == NULL) {
		xasprintf(cause, "unknown command: %s", argv[0]);
		return (NULL);
	}

	optreset = 1;
	optind = 1;
	if (entry->parse == NULL) {
		while ((opt = getopt(argc, argv, "")) != -1) {
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
	cmd->data = NULL;
	if (entry->parse != NULL) {
		if (entry->parse(cmd, argc, argv, cause) != 0) {
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
	xasprintf(cause, "usage: %s %s", entry->name, entry->usage);
	return (NULL);
}

int
cmd_exec(struct cmd *cmd, struct cmd_ctx *ctx)
{
	if (server_locked) {
		ctx->error(ctx, "server is locked");
		return (-1);
	}
	return (cmd->entry->exec(cmd, ctx));
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
		cmd->entry->send(cmd, b);
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
		cmd->entry->recv(cmd, b);
	return (cmd);
}

void
cmd_free(struct cmd *cmd)
{
	if (cmd->data != NULL && cmd->entry->free != NULL)
		cmd->entry->free(cmd);
	xfree(cmd);
}

size_t
cmd_print(struct cmd *cmd, char *buf, size_t len)
{
	if (cmd->entry->print == NULL) {
		return (xsnprintf(buf, len, "%s", cmd->entry->name));
	}
	return (cmd->entry->print(cmd, buf, len));
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

struct session *
cmd_current_session(struct cmd_ctx *ctx)
{
	struct msg_command_data	*data = ctx->msgdata;
	struct timeval		*tv;
	struct session		*s, *newest = NULL;
	u_int			 i;

	if (ctx->cursession != NULL)
		return (ctx->cursession);

	if (data != NULL && data->pid != -1) {
		if (data->pid != getpid()) {
			ctx->error(ctx, "wrong server: %ld", (long) data->pid);
			return (NULL);
		}
		if (data->idx > ARRAY_LENGTH(&sessions)) {
			ctx->error(ctx, "index out of range: %d", data->idx);
			return (NULL);
		}
		if ((s = ARRAY_ITEM(&sessions, data->idx)) == NULL) {
			ctx->error(ctx, "session doesn't exist: %u", data->idx);
			return (NULL);
		}
		return (s);
	}

	tv = NULL;
	for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		s = ARRAY_ITEM(&sessions, i);
		if (s != NULL && (tv == NULL || timercmp(&s->tv, tv, >))) {
			newest = ARRAY_ITEM(&sessions, i);
			tv = &s->tv;
		}
	}
	return (newest);
}

struct client *
cmd_find_client(struct cmd_ctx *ctx, const char *arg)
{
	struct client	*c;

	if (arg == NULL)
		c = ctx->curclient;
	else {
		if ((c = arg_parse_client(arg)) == NULL) {
			if (arg != NULL)
				ctx->error(ctx, "client not found: %s", arg);
			else
				ctx->error(ctx, "no client found");
		}
	}
	return (c);
}

struct session *
cmd_find_session(struct cmd_ctx *ctx, const char *arg)
{
	struct session	*s;

	if (arg == NULL)
		s = cmd_current_session(ctx);
	else {
		if ((s = arg_parse_session(arg)) == NULL) {
			if (arg != NULL)
				ctx->error(ctx, "session not found: %s", arg);
			else
				ctx->error(ctx, "no session found");
		}
	}
	return (s);
}

struct winlink *
cmd_find_window(struct cmd_ctx *ctx, const char *arg, struct session **sp)
{
	struct session	*s;
	struct winlink	*wl;
	int		 idx;

	wl = NULL;
	if (arg_parse_window(arg, &s, &idx) != 0) {
		ctx->error(ctx, "bad window: %s", arg);
		return (NULL);
	}
	if (s == NULL)
		s = ctx->cursession;
	if (s == NULL)
		s = cmd_current_session(ctx);
	if (s == NULL)
		return (NULL);
	if (sp != NULL)
		*sp = s;

	if (idx == -1)
		wl = s->curw;
	else
		wl = winlink_find_by_index(&s->windows, idx);
	if (wl == NULL)
		ctx->error(ctx, "window not found: %s:%d", s->name, idx);
	return (wl);
}
