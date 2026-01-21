// TODO: dj

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

struct sort_criteria *sort_criteria;

enum sort_order
sort_order_from_string(const char* order)
{
	if (order == NULL)
		return (SORT_NONE);

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

	return (SORT_INVALID);
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
	if (sc->order == SORT_NONE || sc->order == SORT_INVALID
	    || sc->cmp == NULL)
		return;

	sort_criteria = sc;
	qsort(list, len, size, sort_criteria->cmp);
}


void sort_ordering_next(struct sort_criteria *sc)
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

