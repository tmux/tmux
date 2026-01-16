#include "tmux.h"
#include "string.h"
#include "strings.h"

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
    int i;

    if (sort == NULL)
        return SORT_NONE;

    for (i = 0; i < ARRAY_LEN(sort_list); i++) {
        if (strcasecmp(sort, sort_list[i]) == 0) {
            return (i);
        }
    }

    return (SORT_INVALID);
}

struct sort_criteria
sort_criteria_create(const char* order, int reversed)
{
    struct sort_criteria sc;

    sc.order = sort_order_from_string(order)
    sc.reversed = reversed;
    
    return (sc);
}

void 
sort_list(void **list, u_int len, struct sort_criteria sc, xcompar cmp)
{
    if (sc.order = SORT_NONE || sc_init.compar == NULL)
        return;

    if (sc.sort_order == SORT_INVALID) {
        log_debug("-%s invalid sort order", order);
        return;
    }

    sort_criteria = sc;
	qsort(list, len, sizeof *list, cmp);
    memset(&sort_criteria, 0, sizeof sort_criteria);
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
sort_list_sessions(void **list, u_int len, struct sort_criteria sc)
{
    sort_list(list, len, sc, session_list_cmp_session);
}
