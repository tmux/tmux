// TODO:

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

static struct sort_criteria sort_criteria = {0};

enum sort_order {
    SORT_NONE,
    SORT_NAME,
    SORT_CREATION_TIME,
    SORT_ACIVITY_TIME,
    SORT_INVALID,
};

static const char *sort_list[] = {
    "",
    "name",
    "creation",
    "activity",
};


static enum sort_order
sort_order_from_string(const char* sort)
{
    u_int i;

    for (i = 0; i < nitems(sort_list); i++) {
        if (strcasecmp(sort, sort_list[i]) == 0) {
            return (i);
        }
    }

    return (SORT_INVALID);
}

void
sort_criteria_init(struct sort_criteria* sc, const char* order, int reversed)
{
    sc->order = sort_order_from_string(order);
    sc->reversed = reversed;
}

static void 
xsort(void **list, u_int len, struct sort_criteria sc, xcompar cmp)
{
    if (sc.order == SORT_NONE)
        return;

    if (sc.order == SORT_INVALID) {
        log_debug("-%u invalid sort order", sc.order);
        return;
    }

    sort_criteria = sc;
	qsort(list, len, sizeof *list, cmp);
}

static int
session_list_cmp_session(const void *a0, const void *b0)
{
    const struct session *const *a = a0;
    const struct session *const *b = b0;
    const struct session        *sa = *a;
    const struct session        *sb = *b;
    int result = 0;

    switch (sort_criteria.order) {
        case SORT_CREATION_TIME:
            //
        case SORT_ACIVITY_TIME:
            //
        case SORT_NAME:
            result = strcmp(sa->name, sb->name);
            break;
        default:
            log_debug("unsupported_sort_order");
    }

    if (sort_criteria.reversed)
        result = -result;
    return (result);
}

void
sort_list_sessions(struct session **list, u_int len, struct sort_criteria sc)
{
    xsort((void**)list, len, sc, session_list_cmp_session);
}
