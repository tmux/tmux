/* $Id: key-bindings.c,v 1.35 2008-06-21 10:19:36 nicm Exp $ */

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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

struct bindings	key_bindings;

void
key_bindings_add(int key, struct cmd *cmd)
{
	struct binding	*bd;
	u_int		 i;

	bd = NULL;
	for (i = 0; i < ARRAY_LENGTH(&key_bindings); i++) {
		bd = ARRAY_ITEM(&key_bindings, i);
		if (bd->key == key)
			break;
	}
	if (i == ARRAY_LENGTH(&key_bindings)) {
		bd = xmalloc(sizeof *bd);
		ARRAY_ADD(&key_bindings, bd);
	} else
		cmd_free(bd->cmd);

	bd->key = key;
	bd->cmd = cmd;
}

void
key_bindings_remove(int key)
{
	struct binding	*bd;
	u_int		 i;

	bd = NULL;
	for (i = 0; i < ARRAY_LENGTH(&key_bindings); i++) {
		bd = ARRAY_ITEM(&key_bindings, i);
		if (bd->key == key)
			break;
	}
	if (i == ARRAY_LENGTH(&key_bindings))
		return;

	ARRAY_REMOVE(&key_bindings, i);

	cmd_free(bd->cmd);
	xfree(bd);
}

void
key_bindings_init(void)
{
	struct {
		int			 key;
		const struct cmd_entry	*entry;
	} table[] = {
		{ 'D', &cmd_detach_client_entry },
		{ 'd', &cmd_detach_client_entry },
		{ 'S', &cmd_list_sessions_entry },
		{ 's', &cmd_list_sessions_entry },
		{ 'W', &cmd_list_windows_entry },
		{ 'w', &cmd_list_windows_entry },
		{ '?', &cmd_list_keys_entry },
		{ '/', &cmd_list_keys_entry },
		{ 'C', &cmd_new_window_entry },
		{ 'c', &cmd_new_window_entry },
		{ 'N', &cmd_next_window_entry },
		{ 'n', &cmd_next_window_entry },
		{ 'P', &cmd_previous_window_entry },
		{ 'p', &cmd_previous_window_entry },
		{ 'L', &cmd_last_window_entry },
		{ 'l', &cmd_last_window_entry },
		{ '0', &cmd_select_window_entry },
		{ '1', &cmd_select_window_entry },
		{ '2', &cmd_select_window_entry },
		{ '3', &cmd_select_window_entry },
		{ '4', &cmd_select_window_entry },
		{ '5', &cmd_select_window_entry },
		{ '6', &cmd_select_window_entry },
		{ '7', &cmd_select_window_entry },
		{ '8', &cmd_select_window_entry },
		{ '9', &cmd_select_window_entry },
		{ 'R', &cmd_refresh_client_entry },
		{ 'r', &cmd_refresh_client_entry },
		{ '&', &cmd_kill_window_entry },
		{ '=', &cmd_scroll_mode_entry },
		{ '[', &cmd_copy_mode_entry },
		{ ']', &cmd_paste_buffer_entry },
		{ '#', &cmd_list_buffers_entry },
		{ '-', &cmd_delete_buffer_entry },
		{ ':', &cmd_command_prompt_entry },
		{ META, &cmd_send_prefix_entry },
	};
	u_int		 i;
	struct cmd	*cmd;

	ARRAY_INIT(&key_bindings);

	for (i = 0; i < (sizeof table / sizeof table[0]); i++) {
		cmd = xmalloc(sizeof *cmd);
		cmd->entry = table[i].entry;
		cmd->data = NULL;
		if (cmd->entry->init != NULL)
			cmd->entry->init(cmd, table[i].key);
		key_bindings_add(table[i].key, cmd);
	}
}

void
key_bindings_free(void)
{
	struct binding	*bd;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&key_bindings); i++) {
		bd = ARRAY_ITEM(&key_bindings, i);

		cmd_free(bd->cmd);
		xfree(bd);
	}

	ARRAY_FREE(&key_bindings);
}

void printflike2
key_bindings_error(struct cmd_ctx *ctx, const char *fmt, ...)
{
	va_list	ap;
	char   *msg;

	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	va_end(ap);

	*msg = toupper((u_char) *msg);
 	server_set_client_message(ctx->curclient, msg);
	xfree(msg);
}

void printflike2
key_bindings_print(struct cmd_ctx *ctx, const char *fmt, ...)
{
	struct window	*w = ctx->cursession->curw->window;
	va_list		 ap;

	window_set_mode(w, &window_more_mode);

	va_start(ap, fmt);
	window_more_vadd(w, fmt, ap);
	va_end(ap);
}

void printflike2
key_bindings_info(struct cmd_ctx *ctx, const char *fmt, ...)
{
	va_list	ap;
	char   *msg;

	if (be_quiet)
		return;

	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	va_end(ap);

	*msg = toupper((u_char) *msg);
 	server_set_client_message(ctx->curclient, msg);
	xfree(msg);
}

void
key_bindings_dispatch(int key, struct client *c)
{
	struct cmd_ctx	 ctx;
	struct binding	*bd;
	u_int		 i;

	bd = NULL;
	for (i = 0; i < ARRAY_LENGTH(&key_bindings); i++) {
		bd = ARRAY_ITEM(&key_bindings, i);
		if (bd->key == key)
			break;
	}
	if (i == ARRAY_LENGTH(&key_bindings))
		return;

	ctx.msgdata = NULL;
	ctx.cursession = c->session;
	ctx.curclient = c;

	ctx.error = key_bindings_error;
	ctx.print = key_bindings_print;
	ctx.info = key_bindings_info;

	ctx.cmdclient = NULL;

	cmd_exec(bd->cmd, &ctx);
}
