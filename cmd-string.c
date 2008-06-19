/* $Id: cmd-string.c,v 1.2 2008-06-19 21:13:56 nicm Exp $ */

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

#include "tmux.h"

/*
 * Parse a command from a string.
 */

int	cmd_string_getc(const char *, size_t *);
char   *cmd_string_string(const char *, size_t *, char, int);

int
cmd_string_getc(const char *s, size_t *p)
{
	if (s[*p] == '\0')
		return (EOF);
	return (s[(*p)++]);
}

struct cmd *
cmd_string_parse(const char *s, char **cause)
{
	size_t	p;
	int		ch, argc;
	char	      **argv, *buf, *t;
	size_t		len;
	struct cmd     *cmd;

	argv = NULL;
	argc = 0;

	buf = NULL;
	len = 0;

	cmd = NULL;

	p = 0;
	for (;;) {
		ch = cmd_string_getc(s, &p);
		switch (ch) {
		case '\'':
			if ((t = cmd_string_string(s, &p, '\'', 0)) == NULL)
				goto error;
			argv = xrealloc(argv, argc + 1, sizeof *argv);
			argv[argc++] = t;
			break;
		case '"':
			if ((t = cmd_string_string(s, &p, '"', 1)) == NULL)
				goto error;
			argv = xrealloc(argv, argc + 1, sizeof *argv);
			argv[argc++] = t;
			break;
		case '#':
			/* Comment: discard rest of line. */
			while ((ch = cmd_string_getc(s, &p)) != EOF)
				;
			/* FALLTHROUGH */
		case EOF:
		case ' ':
		case '\t':
 			if (len != 0) {
				buf = xrealloc(buf, 1, len + 1);
				buf[len] = '\0';

				argv = xrealloc(argv, argc + 1, sizeof *argv);
				argv[argc++] = buf;

				buf = NULL;
				len = 0;
			}

			if (ch != EOF)
				break;
			if (argc == 0)
				goto error;
				
			cmd = cmd_parse(argc, argv, cause);
			goto out;
		default:
			if (len >= SIZE_MAX - 2)
				goto error;

			buf = xrealloc(buf, 1, len + 1);
			buf[len++] = ch;
			break;
		}
	}
	
error:
	xasprintf(cause, "bad command: %s", s);
	
out:
	if (buf != NULL)
		xfree(buf);

	while (--argc >= 0)
		xfree(argv[argc]);
	if (argv != NULL)
		xfree(argv);

	return (cmd);
}

char *
cmd_string_string(const char *s, size_t *p, char endch, int esc)
{
	int	ch;
	char   *buf;
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
