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
attributes_tostring(int attr)
{
	static char	buf[512];
	size_t		len;

	if (attr == 0)
		return ("none");

	len = xsnprintf(buf, sizeof buf, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
	    (attr & GRID_ATTR_CHARSET) ? "acs," : "",
	    (attr & GRID_ATTR_BRIGHT) ? "bright," : "",
	    (attr & GRID_ATTR_DIM) ? "dim," : "",
	    (attr & GRID_ATTR_UNDERSCORE) ? "underscore," : "",
	    (attr & GRID_ATTR_BLINK)? "blink," : "",
	    (attr & GRID_ATTR_REVERSE) ? "reverse," : "",
	    (attr & GRID_ATTR_HIDDEN) ? "hidden," : "",
	    (attr & GRID_ATTR_ITALICS) ? "italics," : "",
	    (attr & GRID_ATTR_STRIKETHROUGH) ? "strikethrough," : "",
	    (attr & GRID_ATTR_UNDERSCORE_2) ? "double-underscore," : "",
	    (attr & GRID_ATTR_UNDERSCORE_3) ? "curly-underscore," : "",
	    (attr & GRID_ATTR_UNDERSCORE_4) ? "dotted-underscore," : "",
	    (attr & GRID_ATTR_UNDERSCORE_5) ? "dashed-underscore," : "",
	    (attr & GRID_ATTR_OVERLINE) ? "overline," : "");
	if (len > 0)
		buf[len - 1] = '\0';

	return (buf);
}

int
attributes_fromstring(const char *str)
{
	const char	delimiters[] = " ,|";
	int		attr;
	size_t		end;
	u_int		i;
	struct {
		const char	*name;
		int		 attr;
	} table[] = {
		{ "acs", GRID_ATTR_CHARSET },
		{ "bright", GRID_ATTR_BRIGHT },
		{ "bold", GRID_ATTR_BRIGHT },
		{ "dim", GRID_ATTR_DIM },
		{ "underscore", GRID_ATTR_UNDERSCORE },
		{ "blink", GRID_ATTR_BLINK },
		{ "reverse", GRID_ATTR_REVERSE },
		{ "hidden", GRID_ATTR_HIDDEN },
		{ "italics", GRID_ATTR_ITALICS },
		{ "strikethrough", GRID_ATTR_STRIKETHROUGH },
		{ "double-underscore", GRID_ATTR_UNDERSCORE_2 },
		{ "curly-underscore", GRID_ATTR_UNDERSCORE_3 },
		{ "dotted-underscore", GRID_ATTR_UNDERSCORE_4 },
		{ "dashed-underscore", GRID_ATTR_UNDERSCORE_5 },
		{ "overline", GRID_ATTR_OVERLINE }
	};

	if (*str == '\0' || strcspn(str, delimiters) == 0)
		return (-1);
	if (strchr(delimiters, str[strlen(str) - 1]) != NULL)
		return (-1);

	if (strcasecmp(str, "default") == 0 || strcasecmp(str, "none") == 0)
		return (0);

	attr = 0;
	do {
		end = strcspn(str, delimiters);
		for (i = 0; i < nitems(table); i++) {
			if (end != strlen(table[i].name))
				continue;
			if (strncasecmp(str, table[i].name, end) == 0) {
				attr |= table[i].attr;
				break;
			}
		}
		if (i == nitems(table))
			return (-1);
		str += end + strspn(str + end, delimiters);
	} while (*str != '\0');

	return (attr);
}
