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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

RB_GENERATE(key_bindings, key_binding, entry, key_bindings_cmp);

struct key_bindings	key_bindings;
struct key_bindings	dead_key_bindings;

int
key_bindings_cmp(struct key_binding *bd1, struct key_binding *bd2)
{
	int	key1, key2;

	key1 = bd1->key & ~KEYC_PREFIX;
	key2 = bd2->key & ~KEYC_PREFIX;
	if (key1 != key2)
		return (key1 - key2);

	if (bd1->key & KEYC_PREFIX && !(bd2->key & KEYC_PREFIX))
		return (-1);
	if (bd2->key & KEYC_PREFIX && !(bd1->key & KEYC_PREFIX))
		return (1);
	return (0);
}

struct key_binding *
key_bindings_lookup(int key)
{
	struct key_binding	bd;

	bd.key = key;
	return (RB_FIND(key_bindings, &key_bindings, &bd));
}

void
key_bindings_add(int key, int can_repeat, struct cmd_list *cmdlist)
{
	struct key_binding	*bd;

	key_bindings_remove(key);

	bd = xmalloc(sizeof *bd);
	bd->key = key;
	RB_INSERT(key_bindings, &key_bindings, bd);

	bd->can_repeat = can_repeat;
	bd->cmdlist = cmdlist;
}

void
key_bindings_remove(int key)
{
	struct key_binding	*bd;

	if ((bd = key_bindings_lookup(key)) == NULL)
		return;
	RB_REMOVE(key_bindings, &key_bindings, bd);
	RB_INSERT(key_bindings, &dead_key_bindings, bd);
}

void
key_bindings_clean(void)
{
	struct key_binding	*bd;

	while (!RB_EMPTY(&dead_key_bindings)) {
		bd = RB_ROOT(&dead_key_bindings);
		RB_REMOVE(key_bindings, &dead_key_bindings, bd);
		cmd_list_free(bd->cmdlist);
		xfree(bd);
	}
}

