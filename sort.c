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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

static struct sort_criteria *sort_criteria;

static int	sort_is_sortable(struct sort_criteria *sort_crit);

static int	sort_session_cmp(const void *a0, const void *b0);
static int	sort_window_tree_winlink_cmp(const void *a0, const void *b0);
static int	sort_window_tree_pane_cmp(const void *a0, const void *b0);
static int	sort_window_client_cmp(const void *a0, const void *b0);
static int	sort_window_buffer_cmp(const void *a0, const void *b0);

void
sort_criteria_init(struct sort_criteria *sort_crit, const char *order_str,
    int reversed, enum sort_order *order_seq)
{
	sort_crit->order     = sort_order_from_string(order_str);
	sort_crit->reversed  = reversed;
	sort_crit->order_seq = order_seq;
}

enum sort_order
sort_order_from_string(const char* order)
{
	if (order == NULL)
		return (SORT_END);
	if (strcasecmp(order, "index") == 0)
		return (SORT_INDEX);
	if (strcasecmp(order, "name") == 0)
		return (SORT_NAME);
	if (strcasecmp(order, "order") == 0)
		return (SORT_ORDER);
	if (strcasecmp(order, "size") == 0)
		return (SORT_SIZE);
	if (strcasecmp(order, "creation") == 0)
		return (SORT_CREATION);
	if (strcasecmp(order, "activity") == 0)
		return (SORT_ACTIVITY);

	return (SORT_END);
}

const char *
sort_order_to_string(enum sort_order order)
{
	if (order == SORT_INDEX)
		return "index";
	if (order == SORT_NAME)
		return "name";
	if (order == SORT_ORDER)
		return "order";
	if (order == SORT_SIZE)
		return "size";
	if (order == SORT_CREATION)
		return "creation";
	if (order == SORT_ACTIVITY)
		return "activity";

	return NULL; // TODO: how should this fail?
}

int
sort_would_window_tree_swap_indices(struct sort_criteria *sort_crit,
    struct winlink *wla, struct winlink *wlb)
{
	sort_criteria = sort_crit;
	return sort_criteria->order != SORT_INDEX &&
		sort_window_tree_winlink_cmp(&wla, &wlb) != 0;
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
		fatalx("-%d order not found in order sequence.",
		    sort_crit->order);

	i++;
	if (sort_crit->order_seq[i] == SORT_END)
		i = 0;

	sort_crit->order = sort_crit->order_seq[i];
}

struct session **
sort_get_sessions(u_int *n, struct sort_criteria *sort_crit)
{
	struct session	*s, **l = NULL;
	u_int		 i = 0;

	RB_FOREACH(s, sessions, &sessions) {
		l = xreallocarray(l, i + 1, sizeof *l);
		l[i++] = s;
	}

	if (sort_is_sortable(sort_crit)) {
		sort_criteria = sort_crit;
		qsort(l, i, sizeof *l, sort_session_cmp);
	}
	*n = i;

	return l;
}

struct window_pane **
sort_get_window_panes(struct winlink *wl, u_int *n, 
    struct sort_criteria *sort_crit)
{
	struct window_pane	*wp, **l = NULL;
	u_int		 	 i = 0;

	TAILQ_FOREACH(wp, &wl->window->panes, entry) {
		l = xreallocarray(l, i + 1, sizeof *l);
		l[i++] = wp;
	}

	if (sort_is_sortable(sort_crit)) {
		sort_criteria = sort_crit;
		qsort(l, i, sizeof *l, sort_window_tree_pane_cmp);
	}
	*n = i;

	return l;
}

struct winlink **
sort_get_winlinks(struct session *s, u_int *n, struct sort_criteria *sort_crit)
{
	struct winlink	*wl, **l = NULL;
	u_int		 i = 0;

	RB_FOREACH(wl, winlinks, &s->windows) {
		l = xreallocarray(l, i + 1, sizeof *l);
		l[i++] = wl;
	}

	if (sort_is_sortable(sort_crit)) {
		sort_criteria = sort_crit;
		qsort(l, i, sizeof *l, sort_window_tree_winlink_cmp);
	}
	*n = i;

	return l;
}

struct window_client_itemdata **
sort_get_window_client_itemdata(struct window_client_modedata *md, u_int *n,
    struct sort_criteria *sort_crit)
{
	struct window_client_itemdata	**l;
	u_int				  i;

	for (i = 0; i < md->item_size; i++) {
		l = xreallocarray(l, i + 1, sizeof *l);
		l[i++] = md->item_list[i];
	}

	if (sort_is_sortable(sort_crit)) {
		sort_criteria = sort_crit;
		qsort(l, i, sizeof *l, sort_window_client_cmp);
	}
	*n = i;

	return l;
}

