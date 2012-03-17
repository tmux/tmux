/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Joshua Elsasser <josh@elsasser.org>
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

#include <string.h>

#include "tmux.h"

const char *
attributes_tostring(u_char attr)
{
	static char	buf[128];
	size_t		len;

	if (attr == 0)
		return ("none");

	len = xsnprintf(buf, sizeof buf, "%s%s%s%s%s%s%s",
		attr & GRID_ATTR_BRIGHT ? "bright," : "",
		attr & GRID_ATTR_DIM ? "dim," : "",
		attr & GRID_ATTR_UNDERSCORE ? "underscore," : "",
		attr & GRID_ATTR_BLINK ? "blink," : "",
		attr & GRID_ATTR_REVERSE ? "reverse," : "",
		attr & GRID_ATTR_HIDDEN ? "hidden," : "",
		attr & GRID_ATTR_ITALICS ? "italics," : "");
	if (len > 0)
		buf[len - 1] = '\0';

	return (buf);
}

int
attributes_fromstring(const char *str)
{
	const char	delimiters[] = " ,|";
	u_char		attr;
	size_t		end;

	if (*str == '\0' || strcspn(str, delimiters) == 0)
		return (-1);
	if (strchr(delimiters, str[strlen(str) - 1]) != NULL)
		return (-1);

	if (strcasecmp(str, "default") == 0 || strcasecmp(str, "none") == 0)
		return (0);

	attr = 0;
	do {
		end = strcspn(str, delimiters);
		if ((end == 6 && strncasecmp(str, "bright", end) == 0) ||
		    (end == 4 && strncasecmp(str, "bold", end) == 0))
			attr |= GRID_ATTR_BRIGHT;
		else if (end == 3 && strncasecmp(str, "dim", end) == 0)
			attr |= GRID_ATTR_DIM;
		else if (end == 10 && strncasecmp(str, "underscore", end) == 0)
			attr |= GRID_ATTR_UNDERSCORE;
		else if (end == 5 && strncasecmp(str, "blink", end) == 0)
			attr |= GRID_ATTR_BLINK;
		else if (end == 7 && strncasecmp(str, "reverse", end) == 0)
			attr |= GRID_ATTR_REVERSE;
		else if (end == 6 && strncasecmp(str, "hidden", end) == 0)
			attr |= GRID_ATTR_HIDDEN;
		else if (end == 7 && strncasecmp(str, "italics", end) == 0)
			attr |= GRID_ATTR_ITALICS;
		else
			return (-1);
		str += end + strspn(str + end, delimiters);
	} while (*str != '\0');

	return (attr);
}
