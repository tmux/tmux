/* $OpenBSD$ */

/*
 * Copyright (c) 2012 Nicholas Marriott <nicholas.marriott@gmail.com>
 * Copyright (c) 2012 George Nachman <tmux@georgester.com>
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

#include <event.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tmux.h"

/* Control offsets. */
struct control_offset {
	u_int				pane;

	struct window_pane_offset	offset;
	int				flags;
#define CONTROL_OFFSET_OFF 0x1

	RB_ENTRY(control_offset)		entry;
};
RB_HEAD(control_offsets, control_offset);

/* Compare client offsets. */
static int
control_offset_cmp(struct control_offset *co1, struct control_offset *co2)
{
	if (co1->pane < co2->pane)
		return (-1);
	if (co1->pane > co2->pane)
		return (1);
	return (0);
}
RB_GENERATE_STATIC(control_offsets, control_offset, entry, control_offset_cmp);

/* Get pane offsets for this client. */
static struct control_offset *
control_get_offset(struct client *c, struct window_pane *wp)
{
	struct control_offset	co = { .pane = wp->id };

	if (c->offsets == NULL)
		return (NULL);
	return (RB_FIND(control_offsets, c->offsets, &co));
}

/* Add pane offsets for this client. */
static struct control_offset *
control_add_offset(struct client *c, struct window_pane *wp)
{
	struct control_offset	*co;

	co = control_get_offset(c, wp);
	if (co != NULL)
		return (co);

	if (c->offsets == NULL) {
		c->offsets = xmalloc(sizeof *c->offsets);
		RB_INIT(c->offsets);
	}

	co = xcalloc(1, sizeof *co);
	co->pane = wp->id;
	RB_INSERT(control_offsets, c->offsets, co);
	memcpy(&co->offset, &wp->offset, sizeof co->offset);
	return (co);
}

/* Free control offsets. */
void
control_free_offsets(struct client *c)
{
	struct control_offset	*co, *co1;

	if (c->offsets == NULL)
		return;
	RB_FOREACH_SAFE(co, control_offsets, c->offsets, co1) {
		RB_REMOVE(control_offsets, c->offsets, co);
		free(co);
	}
	free(c->offsets);
}

/* Get offsets for client. */
struct window_pane_offset *
control_pane_offset(struct client *c, struct window_pane *wp, int *off)
{
	struct control_offset	*co;

	if (c->flags & CLIENT_CONTROL_NOOUTPUT) {
		*off = 0;
		return (NULL);
	}

	co = control_get_offset(c, wp);
	if (co == NULL) {
		*off = 0;
		return (NULL);
	}
	if (co->flags & CONTROL_OFFSET_OFF) {
		*off = 1;
		return (NULL);
	}
	return (&co->offset);
}

/* Set pane as on. */
void
control_set_pane_on(struct client *c, struct window_pane *wp)
{
	struct control_offset	*co;

	co = control_get_offset(c, wp);
	if (co != NULL) {
		co->flags &= ~CONTROL_OFFSET_OFF;
		memcpy(&co->offset, &wp->offset, sizeof co->offset);
	}
}

/* Set pane as off. */
void
control_set_pane_off(struct client *c, struct window_pane *wp)
{
	struct control_offset	*co;

	co = control_add_offset(c, wp);
	co->flags |= CONTROL_OFFSET_OFF;
}

/* Write a line. */
void
control_write(struct client *c, const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	file_vprint(c, fmt, ap);
	file_print(c, "\n");
	va_end(ap);
}

/* Write output from a pane. */
void
control_write_output(struct client *c, struct window_pane *wp)
{
	struct control_offset	*co;
	struct evbuffer		*message;
	u_char			*new_data;
	size_t			 new_size, i;

	if (c->flags & CLIENT_CONTROL_NOOUTPUT)
		return;

	/*
	 * Only write input if the window pane is linked to a window belonging
	 * to the client's session.
	 */
	if (winlink_find_by_window(&c->session->windows, wp->window) == NULL)
		return;

	co = control_add_offset(c, wp);
	if (co->flags & CONTROL_OFFSET_OFF) {
		window_pane_update_used_data(wp, &co->offset, SIZE_MAX, 1);
		return;
	}
	new_data = window_pane_get_new_data(wp, &co->offset, &new_size);
	if (new_size == 0)
		return;

	message = evbuffer_new();
	if (message == NULL)
		fatalx("out of memory");
	evbuffer_add_printf(message, "%%output %%%u ", wp->id);

	for (i = 0; i < new_size; i++) {
		if (new_data[i] < ' ' || new_data[i] == '\\')
			evbuffer_add_printf(message, "\\%03o", new_data[i]);
		else
			evbuffer_add_printf(message, "%c", new_data[i]);
	}
	evbuffer_add(message, "", 1);

	control_write(c, "%s", EVBUFFER_DATA(message));
	evbuffer_free(message);

	window_pane_update_used_data(wp, &co->offset, new_size, 1);
}

/* Control error callback. */
static enum cmd_retval
control_error(struct cmdq_item *item, void *data)
{
	struct client	*c = cmdq_get_client(item);
	char		*error = data;

	cmdq_guard(item, "begin", 1);
	control_write(c, "parse error: %s", error);
	cmdq_guard(item, "error", 1);

	free(error);
	return (CMD_RETURN_NORMAL);
}

/* Control input callback. Read lines and fire commands. */
static void
control_callback(__unused struct client *c, __unused const char *path,
    int read_error, int closed, struct evbuffer *buffer, __unused void *data)
{
	char			*line, *error;
	struct cmdq_state	*state;
	enum cmd_parse_status	 status;

	if (closed || read_error != 0)
		c->flags |= CLIENT_EXIT;

	for (;;) {
		line = evbuffer_readln(buffer, NULL, EVBUFFER_EOL_LF);
		if (line == NULL)
			break;
		log_debug("%s: %s", __func__, line);
		if (*line == '\0') { /* empty line exit */
			free(line);
			c->flags |= CLIENT_EXIT;
			break;
		}

		state = cmdq_new_state(NULL, NULL, CMDQ_STATE_CONTROL);
		status = cmd_parse_and_append(line, NULL, c, state, &error);
		if (status == CMD_PARSE_ERROR)
			cmdq_append(c, cmdq_get_callback(control_error, error));
		cmdq_free_state(state);

		free(line);
	}
}

/* Initialize for control mode. */
void
control_start(struct client *c)
{
	file_read(c, "-", control_callback, c);

	if (c->flags & CLIENT_CONTROLCONTROL)
		file_print(c, "\033P1000p");
}
