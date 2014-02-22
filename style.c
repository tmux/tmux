/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
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

#include <string.h>

#include "tmux.h"

/* Parse an embedded style of the form "fg=colour,bg=colour,bright,...". */
int
style_parse(const struct grid_cell *defgc, struct grid_cell *gc,
    const char *in)
{
	const char	delimiters[] = " ,";
	char		tmp[32];
	int		val;
	size_t		end;
	u_char		fg, bg, attr, flags;

	if (*in == '\0')
		return (0);
	if (strchr(delimiters, in[strlen(in) - 1]) != NULL)
		return (-1);

	fg = gc->fg;
	bg = gc->bg;
	attr = gc->attr;
	flags = gc->flags;
	do {
		end = strcspn(in, delimiters);
		if (end > (sizeof tmp) - 1)
			return (-1);
		memcpy(tmp, in, end);
		tmp[end] = '\0';

		if (strcasecmp(tmp, "default") == 0) {
			fg = defgc->fg;
			bg = defgc->bg;
			attr = defgc->attr;
			flags &= ~(GRID_FLAG_FG256|GRID_FLAG_BG256);
			flags |=
			    defgc->flags & (GRID_FLAG_FG256|GRID_FLAG_BG256);
		} else if (end > 3 && strncasecmp(tmp + 1, "g=", 2) == 0) {
			if ((val = colour_fromstring(tmp + 3)) == -1)
				return (-1);
			if (*in == 'f' || *in == 'F') {
				if (val != 8) {
					if (val & 0x100) {
						flags |= GRID_FLAG_FG256;
						val &= ~0x100;
					} else
						flags &= ~GRID_FLAG_FG256;
					fg = val;
				} else {
					fg = defgc->fg;
					flags &= ~GRID_FLAG_FG256;
					flags |= defgc->flags & GRID_FLAG_FG256;
				}
			} else if (*in == 'b' || *in == 'B') {
				if (val != 8) {
					if (val & 0x100) {
						flags |= GRID_FLAG_BG256;
						val &= ~0x100;
					} else
						flags &= ~GRID_FLAG_BG256;
					bg = val;
				} else {
					bg = defgc->bg;
					flags &= ~GRID_FLAG_BG256;
					flags |= defgc->flags & GRID_FLAG_BG256;
				}
			} else
				return (-1);
		} else if (strcasecmp(tmp, "none") == 0)
			attr = 0;
		else if (end > 2 && strncasecmp(tmp, "no", 2) == 0) {
			if ((val = attributes_fromstring(tmp + 2)) == -1)
				return (-1);
			attr &= ~val;
		} else {
			if ((val = attributes_fromstring(tmp)) == -1)
				return (-1);
			attr |= val;
		}

		in += end + strspn(in + end, delimiters);
	} while (*in != '\0');
	gc->fg = fg;
	gc->bg = bg;
	gc->attr = attr;
	gc->flags = flags;

	return (0);
}

/* Convert style to a string. */
const char *
style_tostring(struct grid_cell *gc)
{
	int		 c, off = 0, comma = 0;
	static char	 s[256];

	*s = '\0';

	if (gc->fg != 8) {
		if (gc->flags & GRID_FLAG_FG256)
			c = gc->fg | 0x100;
		else
			c = gc->fg;
		off += xsnprintf(s, sizeof s, "fg=%s", colour_tostring(c));
		comma = 1;
	}

	if (gc->bg != 8) {
		if (gc->flags & GRID_FLAG_BG256)
			c = gc->bg | 0x100;
		else
			c = gc->bg;
		off += xsnprintf(s + off, sizeof s - off, "%sbg=%s",
		    comma ? "," : "", colour_tostring(c));
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

/* Synchronize new -style option with the old one. */
void
style_update_new(struct options *oo, const char *name, const char *newname)
{
	int			 value;
	struct grid_cell	*gc;

	/* It's a colour or attribute, but with no -style equivalent. */
	if (newname == NULL)
		return;

	gc = options_get_style(oo, newname);
	value = options_get_number(oo, name);

	if (strstr(name, "-bg") != NULL)
		colour_set_bg(gc, value);
	else if (strstr(name, "-fg") != NULL)
		colour_set_fg(gc, value);
	else if (strstr(name, "-attr") != NULL)
		gc->attr = value;
}

/* Synchronize all the old options with the new -style one. */
void
style_update_old(struct options *oo, const char *name, struct grid_cell *gc)
{
	char	newname[128];
	int	c, size;

	size = strrchr(name, '-') - name;

	if (gc->flags & GRID_FLAG_BG256)
		c = gc->bg | 0x100;
	else
		c = gc->bg;
	xsnprintf(newname, sizeof newname, "%.*s-bg", size, name);
	options_set_number(oo, newname, c);

	if (gc->flags & GRID_FLAG_FG256)
		c = gc->fg | 0x100;
	else
		c = gc->fg;
	xsnprintf(newname, sizeof newname, "%.*s-fg", size, name);
	options_set_number(oo, newname, c);

	xsnprintf(newname, sizeof newname, "%.*s-attr", size, name);
	options_set_number(oo, newname, gc->attr);
}

/* Apply a style. */
void
style_apply(struct grid_cell *gc, struct options *oo, const char *name)
{
	struct grid_cell	*gcp;

	memcpy(gc, &grid_default_cell, sizeof *gc);
	gcp = options_get_style(oo, name);
	if (gcp->flags & GRID_FLAG_FG256)
		colour_set_fg(gc, gcp->fg | 0x100);
	else
		colour_set_fg(gc, gcp->fg);
	if (gcp->flags & GRID_FLAG_BG256)
		colour_set_bg(gc, gcp->bg | 0x100);
	else
		colour_set_bg(gc, gcp->bg);
	gc->attr |= gcp->attr;
}

/* Apply a style, updating if default. */
void
style_apply_update(struct grid_cell *gc, struct options *oo, const char *name)
{
	struct grid_cell	*gcp;

	gcp = options_get_style(oo, name);
	if (gcp->fg != 8) {
		if (gcp->flags & GRID_FLAG_FG256)
			colour_set_fg(gc, gcp->fg | 0x100);
		else
			colour_set_fg(gc, gcp->fg);
	}
	if (gcp->bg != 8) {
		if (gcp->flags & GRID_FLAG_BG256)
			colour_set_bg(gc, gcp->bg | 0x100);
		else
			colour_set_bg(gc, gcp->bg);
	}
	if (gcp->attr != 0)
		gc->attr |= gcp->attr;
}
