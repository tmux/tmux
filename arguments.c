/* $Id$ */

/*
 * Copyright (c) 2010 Nicholas Marriott <nicm@users.sourceforge.net>
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

#include "tmux.h"

/* Create an arguments set with no flags. */
struct args *
args_create(int argc, ...)
{
	struct args	*args;
	va_list		 ap;
	int		 i;

	args = xcalloc(1, sizeof *args);
	if ((args->flags = bit_alloc(SCHAR_MAX)) == NULL)
		fatal("bit_alloc failed");

	args->argc = argc;
	if (argc == 0)
		args->argv = NULL;
	else
		args->argv = xcalloc(argc, sizeof *args->argv);

	va_start(ap, argc);
	for (i = 0; i < argc; i++)
		args->argv[i] = xstrdup(va_arg(ap, char *));
	va_end(ap);

	return (args);
}

/* Parse an argv and argc into a new argument set. */
struct args *
args_parse(const char *template, int argc, char **argv)
{
	struct args	*args;
	char		*ptr;
	int		 opt;

	args = xcalloc(1, sizeof *args);
	if ((args->flags = bit_alloc(SCHAR_MAX)) == NULL)
		fatal("bit_alloc failed");

	optreset = 1;
	optind = 1;

	while ((opt = getopt(argc, argv, template)) != -1) {
		if (opt < 0 || opt >= SCHAR_MAX)
			continue;
		if (opt == '?' || (ptr = strchr(template, opt)) == NULL) {
			xfree(args->flags);
			xfree(args);
			return (NULL);
		}

		bit_set(args->flags, opt);
		if (ptr[1] == ':') {
			if (args->values[opt] != NULL)
				xfree(args->values[opt]);
			args->values[opt] = xstrdup(optarg);
		}
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
	u_int	i;

	cmd_free_argv(args->argc, args->argv);

	for (i = 0; i < SCHAR_MAX; i++) {
		if (args->values[i] != NULL)
			xfree(args->values[i]);
	}

	xfree(args->flags);
	xfree(args);
}

/* Print a set of arguments. */
size_t
args_print(struct args *args, char *buf, size_t len)
{
	size_t		 off;
	int		 i;
	const char	*quotes;

	/* There must be at least one byte at the start. */
	if (len == 0)
		return (0);
	off = 0;

	/* Process the flags first. */
	buf[off++] = '-';
	for (i = 0; i < SCHAR_MAX; i++) {
		if (!bit_test(args->flags, i) || args->values[i] != NULL)
			continue;

		if (off == len - 1) {
			buf[off] = '\0';
			return (len);
		}
		buf[off++] = i;
		buf[off] = '\0';
	}
	if (off == 1)
		buf[--off] = '\0';

	/* Then the flags with arguments. */
	for (i = 0; i < SCHAR_MAX; i++) {
		if (!bit_test(args->flags, i) || args->values[i] == NULL)
			continue;

		if (off >= len) {
			/* snprintf will have zero terminated. */
			return (len);
		}

		if (strchr(args->values[i], ' ') != NULL)
			quotes = "\"";
		else
			quotes = "";
		off += xsnprintf(buf + off, len - off, "%s-%c %s%s%s",
		    off != 0 ? " " : "", i, quotes, args->values[i], quotes);
	}

	/* And finally the argument vector. */
	for (i = 0; i < args->argc; i++) {
		if (off >= len) {
			/* snprintf will have zero terminated. */
			return (len);
		}

		if (strchr(args->argv[i], ' ') != NULL)
			quotes = "\"";
		else
			quotes = "";
		off += xsnprintf(buf + off, len - off, "%s%s%s%s",
		    off != 0 ? " " : "", quotes, args->argv[i], quotes);
	}

	return (off);
}

/* Return if an argument is present. */
int
args_has(struct args *args, u_char ch)
{
	return (bit_test(args->flags, ch));
}

/* Set argument value. */
void
args_set(struct args *args, u_char ch, const char *value)
{
	if (args->values[ch] != NULL)
		xfree(args->values[ch]);
	if (value != NULL)
		args->values[ch] = xstrdup(value);
	else
		args->values[ch] = NULL;
	bit_set(args->flags, ch);
}

/* Get argument value. Will be NULL if it isn't present. */
const char *
args_get(struct args *args, u_char ch)
{
	return (args->values[ch]);
}

/* Convert an argument value to a number. */
long long
args_strtonum(struct args *args,
    u_char ch, long long minval, long long maxval, char **cause)
{
	const char	*errstr;
	long long 	 ll;

	if (!args_has(args, ch)) {
		*cause = xstrdup("missing");
		return (0);
	}

	ll = strtonum(args->values[ch], minval, maxval, &errstr);
	if (errstr != NULL) {
		*cause = xstrdup(errstr);
		return (0);
	}

	*cause = NULL;
	return (ll);
}
