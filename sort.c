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

struct sort_criteria *sort_criteria;

static int sort_session_cmp(const void *a0, const void *b0);
static int sort_should_sort(struct sort_criteria *sc);

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

const char *sort_order_to_string(enum sort_order order)
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

void sort_criteria_init(struct sort_criteria *sc, const char *orderstr,
    int reversed, int (*cmp)(const void *, const void *),
    struct sort_ordering *ordering)
{
	sc->order    = sort_order_from_string(orderstr);
	sc->reversed = reversed;
	sc->cmp      = cmp;
	sc->ordering = ordering;
}

void
sort_run(void *list, u_int len, u_int size, struct sort_criteria *sc)
{
	if (sort_should_sort(sc)) {
		sort_criteria = sc;
		qsort(list, len, size, sort_criteria->cmp);
	}
}

struct session **
sort_get_sessions(u_int *n, struct sort_criteria *sc)
{
	struct session	*s, **l = NULL;
	u_int		 i = 0;

	RB_FOREACH(s, sessions, &sessions) {
		l = xreallocarray(l, i + 1, sizeof *l);
		l[i++] = s;
	}

	if (sort_should_sort(sc)) {
		sort_criteria = sc;
		qsort(l, i, sizeof *l, sort_session_cmp);
	}
	*n = i;

	return l;
}

void sort_next_order(struct sort_criteria *sc)
{
	u_int	i;

	if (sc->ordering == NULL || sc->ordering->len == 1)
		return;

	for (i = 0; i < sc->ordering->len; i++) {
		if (sc->order == sc->ordering->data[i])
			break;
	}

	i++;
	if (i >= sc->ordering->len)
		i = 0;
	sc->order = sc->ordering->data[i];
}

static int
sort_should_sort(struct sort_criteria *sc)
{
	return sc != NULL && sc->order != SORT_END;
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
		/* FALLTHROUGH */
	case SORT_NAME:
		result = strcmp(sa->name, sb->name);
		break;
	default:
		fatalx("-%d unsupported sort order for session",
		    sort_criteria->order);
	}
	
	if (sort_criteria->reversed)
		result = -result;
	return (result);
}
