/* $OpenBSD$ */

/*
 * Copyright (c) 2026 Dane Jensen <dhcjensen@gmail.com>
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

static struct sort_criteria *sort_criteria;

static void
sort_qsort(void *l, u_int len, u_int size, int (*cmp)(const void *, const void *),
    struct sort_criteria *sort_crit)
{
	u_int	 i;
	void	*tmp, **ll;

	if (sort_crit->order == SORT_END)
		return;

	if (sort_crit->order == SORT_ORDER) {
		if (sort_crit->reversed) {
			ll = l;
			for (i = 0; i < len / 2; i++) {
				tmp = ll[i];
				ll[i] = ll[len - 1 - i];
				ll[len - 1 - i] = tmp;
			}
		}
	} else {
		sort_criteria = sort_crit;
		qsort(l, len, size, cmp);
	}
}

static int
sort_buffer_cmp(const void *a0, const void *b0)
{
	struct sort_criteria			*sort_crit = sort_criteria;
	const struct paste_buffer *const	*a = a0;
	const struct paste_buffer *const	*b = b0;
	const struct paste_buffer 		*pa = *a;
	const struct paste_buffer 		*pb = *b;
	int					 result = 0;

	switch (sort_crit->order) {
	case SORT_NAME:
		result = strcmp(pa->name, pb->name);
		break;
	case SORT_CREATION:
		result = pa->order - pb->order;
		break;
	case SORT_SIZE:
		result = pa->size - pb->size;
		break;
	case SORT_ACTIVITY:
	case SORT_INDEX:
	case SORT_MODIFIER:
	case SORT_ORDER:
	case SORT_END:
		break;
	}

	if (result == 0)
		result = strcmp(pa->name, pb->name);

	if (sort_crit->reversed)
		result = -result;
	return (result);
}

static int
sort_client_cmp(const void *a0, const void *b0)
{
	struct sort_criteria		*sort_crit = sort_criteria;
	const struct client *const	*a = a0;
	const struct client *const	*b = b0;
	const struct client 		*ca = *a;
	const struct client 		*cb = *b;
	int				 result = 0;

	switch (sort_crit->order) {
	case SORT_NAME:
		result = strcmp(ca->name, cb->name);
		break;
	case SORT_SIZE:
		result = ca->tty.sx - cb->tty.sx;
		if (result == 0)
			result = ca->tty.sy - cb->tty.sy;
		break;
	case SORT_CREATION:
		if (timercmp(&ca->creation_time, &cb->creation_time, >))
			result = 1;
		else if (timercmp(&ca->creation_time, &cb->creation_time, <))
			result = -1;
		break;
	case SORT_ACTIVITY:
		if (timercmp(&ca->activity_time, &cb->activity_time, >))
			result = -1;
		else if (timercmp(&ca->activity_time, &cb->activity_time, <))
			result = 1;
		break;
	case SORT_INDEX:
	case SORT_MODIFIER:
	case SORT_ORDER:
	case SORT_END:
		break;
	}

	if (result == 0)
		result = strcmp(ca->name, cb->name);

	if (sort_crit->reversed)
		result = -result;
	return (result);
}

static int
sort_session_cmp(const void *a0, const void *b0)
{
	struct sort_criteria		*sort_crit = sort_criteria;
	const struct session *const	*a = a0;
	const struct session *const	*b = b0;
	const struct session		*sa = *a;
	const struct session		*sb = *b;
	int				 result = 0;

	switch (sort_crit->order) {
	case SORT_INDEX:
		result = sa->id - sb->id;
		break;
	case SORT_CREATION:
		if (timercmp(&sa->creation_time, &sb->creation_time, >)) {
			result = 1;
			break;
		}
		if (timercmp(&sa->creation_time, &sb->creation_time, <)) {
			result = -1;
			break;
		}
		break;
	case SORT_ACTIVITY:
		if (timercmp(&sa->activity_time, &sb->activity_time, >)) {
			result = -1;
			break;
		}
		if (timercmp(&sa->activity_time, &sb->activity_time, <)) {
			result = 1;
			break;
		}
		break;
	case SORT_NAME:
		result = strcmp(sa->name, sb->name);
		break;
	case SORT_MODIFIER:
	case SORT_ORDER:
	case SORT_SIZE:
	case SORT_END:
		break;
	}

	if (result == 0)
		result = strcmp(sa->name, sb->name);

	if (sort_crit->reversed)
		result = -result;
	return (result);
}

static int
sort_pane_cmp(const void *a0, const void *b0)
{
	struct sort_criteria	*sort_crit = sort_criteria;
	struct window_pane	*a = *(struct window_pane **)a0;
	struct window_pane	*b = *(struct window_pane **)b0;
	int			 result = 0;
	u_int			 ai, bi;

	switch (sort_crit->order) {
	case SORT_ACTIVITY:
		result = a->active_point - b->active_point;
		break;
	case SORT_CREATION:
		result = a->id - b->id;
		break;
	case SORT_SIZE:
		result = a->sx * a->sy - b->sx * b->sy;
		break;
	case SORT_INDEX:
		window_pane_index(a, &ai);
		window_pane_index(b, &bi);
		result = ai - bi;
		break;
	case SORT_NAME:
		result = strcmp(a->screen->title, b->screen->title);
		break;
	case SORT_MODIFIER:
	case SORT_ORDER:
	case SORT_END:
		break;
	}

	if (result == 0)
		result = strcmp(a->screen->title, b->screen->title);

	if (sort_crit->reversed)
		result = -result;
	return (result);
}

static int
sort_winlink_cmp(const void *a0, const void *b0)
{
	struct sort_criteria		*sort_crit = sort_criteria;
	const struct winlink *const	*a = a0;
	const struct winlink *const	*b = b0;
	const struct winlink		*wla = *a;
	const struct winlink		*wlb = *b;
	struct window			*wa = wla->window;
	struct window			*wb = wlb->window;
	int				 result = 0;

	switch (sort_crit->order) {
	case SORT_INDEX:
		result = wla->idx - wlb->idx;
		break;
	case SORT_CREATION:
		if (timercmp(&wa->creation_time, &wb->creation_time, >)) {
			result = -1;
			break;
		}
		if (timercmp(&wa->creation_time, &wb->creation_time, <)) {
			result = 1;
			break;
		}
		break;
	case SORT_ACTIVITY:
		if (timercmp(&wa->activity_time, &wb->activity_time, >)) {
			result = -1;
			break;
		}
		if (timercmp(&wa->activity_time, &wb->activity_time, <)) {
			result = 1;
			break;
		}
		break;
	case SORT_NAME:
		result = strcmp(wa->name, wb->name);
		break;
	case SORT_SIZE:
		result = wa->sx * wa->sy - wb->sx * wb->sy;
		break;
	case SORT_MODIFIER:
	case SORT_ORDER:
	case SORT_END:
		break;
	}

	if (result == 0)
		result = strcmp(wa->name, wb->name);

	if (sort_crit->reversed)
		result = -result;
	return (result);
}

static int
sort_key_binding_cmp(const void *a0, const void *b0)
{
	struct sort_criteria		*sort_crit = sort_criteria;
	const struct key_binding	*a = *(struct key_binding **)a0;
	const struct key_binding	*b = *(struct key_binding **)b0;
	int				 result = 0;

	switch (sort_crit->order) {
	case SORT_INDEX:
		result = a->key - b->key;
		break;
	case SORT_MODIFIER:
		result = (a->key & KEYC_MASK_MODIFIERS) -
		    (b->key & KEYC_MASK_MODIFIERS);
		break;
	case SORT_NAME:
		result = strcasecmp(a->tablename, b->tablename) == 0;
		break;
	case SORT_ACTIVITY:
	case SORT_CREATION:
	case SORT_ORDER:
	case SORT_SIZE:
	case SORT_END:
		break;
	}

	if (result == 0)
		result = strcasecmp(a->tablename, b->tablename) == 0;

	if (sort_crit->reversed)
		result = -result;
	return (result);
}

void
sort_next_order(struct sort_criteria *sort_crit)
{
	u_int	i;

	if (sort_crit->order_seq == NULL)
		return;
	for (i = 0; sort_crit->order_seq[i] != SORT_END; i++) {
		if (sort_crit->order == sort_crit->order_seq[i])
			break;
	}

	if (sort_crit->order_seq[i] == SORT_END)
		i = 0;
	else {
		i++;
		if (sort_crit->order_seq[i] == SORT_END)
			i = 0;
	}
	sort_crit->order = sort_crit->order_seq[i];
}

enum sort_order
sort_order_from_string(const char* order)
{
	if (order != NULL) {
		if (strcasecmp(order, "activity") == 0)
			return (SORT_ACTIVITY);
		if (strcasecmp(order, "creation") == 0)
			return (SORT_CREATION);
		if (strcasecmp(order, "index") == 0 ||
		    strcasecmp(order, "key") == 0)
			return (SORT_INDEX);
		if (strcasecmp(order, "modifier") == 0)
			return (SORT_MODIFIER);
		if (strcasecmp(order, "name") == 0 ||
		    strcasecmp(order, "title") == 0)
			return (SORT_NAME);
		if (strcasecmp(order, "order") == 0)
			return (SORT_ORDER);
		if (strcasecmp(order, "size") == 0)
			return (SORT_SIZE);
	}
	return (SORT_END);
}

const char *
sort_order_to_string(enum sort_order order)
{
	if (order == SORT_ACTIVITY)
		return "activity";
	if (order == SORT_CREATION)
		return "creation";
	if (order == SORT_INDEX)
		return "index";
	if (order == SORT_MODIFIER)
		return "modifier";
	if (order == SORT_NAME)
		return "name";
	if (order == SORT_ORDER)
		return "order";
	if (order == SORT_SIZE)
		return "size";
	return (NULL);
}

int
sort_would_window_tree_swap(struct sort_criteria *sort_crit,
    struct winlink *wla, struct winlink *wlb)
{
	if (sort_crit->order == SORT_INDEX)
		return (0);
	sort_criteria = sort_crit;
	return (sort_winlink_cmp(&wla, &wlb) != 0);
}

struct paste_buffer **
sort_get_buffers(u_int *n, struct sort_criteria *sort_crit)
{
	struct paste_buffer		 *pb = NULL;
	u_int				  i;
	static struct paste_buffer	**l = NULL;
	static u_int			  lsz = 0;

	i = 0;
	while ((pb = paste_walk(pb)) != NULL) {
		if (lsz <= i) {
			lsz += 100;
			l = xreallocarray(l, lsz, sizeof *l);
		}
		l[i++] = pb;
	}

	sort_qsort(l, i, sizeof *l, sort_buffer_cmp, sort_crit);
	*n = i;

	return (l);
}

struct client **
sort_get_clients(u_int *n, struct sort_criteria *sort_crit)
{
	struct client		 *c;
	u_int			  i;
	static struct client	**l = NULL;
	static u_int		  lsz = 0;

	i = 0;
	TAILQ_FOREACH(c, &clients, entry) {
		if (lsz <= i) {
			lsz += 100;
			l = xreallocarray(l, lsz, sizeof *l);
		}
		l[i++] = c;
	}

	sort_qsort(l, i, sizeof *l, sort_client_cmp, sort_crit);
	*n = i;

	return (l);
}

struct session **
sort_get_sessions(u_int *n, struct sort_criteria *sort_crit)
{
	struct session		 *s;
	u_int			  i;
	static struct session	**l = NULL;
	static u_int		  lsz = 0;

	i = 0;
	RB_FOREACH(s, sessions, &sessions) {
		if (lsz <= i) {
			lsz += 100;
			l = xreallocarray(l, lsz, sizeof *l);
		}
		l[i++] = s;
	}

	sort_qsort(l, i, sizeof *l, sort_session_cmp, sort_crit);
	*n = i;

	return (l);
}

struct window_pane **
sort_get_panes(u_int *n, struct sort_criteria *sort_crit)
{
	struct session			 *s;
	struct winlink			 *wl;
	struct window			 *w;
	struct window_pane		 *wp;
	u_int		 		  i;
	static struct window_pane	**l = NULL;
	static u_int			  lsz = 0;

	i = 0;
	RB_FOREACH(s, sessions, &sessions) {
		RB_FOREACH(wl, winlinks, &s->windows)  {
			w = wl->window;
			TAILQ_FOREACH(wp, &w->panes, entry) {
				if (lsz <= i) {
					lsz += 100;
					l = xreallocarray(l, lsz, sizeof *l);
				}
				l[i++] = wp;
			}
		}
	}

	sort_qsort(l, i, sizeof *l, sort_pane_cmp, sort_crit);
	*n = i;

	return (l);
}

struct window_pane **
sort_get_panes_session(struct session *s, u_int *n,
    struct sort_criteria *sort_crit)
{
	struct winlink			 *wl = NULL;
	struct window			 *w = NULL;
	struct window_pane		 *wp = NULL;
	u_int		 		  i;
	static struct window_pane	**l = NULL;
	static u_int			  lsz = 0;

	i = 0;
	RB_FOREACH(wl, winlinks, &s->windows)  {
		TAILQ_FOREACH(wp, &w->panes, entry) {
			if (lsz <= i) {
				lsz += 100;
				l = xreallocarray(l, lsz, sizeof *l);
			}
			l[i++] = wp;
		}
	}

	sort_qsort(l, i, sizeof *l, sort_pane_cmp, sort_crit);
	*n = i;

	return (l);
}

struct window_pane **
sort_get_panes_window(struct window *w, u_int *n,
    struct sort_criteria *sort_crit)
{
	struct window_pane		 *wp;
	u_int		 		  i;
	static struct window_pane	**l = NULL;
	static u_int			  lsz = 0;

	i = 0;
	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (lsz <= i) {
			lsz += 100;
			l = xreallocarray(l, lsz, sizeof *l);
		}
		l[i++] = wp;
	}

	sort_qsort(l, i, sizeof *l, sort_pane_cmp, sort_crit);
	*n = i;

	return (l);
}

struct winlink **
sort_get_winlinks(u_int *n, struct sort_criteria *sort_crit)
{
	struct session		 *s;
	struct winlink		 *wl;
	u_int			  i;
	static struct winlink	**l = NULL;
	static u_int		  lsz = 0;

	i = 0;
	RB_FOREACH(s, sessions, &sessions) {
		RB_FOREACH(wl, winlinks, &s->windows) {
			if (lsz <= i) {
				lsz += 100;
				l = xreallocarray(l, lsz, sizeof *l);
			}
			l[i++] = wl;
		}
	}

	sort_qsort(l, i, sizeof *l, sort_winlink_cmp, sort_crit);
	*n = i;

	return (l);
}

struct winlink **
sort_get_winlinks_session(struct session *s, u_int *n,
    struct sort_criteria *sort_crit)
{
	struct winlink		 *wl;
	u_int			  i;
	static struct winlink	**l = NULL;
	static u_int		  lsz = 0;

	i = 0;
	RB_FOREACH(wl, winlinks, &s->windows) {
		if (lsz <= i) {
			lsz += 100;
			l = xreallocarray(l, lsz, sizeof *l);
		}
		l[i++] = wl;
	}

	sort_qsort(l, i, sizeof *l, sort_winlink_cmp, sort_crit);
	*n = i;

	return (l);
}

struct key_binding **
sort_get_key_bindings(u_int *n, struct sort_criteria *sort_crit)
{
	struct key_table		 *table;
	struct key_binding		 *bd;
	u_int				  i = 0;
	static struct key_binding	**l = NULL;
	static u_int			  lsz = 0;

	table = key_bindings_first_table();
	while (table != NULL) {
		bd = key_bindings_first(table);
		while (bd != NULL) {
			if (lsz <= i) {
				lsz += 100;
				l = xreallocarray(l, lsz, sizeof *l);
			}
			l[i++] = bd;
			bd = key_bindings_next(table, bd);
		}
		table = key_bindings_next_table(table);
	}

	sort_qsort(l, i, sizeof *l, sort_key_binding_cmp, sort_crit);
	*n = i;

	return (l);
}

struct key_binding **
sort_get_key_bindings_table(struct key_table *table, u_int *n,
    struct sort_criteria *sort_crit)
{
	struct key_binding		 *bd;
	u_int				  i = 0;
	static struct key_binding	**l = NULL;
	static u_int			  lsz = 0;

	bd = key_bindings_first(table);
	while (bd != NULL) {
		if (lsz <= i) {
			lsz += 100;
			l = xreallocarray(l, lsz, sizeof *l);
		}
		l[i++] = bd;
		bd = key_bindings_next(table, bd);
	}

	sort_qsort(l, i, sizeof *l, sort_key_binding_cmp, sort_crit);
	*n = i;

	return (l);
}
