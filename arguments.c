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
#include <vis.h>

#include "tmux.h"

/*
 * Manipulate command arguments.
 */

struct args_value {
	char			*value;
	TAILQ_ENTRY(args_value)	 entry;
};
TAILQ_HEAD(args_values, args_value);

struct args_entry {
	u_char			 flag;
	struct args_values	 values;
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
	struct args_value	*value;
	struct args_value	*value1;

	cmd_free_argv(args->argc, args->argv);

	RB_FOREACH_SAFE(entry, args_tree, &args->tree, entry1) {
		RB_REMOVE(args_tree, &args->tree, entry);
		TAILQ_FOREACH_SAFE(value, &entry->values, entry, value1) {
			TAILQ_REMOVE(&entry->values, value, entry);
			free(value->value);
			free(value);
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

/* Add value to string. */
static void
args_print_add_value(char **buf, size_t *len, struct args_entry *entry,
    struct args_value *value)
{
	static const char	 quoted[] = " #\"';$";
	char			*escaped;
	int			 flags;

	if (**buf != '\0')
		args_print_add(buf, len, " -%c ", entry->flag);
	else
		args_print_add(buf, len, "-%c ", entry->flag);

	flags = VIS_OCTAL|VIS_TAB|VIS_NL;
	if (value->value[strcspn(value->value, quoted)] != '\0')
		flags |= VIS_DQ;
	utf8_stravis(&escaped, value->value, flags);
	if (flags & VIS_DQ)
		args_print_add(buf, len, "\"%s\"", escaped);
	else
		args_print_add(buf, len, "%s", escaped);
	free(escaped);
}

/* Add argument to string. */
static void
args_print_add_argument(char **buf, size_t *len, const char *argument)
{
	static const char	 quoted[] = " #\"';$";
	char			*escaped;
	int			 flags;

	if (**buf != '\0')
		args_print_add(buf, len, " ");

	flags = VIS_OCTAL|VIS_TAB|VIS_NL;
	if (argument[strcspn(argument, quoted)] != '\0')
		flags |= VIS_DQ;
	utf8_stravis(&escaped, argument, flags);
	if (flags & VIS_DQ)
		args_print_add(buf, len, "\"%s\"", escaped);
	else
		args_print_add(buf, len, "%s", escaped);
	free(escaped);
}

/* Print a set of arguments. */
char *
args_print(struct args *args)
{
	size_t		 	 len;
	char			*buf;
	int			 i;
	struct args_entry	*entry;
	struct args_value	*value;

	len = 1;
	buf = xcalloc(1, len);

	/* Process the flags first. */
	RB_FOREACH(entry, args_tree, &args->tree) {
		if (!TAILQ_EMPTY(&entry->values))
			continue;

		if (*buf == '\0')
			args_print_add(&buf, &len, "-");
		args_print_add(&buf, &len, "%c", entry->flag);
	}

	/* Then the flags with arguments. */
	RB_FOREACH(entry, args_tree, &args->tree) {
		TAILQ_FOREACH(value, &entry->values, entry)
			args_print_add_value(&buf, &len, entry, value);
	}

	/* And finally the argument vector. */
	for (i = 0; i < args->argc; i++)
		args_print_add_argument(&buf, &len, args->argv[i]);

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
args_set(struct args *args, u_char ch, const char *s)
{
	struct args_entry	*entry;
	struct args_value	*value;

	entry = args_find(args, ch);
	if (entry == NULL) {
		entry = xcalloc(1, sizeof *entry);
		entry->flag = ch;
		TAILQ_INIT(&entry->values);
		RB_INSERT(args_tree, &args->tree, entry);
	}

	if (s != NULL) {
		value = xcalloc(1, sizeof *value);
		value->value = xstrdup(s);
		TAILQ_INSERT_TAIL(&entry->values, value, entry);
	}
}

/* Get argument value. Will be NULL if it isn't present. */
const char *
args_get(struct args *args, u_char ch)
{
	struct args_entry	*entry;

	if ((entry = args_find(args, ch)) == NULL)
		return (NULL);
	return (TAILQ_LAST(&entry->values, args_values)->value);
}

/* Get first value in argument. */
const char *
args_first_value(struct args *args, u_char ch, struct args_value **value)
{
	struct args_entry	*entry;

	if ((entry = args_find(args, ch)) == NULL)
		return (NULL);

	*value = TAILQ_FIRST(&entry->values);
	if (*value == NULL)
		return (NULL);
	return ((*value)->value);
}

/* Get next value in argument. */
const char *
args_next_value(struct args_value **value)
{
	if (*value == NULL)
		return (NULL);
	*value = TAILQ_NEXT(*value, entry);
	if (*value == NULL)
		return (NULL);
	return ((*value)->value);
}

/* Convert an argument value to a number. */
long long
args_strtonum(struct args *args, u_char ch, long long minval, long long maxval,
    char **cause)
{
	const char		*errstr;
	long long 	 	 ll;
	struct args_entry	*entry;
	struct args_value	*value;

	if ((entry = args_find(args, ch)) == NULL) {
		*cause = xstrdup("missing");
		return (0);
	}
	value = TAILQ_LAST(&entry->values, args_values);

	ll = strtonum(value->value, minval, maxval, &errstr);
	if (errstr != NULL) {
		*cause = xstrdup(errstr);
		return (0);
	}

	*cause = NULL;
	return (ll);
}
