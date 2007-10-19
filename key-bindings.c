/* $Id: key-bindings.c,v 1.9 2007-10-19 09:21:26 nicm Exp $ */

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

void	key_bindings_error(struct cmd_ctx *, const char *, ...);
void	key_bindings_print(struct cmd_ctx *, const char *, ...);

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
	}

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

	cmd_free(bd->cmd);
	xfree(bd);
}

void
key_bindings_init(void)
{
	struct {
		int			 key;
		const struct cmd_entry	*entry;
		void 			 (*fn)(void **, int);
	} table[] = {
		{ 'D', &cmd_detach_session_entry, NULL },
		{ 'd', &cmd_detach_session_entry, NULL },
		{ 'S', &cmd_list_sessions_entry, NULL },
		{ 's', &cmd_list_sessions_entry, NULL },
		{ 'W', &cmd_list_windows_entry, NULL },
		{ 'w', &cmd_list_windows_entry, NULL },	
		{ '?', &cmd_list_keys_entry, NULL },
		{ '/', &cmd_list_keys_entry, NULL },
		{ 'C', &cmd_new_window_entry, NULL },
		{ 'c', &cmd_new_window_entry, NULL },
		{ 'N', &cmd_next_window_entry, NULL },
		{ 'n', &cmd_next_window_entry, NULL },
		{ 'P', &cmd_previous_window_entry, NULL },
		{ 'p', &cmd_previous_window_entry, NULL },
		{ 'L', &cmd_last_window_entry, NULL },
		{ 'l', &cmd_last_window_entry, NULL },
		{ '0', &cmd_select_window_entry, cmd_select_window_default },
		{ '1', &cmd_select_window_entry, cmd_select_window_default },
		{ '2', &cmd_select_window_entry, cmd_select_window_default },
		{ '3', &cmd_select_window_entry, cmd_select_window_default },
		{ '4', &cmd_select_window_entry, cmd_select_window_default },
		{ '5', &cmd_select_window_entry, cmd_select_window_default },
		{ '6', &cmd_select_window_entry, cmd_select_window_default },
		{ '7', &cmd_select_window_entry, cmd_select_window_default },
		{ '8', &cmd_select_window_entry, cmd_select_window_default },
		{ '9', &cmd_select_window_entry, cmd_select_window_default },
		{ 'R', &cmd_refresh_session_entry, NULL },
		{ 'r', &cmd_refresh_session_entry, NULL },
		{ META, &cmd_send_prefix_entry, NULL },
/*
		{ 'I', &cmd_windo_info_entry },
		{ 'i', &cmd_window_info_entry },
		{ META, &cmd_meta_entry_entry },
*/
	};
	u_int		 i;
	struct cmd	*cmd;

	ARRAY_INIT(&key_bindings);

	for (i = 0; i < (sizeof table / sizeof table[0]); i++) {
		cmd = xmalloc(sizeof *cmd);
		cmd->entry = table[i].entry;
		cmd->data = NULL;
		if (table[i].fn != NULL)
			table[i].fn(&cmd->data, table[i].key);
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

void
key_bindings_error(struct cmd_ctx *ctx, const char *fmt, ...)
{
	va_list	ap;
	char   *msg;

	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	va_end(ap);

	*msg = toupper((u_char) *msg);
	server_write_message(ctx->client, msg);
	xfree(msg);
}

void
key_bindings_print(struct cmd_ctx *ctx, const char *fmt, ...)
{
	struct client	*c = ctx->client;
	struct hdr	 hdr;
	va_list		 ap;
	char		*msg;
	size_t		 size;
	u_int		 i;

	buffer_ensure(c->out, sizeof hdr);
	buffer_add(c->out, sizeof hdr);
	size = BUFFER_USED(c->out);

	if (!(c->flags & CLIENT_HOLD)) {
		input_store_zero(c->out, CODE_CURSOROFF);
		for (i = 0; i < c->session->window->screen.sy; i++) {
			input_store_two(c->out, CODE_CURSORMOVE, i + 1, 1);
			input_store_zero(c->out, CODE_CLEARLINE);
		}			
		input_store_two(c->out, CODE_CURSORMOVE, 1, 1);
		input_store_two(c->out, CODE_ATTRIBUTES, 0, 0x88);
		
		c->flags |= CLIENT_HOLD;
	}

	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	va_end(ap);

	buffer_write(c->out, msg, strlen(msg));
	input_store8(c->out, '\r');
	input_store8(c->out, '\n');
	xfree(msg);

	size = BUFFER_USED(c->out) - size;
	hdr.type = MSG_DATA;
	hdr.size = size;
	memcpy(BUFFER_IN(c->out) - size - sizeof hdr, &hdr, sizeof hdr);
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

	ctx.session = c->session;

	ctx.error = key_bindings_error;
	ctx.print = key_bindings_print;

	ctx.client = c;
	ctx.flags = CMD_KEY;

	cmd_exec(bd->cmd, &ctx);
}
