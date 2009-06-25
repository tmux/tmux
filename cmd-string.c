/* $OpenBSD: cmd-string.c,v 1.2 2009/06/05 07:18:37 nicm Exp $ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicm@users.sourceforge.net>
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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "tmux.h"

/*
 * Parse a command from a string.
 */

int	cmd_string_getc(const char *, size_t *);
void	cmd_string_ungetc(const char *, size_t *);
char   *cmd_string_string(const char *, size_t *, char, int);
char   *cmd_string_variable(const char *, size_t *);

int
cmd_string_getc(const char *s, size_t *p)
{
	if (s[*p] == '\0')
		return (EOF);
	return (s[(*p)++]);
}

void
cmd_string_ungetc(unused const char *s, size_t *p)
{
	(*p)--;
}

/*
 * Parse command string. Returns -1 on error. If returning -1, cause is error
 * string, or NULL for empty command.
 */
int
cmd_string_parse(const char *s, struct cmd_list **cmdlist, char **cause)
{
	size_t		p;
	int		ch, argc, rval, have_arg;
	char	      **argv, *buf, *t, *u;
	size_t		len;

	if ((t = strchr(s, ' ')) == NULL && (t = strchr(s, '\t')) == NULL)
		t = strchr(s, '\0');
	if ((u = strchr(s, '=')) != NULL && u < t) {
		if (putenv(xstrdup(s)) != 0) {
			xasprintf(cause, "assignment failed: %s", s);
			return (-1);
		}
		*cmdlist = NULL;
		return (0);
	}

	argv = NULL;
	argc = 0;

	buf = NULL;
	len = 0;

	have_arg = 0;

	*cause = NULL;

	*cmdlist = NULL;
	rval = -1;

	p = 0;
	for (;;) {
		ch = cmd_string_getc(s, &p);
		switch (ch) {
		case '\'':
			if ((t = cmd_string_string(s, &p, '\'', 0)) == NULL)
				goto error;
			buf = xrealloc(buf, 1, len + strlen(t) + 1);
			strlcpy(buf + len, t, strlen(t) + 1);
			len += strlen(t);
			xfree(t);

			have_arg = 1;
			break;
		case '"':
			if ((t = cmd_string_string(s, &p, '"', 1)) == NULL)
				goto error;
			buf = xrealloc(buf, 1, len + strlen(t) + 1);
			strlcpy(buf + len, t, strlen(t) + 1);
			len += strlen(t);
			xfree(t);

			have_arg = 1;
			break;
		case '$':
			if ((t = cmd_string_variable(s, &p)) == NULL)
				goto error;
			buf = xrealloc(buf, 1, len + strlen(t) + 1);
			strlcpy(buf + len, t, strlen(t) + 1);
			len += strlen(t);

			have_arg = 1;
			break;
		case '#':
			/* Comment: discard rest of line. */
			while ((ch = cmd_string_getc(s, &p)) != EOF)
				;
			/* FALLTHROUGH */
		case EOF:
		case ' ':
		case '\t':
 			if (have_arg) {
				buf = xrealloc(buf, 1, len + 1);
				buf[len] = '\0';

				argv = xrealloc(argv, argc + 1, sizeof *argv);
				argv[argc++] = buf;

				buf = NULL;
				len = 0;

				have_arg = 0;
			}

			if (ch != EOF)
				break;
			if (argc == 0)
				goto out;

			*cmdlist = cmd_list_parse(argc, argv, cause);
			if (*cmdlist == NULL)
				goto out;

			do
				xfree(argv[argc - 1]);
			while (--argc > 0);

			rval = 0;
			goto out;
		default:
			if (len >= SIZE_MAX - 2)
				goto error;

			buf = xrealloc(buf, 1, len + 1);
			buf[len++] = ch;

			have_arg = 1;
			break;
		}
	}

error:
	xasprintf(cause, "invalid or unknown command: %s", s);

out:
	if (buf != NULL)
		xfree(buf);

	while (--argc >= 0)
		xfree(argv[argc]);
	if (argv != NULL)
		xfree(argv);

	return (rval);
}

char *
cmd_string_string(const char *s, size_t *p, char endch, int esc)
{
	int	ch;
	char   *buf, *t;
	size_t	len;

        buf = NULL;
	len = 0;

        while ((ch = cmd_string_getc(s, p)) != endch) {
                switch (ch) {
		case EOF:
			goto error;
                case '\\':
			if (!esc)
				break;
                        switch (ch = cmd_string_getc(s, p)) {
			case EOF:
				goto error;
                        case 'r':
                                ch = '\r';
                                break;
                        case 'n':
                                ch = '\n';
                                break;
                        case 't':
                                ch = '\t';
                                break;
                        }
                        break;
		case '$':
			if (!esc)
				break;
			if ((t = cmd_string_variable(s, p)) == NULL)
				goto error;
			buf = xrealloc(buf, 1, len + strlen(t) + 1);
			strlcpy(buf + len, t, strlen(t) + 1);
			len += strlen(t);
			continue;
                }

		if (len >= SIZE_MAX - 2)
			goto error;
		buf = xrealloc(buf, 1, len + 1);
                buf[len++] = ch;
        }

	buf = xrealloc(buf, 1, len + 1);
	buf[len] = '\0';
	return (buf);

error:
	if (buf != NULL)
		xfree(buf);
	return (NULL);
}

char *
cmd_string_variable(const char *s, size_t *p)
{
	int	ch, fch;
	char   *buf, *t;
	size_t	len;

#define cmd_string_first(ch) ((ch) == '_' || \
	((ch) >= 'a' && (ch) <= 'z') || ((ch) >= 'A' && (ch) <= 'Z'))
#define cmd_string_other(ch) ((ch) == '_' || \
	((ch) >= 'a' && (ch) <= 'z') || ((ch) >= 'A' && (ch) <= 'Z') || \
	((ch) >= '0' && (ch) <= '9'))

        buf = NULL;
	len = 0;

	fch = EOF;
	switch (ch = cmd_string_getc(s, p)) {
	case EOF:
		goto error;
	case '{':
		fch = '{';

		ch = cmd_string_getc(s, p);
		if (!cmd_string_first(ch))
			goto error;
		/* FALLTHROUGH */
	default:
		if (!cmd_string_first(ch)) {
			xasprintf(&t, "$%c", ch);
			return (t);
		}

		buf = xrealloc(buf, 1, len + 1);
		buf[len++] = ch;

		for (;;) {
			ch = cmd_string_getc(s, p);
			if (ch == EOF || !cmd_string_other(ch))
				break;
			else {
				if (len >= SIZE_MAX - 3)
					goto error;
				buf = xrealloc(buf, 1, len + 1);
				buf[len++] = ch;
			}
		}
	}

	if (fch == '{' && ch != '}')
		goto error;
	if (ch != EOF && fch != '{')
		cmd_string_ungetc(s, p); /* ch */

	buf = xrealloc(buf, 1, len + 1);
	buf[len] = '\0';

	if ((t = getenv(buf)) == NULL) {
		xfree(buf);
		return (xstrdup(""));
	}
	xfree(buf);
	return (xstrdup(t));

error:
	if (buf != NULL)
		xfree(buf);
	return (NULL);
}