struct window_buffer_itemdata **
sort_get_window_buffer_itemdata(struct window_buffer_modedata *md, u_int *n,
    struct sort_criteria *sort_crit)
{
	struct window_buffer_itemdata	**l;
	u_int				  i;

	for (i = 0; i < md->item_size; i++) {
		l = xreallocarray(l, i + 1, sizeof *l);
		l[i++] = md->item_list[i];
	}

	if (sort_is_sortable(sort_crit)) {
		sort_criteria = sort_crit;
		qsort(l, i, sizeof *l, sort_window_buffer_cmp);
	}
	*n = i;

	return l;
}

static int
sort_is_sortable(struct sort_criteria *sort_crit)
{
	return sort_crit != NULL && sort_crit->order != SORT_END;
}

static int
sort_session_cmp(const void *a0, const void *b0)
{
	const struct session *const	*a = a0;
	const struct session *const	*b = b0;
	const struct session		*sa = *a;
	const struct session		*sb = *b;
	int				 result = 0;
	
	switch (sort_criteria->order) {
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
	default:
		fatalx("-%d unsupported sort order for session",
		    sort_criteria->order);
	}

	/* Use SORT_NAME as default order and tie breaker. */
	if (result == 0)
		result = strcmp(sa->name, sb->name);
	
	if (sort_criteria->reversed)
		result = -result;
	return (result);
}

static int
sort_window_tree_winlink_cmp(const void *a0, const void *b0)
{
	const struct winlink *const	*a = a0;
	const struct winlink *const	*b = b0;
	const struct winlink		*wla = *a;
	const struct winlink		*wlb = *b;
	struct window			*wa = wla->window;
	struct window			*wb = wlb->window;
	int				 result = 0;

	switch (sort_criteria->order) {
	case SORT_INDEX:
		result = wla->idx - wlb->idx;
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
	default:
		fatalx("-%d unsupported sort order for winlink",
		    sort_criteria->order);
	}

	/* Use SORT_NAME as default order and tie breaker. */
	if (result == 0)
		result = strcmp(wa->name, wb->name);

	if (sort_criteria->reversed)
		result = -result;
	return (result);
}

static int
sort_window_tree_pane_cmp(const void *a0, const void *b0)
{
	struct window_pane	**a = (struct window_pane **)a0;
	struct window_pane	**b = (struct window_pane **)b0;
	int			  result;
	u_int			  ai, bi;

	if (sort_criteria->order == SORT_ACTIVITY)
		result = (*a)->active_point - (*b)->active_point;
	else {
		/*
		 * Panes don't have names, so use number order for any other
		 * sort field.
		 */
		window_pane_index(*a, &ai);
		window_pane_index(*b, &bi);
		result = ai - bi;
	}
	if (sort_criteria->reversed)
		result = -result;
	return (result);
}

static int
sort_window_client_cmp(const void *a0, const void *b0)
{
	const struct window_client_itemdata *const	*a = a0;
	const struct window_client_itemdata *const	*b = b0;
	const struct window_client_itemdata		*itema = *a;
	const struct window_client_itemdata		*itemb = *b;
	struct client					*ca = itema->c;
	struct client					*cb = itemb->c;
	int						 result = 0;

	switch (sort_criteria->order) {
	case SORT_SIZE:
		result = ca->tty.sx - cb->tty.sx;
		if (result == 0)
			result = ca->tty.sy - cb->tty.sy;
		break;
	case SORT_CREATION:
		if (timercmp(&ca->creation_time, &cb->creation_time, >))
			result = -1;
		else if (timercmp(&ca->creation_time, &cb->creation_time, <))
			result = 1;
		break;
	case SORT_ACTIVITY:
		if (timercmp(&ca->activity_time, &cb->activity_time, >))
			result = -1;
		else if (timercmp(&ca->activity_time, &cb->activity_time, <))
			result = 1;
		break;
	default:
		fatalx("-%d unsupported sort order for client",
		    sort_criteria->order);
	}

	/* Use SORT_NAME as default order and tie breaker. */
	if (result == 0)
		result = strcmp(ca->name, cb->name);

	if (sort_criteria->reversed)
		result = -result;
	return (result);
}

static int
sort_window_buffer_cmp(const void *a0, const void *b0)
{
	const struct window_buffer_itemdata *const	*a = a0;
	const struct window_buffer_itemdata *const	*b = b0;
	int						 result = 0;

	if (sort_criteria->order == SORT_ORDER)
		result = (*b)->order - (*a)->order;
	else if (sort_criteria->order == SORT_SIZE)
		result = (*b)->size - (*a)->size;

	/* Use SORT_NAME as default order and tie breaker. */
	if (result == 0)
		result = strcmp((*a)->name, (*b)->name);

	if (sort_criteria->reversed)
		result = -result;
	return (result);
}

