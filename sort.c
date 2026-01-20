// TODO: dj

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

struct sort_criteria sort_criteria;

static struct sort_ordering default_sort_ordering = {
    .data = NULL,
    .len  = 0,
};

static enum sort_order
sort_order_from_string(const char* order)
{
    if (order == NULL) return (SORT_NONE);

    if (strcasecmp(order, "index") == 0)
        return SORT_INDEX;
    if (strcasecmp(order, "name") == 0)
        return SORT_NAME;
    if (strcasecmp(order, "order") == 0)
        return SORT_ORDER;
    if (strcasecmp(order, "size") == 0)
        return SORT_SIZE;
    if (strcasecmp(order, "creation") == 0)
        return SORT_CREATION;
    if (strcasecmp(order, "activity") == 0)
        return SORT_ACTIVITY;

    return (SORT_INVALID);
}

void sort_criteria_init(struct sort_criteria *sc, const char *orderstr, int reversed,
        xcompar cmp, struct sort_ordering *ordering)
{
    sc->order    = sort_order_from_string(orderstr);
    sc->reversed = reversed;
    sc->cmp = cmp;
    if (ordering == NULL) 
        ordering = &default_sort_ordering;
    sc->ordering = *ordering;
}

void
sort_run(void *list, u_int len, u_int size, struct sort_criteria *sc)
{
    if (sc->order == SORT_NONE || sc->order == SORT_INVALID  || sc->cmp == NULL)
        return;

    sort_criteria = *sc;
	qsort(list, len, size, sort_criteria.cmp);
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

    return "\0";
}

void sort_ordering_next(struct sort_criteria * sc)
{
    u_int i;

    if (sc->ordering.len == 1) return;

    for (i = 0; i < sc->ordering.len; i++) {
        if (sc->order == sc->ordering.data[i]) {
            break;
        }
    }

    i++;
    if (i >= sc->ordering.len) i = 0;
    sc->order = sc->ordering.data[i];
}

