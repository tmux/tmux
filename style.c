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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/* Mask for bits not included in style. */
#define STYLE_ATTR_MASK (~0)

/* Default style. */
static struct style style_default = {
	{ { { ' ' }, 0, 1, 1 }, 0, 0, 8, 8, 0  },

	8,
	STYLE_ALIGN_DEFAULT,
	STYLE_LIST_OFF,

	STYLE_RANGE_NONE, 0,

	STYLE_DEFAULT_BASE
};

/*
 * Parse an embedded style of the form "fg=colour,bg=colour,bright,...".  Note
 * that this adds onto the given style, so it must have been initialized
 * already.
 */
int
style_parse(struct style *sy, const struct grid_cell *base, const char *in)
{
	struct style	saved;
	const char	delimiters[] = " ,", *cp;
	char		tmp[256], *found;
	int		value;
	size_t		end;

	if (*in == '\0')
		return (0);
	style_copy(&saved, sy);

	do {
		while (*in != '\0' && strchr(delimiters, *in) != NULL)
			in++;
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
		} else if (strcasecmp(tmp, "push-default") == 0)
			sy->default_type = STYLE_DEFAULT_PUSH;
		else if (strcasecmp(tmp, "pop-default") == 0)
			sy->default_type = STYLE_DEFAULT_POP;
		else if (strcasecmp(tmp, "nolist") == 0)
			sy->list = STYLE_LIST_OFF;
		else if (strncasecmp(tmp, "list=", 5) == 0) {
			if (strcasecmp(tmp + 5, "on") == 0)
				sy->list = STYLE_LIST_ON;
			else if (strcasecmp(tmp + 5, "focus") == 0)
				sy->list = STYLE_LIST_FOCUS;
			else if (strcasecmp(tmp + 5, "left-marker") == 0)
				sy->list = STYLE_LIST_LEFT_MARKER;
			else if (strcasecmp(tmp + 5, "right-marker") == 0)
				sy->list = STYLE_LIST_RIGHT_MARKER;
			else
				goto error;
		} else if (strcasecmp(tmp, "norange") == 0) {
			sy->range_type = style_default.range_type;
			sy->range_argument = style_default.range_type;
		} else if (end > 6 && strncasecmp(tmp, "range=", 6) == 0) {
			found = strchr(tmp + 6, '|');
			if (found != NULL) {
				*found++ = '\0';
				if (*found == '\0')
					goto error;
				for (cp = found; *cp != '\0'; cp++) {
					if (!isdigit((u_char)*cp))
						goto error;
				}
			}
			if (strcasecmp(tmp + 6, "left") == 0) {
				if (found != NULL)
					goto error;
				sy->range_type = STYLE_RANGE_LEFT;
				sy->range_argument = 0;
			} else if (strcasecmp(tmp + 6, "right") == 0) {
				if (found != NULL)
					goto error;
				sy->range_type = STYLE_RANGE_RIGHT;
				sy->range_argument = 0;
			} else if (strcasecmp(tmp + 6, "window") == 0) {
				if (found == NULL)
					goto error;
				sy->range_type = STYLE_RANGE_WINDOW;
				sy->range_argument = atoi(found);
			}
		} else if (strcasecmp(tmp, "noalign") == 0)
			sy->align = style_default.align;
		else if (end > 6 && strncasecmp(tmp, "align=", 6) == 0) {
			if (strcasecmp(tmp + 6, "left") == 0)
				sy->align = STYLE_ALIGN_LEFT;
			else if (strcasecmp(tmp + 6, "centre") == 0)
				sy->align = STYLE_ALIGN_CENTRE;
			else if (strcasecmp(tmp + 6, "right") == 0)
				sy->align = STYLE_ALIGN_RIGHT;
			else
				goto error;
		} else if (end > 5 && strncasecmp(tmp, "fill=", 5) == 0) {
			if ((value = colour_fromstring(tmp + 5)) == -1)
				goto error;
			sy->fill = value;
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
	const char		*comma = "", *tmp = "";
	static char		 s[256];
	char			 b[16];

	*s = '\0';

	if (sy->list != STYLE_LIST_OFF) {
		if (sy->list == STYLE_LIST_ON)
			tmp = "on";
		else if (sy->list == STYLE_LIST_FOCUS)
			tmp = "focus";
		else if (sy->list == STYLE_LIST_LEFT_MARKER)
			tmp = "left-marker";
		else if (sy->list == STYLE_LIST_RIGHT_MARKER)
			tmp = "right-marker";
		off += xsnprintf(s + off, sizeof s - off, "%slist=%s", comma,
		    tmp);
		comma = ",";
	}
	if (sy->range_type != STYLE_RANGE_NONE) {
		if (sy->range_type == STYLE_RANGE_LEFT)
			tmp = "left";
		else if (sy->range_type == STYLE_RANGE_RIGHT)
			tmp = "right";
		else if (sy->range_type == STYLE_RANGE_WINDOW) {
			snprintf(b, sizeof b, "window|%u", sy->range_argument);
			tmp = b;
		}
		off += xsnprintf(s + off, sizeof s - off, "%srange=%s", comma,
		    tmp);
		comma = ",";
	}
	if (sy->align != STYLE_ALIGN_DEFAULT) {
		if (sy->align == STYLE_ALIGN_LEFT)
			tmp = "left";
		else if (sy->align == STYLE_ALIGN_CENTRE)
			tmp = "centre";
		else if (sy->align == STYLE_ALIGN_RIGHT)
			tmp = "right";
		off += xsnprintf(s + off, sizeof s - off, "%salign=%s", comma,
		    tmp);
		comma = ",";
	}
	if (sy->default_type != STYLE_DEFAULT_BASE) {
		if (sy->default_type == STYLE_DEFAULT_PUSH)
			tmp = "push-default";
		else if (sy->default_type == STYLE_DEFAULT_POP)
			tmp = "pop-default";
		off += xsnprintf(s + off, sizeof s - off, "%s%s", comma, tmp);
		comma = ",";
	}
	if (sy->fill != 8) {
		off += xsnprintf(s + off, sizeof s - off, "%sfill=%s", comma,
		    colour_tostring(sy->fill));
		comma = ",";
	}
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
	if (gc->attr != 0) {
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

/* Check if two styles are (visibly) the same. */
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
	if (sy1->fill != sy2->fill)
		return (0);
	if (sy1->align != sy2->align)
		return (0);
	return (1);
}

/* Is this style default? */
int
style_is_default(struct style *sy)
{
	return (style_equal(sy, &style_default));
}
