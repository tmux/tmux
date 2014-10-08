/* $OpenBSD$ */

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
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Parse a command from a string.
 */

int	 cmd_string_getc(const char *, size_t *);
void	 cmd_string_ungetc(size_t *);
void	 cmd_string_copy(char **, char *, size_t *);
char	*cmd_string_string(const char *, size_t *, char, int);
char	*cmd_string_variable(const char *, size_t *);
char	*cmd_string_expand_tilde(const char *, size_t *);

int
cmd_string_getc(const char *s, size_t *p)
{
	const u_char	*ucs = s;

	if (ucs[*p] == '\0')
		return (EOF);
	return (ucs[(*p)++]);
}

void
cmd_string_ungetc(size_t *p)
{
	(*p)--;
}

/*
 * Parse command string. Returns -1 on error. If returning -1, cause is error
 * string, or NULL for empty command.
 */
int
cmd_string_parse(const char *s, struct cmd_list **cmdlist, const char *file,
    u_int line, char **cause)
{
	size_t		p;
	int		ch, i, argc, rval;
	char	      **argv, *buf, *t;
	const char     *whitespace, *equals;
	size_t		len;

	argv = NULL;
	argc = 0;

	buf = NULL;
	len = 0;

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
			cmd_string_copy(&buf, t, &len);
			break;
		case '"':
			if ((t = cmd_string_string(s, &p, '"', 1)) == NULL)
				goto error;
			cmd_string_copy(&buf, t, &len);
			break;
		case '$':
			if ((t = cmd_string_variable(s, &p)) == NULL)
				goto error;
			cmd_string_copy(&buf, t, &len);
			break;
		case '#':
			/* Comment: discard rest of line. */
			while ((ch = cmd_string_getc(s, &p)) != EOF)
				;
			/* FALLTHROUGH */
		case EOF:
		case ' ':
		case '\t':
			if (buf != NULL) {
				buf = xrealloc(buf, len + 1);
				buf[len] = '\0';

				argv = xreallocarray(argv, argc + 1,
				    sizeof *argv);
				argv[argc++] = buf;

				buf = NULL;
				len = 0;
			}

			if (ch != EOF)
				break;

			while (argc != 0) {
				equals = strchr(argv[0], '=');
				whitespace = argv[0] + strcspn(argv[0], " \t");
				if (equals == NULL || equals > whitespace)
					break;
				environ_put(&global_environ, argv[0]);
				argc--;
				memmove(argv, argv + 1, argc * (sizeof *argv));
			}
			if (argc == 0)
				goto out;

			*cmdlist = cmd_list_parse(argc, argv, file, line, cause);
			if (*cmdlist == NULL)
				goto out;

			rval = 0;
			goto out;
		case '~':
			if (buf == NULL) {
				t = cmd_string_expand_tilde(s, &p);
				if (t == NULL)
					goto error;
				cmd_string_copy(&buf, t, &len);
				break;
			}
			/* FALLTHROUGH */
		default:
			if (len >= SIZE_MAX - 2)
				goto error;

			buf = xrealloc(buf, len + 1);
			buf[len++] = ch;
			break;
		}
	}

error:
	xasprintf(cause, "invalid or unknown command: %s", s);

out:
	free(buf);

	if (argv != NULL) {
		for (i = 0; i < argc; i++)
			free(argv[i]);
		free(argv);
	}

	return (rval);
}

void
cmd_string_copy(char **dst, char *src, size_t *len)
{
	size_t srclen;

	srclen = strlen(src);

	*dst = xrealloc(*dst, *len + srclen + 1);
	strlcpy(*dst + *len, src, srclen + 1);

	*len += srclen;
	free(src);
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
			case 'e':
				ch = '\033';
				break;
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
			cmd_string_copy(&buf, t, &len);
			continue;
		}

		if (len >= SIZE_MAX - 2)
			goto error;
		buf = xrealloc(buf, len + 1);
		buf[len++] = ch;
	}

	buf = xrealloc(buf, len + 1);
	buf[len] = '\0';
	return (buf);

error:
	free(buf);
	return (NULL);
}

char *
cmd_string_variable(const char *s, size_t *p)
{
	int			ch, fch;
	char		       *buf, *t;
	size_t			len;
	struct environ_entry   *envent;

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

		buf = xrealloc(buf, len + 1);
		buf[len++] = ch;

		for (;;) {
			ch = cmd_string_getc(s, p);
			if (ch == EOF || !cmd_string_other(ch))
				break;
			else {
				if (len >= SIZE_MAX - 3)
					goto error;
				buf = xrealloc(buf, len + 1);
				buf[len++] = ch;
			}
		}
	}

	if (fch == '{' && ch != '}')
		goto error;
	if (ch != EOF && fch != '{')
		cmd_string_ungetc(p); /* ch */

	buf = xrealloc(buf, len + 1);
	buf[len] = '\0';

	envent = environ_find(&global_environ, buf);
	free(buf);
	if (envent == NULL)
		return (xstrdup(""));
	return (xstrdup(envent->value));

error:
	free(buf);
	return (NULL);
}

char *
cmd_string_expand_tilde(const char *s, size_t *p)
{
	struct passwd		*pw;
	struct environ_entry	*envent;
	char			*home, *path, *user, *cp;
	int			 last;

	home = NULL;

	last = cmd_string_getc(s, p);
	if (last == EOF || last == '/' || last == ' '|| last == '\t') {
		envent = environ_find(&global_environ, "HOME");
		if (envent != NULL && *envent->value != '\0')
			home = envent->value;
		else if ((pw = getpwuid(getuid())) != NULL)
			home = pw->pw_dir;
	} else {
		cmd_string_ungetc(p);

		cp = user = xmalloc(strlen(s));
		for (;;) {
			last = cmd_string_getc(s, p);
			if (last == EOF || last == '/' || last == ' '|| last == '\t')
				break;
			*cp++ = last;
		}
		*cp = '\0';

		if ((pw = getpwnam(user)) != NULL)
			home = pw->pw_dir;
		free(user);
	}

	if (home == NULL)
		return (NULL);

	if (last != EOF)
		xasprintf(&path, "%s%c", home, last);
	else
		xasprintf(&path, "%s", home);
	return (path);
}
