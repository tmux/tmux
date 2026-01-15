/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <time.h>

#include "tmux.h"

/*
 * List all sessions.
 */

#define LIST_SESSIONS_TEMPLATE				\
	"#{session_name}: #{session_windows} windows "	\
	"(created #{t:session_created})"		\
	"#{?session_grouped, (group ,}"			\
	"#{session_group}#{?session_grouped,),}"	\
	"#{?session_attached, (attached),}"

static enum cmd_retval	cmd_list_sessions_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_list_sessions_entry = {
	.name = "list-sessions",
	.alias = "ls",

	.args = { "F:f:", 0, 0, NULL },
	.usage = "[-F format] [-f filter]",

	.flags = CMD_AFTERHOOK,
	.exec = cmd_list_sessions_exec
};

enum list_sessions_sort_type {
    SESSION_BY_NAME,
    SESSION_BY_CREATION_TIME,
    SESSION_BY_ACIVITY_TIME,
};
static const char *list_session_sort_list[] = {
    "name",
    "creation",
    "activity",
};

struct list_session_sort_criteria {
    u_int mode;
    int reversed;
};
static struct list_session_sort_criteria list_session_sort_crit = {0};

static int
session_list_cmp_session(const void *a0, const void *b0)
{
    const struct session *const *a = a0;
    const struct session *const *b = b0;
    const struct session        *sa = *a;
    const struct session        *sb = *b;
    int result = 0;

    switch (list_session_sort_crit.field) {
        case SESSION_BY_CREATION_TIME:
            break;
        case SESSION_BY_ACIVITY_TIME:
            break;
        case SESSION_BY_NAME:
            result = strcmp(sa->name, sb->name);
    }

    if (list_session_sort_crit.reversed)
        result = -result;
    return (result);
}


static void
cmd_list_sessions_print(struct session *s, u_int n, struct cmdq_item* item,
        const char* template, const char* filter)
{
	struct format_tree	*ft;
	char		        *line, *expanded;
	int		            flag;

    ft = format_create(cmdq_get_client(item), item, FORMAT_NONE, 0);
    format_add(ft, "line", "%u", n);
    format_defaults(ft, NULL, s, NULL, NULL);

    if (filter != NULL) {
        expanded = format_expand(ft, filter);
        flag = format_true(expanded);
        free(expanded);
    } else
        flag = 1;
    if (flag) {
        line = format_expand(ft, template);
        cmdq_print(item, "%s", line);
        free(line);
    }

    format_free(ft);
}

static enum cmd_retval
cmd_list_sessions_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct session		*s, **l;
	u_int		 	 n;
	const char		*template, *filter;

	if ((template = args_get(args, 'F')) == NULL)
		template = LIST_SESSIONS_TEMPLATE;
	filter = args_get(args, 'f');

	sort = args_get(args, 'O');
	if (sort != NULL) {
		for (i = 0; i < sort_size; i++) {
			if (strcasecmp(sort, list_session_sort_list[i]) == 0)
				list_session_sort_crit.field = i;
		}
	}
	list_session_sort_crit.reversed = args_has(args, 'r');

    l = NULL;
	n = 0;
	RB_FOREACH(s, sessions, &sessions) {
		l = xreallocarray(l, n + 1, sizeof *l);
		l[n++] = s;
    }
	qsort(l, n, sizeof *l, session_list_cmp_session);

	for (i = 0; i < n; i++)
        cmd_list_sessions_print(l[i], i, item, template, filter);
	free(l);

	return (CMD_RETURN_NORMAL);
}
