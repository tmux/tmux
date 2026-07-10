/* $OpenBSD: events-payload.c,v 1.1 2026/07/10 13:38:45 nicm Exp $ */

/*
 * Copyright (c) 2026 Nicholas Marriott <nicholas.marriott@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <event.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tmux.h"

/* Event payload item. */
struct event_payload_item {
	char					*name;
	enum event_payload_type			 type;

	union {
		char				*string;
		time_t				 time;
		int				 number;
		u_int				 unsigned_number;
		struct client			*client;
		struct session			*session;
		struct window			*window;
		struct window_pane		*pane;
		struct {
			void			*ptr;
			event_payload_free_cb	 free_cb;
			event_payload_print_cb	 print_cb;
		} pointer;
	};

	RB_ENTRY(event_payload_item)	 entry;
};

RB_HEAD(event_payload_tree, event_payload_item);

struct event_payload {
	struct event_payload_tree	 items;
	struct cmd_find_state		 target;
};

static int
event_payload_cmp(struct event_payload_item *epi1,
    struct event_payload_item *epi2)
{
	return (strcmp(epi1->name, epi2->name));
}
RB_GENERATE_STATIC(event_payload_tree, event_payload_item, entry,
    event_payload_cmp);

/* Find an item. */
static struct event_payload_item *
event_payload_find(struct event_payload *ep, const char *name)
{
	struct event_payload_item	find = { .name = (char *)name };

	return (RB_FIND(event_payload_tree, &ep->items, &find));
}

/* Free the target in a payload. */
static void
event_payload_free_target(struct event_payload *ep)
{
	struct cmd_find_state	*target = &ep->target;

	if (target->s != NULL)
		session_remove_ref(target->s, __func__);
	if (target->w != NULL)
		window_remove_ref(target->w, __func__);
	if (target->wp != NULL)
		window_pane_remove_ref(target->wp, __func__);

	cmd_find_clear_state(target, 0);
}

/* Free the value in an item. */
static void
event_payload_free_value(struct event_payload_item *epi)
{
	switch (epi->type) {
	case EVENT_PAYLOAD_STRING:
		free(epi->string);
		break;
	case EVENT_PAYLOAD_CLIENT:
		server_client_unref(epi->client);
		break;
	case EVENT_PAYLOAD_SESSION:
		session_remove_ref(epi->session, __func__);
		break;
	case EVENT_PAYLOAD_WINDOW:
		window_remove_ref(epi->window, __func__);
		break;
	case EVENT_PAYLOAD_PANE:
		window_pane_remove_ref(epi->pane, __func__);
		break;
	case EVENT_PAYLOAD_POINTER:
		if (epi->pointer.free_cb != NULL)
			epi->pointer.free_cb(epi->pointer.ptr);
		break;
	case EVENT_PAYLOAD_INT:
	case EVENT_PAYLOAD_UINT:
	case EVENT_PAYLOAD_TIME:
		break;
	}
}

/* Set an item. */
static void
event_payload_set_item(struct event_payload *ep, const char *name,
    struct event_payload_item *new)
{
	struct event_payload_item	*old;

	new->name = xstrdup(name);
	old = RB_INSERT(event_payload_tree, &ep->items, new);
	if (old != NULL) {
		RB_REMOVE(event_payload_tree, &ep->items, old);
		event_payload_free_value(old);
		free(old->name);
		free(old);
		RB_INSERT(event_payload_tree, &ep->items, new);
	}
}

/* Create an event payload. */
struct event_payload *
event_payload_create(void)
{
	struct event_payload	*ep;

	ep = xcalloc(1, sizeof *ep);
	RB_INIT(&ep->items);
	cmd_find_clear_state(&ep->target, 0);
	return (ep);
}

/* Free an event payload. */
void
event_payload_free(struct event_payload *ep)
{
	struct event_payload_item	*epi, *epi1;

	if (ep != NULL) {
		RB_FOREACH_SAFE(epi, event_payload_tree, &ep->items, epi1) {
			RB_REMOVE(event_payload_tree, &ep->items, epi);
			event_payload_free_value(epi);
			free(epi->name);
			free(epi);
		}
		event_payload_free_target(ep);
		free(ep);
	}
}

