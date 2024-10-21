/* $OpenBSD$ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicholas.marriott@gmail.com>
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
 * Send keys to client.
 */

static enum cmd_retval	cmd_send_keys_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_send_keys_entry = {
	.name = "send-keys",
	.alias = "send",

	.args = { "c:FHKlMN:Rt:X", 0, -1, NULL },
	.usage = "[-FHKlMRX] [-c target-client] [-N repeat-count] "
	         CMD_TARGET_PANE_USAGE " key ...",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK|CMD_CLIENT_CFLAG|CMD_CLIENT_CANFAIL,
	.exec = cmd_send_keys_exec
};

const struct cmd_entry cmd_send_prefix_entry = {
	.name = "send-prefix",
	.alias = NULL,

	.args = { "2t:", 0, 0, NULL },
	.usage = "[-2] " CMD_TARGET_PANE_USAGE,

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_send_keys_exec
};

static struct cmdq_item *
cmd_send_keys_inject_key(struct cmdq_item *item, struct cmdq_item *after,
    struct args *args, key_code key)
{
	struct cmd_find_state		*target = cmdq_get_target(item);
	struct client			*tc = cmdq_get_target_client(item);
	struct session			*s = target->s;
	struct winlink			*wl = target->wl;
	struct window_pane		*wp = target->wp;
	struct window_mode_entry	*wme;
	struct key_table		*table = NULL;
	struct key_binding		*bd;
	struct key_event		*event;

	if (args_has(args, 'K')) {
		if (tc == NULL)
			return (item);
		event = xcalloc(1, sizeof *event);
		event->key = key|KEYC_SENT;
		memset(&event->m, 0, sizeof event->m);
		if (server_client_handle_key(tc, event) == 0) {
			free(event->buf);
			free(event);
		}
		return (item);
	}

	wme = TAILQ_FIRST(&wp->modes);
	if (wme == NULL || wme->mode->key_table == NULL) {
		if (window_pane_key(wp, tc, s, wl, key, NULL) != 0)
			return (NULL);
		return (item);
	}
	table = key_bindings_get_table(wme->mode->key_table(wme), 1);

	bd = key_bindings_get(table, key & ~KEYC_MASK_FLAGS);
	if (bd != NULL) {
		table->references++;
		after = key_bindings_dispatch(bd, after, tc, NULL, target);
		key_bindings_unref_table(table);
	}
	return (after);
}

static struct cmdq_item *
cmd_send_keys_inject_string(struct cmdq_item *item, struct cmdq_item *after,
    struct args *args, int i)
{
	const char		*s = args_string(args, i);
	struct utf8_data	*ud, *loop;
	utf8_char		 uc;
	key_code		 key;
	char			*endptr;
	long			 n;
	int			 literal;

	if (args_has(args, 'H')) {
		n = strtol(s, &endptr, 16);
		if (*s =='\0' || n < 0 || n > 0xff || *endptr != '\0')
			return (item);
		return (cmd_send_keys_inject_key(item, after, args,
		    KEYC_LITERAL|n));
	}

	literal = args_has(args, 'l');
	if (!literal) {
		key = key_string_lookup_string(s);
		if (key != KEYC_NONE && key != KEYC_UNKNOWN) {
			after = cmd_send_keys_inject_key(item, after, args,
			    key);
			if (after != NULL)
				return (after);
		}
		literal = 1;
	}
	if (literal) {
		ud = utf8_fromcstr(s);
		for (loop = ud; loop->size != 0; loop++) {
			if (loop->size == 1 && loop->data[0] <= 0x7f)
				key = loop->data[0];
			else {
				if (utf8_from_data(loop, &uc) != UTF8_DONE)
					continue;
				key = uc;
			}
			after = cmd_send_keys_inject_key(item, after, args,
			    key);
		}
		free(ud);
	}
	return (after);
}

static enum cmd_retval
cmd_send_keys_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = cmd_get_args(self);
	struct cmd_find_state		*target = cmdq_get_target(item);
	struct client			*tc = cmdq_get_target_client(item);
	struct session			*s = target->s;
	struct winlink			*wl = target->wl;
	struct window_pane		*wp = target->wp;
	struct key_event		*event = cmdq_get_event(item);
	struct mouse_event		*m = &event->m;
	struct window_mode_entry	*wme = TAILQ_FIRST(&wp->modes);
	struct cmdq_item		*after = item;
	key_code			 key;
	u_int				 i, np = 1;
	u_int				 count = args_count(args);
	char				*cause = NULL;

	if (args_has(args, 'N')) {
		np = args_strtonum_and_expand(args, 'N', 1, UINT_MAX, item,
			 &cause);
		if (cause != NULL) {
			cmdq_error(item, "repeat count %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
		if (wme != NULL && (args_has(args, 'X') || count == 0)) {
			if (wme->mode->command == NULL) {
				cmdq_error(item, "not in a mode");
				return (CMD_RETURN_ERROR);
			}
			wme->prefix = np;
		}
	}

	if (args_has(args, 'X')) {
		if (wme == NULL || wme->mode->command == NULL) {
			cmdq_error(item, "not in a mode");
			return (CMD_RETURN_ERROR);
		}
		if (!m->valid)
			m = NULL;
		wme->mode->command(wme, tc, s, wl, args, m);
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'M')) {
		wp = cmd_mouse_pane(m, &s, NULL);
		if (wp == NULL) {
			cmdq_error(item, "no mouse target");
			return (CMD_RETURN_ERROR);
		}
		window_pane_key(wp, tc, s, wl, m->key, m);
		return (CMD_RETURN_NORMAL);
	}

	if (cmd_get_entry(self) == &cmd_send_prefix_entry) {
		if (args_has(args, '2'))
			key = options_get_number(s->options, "prefix2");
		else
			key = options_get_number(s->options, "prefix");
		cmd_send_keys_inject_key(item, item, args, key);
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'R')) {
		colour_palette_clear(&wp->palette);
		input_reset(wp->ictx, 1);
		wp->flags |= (PANE_STYLECHANGED|PANE_REDRAW);
	}

	if (count == 0) {
		if (args_has(args, 'N') || args_has(args, 'R'))
			return (CMD_RETURN_NORMAL);
		for (; np != 0; np--)
			cmd_send_keys_inject_key(item, NULL, args, event->key);
		return (CMD_RETURN_NORMAL);
	}

	for (; np != 0; np--) {
		for (i = 0; i < count; i++) {
			after = cmd_send_keys_inject_string(item, after, args,
			    i);
		}
	}

	return (CMD_RETURN_NORMAL);
}