void
key_bindings_init(void)
{
	static const struct {
		int			 key;
		int			 can_repeat;
		const struct cmd_entry	*entry;
	} table[] = {
		{ ' ',			  0, &cmd_next_layout_entry },
		{ '!', 			  0, &cmd_break_pane_entry },
		{ '"', 			  0, &cmd_split_window_entry },
		{ '#', 			  0, &cmd_list_buffers_entry },
		{ '$',			  0, &cmd_command_prompt_entry },
		{ '%', 			  0, &cmd_split_window_entry },
		{ '&', 			  0, &cmd_confirm_before_entry },
		{ '(',                    0, &cmd_switch_client_entry },
		{ ')',                    0, &cmd_switch_client_entry },
		{ ',', 			  0, &cmd_command_prompt_entry },
		{ '-', 			  0, &cmd_delete_buffer_entry },
		{ '.', 			  0, &cmd_command_prompt_entry },
		{ '0', 			  0, &cmd_select_window_entry },
		{ '1', 			  0, &cmd_select_window_entry },
		{ '2', 			  0, &cmd_select_window_entry },
		{ '3', 			  0, &cmd_select_window_entry },
		{ '4', 			  0, &cmd_select_window_entry },
		{ '5', 			  0, &cmd_select_window_entry },
		{ '6', 			  0, &cmd_select_window_entry },
		{ '7', 			  0, &cmd_select_window_entry },
		{ '8', 			  0, &cmd_select_window_entry },
		{ '9', 			  0, &cmd_select_window_entry },
		{ ':', 			  0, &cmd_command_prompt_entry },
		{ ';', 			  0, &cmd_last_pane_entry },
		{ '=', 			  0, &cmd_choose_buffer_entry },
		{ '?', 			  0, &cmd_list_keys_entry },
		{ 'D',			  0, &cmd_choose_client_entry },
		{ 'L',			  0, &cmd_switch_client_entry },
		{ '[', 			  0, &cmd_copy_mode_entry },
		{ '\'',			  0, &cmd_command_prompt_entry },
		{ '\002', /* C-b */	  0, &cmd_send_prefix_entry },
		{ '\017', /* C-o */	  0, &cmd_rotate_window_entry },
		{ '\032', /* C-z */	  0, &cmd_suspend_client_entry },
		{ ']', 			  0, &cmd_paste_buffer_entry },
		{ 'c', 			  0, &cmd_new_window_entry },
		{ 'd', 			  0, &cmd_detach_client_entry },
		{ 'f', 			  0, &cmd_command_prompt_entry },
		{ 'i',			  0, &cmd_display_message_entry },
		{ 'l', 			  0, &cmd_last_window_entry },
		{ 'n', 			  0, &cmd_next_window_entry },
		{ 'o', 			  0, &cmd_select_pane_entry },
		{ 'p', 			  0, &cmd_previous_window_entry },
		{ 'q',			  0, &cmd_display_panes_entry },
		{ 'r', 			  0, &cmd_refresh_client_entry },
		{ 's', 			  0, &cmd_choose_session_entry },
		{ 't', 			  0, &cmd_clock_mode_entry },
		{ 'w', 			  0, &cmd_choose_window_entry },
		{ 'x', 			  0, &cmd_confirm_before_entry },
		{ '{',			  0, &cmd_swap_pane_entry },
		{ '}',			  0, &cmd_swap_pane_entry },
		{ '~',			  0, &cmd_show_messages_entry },
		{ '1' | KEYC_ESCAPE,	  0, &cmd_select_layout_entry },
		{ '2' | KEYC_ESCAPE,	  0, &cmd_select_layout_entry },
		{ '3' | KEYC_ESCAPE,	  0, &cmd_select_layout_entry },
		{ '4' | KEYC_ESCAPE,	  0, &cmd_select_layout_entry },
		{ '5' | KEYC_ESCAPE,	  0, &cmd_select_layout_entry },
		{ KEYC_PPAGE, 		  0, &cmd_copy_mode_entry },
		{ 'n' | KEYC_ESCAPE, 	  0, &cmd_next_window_entry },
		{ 'o' | KEYC_ESCAPE,	  0, &cmd_rotate_window_entry },
		{ 'p' | KEYC_ESCAPE, 	  0, &cmd_previous_window_entry },
		{ KEYC_UP, 		  1, &cmd_select_pane_entry },
		{ KEYC_DOWN, 		  1, &cmd_select_pane_entry },
		{ KEYC_LEFT, 		  1, &cmd_select_pane_entry },
		{ KEYC_RIGHT, 		  1, &cmd_select_pane_entry },
		{ KEYC_UP | KEYC_ESCAPE,  1, &cmd_resize_pane_entry },
		{ KEYC_DOWN | KEYC_ESCAPE,  1, &cmd_resize_pane_entry },
		{ KEYC_LEFT | KEYC_ESCAPE,  1, &cmd_resize_pane_entry },
		{ KEYC_RIGHT | KEYC_ESCAPE, 1, &cmd_resize_pane_entry },
		{ KEYC_UP | KEYC_CTRL,    1, &cmd_resize_pane_entry },
		{ KEYC_DOWN | KEYC_CTRL,  1, &cmd_resize_pane_entry },
		{ KEYC_LEFT | KEYC_CTRL,  1, &cmd_resize_pane_entry },
		{ KEYC_RIGHT | KEYC_CTRL, 1, &cmd_resize_pane_entry },
	};
	u_int		 i;
	struct cmd	*cmd;
	struct cmd_list	*cmdlist;

	RB_INIT(&key_bindings);

	for (i = 0; i < nitems(table); i++) {
		cmdlist = xmalloc(sizeof *cmdlist);
		TAILQ_INIT(&cmdlist->list);
		cmdlist->references = 1;

		cmd = xmalloc(sizeof *cmd);
		cmd->entry = table[i].entry;
		if (cmd->entry->key_binding != NULL)
			cmd->entry->key_binding(cmd, table[i].key);
		else
			cmd->args = args_create(0);
		TAILQ_INSERT_HEAD(&cmdlist->list, cmd, qentry);

		key_bindings_add(
		    table[i].key | KEYC_PREFIX, table[i].can_repeat, cmdlist);
	}
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
	status_message_set(ctx->curclient, "%s", msg);
	xfree(msg);
}

void printflike2
key_bindings_print(struct cmd_ctx *ctx, const char *fmt, ...)
{
	struct winlink	*wl = ctx->curclient->session->curw;
	va_list		 ap;

	if (wl->window->active->mode != &window_copy_mode) {
		window_pane_reset_mode(wl->window->active);
		window_pane_set_mode(wl->window->active, &window_copy_mode);
		window_copy_init_for_output(wl->window->active);
	}

	va_start(ap, fmt);
	window_copy_vadd(wl->window->active, fmt, ap);
	va_end(ap);
}

void printflike2
key_bindings_info(struct cmd_ctx *ctx, const char *fmt, ...)
{
	va_list	ap;
	char   *msg;

	if (options_get_number(&global_options, "quiet"))
		return;

	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	va_end(ap);

	*msg = toupper((u_char) *msg);
	status_message_set(ctx->curclient, "%s", msg);
	xfree(msg);
}

void
key_bindings_dispatch(struct key_binding *bd, struct client *c)
{
	struct cmd_ctx	 ctx;
	struct cmd	*cmd;
	int		 readonly;

	ctx.msgdata = NULL;
	ctx.curclient = c;

	ctx.error = key_bindings_error;
	ctx.print = key_bindings_print;
	ctx.info = key_bindings_info;

	ctx.cmdclient = NULL;

	readonly = 1;
	TAILQ_FOREACH(cmd, &bd->cmdlist->list, qentry) {
		if (!(cmd->entry->flags & CMD_READONLY))
			readonly = 0;
	}
	if (!readonly && c->flags & CLIENT_READONLY) {
		key_bindings_info(&ctx, "Client is read-only");
		return;
	}

	cmd_list_exec(bd->cmdlist, &ctx);
}
