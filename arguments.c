/* $OpenBSD$ */

/*
 * Copyright (c) 2010 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <unistd.h>

#include "tmux.h"

/*
 * Manipulate command arguments.
 */

struct arg_value {
	char			*value;
	TAILQ_ENTRY(arg_value)	 entry;
};
TAILQ_HEAD(arg_values, arg_value);

struct args_entry {
	u_char			 flag;
	struct arg_values	 values;
	RB_ENTRY(args_entry)	 entry;
};

static struct args_entry	*args_find(struct args *, u_char);

static int	args_cmp(struct args_entry *, struct args_entry *);
RB_GENERATE_STATIC(args_tree, args_entry, entry, args_cmp);

/* Arguments tree comparison function. */
static int
args_cmp(struct args_entry *a1, struct args_entry *a2)
{
	return (a1->flag - a2->flag);
}

/* Find a flag in the arguments tree. */
static struct args_entry *
args_find(struct args *args, u_char ch)
{
	struct args_entry	entry;

	entry.flag = ch;
	return (RB_FIND(args_tree, &args->tree, &entry));
}

/* Parse an argv and argc into a new argument set. */
struct args *
args_parse(const char *template, int argc, char **argv)
{
	struct args	*args;
	int		 opt;

	args = xcalloc(1, sizeof *args);

	optreset = 1;
	optind = 1;

	while ((opt = getopt(argc, argv, template)) != -1) {
		if (opt < 0)
			continue;
		if (opt == '?' || strchr(template, opt) == NULL) {
			args_free(args);
			return (NULL);
		}

		args_set(args, opt, optarg);
	}
	argc -= optind;
	argv += optind;

	args->argc = argc;
	args->argv = cmd_copy_argv(argc, argv);

	return (args);
}

/* Free an arguments set. */
void
args_free(struct args *args)
{
	struct args_entry	*entry;
	struct args_entry	*entry1;
	struct arg_value	*head;

	cmd_free_argv(args->argc, args->argv);

	RB_FOREACH_SAFE(entry, args_tree, &args->tree, entry1) {
		RB_REMOVE(args_tree, &args->tree, entry);
		while (!TAILQ_EMPTY(&entry->values)) {
			head = TAILQ_FIRST(&entry->values);
			TAILQ_REMOVE(&entry->values, head, entry);
			free(head->value);
			free(head);
		}
		free(entry);
	}

	free(args);
}

/* Add to string. */
static void printflike(3, 4)
args_print_add(char **buf, size_t *len, const char *fmt, ...)
{
	va_list  ap;
	char	*s;
	size_t	 slen;

	va_start(ap, fmt);
	slen = xvasprintf(&s, fmt, ap);
	va_end(ap);

	*len += slen;
	*buf = xrealloc(*buf, *len);

	strlcat(*buf, s, *len);
	free(s);
}

/* Print a set of arguments. */
char *
args_print(struct args *args)
{
	size_t		 	 len;
	char			*buf, *escaped;
	int			 i, flags;
	struct args_entry	*entry;
	static const char	 quoted[] = " #\"';$";
	char			*value;

	len = 1;
	buf = xcalloc(1, len);

	/* Process the flags first. */
	RB_FOREACH(entry, args_tree, &args->tree) {
		if (TAILQ_EMPTY(&entry->values))
			continue;

		if (*buf == '\0')
			args_print_add(&buf, &len, "-");
		args_print_add(&buf, &len, "%c", entry->flag);
	}

	/* Then the flags with arguments. */
	RB_FOREACH(entry, args_tree, &args->tree) {
		if (!TAILQ_EMPTY(&entry->values))
			continue;

		if (*buf != '\0')
			args_print_add(&buf, &len, " -%c ", entry->flag);
		else
			args_print_add(&buf, &len, "-%c ", entry->flag);

		value = TAILQ_LAST(&entry->values, arg_values)->value;
		flags = VIS_OCTAL|VIS_TAB|VIS_NL;
		if (value[strcspn(value, quoted)] != '\0')
			flags |= VIS_DQ;
		utf8_stravis(&escaped, value, flags);
		if (flags & VIS_DQ)
			args_print_add(&buf, &len, "\"%s\"", escaped);
		else
			args_print_add(&buf, &len, "%s", escaped);
		free(escaped);
	}

	/* And finally the argument vector. */
	for (i = 0; i < args->argc; i++) {
		if (*buf != '\0')
			args_print_add(&buf, &len, " ");

		flags = VIS_OCTAL|VIS_TAB|VIS_NL;
		if (args->argv[i][strcspn(args->argv[i], quoted)] != '\0')
			flags |= VIS_DQ;
		utf8_stravis(&escaped, args->argv[i], flags);
		if (flags & VIS_DQ)
			args_print_add(&buf, &len, "\"%s\"", escaped);
		else
			args_print_add(&buf, &len, "%s", escaped);
		free(escaped);
	}

	return (buf);
}

/* Return if an argument is present. */
int
args_has(struct args *args, u_char ch)
{
	return (args_find(args, ch) != NULL);
}

/* Set argument value in the arguments tree. */
void
args_set(struct args *args, u_char ch, const char *value)
{
	struct args_entry	*entry;
	struct arg_value	*value_entry;

	entry = args_find(args, ch);
	if (entry == NULL) {
		entry = xcalloc(1, sizeof *entry);
		entry->flag = ch;
		TAILQ_INIT(&entry->values);
		RB_INSERT(args_tree, &args->tree, entry);
	}

	if (value != NULL) {
		value_entry = xcalloc(1, sizeof *value_entry);
		value_entry->value = xstrdup(value);
		TAILQ_INSERT_TAIL(&entry->values, value_entry, entry);
	}
}

/* Get argument value. Will be NULL if it isn't present. */
const char *
args_get(struct args *args, u_char ch)
{
	struct args_entry	*entry;

	if ((entry = args_find(args, ch)) == NULL)
		return (NULL);
	return (TAILQ_LAST(&entry->values, arg_values)->value);
}

/* get opaque pointer to the first value of a flag, or NULL if flag not present */
void *
args_find_first_value(struct args *args, u_char ch, char **value)
{
	struct args_entry	*entry;
	struct arg_value	*value_entry;

	if ((entry = args_find(args, ch)) == NULL) {
		*value = NULL;
		return (NULL);
	}

	value_entry = TAILQ_FIRST(&entry->values);
	*value = value_entry->value;

	return (value_entry);
}

/* get next value from a previous find_first/next call */
void *
args_find_next_value(void *current, char **value)
{
	struct arg_value	*item;

	if (current == NULL) {
		*value = NULL;
		return NULL;
	}

	if ((item = TAILQ_NEXT((struct arg_value*) current, entry)) == NULL) {
		*value = NULL;
		return NULL;
	}

	*value = item->value;
	return (item);
}


/* Convert an argument value to a number. */
long long
args_strtonum(struct args *args, u_char ch, long long minval, long long maxval,
    char **cause)
{
	const char		*errstr;
	long long 	 	 ll;
	struct args_entry	*entry;

	if ((entry = args_find(args, ch)) == NULL) {
		*cause = xstrdup("missing");
		return (0);
	}

	ll = strtonum(TAILQ_LAST(&entry->values, arg_values)->value, minval, maxval, &errstr);
	if (errstr != NULL) {
		*cause = xstrdup(errstr);
		return (0);
	}

	*cause = NULL;
	return (ll);
}
