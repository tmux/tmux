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

/* Parse an embedded style of the form "fg=colour,bg=colour,bright,...". */
int
style_parse(const struct grid_cell *defgc, struct grid_cell *gc,
    const char *in)
{
	struct grid_cell	savedgc;
	const char		delimiters[] = " ,";
	char			tmp[32];
	int			val, fg, bg, attr, flags;
	size_t			end;

	if (*in == '\0')
		return (0);
	if (strchr(delimiters, in[strlen(in) - 1]) != NULL)
		return (-1);
	memcpy(&savedgc, gc, sizeof savedgc);

	fg = gc->fg;
	bg = gc->bg;
	attr = gc->attr;
	flags = gc->flags;
	do {
		end = strcspn(in, delimiters);
		if (end > (sizeof tmp) - 1)
			goto error;
		memcpy(tmp, in, end);
		tmp[end] = '\0';

		if (strcasecmp(tmp, "default") == 0) {
			fg = defgc->fg;
			bg = defgc->bg;
			attr = defgc->attr;
			flags = defgc->flags;
		} else if (end > 3 && strncasecmp(tmp + 1, "g=", 2) == 0) {
			if ((val = colour_fromstring(tmp + 3)) == -1)
				goto error;
			if (*in == 'f' || *in == 'F') {
				if (val != 8)
					fg = val;
				else
					fg = defgc->fg;
			} else if (*in == 'b' || *in == 'B') {
				if (val != 8)
					bg = val;
				else
					bg = defgc->bg;
			} else
				goto error;
		} else if (strcasecmp(tmp, "none") == 0)
			attr = 0;
		else if (end > 2 && strncasecmp(tmp, "no", 2) == 0) {
			if ((val = attributes_fromstring(tmp + 2)) == -1)
				goto error;
			attr &= ~val;
		} else {
			if ((val = attributes_fromstring(tmp)) == -1)
				goto error;
			attr |= val;
		}

		in += end + strspn(in + end, delimiters);
	} while (*in != '\0');
	gc->fg = fg;
	gc->bg = bg;
	gc->attr = attr;
	gc->flags = flags;

	return (0);

error:
	memcpy(gc, &savedgc, sizeof *gc);
	return (-1);
}

/* Convert style to a string. */
const char *
style_tostring(struct grid_cell *gc)
{
	int		 off = 0, comma = 0;
	static char	 s[256];

	*s = '\0';

	if (gc->fg != 8) {
		off += xsnprintf(s, sizeof s, "fg=%s", colour_tostring(gc->fg));
		comma = 1;
	}

	if (gc->bg != 8) {
		off += xsnprintf(s + off, sizeof s - off, "%sbg=%s",
		    comma ? "," : "", colour_tostring(gc->bg));
		comma = 1;
	}

	if (gc->attr != 0 && gc->attr != GRID_ATTR_CHARSET) {
		xsnprintf(s + off, sizeof s - off, "%s%s",
		    comma ? "," : "", attributes_tostring(gc->attr));
	}

	if (*s == '\0')
		return ("default");
	return (s);
}

/* Apply a style. */
void
style_apply(struct grid_cell *gc, struct options *oo, const char *name)
{
	const struct grid_cell	*gcp;

	memcpy(gc, &grid_default_cell, sizeof *gc);
	gcp = options_get_style(oo, name);
	gc->fg = gcp->fg;
	gc->bg = gcp->bg;
	gc->attr |= gcp->attr;
}

/* Apply a style, updating if default. */
void
style_apply_update(struct grid_cell *gc, struct options *oo, const char *name)
{
	const struct grid_cell	*gcp;

	gcp = options_get_style(oo, name);
	if (gcp->fg != 8)
		gc->fg = gcp->fg;
	if (gcp->bg != 8)
		gc->bg = gcp->bg;
	if (gcp->attr != 0)
		gc->attr |= gcp->attr;
}

/* Check if two styles are the same. */
int
style_equal(const struct grid_cell *gc1, const struct grid_cell *gc2)
{
	return (gc1->fg == gc2->fg &&
	    gc1->bg == gc2->bg &&
	    (gc1->flags & ~GRID_FLAG_PADDING) ==
	    (gc2->flags & ~GRID_FLAG_PADDING) &&
	    (gc1->attr & ~GRID_ATTR_CHARSET) ==
	    (gc2->attr & ~GRID_ATTR_CHARSET));
}