/* Set the target. */
void
event_payload_set_target(struct event_payload *ep, struct cmd_find_state *fs)
{
	struct cmd_find_state	*target = &ep->target;

	event_payload_free_target(ep);

	if (fs->s != NULL) {
		session_add_ref(fs->s, __func__);
		target->s = fs->s;
	}
	if (fs->wl != NULL) {
		target->idx = fs->wl->idx;
		if (target->s == NULL) {
			session_add_ref(fs->wl->session, __func__);
			target->s = fs->wl->session;
		}
	} else
		target->idx = -1;
	if (fs->w != NULL) {
		window_add_ref(fs->w, __func__);
		target->w = fs->w;
	} else if (fs->wl != NULL) {
		window_add_ref(fs->wl->window, __func__);
		target->w = fs->wl->window;
	}
	if (fs->wp != NULL) {
		window_pane_add_ref(fs->wp, __func__);
		target->wp = fs->wp;
	}
}

/* Get the target. */
int
event_payload_get_target(struct event_payload *ep, struct cmd_find_state *fs)
{
	struct cmd_find_state	*t = &ep->target;
	struct winlink		*wl = NULL;
	int			 flags = fs->flags;

	if (t->idx != -1 &&
	    t->s != NULL &&
	    t->w != NULL &&
	    session_alive(t->s)) {
		wl = winlink_find_by_index(&t->s->windows, t->idx);
		if (wl != NULL && wl->window != t->w)
			wl = NULL;
	}

	cmd_find_clear_state(fs, flags);
	fs->s = t->s;
	fs->w = t->w;
	fs->wp = t->wp;
	fs->wl = wl;
	fs->idx = (wl != NULL ? wl->idx : -1);
	if (cmd_find_valid_state(fs))
		return (1);

	if (wl != NULL &&
	    t->wp != NULL &&
	    window_has_pane(wl->window, t->wp)) {
		cmd_find_from_winlink_pane(fs, wl, t->wp, flags);
		if (cmd_find_valid_state(fs))
			return (1);
	}

	if (t->wp != NULL &&
	    cmd_find_from_pane(fs, t->wp, flags) == 0 &&
	    cmd_find_valid_state(fs))
		return (1);

	if (wl != NULL) {
		cmd_find_from_winlink(fs, wl, flags);
		if (cmd_find_valid_state(fs))
			return (1);
	}

	if (t->s != NULL &&
	    t->w != NULL &&
	    session_alive(t->s) &&
	    cmd_find_from_session_window(fs, t->s, t->w, flags) == 0 &&
	    cmd_find_valid_state(fs))
		return (1);

	if (t->s != NULL && session_alive(t->s)) {
		cmd_find_from_session(fs, t->s, flags);
		if (cmd_find_valid_state(fs))
			return (1);
	}

	if (cmd_find_from_nothing(fs, flags) == 0)
		return (1);

	cmd_find_clear_state(fs, flags);
	return (0);
}

/* Set a string item. */
void
event_payload_set_string(struct event_payload *ep, const char *name,
    const char *fmt, ...)
{
	struct event_payload_item	*epi;
	va_list				 ap;

	va_start(ap, fmt);

	epi = xcalloc(1, sizeof *epi);
	epi->type = EVENT_PAYLOAD_STRING;
	xvasprintf(&epi->string, fmt, ap);
	event_payload_set_item(ep, name, epi);

	va_end(ap);
}

/* Set a time item. */
void
event_payload_set_time(struct event_payload *ep, const char *name,
    time_t value)
{
	struct event_payload_item	*epi;

	epi = xcalloc(1, sizeof *epi);
	epi->type = EVENT_PAYLOAD_TIME;
	epi->time = value;
	event_payload_set_item(ep, name, epi);
}

/* Set a number item. */
void
event_payload_set_int(struct event_payload *ep, const char *name, int value)
{
	struct event_payload_item	*epi;

	epi = xcalloc(1, sizeof *epi);
	epi->type = EVENT_PAYLOAD_INT;
	epi->number = value;
	event_payload_set_item(ep, name, epi);
}

/* Set an unsigned number item. */
void
event_payload_set_uint(struct event_payload *ep, const char *name, u_int value)
{
	struct event_payload_item	*epi;

	epi = xcalloc(1, sizeof *epi);
	epi->type = EVENT_PAYLOAD_UINT;
	epi->unsigned_number = value;
	event_payload_set_item(ep, name, epi);
}

/* Set a client item. */
void
event_payload_set_client(struct event_payload *ep, const char *name,
    struct client *c)
{
	struct event_payload_item	*epi;

	c->references++;

	epi = xcalloc(1, sizeof *epi);
	epi->type = EVENT_PAYLOAD_CLIENT;
	epi->client = c;
	event_payload_set_item(ep, name, epi);
}

/* Set a session item. */
void
event_payload_set_session(struct event_payload *ep, const char *name,
    struct session *s)
{
	struct event_payload_item	*epi;

	session_add_ref(s, __func__);

	epi = xcalloc(1, sizeof *epi);
	epi->type = EVENT_PAYLOAD_SESSION;
	epi->session = s;
	event_payload_set_item(ep, name, epi);
}

