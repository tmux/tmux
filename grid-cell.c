/* $OpenBSD$ */

/*
 * Copyright (c) 2012 Nicholas Marriott <nicm@users.sourceforge.net>
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

/* Get cell width. */
u_int
grid_cell_width(const struct grid_cell *gc)
{
	return (gc->xstate >> 4);
}

/* Get cell data. */
void
grid_cell_get(const struct grid_cell *gc, struct utf8_data *ud)
{
	ud->size = gc->xstate & 0xf;
	ud->width = gc->xstate >> 4;
	memcpy(ud->data, gc->xdata, ud->size);
}

/* Set cell data. */
void
grid_cell_set(struct grid_cell *gc, const struct utf8_data *ud)
{
	memcpy(gc->xdata, ud->data, ud->size);
	gc->xstate = (ud->width << 4) | ud->size;
}

/* Set a single character as cell data. */
void
grid_cell_one(struct grid_cell *gc, u_char ch)
{
	*gc->xdata = ch;
	gc->xstate = (1 << 4) | 1;
}
