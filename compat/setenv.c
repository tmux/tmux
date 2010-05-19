/* $Id: setenv.c,v 1.1 2010-05-19 21:31:39 nicm Exp $ */

/*
 * Copyright (c) 2010 Dagobert Michelsen
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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

int
setenv(const char *name, const char *value, unused int overwrite)
{
	char	buf[1024];

	snprintf(buf, sizeof(buf), "%s=%s", name, value);
	return (putenv(buf));
}

int
unsetenv(const char *name)
{
	char  **envptr;
	int	namelen;

	namelen = strlen(name);
	for (envptr = environ; *envptr != NULL; envptr++) {
		if (strncmp(name, *envptr, namelen) == 0 &&
		    ((*envptr)[namelen] == '=' || (*envptr)[namelen] == '\0'))
			break;
	}
	for (; *envptr != NULL; envptr++)
		*envptr = *(envptr + 1);
	return (0);
}