/* Set a window item. */
void
event_payload_set_window(struct event_payload *ep, const char *name,
    struct window *w)
{
	struct event_payload_item	*epi;

	window_add_ref(w, __func__);

	epi = xcalloc(1, sizeof *epi);
	epi->type = EVENT_PAYLOAD_WINDOW;
	epi->window = w;
	event_payload_set_item(ep, name, epi);
}

/* Set a pane item. */
void
event_payload_set_pane(struct event_payload *ep, const char *name,
    struct window_pane *wp)
{
	struct event_payload_item	*epi;

	window_pane_add_ref(wp, __func__);

	epi = xcalloc(1, sizeof *epi);
	epi->type = EVENT_PAYLOAD_PANE;
	epi->pane = wp;
	event_payload_set_item(ep, name, epi);
}

/* Set a pointer item. */
void
event_payload_set_pointer(struct event_payload *ep, const char *name,
    void *ptr, event_payload_free_cb free_cb, event_payload_print_cb print_cb)
{
	struct event_payload_item	*epi;

	epi = xcalloc(1, sizeof *epi);
	epi->type = EVENT_PAYLOAD_POINTER;
	epi->pointer.ptr = ptr;
	epi->pointer.free_cb = free_cb;
	epi->pointer.print_cb = print_cb;
	event_payload_set_item(ep, name, epi);
}

/* Get a string item. */
const char *
event_payload_get_string(struct event_payload *ep, const char *name)
{
	struct event_payload_item	*epi;

	epi = event_payload_find(ep, name);
	if (epi == NULL || epi->type != EVENT_PAYLOAD_STRING)
		return (NULL);
	return (epi->string);
}

/* Print a payload item. */
static void
event_payload_add_item(struct event_payload_item *epi, struct evbuffer *evb)
{
	switch (epi->type) {
	case EVENT_PAYLOAD_STRING:
		evbuffer_add_printf(evb, "%s", epi->string);
		break;
	case EVENT_PAYLOAD_TIME:
		evbuffer_add_printf(evb, "%lld", (long long)epi->time);
		break;
	case EVENT_PAYLOAD_INT:
		evbuffer_add_printf(evb, "%d", epi->number);
		break;
	case EVENT_PAYLOAD_UINT:
		evbuffer_add_printf(evb, "%u", epi->unsigned_number);
		break;
	case EVENT_PAYLOAD_CLIENT:
		evbuffer_add_printf(evb, "%s", epi->client->name);
		break;
	case EVENT_PAYLOAD_SESSION:
		evbuffer_add_printf(evb, "$%u", epi->session->id);
		break;
	case EVENT_PAYLOAD_WINDOW:
		evbuffer_add_printf(evb, "@%u", epi->window->id);
		break;
	case EVENT_PAYLOAD_PANE:
		evbuffer_add_printf(evb, "%%%u", epi->pane->id);
		break;
	case EVENT_PAYLOAD_POINTER:
		if (epi->pointer.print_cb != NULL)
			epi->pointer.print_cb(epi->pointer.ptr, evb);
		else
			evbuffer_add_printf(evb, "%p", epi->pointer.ptr);
		break;
	}
}

/* Print a payload item. */
char *
event_payload_item_print(struct event_payload_item *epi)
{
	struct evbuffer			*evb;
	char				*value = NULL;
	size_t				 size;

	evb = evbuffer_new();
	if (evb == NULL)
		fatalx("out of memory");
	event_payload_add_item(epi, evb);
	if ((size = EVBUFFER_LENGTH(evb)) != 0)
		value = xmemdup(EVBUFFER_DATA(evb), size);
	else
		value = xstrdup("");
	evbuffer_free(evb);
	return (value);
}

/* Print an item value. */
char *
event_payload_print(struct event_payload *ep, const char *name)
{
	struct event_payload_item	*epi;

	epi = event_payload_find(ep, name);
	if (epi == NULL)
		return (NULL);
	return (event_payload_item_print(epi));
}

/* Add payload items as formats. */
void
event_payload_add_formats(struct event_payload *ep, struct format_tree *ft,
    const char *prefix)
{
	struct event_payload_item	*epi;
	char				*name, *value;
	const char			*key;

	if (prefix == NULL)
		prefix = "";

	RB_FOREACH(epi, event_payload_tree, &ep->items) {
		key = epi->name;
		if (*key == '_')
			continue;

		value = event_payload_item_print(epi);
		xasprintf(&name, "%s%s", prefix, key);
		format_add(ft, name, "%s", value);
		free(name);
		free(value);

		if (epi->type == EVENT_PAYLOAD_SESSION) {
			xasprintf(&name, "%s%s_name", prefix, key);
			format_add(ft, name, "%s", epi->session->name);
			free(name);
		} else if (epi->type == EVENT_PAYLOAD_WINDOW) {
			xasprintf(&name, "%s%s_name", prefix, key);
			format_add(ft, name, "%s", epi->window->name);
			free(name);
		}
	}
}

