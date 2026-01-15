#include "tmux.h"
#include "string.h"
#include "strings.h"

/*************
 * tmux.h
*/
#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(arr[0])) // helpful?
typedef int (*xcompar)(const void *, const void *);   // 

struct sort_criteria {
    u_int      order;
    int        reversed;
    xcompar    compar;
};

extern struct sort_criteria global_sort_crit = {0};

struct sort_criteria
sort_criteria_create(const char* order, int reversed, xcompar compar);

void 
sort_list(void **list, u_int len, struct sort_criteria sc);
/************/


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
sort_criteria_create(const char* order, int reversed, xcompar compar)
{
    struct sort_criteria sc;

    sc.order = sort_order_from_string(order)
    sc.reversed = reversed;
    sc.compar = compar;
    
    return (sc);
}

void 
sort_list(void **list, u_int len, struct sort_criteria sc)
{
    if (sc.order = SORT_NONE || sc_init.compar == NULL)
        return;

    if (sc.sort_order == SORT_INVALID) {
        log_debug("-%s invalid sort order", order);
        return;
    }

    global_sort_crit = sc;
	qsort(list, len, sizeof *list, global_sort_crit.compar);
    memset(&global_sort_crit, 0, sizeof global_sort_crit);
}
