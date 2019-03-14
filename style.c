/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
 * Copyright (c) 2014 Tiago Cunha <tcunha@users.sourceforge.net>
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

/* Mask for bits not included in style. */
#define STYLE_ATTR_MASK (~GRID_ATTR_CHARSET)

/* Default style. */
static struct style style_default = {
	{ 0, 0, 8, 8, { { ' ' }, 0, 1, 1 } }
};

/*
 * Parse an embedded style of the form "fg=colour,bg=colour,bright,...".
 * Note that this adds onto the given style, so it must have been initialized
 * alredy.
 */
int
style_parse(struct style *sy, const struct grid_cell *base, const char *in)
{
	struct style	saved;
	const char	delimiters[] = " ,";
	char		tmp[32];
	int		value;
	size_t		end;

	if (*in == '\0')
		return (0);
	style_copy(&saved, sy);

	do {
		while (*in != '\0' && strchr(delimiters, *in) != NULL) {
			in++;
			end--;
		}
		if (*in == '\0')
			break;

		end = strcspn(in, delimiters);
		if (end > (sizeof tmp) - 1)
			goto error;
		memcpy(tmp, in, end);
		tmp[end] = '\0';

		if (strcasecmp(tmp, "default") == 0) {
			sy->gc.fg = base->fg;
			sy->gc.bg = base->bg;
			sy->gc.attr = base->attr;
			sy->gc.flags = base->flags;
		} else if (end > 3 && strncasecmp(tmp + 1, "g=", 2) == 0) {
			if ((value = colour_fromstring(tmp + 3)) == -1)
				goto error;
			if (*in == 'f' || *in == 'F') {
				if (value != 8)
					sy->gc.fg = value;
				else
					sy->gc.fg = base->fg;
			} else if (*in == 'b' || *in == 'B') {
				if (value != 8)
					sy->gc.bg = value;
				else
					sy->gc.bg = base->bg;
			} else
				goto error;
		} else if (strcasecmp(tmp, "none") == 0)
			sy->gc.attr = 0;
		else if (end > 2 && strncasecmp(tmp, "no", 2) == 0) {
			if ((value = attributes_fromstring(tmp + 2)) == -1)
				goto error;
			sy->gc.attr &= ~value;
		} else {
			if ((value = attributes_fromstring(tmp)) == -1)
				goto error;
			sy->gc.attr |= value;
		}

		in += end + strspn(in + end, delimiters);
	} while (*in != '\0');

	return (0);

error:
	style_copy(sy, &saved);
	return (-1);
}

/* Convert style to a string. */
const char *
style_tostring(struct style *sy)
{
	struct grid_cell	*gc = &sy->gc;
	int			 off = 0;
	const char		*comma = "";
	static char		 s[256];

	*s = '\0';

	if (gc->fg != 8) {
		off += xsnprintf(s + off, sizeof s - off, "%sfg=%s", comma,
		    colour_tostring(gc->fg));
		comma = ",";
	}
	if (gc->bg != 8) {
		off += xsnprintf(s + off, sizeof s - off, "%sbg=%s", comma,
		    colour_tostring(gc->bg));
		comma = ",";
	}
	if (gc->attr != 0 && gc->attr != GRID_ATTR_CHARSET) {
		xsnprintf(s + off, sizeof s - off, "%s%s", comma,
		    attributes_tostring(gc->attr));
		comma = ",";
	}

	if (*s == '\0')
		return ("default");
	return (s);
}

/* Apply a style. */
void
style_apply(struct grid_cell *gc, struct options *oo, const char *name)
{
	struct style	*sy;

	memcpy(gc, &grid_default_cell, sizeof *gc);
	sy = options_get_style(oo, name);
	gc->fg = sy->gc.fg;
	gc->bg = sy->gc.bg;
	gc->attr |= sy->gc.attr;
}

/* Apply a style, updating if default. */
void
style_apply_update(struct grid_cell *gc, struct options *oo, const char *name)
{
	struct style	*sy;

	sy = options_get_style(oo, name);
	if (sy->gc.fg != 8)
		gc->fg = sy->gc.fg;
	if (sy->gc.bg != 8)
		gc->bg = sy->gc.bg;
	if (sy->gc.attr != 0)
		gc->attr |= sy->gc.attr;
}

/* Initialize style from cell. */
void
style_set(struct style *sy, const struct grid_cell *gc)
{
	memcpy(sy, &style_default, sizeof *sy);
	memcpy(&sy->gc, gc, sizeof sy->gc);
}

/* Copy style. */
void
style_copy(struct style *dst, struct style *src)
{
	memcpy(dst, src, sizeof *dst);
}

/* Check if two styles are the same. */
int
style_equal(struct style *sy1, struct style *sy2)
{
	struct grid_cell	*gc1 = &sy1->gc;
	struct grid_cell	*gc2 = &sy2->gc;

	if (gc1->fg != gc2->fg)
		return (0);
	if (gc1->bg != gc2->bg)
		return (0);
	if ((gc1->attr & STYLE_ATTR_MASK) != (gc2->attr & STYLE_ATTR_MASK))
		return (0);
	return (1);
}

/* Is this style default? */
int
style_is_default(struct style *sy)
{
	return (style_equal(sy, &style_default));
}