/* Get the first payload item. */
struct event_payload_item *
event_payload_first(struct event_payload *ep)
{
	return (RB_MIN(event_payload_tree, &ep->items));
}

/* Get the next payload item. */
struct event_payload_item *
event_payload_next(struct event_payload_item *epi)
{
	return (RB_NEXT(event_payload_tree, , epi));
}

/* Get a payload item name. */
const char *
event_payload_item_name(struct event_payload_item *epi)
{
	return (epi->name);
}

/* Get a payload item type. */
enum event_payload_type
event_payload_item_type(struct event_payload_item *epi)
{
	return (epi->type);
}

/* Log a payload. */
void
event_payload_log(struct event_payload *ep, const char *fmt, ...)
{
	struct event_payload_item	*epi;
	struct evbuffer			*evb;
	va_list				 ap;
	char				*prefix;

	va_start(ap, fmt);
	xvasprintf(&prefix, fmt, ap);
	va_end(ap);

	evb = evbuffer_new();
	if (evb == NULL)
		fatalx("out of memory");
	if (ep != NULL) {
		RB_FOREACH(epi, event_payload_tree, &ep->items) {
			if (EVBUFFER_LENGTH(evb) != 0)
				evbuffer_add_printf(evb, ", ");
			evbuffer_add_printf(evb, "%s=", epi->name);
			event_payload_add_item(epi, evb);
		}
	}
	log_debug("%s%.*s", prefix, (int)EVBUFFER_LENGTH(evb),
	    (char *)EVBUFFER_DATA(evb));
	evbuffer_free(evb);
	free(prefix);
}

/* Get a time item. */
time_t
event_payload_get_time(struct event_payload *ep, const char *name)
{
	struct event_payload_item	*epi;

	epi = event_payload_find(ep, name);
	if (epi == NULL || epi->type != EVENT_PAYLOAD_TIME)
		return (0);
	return (epi->time);
}

/* Get a number item. */
int
event_payload_get_int(struct event_payload *ep, const char *name, int *value)
{
	struct event_payload_item	*epi;

	epi = event_payload_find(ep, name);
	if (epi == NULL || epi->type != EVENT_PAYLOAD_INT)
		return (-1);
	*value = epi->number;
	return (0);
}

/* Get an unsigned number item. */
int
event_payload_get_uint(struct event_payload *ep, const char *name, u_int *value)
{
	struct event_payload_item	*epi;

	epi = event_payload_find(ep, name);
	if (epi == NULL || epi->type != EVENT_PAYLOAD_UINT)
		return (-1);
	*value = epi->unsigned_number;
	return (0);
}

/* Get a client item. */
struct client *
event_payload_get_client(struct event_payload *ep, const char *name)
{
	struct event_payload_item	*epi;

	epi = event_payload_find(ep, name);
	if (epi == NULL || epi->type != EVENT_PAYLOAD_CLIENT)
		return (NULL);
	return (epi->client);
}

/* Get a session item. */
struct session *
event_payload_get_session(struct event_payload *ep, const char *name)
{
	struct event_payload_item	*epi;

	epi = event_payload_find(ep, name);
	if (epi == NULL || epi->type != EVENT_PAYLOAD_SESSION)
		return (NULL);
	return (epi->session);
}

/* Get a window item. */
struct window *
event_payload_get_window(struct event_payload *ep, const char *name)
{
	struct event_payload_item	*epi;

	epi = event_payload_find(ep, name);
	if (epi == NULL || epi->type != EVENT_PAYLOAD_WINDOW)
		return (NULL);
	return (epi->window);
}

/* Get a pane item. */
struct window_pane *
event_payload_get_pane(struct event_payload *ep, const char *name)
{
	struct event_payload_item	*epi;

	epi = event_payload_find(ep, name);
	if (epi == NULL || epi->type != EVENT_PAYLOAD_PANE)
		return (NULL);
	return (epi->pane);
}

/* Get a pointer item. */
void *
event_payload_get_pointer(struct event_payload *ep, const char *name)
{
	struct event_payload_item	*epi;

	epi = event_payload_find(ep, name);
	if (epi == NULL || epi->type != EVENT_PAYLOAD_POINTER)
		return (NULL);
	return (epi->pointer.ptr);
}
