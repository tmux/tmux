/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

static struct screen_write_citem *screen_write_collect_trim(
		    struct screen_write_ctx *, u_int, u_int, u_int, int *);
static void	screen_write_collect_clear(struct screen_write_ctx *, u_int,
		    u_int);
static void	screen_write_collect_scroll(struct screen_write_ctx *, u_int);
static void	screen_write_collect_flush(struct screen_write_ctx *, int,
		    const char *);
static int	screen_write_overwrite(struct screen_write_ctx *,
		    struct grid_cell *, u_int);
static int	screen_write_combine(struct screen_write_ctx *,
		    const struct grid_cell *);

struct screen_write_citem {
	u_int				x;
	int				wrapped;

	enum { TEXT, CLEAR }		type;
	u_int				used;
	u_int				bg;

	struct grid_cell		gc;

	TAILQ_ENTRY(screen_write_citem) entry;
};
struct screen_write_cline {
	char				*data;
	TAILQ_HEAD(, screen_write_citem) items;
};
TAILQ_HEAD(, screen_write_citem)  screen_write_citem_freelist =
    TAILQ_HEAD_INITIALIZER(screen_write_citem_freelist);

static struct screen_write_citem *
screen_write_get_citem(void)
{
    struct screen_write_citem	*ci;

    ci = TAILQ_FIRST(&screen_write_citem_freelist);
    if (ci != NULL) {
        TAILQ_REMOVE(&screen_write_citem_freelist, ci, entry);
        memset(ci, 0, sizeof *ci);
        return (ci);
    }
    return (xcalloc(1, sizeof *ci));
}

static void
screen_write_free_citem(struct screen_write_citem *ci)
{
    TAILQ_INSERT_TAIL(&screen_write_citem_freelist, ci, entry);
}

static void
screen_write_offset_timer(__unused int fd, __unused short events, void *data)
{
	struct window	*w = data;

	tty_update_window_offset(w);
}

/* Set cursor position. */
static void
screen_write_set_cursor(struct screen_write_ctx *ctx, int cx, int cy)
{
	struct window_pane	*wp = ctx->wp;
	struct window		*w;
	struct screen		*s = ctx->s;
	struct timeval		 tv = { .tv_usec = 10000 };

	if (cx != -1 && (u_int)cx == s->cx && cy != -1 && (u_int)cy == s->cy)
		return;

	if (cx != -1) {
		if ((u_int)cx > screen_size_x(s)) /* allow last column */
			cx = screen_size_x(s) - 1;
		s->cx = cx;
	}
	if (cy != -1) {
		if ((u_int)cy > screen_size_y(s) - 1)
			cy = screen_size_y(s) - 1;
		s->cy = cy;
	}

	if (wp == NULL)
		return;
	w = wp->window;

	if (!event_initialized(&w->offset_timer))
		evtimer_set(&w->offset_timer, screen_write_offset_timer, w);
	if (!evtimer_pending(&w->offset_timer, NULL))
		evtimer_add(&w->offset_timer, &tv);
}

/* Do a full redraw. */
static void
screen_write_redraw_cb(const struct tty_ctx *ttyctx)
{
	struct window_pane	*wp = ttyctx->arg;

	if (wp != NULL)
		wp->flags |= PANE_REDRAW;
}

/* Update context for client. */
static int
screen_write_set_client_cb(struct tty_ctx *ttyctx, struct client *c)
{
	struct window_pane	*wp = ttyctx->arg;

	if (ttyctx->allow_invisible_panes) {
		if (session_has(c->session, wp->window))
			return (1);
		return (0);
	}

	if (c->session->curw->window != wp->window)
		return (0);
	if (wp->layout_cell == NULL)
		return (0);

	if (wp->flags & (PANE_REDRAW|PANE_DROP))
		return (-1);
	if (c->flags & CLIENT_REDRAWPANES) {
		/*
		 * Redraw is already deferred to redraw another pane - redraw
		 * this one also when that happens.
		 */
		log_debug("%s: adding %%%u to deferred redraw", __func__,
		    wp->id);
		wp->flags |= (PANE_REDRAW|PANE_REDRAWSCROLLBAR);
		return (-1);
	}

	ttyctx->bigger = tty_window_offset(&c->tty, &ttyctx->wox, &ttyctx->woy,
	    &ttyctx->wsx, &ttyctx->wsy);

	ttyctx->xoff = ttyctx->rxoff = wp->xoff;
	ttyctx->yoff = ttyctx->ryoff = wp->yoff;

	if (status_at_line(c) == 0)
		ttyctx->yoff += status_line_size(c);

	return (1);
}

/* Set up context for TTY command. */
static void
screen_write_initctx(struct screen_write_ctx *ctx, struct tty_ctx *ttyctx,
    int sync)
{
	struct screen	*s = ctx->s;

	memset(ttyctx, 0, sizeof *ttyctx);

	ttyctx->s = s;
	ttyctx->sx = screen_size_x(s);
	ttyctx->sy = screen_size_y(s);

	ttyctx->ocx = s->cx;
	ttyctx->ocy = s->cy;
	ttyctx->orlower = s->rlower;
	ttyctx->orupper = s->rupper;

	memcpy(&ttyctx->defaults, &grid_default_cell, sizeof ttyctx->defaults);
	if (ctx->init_ctx_cb != NULL) {
		ctx->init_ctx_cb(ctx, ttyctx);
		if (ttyctx->palette != NULL) {
			if (ttyctx->defaults.fg == 8)
				ttyctx->defaults.fg = ttyctx->palette->fg;
			if (ttyctx->defaults.bg == 8)
				ttyctx->defaults.bg = ttyctx->palette->bg;
		}
	} else {
		ttyctx->redraw_cb = screen_write_redraw_cb;
		if (ctx->wp != NULL) {
			tty_default_colours(&ttyctx->defaults, ctx->wp);
			ttyctx->palette = &ctx->wp->palette;
			ttyctx->set_client_cb = screen_write_set_client_cb;
			ttyctx->arg = ctx->wp;
		}
	}

	if (~ctx->flags & SCREEN_WRITE_SYNC) {
		/*
		 * For the active pane or for an overlay (no pane), we want to
		 * only use synchronized updates if requested (commands that
		 * move the cursor); for other panes, always use it, since the
		 * cursor will have to move.
		 */
		if (ctx->wp != NULL) {
			if (ctx->wp != ctx->wp->window->active)
				ttyctx->num = 1;
			else
				ttyctx->num = sync;
		} else
			ttyctx->num = 0x10|sync;
		tty_write(tty_cmd_syncstart, ttyctx);
		ctx->flags |= SCREEN_WRITE_SYNC;
	}
}

/* Make write list. */
void
screen_write_make_list(struct screen *s)
{
	u_int	y;

	s->write_list = xcalloc(screen_size_y(s), sizeof *s->write_list);
	for (y = 0; y < screen_size_y(s); y++)
		TAILQ_INIT(&s->write_list[y].items);
}

/* Free write list. */
void
screen_write_free_list(struct screen *s)
{
	u_int	y;

	for (y = 0; y < screen_size_y(s); y++)
		free(s->write_list[y].data);
	free(s->write_list);
}

/* Set up for writing. */
static void
screen_write_init(struct screen_write_ctx *ctx, struct screen *s)
{
	memset(ctx, 0, sizeof *ctx);

	ctx->s = s;

	if (ctx->s->write_list == NULL)
		screen_write_make_list(ctx->s);
	ctx->item = screen_write_get_citem();

	ctx->scrolled = 0;
	ctx->bg = 8;
}

/* Initialize writing with a pane. */
void
screen_write_start_pane(struct screen_write_ctx *ctx, struct window_pane *wp,
    struct screen *s)
{
	if (s == NULL)
		s = wp->screen;
	screen_write_init(ctx, s);
	ctx->wp = wp;

	if (log_get_level() != 0) {
		log_debug("%s: size %ux%u, pane %%%u (at %u,%u)",
		    __func__, screen_size_x(ctx->s), screen_size_y(ctx->s),
		    wp->id, wp->xoff, wp->yoff);
	}
}

/* Initialize writing with a callback. */
void
screen_write_start_callback(struct screen_write_ctx *ctx, struct screen *s,
    screen_write_init_ctx_cb cb, void *arg)
{
	screen_write_init(ctx, s);

	ctx->init_ctx_cb = cb;
	ctx->arg = arg;

	if (log_get_level() != 0) {
		log_debug("%s: size %ux%u, with callback", __func__,
		    screen_size_x(ctx->s), screen_size_y(ctx->s));
	}
}

/* Initialize writing. */
void
screen_write_start(struct screen_write_ctx *ctx, struct screen *s)
{
	screen_write_init(ctx, s);

	if (log_get_level() != 0) {
		log_debug("%s: size %ux%u, no pane", __func__,
		    screen_size_x(ctx->s), screen_size_y(ctx->s));
	}
}

/* Finish writing. */
void
screen_write_stop(struct screen_write_ctx *ctx)
{
	screen_write_collect_end(ctx);
	screen_write_collect_flush(ctx, 0, __func__);

	screen_write_free_citem(ctx->item);
}

/* Reset screen state. */
void
screen_write_reset(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;

	screen_reset_tabs(s);
	screen_write_scrollregion(ctx, 0, screen_size_y(s) - 1);

	s->mode = MODE_CURSOR|MODE_WRAP;

	if (options_get_number(global_options, "extended-keys") == 2)
		s->mode = (s->mode & ~EXTENDED_KEY_MODES)|MODE_KEYS_EXTENDED;

	screen_write_clearscreen(ctx, 8);
	screen_write_set_cursor(ctx, 0, 0);
}

/* Write character. */
void
screen_write_putc(struct screen_write_ctx *ctx, const struct grid_cell *gcp,
    u_char ch)
{
	struct grid_cell	gc;

	memcpy(&gc, gcp, sizeof gc);

	utf8_set(&gc.data, ch);
	screen_write_cell(ctx, &gc);
}

/* Calculate string length. */
size_t
screen_write_strlen(const char *fmt, ...)
{
	va_list			ap;
	char   	       	       *msg;
	struct utf8_data	ud;
	u_char 	      	       *ptr;
	size_t			left, size = 0;
	enum utf8_state		more;

	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	va_end(ap);

	ptr = msg;
	while (*ptr != '\0') {
		if (*ptr > 0x7f && utf8_open(&ud, *ptr) == UTF8_MORE) {
			ptr++;

			left = strlen(ptr);
			if (left < (size_t)ud.size - 1)
				break;
			while ((more = utf8_append(&ud, *ptr)) == UTF8_MORE)
				ptr++;
			ptr++;

			if (more == UTF8_DONE)
				size += ud.width;
		} else {
			if (*ptr == '\t' || (*ptr > 0x1f && *ptr < 0x7f))
				size++;
			ptr++;
		}
	}

	free(msg);
	return (size);
}

/* Write string wrapped over lines. */
int
screen_write_text(struct screen_write_ctx *ctx, u_int cx, u_int width,
    u_int lines, int more, const struct grid_cell *gcp, const char *fmt, ...)
{
	struct screen		*s = ctx->s;
	va_list			 ap;
	char			*tmp;
	u_int			 cy = s->cy, i, end, next, idx = 0, at, left;
	struct utf8_data	*text;
	struct grid_cell	 gc;

	memcpy(&gc, gcp, sizeof gc);

	va_start(ap, fmt);
	xvasprintf(&tmp, fmt, ap);
	va_end(ap);

	text = utf8_fromcstr(tmp);
	free(tmp);

	left = (cx + width) - s->cx;
	for (;;) {
		/* Find the end of what can fit on the line. */
		at = 0;
		for (end = idx; text[end].size != 0; end++) {
			if (text[end].size == 1 && text[end].data[0] == '\n')
				break;
			if (at + text[end].width > left)
				break;
			at += text[end].width;
		}

		/*
		 * If we're on a space, that's the end. If not, walk back to
		 * try and find one.
		 */
		if (text[end].size == 0)
			next = end;
		else if (text[end].size == 1 && text[end].data[0] == '\n')
			next = end + 1;
		else if (text[end].size == 1 && text[end].data[0] == ' ')
			next = end + 1;
		else {
			for (i = end; i > idx; i--) {
				if (text[i].size == 1 && text[i].data[0] == ' ')
					break;
			}
			if (i != idx) {
				next = i + 1;
				end = i;
			} else
				next = end;
		}

		/* Print the line. */
		for (i = idx; i < end; i++) {
			utf8_copy(&gc.data, &text[i]);
			screen_write_cell(ctx, &gc);
		}

		/* If at the bottom, stop. */
		idx = next;
		if (s->cy == cy + lines - 1 || text[idx].size == 0)
			break;

		screen_write_cursormove(ctx, cx, s->cy + 1, 0);
		left = width;
	}

	/*
	 * Fail if on the last line and there is more to come or at the end, or
	 * if the text was not entirely consumed.
	 */
	if ((s->cy == cy + lines - 1 && (!more || s->cx == cx + width)) ||
	    text[idx].size != 0) {
		free(text);
		return (0);
	}
	free(text);

	/*
	 * If no more to come, move to the next line. Otherwise, leave on
	 * the same line (except if at the end).
	 */
	if (!more || s->cx == cx + width)
		screen_write_cursormove(ctx, cx, s->cy + 1, 0);
	return (1);
}

/* Write simple string (no maximum length). */
void
screen_write_puts(struct screen_write_ctx *ctx, const struct grid_cell *gcp,
    const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	screen_write_vnputs(ctx, -1, gcp, fmt, ap);
	va_end(ap);
}

/* Write string with length limit (-1 for unlimited). */
void
screen_write_nputs(struct screen_write_ctx *ctx, ssize_t maxlen,
    const struct grid_cell *gcp, const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	screen_write_vnputs(ctx, maxlen, gcp, fmt, ap);
	va_end(ap);
}

void
screen_write_vnputs(struct screen_write_ctx *ctx, ssize_t maxlen,
    const struct grid_cell *gcp, const char *fmt, va_list ap)
{
	struct grid_cell	gc;
	struct utf8_data       *ud = &gc.data;
	char   		       *msg;
	u_char 		       *ptr;
	size_t		 	left, size = 0;
	enum utf8_state		more;

	memcpy(&gc, gcp, sizeof gc);
	xvasprintf(&msg, fmt, ap);

	ptr = msg;
	while (*ptr != '\0') {
		if (*ptr > 0x7f && utf8_open(ud, *ptr) == UTF8_MORE) {
			ptr++;

			left = strlen(ptr);
			if (left < (size_t)ud->size - 1)
				break;
			while ((more = utf8_append(ud, *ptr)) == UTF8_MORE)
				ptr++;
			ptr++;

			if (more != UTF8_DONE)
				continue;
			if (maxlen > 0 && size + ud->width > (size_t)maxlen) {
				while (size < (size_t)maxlen) {
					screen_write_putc(ctx, &gc, ' ');
					size++;
				}
				break;
			}
			size += ud->width;
			screen_write_cell(ctx, &gc);
		} else {
			if (maxlen > 0 && size + 1 > (size_t)maxlen)
				break;

			if (*ptr == '\001')
				gc.attr ^= GRID_ATTR_CHARSET;
			else if (*ptr == '\n') {
				screen_write_linefeed(ctx, 0, 8);
				screen_write_carriagereturn(ctx);
			} else if (*ptr == '\t' || (*ptr > 0x1f && *ptr < 0x7f)) {
				size++;
				screen_write_putc(ctx, &gc, *ptr);
			}
			ptr++;
		}
	}

	free(msg);
}

/*
 * Copy from another screen but without the selection stuff. Assumes the target
 * region is already big enough.
 */
void
screen_write_fast_copy(struct screen_write_ctx *ctx, struct screen *src,
    u_int px, u_int py, u_int nx, u_int ny)
{
	struct screen		*s = ctx->s;
	struct window_pane	*wp = ctx->wp;
	struct tty_ctx	 	 ttyctx;
	struct grid		*gd = src->grid;
	struct grid_cell	 gc;
	u_int		 	 xx, yy, cx = s->cx, cy = s->cy;

	if (nx == 0 || ny == 0)
		return;

	cy = s->cy;
	for (yy = py; yy < py + ny; yy++) {
		if (yy >= gd->hsize + gd->sy)
			break;
		s->cx = cx;
		if (wp != NULL)
			screen_write_initctx(ctx, &ttyctx, 0);
		for (xx = px; xx < px + nx; xx++) {
			if (xx >= grid_get_line(gd, yy)->cellsize)
				break;
			grid_get_cell(gd, xx, yy, &gc);
			if (xx + gc.data.width > px + nx)
				break;
			grid_view_set_cell(ctx->s->grid, s->cx, s->cy, &gc);
			if (wp != NULL) {
				ttyctx.cell = &gc;
				tty_write(tty_cmd_cell, &ttyctx);
				ttyctx.ocx++;
			}
			s->cx++;
		}
		s->cy++;
	}

	s->cx = cx;
	s->cy = cy;
}

/* Select character set for drawing border lines. */
static void
screen_write_box_border_set(enum box_lines lines, int cell_type,
    struct grid_cell *gc)
{
	switch (lines) {
        case BOX_LINES_NONE:
		break;
        case BOX_LINES_DOUBLE:
                gc->attr &= ~GRID_ATTR_CHARSET;
                utf8_copy(&gc->data, tty_acs_double_borders(cell_type));
		break;
        case BOX_LINES_HEAVY:
                gc->attr &= ~GRID_ATTR_CHARSET;
                utf8_copy(&gc->data, tty_acs_heavy_borders(cell_type));
		break;
        case BOX_LINES_ROUNDED:
                gc->attr &= ~GRID_ATTR_CHARSET;
                utf8_copy(&gc->data, tty_acs_rounded_borders(cell_type));
		break;
        case BOX_LINES_SIMPLE:
                gc->attr &= ~GRID_ATTR_CHARSET;
                utf8_set(&gc->data, SIMPLE_BORDERS[cell_type]);
                break;
        case BOX_LINES_PADDED:
                gc->attr &= ~GRID_ATTR_CHARSET;
                utf8_set(&gc->data, PADDED_BORDERS[cell_type]);
                break;
	case BOX_LINES_SINGLE:
	case BOX_LINES_DEFAULT:
		gc->attr |= GRID_ATTR_CHARSET;
		utf8_set(&gc->data, CELL_BORDERS[cell_type]);
		break;
	}
}

/* Draw a horizontal line on screen. */
void
screen_write_hline(struct screen_write_ctx *ctx, u_int nx, int left, int right,
   enum box_lines lines, const struct grid_cell *border_gc)
{
	struct screen		*s = ctx->s;
	struct grid_cell	 gc;
	u_int			 cx, cy, i;

	cx = s->cx;
	cy = s->cy;

	if (border_gc != NULL)
		memcpy(&gc, border_gc, sizeof gc);
	else
		memcpy(&gc, &grid_default_cell, sizeof gc);
	gc.attr |= GRID_ATTR_CHARSET;

	if (left)
		screen_write_box_border_set(lines, CELL_LEFTJOIN, &gc);
	else
		screen_write_box_border_set(lines, CELL_LEFTRIGHT, &gc);
	screen_write_cell(ctx, &gc);

	screen_write_box_border_set(lines, CELL_LEFTRIGHT, &gc);
	for (i = 1; i < nx - 1; i++)
		screen_write_cell(ctx, &gc);

	if (right)
		screen_write_box_border_set(lines, CELL_RIGHTJOIN, &gc);
	else
		screen_write_box_border_set(lines, CELL_LEFTRIGHT, &gc);
	screen_write_cell(ctx, &gc);

	screen_write_set_cursor(ctx, cx, cy);
}

/* Draw a vertical line on screen. */
void
screen_write_vline(struct screen_write_ctx *ctx, u_int ny, int top, int bottom)
{
	struct screen		*s = ctx->s;
	struct grid_cell	 gc;
	u_int			 cx, cy, i;

	cx = s->cx;
	cy = s->cy;

	memcpy(&gc, &grid_default_cell, sizeof gc);
	gc.attr |= GRID_ATTR_CHARSET;

	screen_write_putc(ctx, &gc, top ? 'w' : 'x');
	for (i = 1; i < ny - 1; i++) {
		screen_write_set_cursor(ctx, cx, cy + i);
		screen_write_putc(ctx, &gc, 'x');
	}
	screen_write_set_cursor(ctx, cx, cy + ny - 1);
	screen_write_putc(ctx, &gc, bottom ? 'v' : 'x');

	screen_write_set_cursor(ctx, cx, cy);
}

/* Draw a menu on screen. */
void
screen_write_menu(struct screen_write_ctx *ctx, struct menu *menu, int choice,
    enum box_lines lines, const struct grid_cell *menu_gc,
    const struct grid_cell *border_gc, const struct grid_cell *choice_gc)
{
	struct screen		*s = ctx->s;
	struct grid_cell	 default_gc;
	const struct grid_cell	*gc = &default_gc;
	u_int			 cx, cy, i, j, width = menu->width;
	const char		*name;

	cx = s->cx;
	cy = s->cy;

	memcpy(&default_gc, menu_gc, sizeof default_gc);

	screen_write_box(ctx, menu->width + 4, menu->count + 2, lines,
	    border_gc, menu->title);

	for (i = 0; i < menu->count; i++) {
		name = menu->items[i].name;
		if (name == NULL) {
			screen_write_cursormove(ctx, cx, cy + 1 + i, 0);
			screen_write_hline(ctx, width + 4, 1, 1, lines,
			    border_gc);
			continue;
		}

		if (choice >= 0 && i == (u_int)choice && *name != '-')
			gc = choice_gc;

		screen_write_cursormove(ctx, cx + 1, cy + 1 + i, 0);
		for (j = 0; j < width + 2; j++)
			screen_write_putc(ctx, gc, ' ');

		screen_write_cursormove(ctx, cx + 2, cy + 1 + i, 0);
		if (*name == '-') {
			default_gc.attr |= GRID_ATTR_DIM;
			format_draw(ctx, gc, width, name + 1, NULL, 0);
			default_gc.attr &= ~GRID_ATTR_DIM;
			continue;
		}

		format_draw(ctx, gc, width, name, NULL, 0);
		gc = &default_gc;
	}

	screen_write_set_cursor(ctx, cx, cy);
}

/* Draw a box on screen. */
void
screen_write_box(struct screen_write_ctx *ctx, u_int nx, u_int ny,
    enum box_lines lines, const struct grid_cell *gcp, const char *title)
{
	struct screen		*s = ctx->s;
	struct grid_cell         gc;
	u_int			 cx, cy, i;

	cx = s->cx;
	cy = s->cy;

	if (gcp != NULL)
		memcpy(&gc, gcp, sizeof gc);
	else
		memcpy(&gc, &grid_default_cell, sizeof gc);

	gc.attr |= GRID_ATTR_CHARSET;
	gc.flags |= GRID_FLAG_NOPALETTE;

	/* Draw top border */
	screen_write_box_border_set(lines, CELL_TOPLEFT, &gc);
	screen_write_cell(ctx, &gc);
	screen_write_box_border_set(lines, CELL_LEFTRIGHT, &gc);
	for (i = 1; i < nx - 1; i++)
		screen_write_cell(ctx, &gc);
	screen_write_box_border_set(lines, CELL_TOPRIGHT, &gc);
	screen_write_cell(ctx, &gc);

	/* Draw bottom border */
	screen_write_set_cursor(ctx, cx, cy + ny - 1);
	screen_write_box_border_set(lines, CELL_BOTTOMLEFT, &gc);
	screen_write_cell(ctx, &gc);
	screen_write_box_border_set(lines, CELL_LEFTRIGHT, &gc);
	for (i = 1; i < nx - 1; i++)
		screen_write_cell(ctx, &gc);
	screen_write_box_border_set(lines, CELL_BOTTOMRIGHT, &gc);
	screen_write_cell(ctx, &gc);

	/* Draw sides */
	screen_write_box_border_set(lines, CELL_TOPBOTTOM, &gc);
	for (i = 1; i < ny - 1; i++) {
		/* left side */
		screen_write_set_cursor(ctx, cx, cy + i);
		screen_write_cell(ctx, &gc);
		/* right side */
		screen_write_set_cursor(ctx, cx + nx - 1, cy + i);
		screen_write_cell(ctx, &gc);
	}

	if (title != NULL) {
		gc.attr &= ~GRID_ATTR_CHARSET;
		screen_write_cursormove(ctx, cx + 2, cy, 0);
		format_draw(ctx, &gc, nx - 4, title, NULL, 0);
	}

	screen_write_set_cursor(ctx, cx, cy);
}

/*
 * Write a preview version of a window. Assumes target area is big enough and
 * already cleared.
 */
void
screen_write_preview(struct screen_write_ctx *ctx, struct screen *src, u_int nx,
    u_int ny)
{
	struct screen		*s = ctx->s;
	struct grid_cell	 gc;
	u_int			 cx, cy, px, py;

	cx = s->cx;
	cy = s->cy;

	/*
	 * If the cursor is on, pick the area around the cursor, otherwise use
	 * the top left.
	 */
	if (src->mode & MODE_CURSOR) {
		px = src->cx;
		if (px < nx / 3)
			px = 0;
		else
			px = px - nx / 3;
		if (px + nx > screen_size_x(src)) {
			if (nx > screen_size_x(src))
				px = 0;
			else
				px = screen_size_x(src) - nx;
		}
		py = src->cy;
		if (py < ny / 3)
			py = 0;
		else
			py = py - ny / 3;
		if (py + ny > screen_size_y(src)) {
			if (ny > screen_size_y(src))
				py = 0;
			else
				py = screen_size_y(src) - ny;
		}
	} else {
		px = 0;
		py = 0;
	}

	screen_write_fast_copy(ctx, src, px, src->grid->hsize + py, nx, ny);

	if (src->mode & MODE_CURSOR) {
		grid_view_get_cell(src->grid, src->cx, src->cy, &gc);
		gc.attr |= GRID_ATTR_REVERSE;
		screen_write_set_cursor(ctx, cx + (src->cx - px),
		    cy + (src->cy - py));
		screen_write_cell(ctx, &gc);
	}
}

/* Set a mode. */
void
screen_write_mode_set(struct screen_write_ctx *ctx, int mode)
{
	struct screen	*s = ctx->s;

	s->mode |= mode;

	if (log_get_level() != 0)
		log_debug("%s: %s", __func__, screen_mode_to_string(mode));
}

/* Clear a mode. */
void
screen_write_mode_clear(struct screen_write_ctx *ctx, int mode)
{
	struct screen	*s = ctx->s;

	s->mode &= ~mode;

	if (log_get_level() != 0)
		log_debug("%s: %s", __func__, screen_mode_to_string(mode));
}

/* Cursor up by ny. */
void
screen_write_cursorup(struct screen_write_ctx *ctx, u_int ny)
{
	struct screen	*s = ctx->s;
	u_int		 cx = s->cx, cy = s->cy;

	if (ny == 0)
		ny = 1;

	if (cy < s->rupper) {
		/* Above region. */
		if (ny > cy)
			ny = cy;
	} else {
		/* Below region. */
		if (ny > cy - s->rupper)
			ny = cy - s->rupper;
	}
	if (cx == screen_size_x(s))
		cx--;

	cy -= ny;

	screen_write_set_cursor(ctx, cx, cy);
}

/* Cursor down by ny. */
void
screen_write_cursordown(struct screen_write_ctx *ctx, u_int ny)
{
	struct screen	*s = ctx->s;
	u_int		 cx = s->cx, cy = s->cy;

	if (ny == 0)
		ny = 1;

	if (cy > s->rlower) {
		/* Below region. */
		if (ny > screen_size_y(s) - 1 - cy)
			ny = screen_size_y(s) - 1 - cy;
	} else {
		/* Above region. */
		if (ny > s->rlower - cy)
			ny = s->rlower - cy;
	}
	if (cx == screen_size_x(s))
	    cx--;
	else if (ny == 0)
		return;

	cy += ny;

	screen_write_set_cursor(ctx, cx, cy);
}

/* Cursor right by nx. */
void
screen_write_cursorright(struct screen_write_ctx *ctx, u_int nx)
{
	struct screen	*s = ctx->s;
	u_int		 cx = s->cx, cy = s->cy;

	if (nx == 0)
		nx = 1;

	if (nx > screen_size_x(s) - 1 - cx)
		nx = screen_size_x(s) - 1 - cx;
	if (nx == 0)
		return;

	cx += nx;

	screen_write_set_cursor(ctx, cx, cy);
}

/* Cursor left by nx. */
void
screen_write_cursorleft(struct screen_write_ctx *ctx, u_int nx)
{
	struct screen	*s = ctx->s;
	u_int		 cx = s->cx, cy = s->cy;

	if (nx == 0)
		nx = 1;

	if (nx > cx)
		nx = cx;
	if (nx == 0)
		return;

	cx -= nx;

	screen_write_set_cursor(ctx, cx, cy);
}

/* Backspace; cursor left unless at start of wrapped line when can move up. */
void
screen_write_backspace(struct screen_write_ctx *ctx)
{
	struct screen		*s = ctx->s;
	struct grid_line	*gl;
	u_int			 cx = s->cx, cy = s->cy;

	if (cx == 0) {
		if (cy == 0)
			return;
		gl = grid_get_line(s->grid, s->grid->hsize + cy - 1);
		if (gl->flags & GRID_LINE_WRAPPED) {
			cy--;
			cx = screen_size_x(s) - 1;
		}
	} else
		cx--;

	screen_write_set_cursor(ctx, cx, cy);
}

/* VT100 alignment test. */
void
screen_write_alignmenttest(struct screen_write_ctx *ctx)
{
	struct screen		*s = ctx->s;
	struct tty_ctx	 	 ttyctx;
	struct grid_cell       	 gc;
	u_int			 xx, yy;

	memcpy(&gc, &grid_default_cell, sizeof gc);
	utf8_set(&gc.data, 'E');

#ifdef ENABLE_SIXEL
	if (image_free_all(s) && ctx->wp != NULL)
		ctx->wp->flags |= PANE_REDRAW;
#endif

	for (yy = 0; yy < screen_size_y(s); yy++) {
		for (xx = 0; xx < screen_size_x(s); xx++)
			grid_view_set_cell(s->grid, xx, yy, &gc);
	}

	screen_write_set_cursor(ctx, 0, 0);

	s->rupper = 0;
	s->rlower = screen_size_y(s) - 1;

	screen_write_initctx(ctx, &ttyctx, 1);

	screen_write_collect_clear(ctx, 0, screen_size_y(s) - 1);
	tty_write(tty_cmd_alignmenttest, &ttyctx);
}

/* Insert nx characters. */
void
screen_write_insertcharacter(struct screen_write_ctx *ctx, u_int nx, u_int bg)
{
	struct screen	*s = ctx->s;
	struct tty_ctx	 ttyctx;

	if (nx == 0)
		nx = 1;

	if (nx > screen_size_x(s) - s->cx)
		nx = screen_size_x(s) - s->cx;
	if (nx == 0)
		return;

	if (s->cx > screen_size_x(s) - 1)
		return;

#ifdef ENABLE_SIXEL
	if (image_check_line(s, s->cy, 1) && ctx->wp != NULL)
		ctx->wp->flags |= PANE_REDRAW;
#endif

	screen_write_initctx(ctx, &ttyctx, 0);
	ttyctx.bg = bg;

	grid_view_insert_cells(s->grid, s->cx, s->cy, nx, bg);

	screen_write_collect_flush(ctx, 0, __func__);
	ttyctx.num = nx;
	tty_write(tty_cmd_insertcharacter, &ttyctx);
}

/* Delete nx characters. */
void
screen_write_deletecharacter(struct screen_write_ctx *ctx, u_int nx, u_int bg)
{
	struct screen	*s = ctx->s;
	struct tty_ctx	 ttyctx;

	if (nx == 0)
		nx = 1;

	if (nx > screen_size_x(s) - s->cx)
		nx = screen_size_x(s) - s->cx;
	if (nx == 0)
		return;

	if (s->cx > screen_size_x(s) - 1)
		return;

#ifdef ENABLE_SIXEL
	if (image_check_line(s, s->cy, 1) && ctx->wp != NULL)
		ctx->wp->flags |= PANE_REDRAW;
#endif

	screen_write_initctx(ctx, &ttyctx, 0);
	ttyctx.bg = bg;

	grid_view_delete_cells(s->grid, s->cx, s->cy, nx, bg);

	screen_write_collect_flush(ctx, 0, __func__);
	ttyctx.num = nx;
	tty_write(tty_cmd_deletecharacter, &ttyctx);
}

/* Clear nx characters. */
void
screen_write_clearcharacter(struct screen_write_ctx *ctx, u_int nx, u_int bg)
{
	struct screen	*s = ctx->s;
	struct tty_ctx	 ttyctx;

	if (nx == 0)
		nx = 1;

	if (nx > screen_size_x(s) - s->cx)
		nx = screen_size_x(s) - s->cx;
	if (nx == 0)
		return;

	if (s->cx > screen_size_x(s) - 1)
		return;

#ifdef ENABLE_SIXEL
	if (image_check_line(s, s->cy, 1) && ctx->wp != NULL)
		ctx->wp->flags |= PANE_REDRAW;
#endif

	screen_write_initctx(ctx, &ttyctx, 0);
	ttyctx.bg = bg;

	grid_view_clear(s->grid, s->cx, s->cy, nx, 1, bg);

	screen_write_collect_flush(ctx, 0, __func__);
	ttyctx.num = nx;
	tty_write(tty_cmd_clearcharacter, &ttyctx);
}

/* Insert ny lines. */
void
screen_write_insertline(struct screen_write_ctx *ctx, u_int ny, u_int bg)
{
	struct screen	*s = ctx->s;
	struct grid	*gd = s->grid;
	struct tty_ctx	 ttyctx;

#ifdef ENABLE_SIXEL
	u_int		 sy = screen_size_y(s);
#endif

	if (ny == 0)
		ny = 1;

#ifdef ENABLE_SIXEL
	if (image_check_line(s, s->cy, sy - s->cy) && ctx->wp != NULL)
		ctx->wp->flags |= PANE_REDRAW;
#endif

	if (s->cy < s->rupper || s->cy > s->rlower) {
		if (ny > screen_size_y(s) - s->cy)
			ny = screen_size_y(s) - s->cy;
		if (ny == 0)
			return;

		screen_write_initctx(ctx, &ttyctx, 1);
		ttyctx.bg = bg;

		grid_view_insert_lines(gd, s->cy, ny, bg);

		screen_write_collect_flush(ctx, 0, __func__);
		ttyctx.num = ny;
		tty_write(tty_cmd_insertline, &ttyctx);
		return;
	}

	if (ny > s->rlower + 1 - s->cy)
		ny = s->rlower + 1 - s->cy;
	if (ny == 0)
		return;

	screen_write_initctx(ctx, &ttyctx, 1);
	ttyctx.bg = bg;

	if (s->cy < s->rupper || s->cy > s->rlower)
		grid_view_insert_lines(gd, s->cy, ny, bg);
	else
		grid_view_insert_lines_region(gd, s->rlower, s->cy, ny, bg);

	screen_write_collect_flush(ctx, 0, __func__);

	ttyctx.num = ny;
	tty_write(tty_cmd_insertline, &ttyctx);
}

/* Delete ny lines. */
void
screen_write_deleteline(struct screen_write_ctx *ctx, u_int ny, u_int bg)
{
	struct screen	*s = ctx->s;
	struct grid	*gd = s->grid;
	struct tty_ctx	 ttyctx;
	u_int		 sy = screen_size_y(s);

	if (ny == 0)
		ny = 1;

#ifdef ENABLE_SIXEL
	if (image_check_line(s, s->cy, sy - s->cy) && ctx->wp != NULL)
		ctx->wp->flags |= PANE_REDRAW;
#endif

	if (s->cy < s->rupper || s->cy > s->rlower) {
		if (ny > sy - s->cy)
			ny = sy - s->cy;
		if (ny == 0)
			return;

		screen_write_initctx(ctx, &ttyctx, 1);
		ttyctx.bg = bg;

		grid_view_delete_lines(gd, s->cy, ny, bg);

		screen_write_collect_flush(ctx, 0, __func__);
		ttyctx.num = ny;
		tty_write(tty_cmd_deleteline, &ttyctx);
		return;
	}

	if (ny > s->rlower + 1 - s->cy)
		ny = s->rlower + 1 - s->cy;
	if (ny == 0)
		return;

	screen_write_initctx(ctx, &ttyctx, 1);
	ttyctx.bg = bg;

	if (s->cy < s->rupper || s->cy > s->rlower)
		grid_view_delete_lines(gd, s->cy, ny, bg);
	else
		grid_view_delete_lines_region(gd, s->rlower, s->cy, ny, bg);

	screen_write_collect_flush(ctx, 0, __func__);
	ttyctx.num = ny;
	tty_write(tty_cmd_deleteline, &ttyctx);
}

/* Clear line at cursor. */
void
screen_write_clearline(struct screen_write_ctx *ctx, u_int bg)
{
	struct screen			*s = ctx->s;
	struct grid_line		*gl;
	u_int				 sx = screen_size_x(s);
	struct screen_write_citem	*ci = ctx->item;

	gl = grid_get_line(s->grid, s->grid->hsize + s->cy);
	if (gl->cellsize == 0 && COLOUR_DEFAULT(bg))
		return;

#ifdef ENABLE_SIXEL
	if (image_check_line(s, s->cy, 1) && ctx->wp != NULL)
		ctx->wp->flags |= PANE_REDRAW;
#endif

	grid_view_clear(s->grid, 0, s->cy, sx, 1, bg);

	screen_write_collect_clear(ctx, s->cy, 1);
	ci->x = 0;
	ci->used = sx;
	ci->type = CLEAR;
	ci->bg = bg;
	TAILQ_INSERT_TAIL(&ctx->s->write_list[s->cy].items, ci, entry);
	ctx->item = screen_write_get_citem();
}

/* Clear to end of line from cursor. */
void
screen_write_clearendofline(struct screen_write_ctx *ctx, u_int bg)
{
	struct screen			*s = ctx->s;
	struct grid_line		*gl;
	u_int				 sx = screen_size_x(s);
	struct screen_write_citem	*ci = ctx->item, *before;

	if (s->cx == 0) {
		screen_write_clearline(ctx, bg);
		return;
	}

	gl = grid_get_line(s->grid, s->grid->hsize + s->cy);
	if (s->cx > sx - 1 || (s->cx >= gl->cellsize && COLOUR_DEFAULT(bg)))
		return;

#ifdef ENABLE_SIXEL
	if (image_check_line(s, s->cy, 1) && ctx->wp != NULL)
		ctx->wp->flags |= PANE_REDRAW;
#endif

	grid_view_clear(s->grid, s->cx, s->cy, sx - s->cx, 1, bg);

 	before = screen_write_collect_trim(ctx, s->cy, s->cx, sx - s->cx, NULL);
	ci->x = s->cx;
	ci->used = sx - s->cx;
	ci->type = CLEAR;
	ci->bg = bg;
	if (before == NULL)
		TAILQ_INSERT_TAIL(&ctx->s->write_list[s->cy].items, ci, entry);
	else
		TAILQ_INSERT_BEFORE(before, ci, entry);
	ctx->item = screen_write_get_citem();
}

/* Clear to start of line from cursor. */
void
screen_write_clearstartofline(struct screen_write_ctx *ctx, u_int bg)
{
	struct screen			 *s = ctx->s;
	u_int				 sx = screen_size_x(s);
	struct screen_write_citem	*ci = ctx->item, *before;

	if (s->cx >= sx - 1) {
		screen_write_clearline(ctx, bg);
		return;
	}

#ifdef ENABLE_SIXEL
	if (image_check_line(s, s->cy, 1) && ctx->wp != NULL)
		ctx->wp->flags |= PANE_REDRAW;
#endif

	if (s->cx > sx - 1)
		grid_view_clear(s->grid, 0, s->cy, sx, 1, bg);
	else
		grid_view_clear(s->grid, 0, s->cy, s->cx + 1, 1, bg);

	before = screen_write_collect_trim(ctx, s->cy, 0, s->cx + 1, NULL);
	ci->x = 0;
	ci->used = s->cx + 1;
	ci->type = CLEAR;
	ci->bg = bg;
	if (before == NULL)
		TAILQ_INSERT_TAIL(&ctx->s->write_list[s->cy].items, ci, entry);
	else
		TAILQ_INSERT_BEFORE(before, ci, entry);
	ctx->item = screen_write_get_citem();
}

/* Move cursor to px,py. */
void
screen_write_cursormove(struct screen_write_ctx *ctx, int px, int py,
    int origin)
{
	struct screen	*s = ctx->s;

	if (origin && py != -1 && (s->mode & MODE_ORIGIN)) {
		if ((u_int)py > s->rlower - s->rupper)
			py = s->rlower;
		else
			py += s->rupper;
	}

	if (px != -1 && (u_int)px > screen_size_x(s) - 1)
		px = screen_size_x(s) - 1;
	if (py != -1 && (u_int)py > screen_size_y(s) - 1)
		py = screen_size_y(s) - 1;

	log_debug("%s: from %u,%u to %u,%u", __func__, s->cx, s->cy, px, py);
	screen_write_set_cursor(ctx, px, py);
}

/* Reverse index (up with scroll). */
void
screen_write_reverseindex(struct screen_write_ctx *ctx, u_int bg)
{
	struct screen	*s = ctx->s;
	struct tty_ctx	 ttyctx;

	if (s->cy == s->rupper) {
#ifdef ENABLE_SIXEL
		if (image_free_all(s) && ctx->wp != NULL)
			ctx->wp->flags |= PANE_REDRAW;
#endif

		grid_view_scroll_region_down(s->grid, s->rupper, s->rlower, bg);
		screen_write_collect_flush(ctx, 0, __func__);

		screen_write_initctx(ctx, &ttyctx, 1);
		ttyctx.bg = bg;

		tty_write(tty_cmd_reverseindex, &ttyctx);
	} else if (s->cy > 0)
		screen_write_set_cursor(ctx, -1, s->cy - 1);

}

/* Set scroll region. */
void
screen_write_scrollregion(struct screen_write_ctx *ctx, u_int rupper,
    u_int rlower)
{
	struct screen	*s = ctx->s;

	if (rupper > screen_size_y(s) - 1)
		rupper = screen_size_y(s) - 1;
	if (rlower > screen_size_y(s) - 1)
		rlower = screen_size_y(s) - 1;
	if (rupper >= rlower)	/* cannot be one line */
		return;

	screen_write_collect_flush(ctx, 0, __func__);

	/* Cursor moves to top-left. */
	screen_write_set_cursor(ctx, 0, 0);

	s->rupper = rupper;
	s->rlower = rlower;
}

/* Line feed. */
void
screen_write_linefeed(struct screen_write_ctx *ctx, int wrapped, u_int bg)
{
	struct screen		*s = ctx->s;
	struct grid		*gd = s->grid;
	struct grid_line	*gl;
#ifdef ENABLE_SIXEL
	int			 redraw = 0;
#endif
	u_int			 rupper = s->rupper, rlower = s->rlower;

	gl = grid_get_line(gd, gd->hsize + s->cy);
	if (wrapped)
		gl->flags |= GRID_LINE_WRAPPED;

	log_debug("%s: at %u,%u (region %u-%u)", __func__, s->cx, s->cy,
	    rupper, rlower);

	if (bg != ctx->bg) {
		screen_write_collect_flush(ctx, 1, __func__);
		ctx->bg = bg;
	}

	if (s->cy == s->rlower) {
#ifdef ENABLE_SIXEL
		if (rlower == screen_size_y(s) - 1)
			redraw = image_scroll_up(s, 1);
		else
			redraw = image_check_line(s, rupper, rlower - rupper);
		if (redraw && ctx->wp != NULL)
			ctx->wp->flags |= PANE_REDRAW;
#endif
		grid_view_scroll_region_up(gd, s->rupper, s->rlower, bg);
		screen_write_collect_scroll(ctx, bg);
		ctx->scrolled++;
	} else if (s->cy < screen_size_y(s) - 1)
		screen_write_set_cursor(ctx, -1, s->cy + 1);
}

/* Scroll up. */
void
screen_write_scrollup(struct screen_write_ctx *ctx, u_int lines, u_int bg)
{
	struct screen	*s = ctx->s;
	struct grid	*gd = s->grid;
	u_int		 i;

	if (lines == 0)
		lines = 1;
	else if (lines > s->rlower - s->rupper + 1)
		lines = s->rlower - s->rupper + 1;

	if (bg != ctx->bg) {
		screen_write_collect_flush(ctx, 1, __func__);
		ctx->bg = bg;
	}

#ifdef ENABLE_SIXEL
	if (image_scroll_up(s, lines) && ctx->wp != NULL)
		ctx->wp->flags |= PANE_REDRAW;
#endif

	for (i = 0; i < lines; i++) {
		grid_view_scroll_region_up(gd, s->rupper, s->rlower, bg);
		screen_write_collect_scroll(ctx, bg);
	}
	ctx->scrolled += lines;
}

/* Scroll down. */
void
screen_write_scrolldown(struct screen_write_ctx *ctx, u_int lines, u_int bg)
{
	struct screen	*s = ctx->s;
	struct grid	*gd = s->grid;
	struct tty_ctx	 ttyctx;
	u_int		 i;

	screen_write_initctx(ctx, &ttyctx, 1);
	ttyctx.bg = bg;

	if (lines == 0)
		lines = 1;
	else if (lines > s->rlower - s->rupper + 1)
		lines = s->rlower - s->rupper + 1;

#ifdef ENABLE_SIXEL
	if (image_free_all(s) && ctx->wp != NULL)
		ctx->wp->flags |= PANE_REDRAW;
#endif

	for (i = 0; i < lines; i++)
		grid_view_scroll_region_down(gd, s->rupper, s->rlower, bg);

	screen_write_collect_flush(ctx, 0, __func__);
	ttyctx.num = lines;
	tty_write(tty_cmd_scrolldown, &ttyctx);
}

/* Carriage return (cursor to start of line). */
void
screen_write_carriagereturn(struct screen_write_ctx *ctx)
{
	screen_write_set_cursor(ctx, 0, -1);
}

/* Clear to end of screen from cursor. */
void
screen_write_clearendofscreen(struct screen_write_ctx *ctx, u_int bg)
{
	struct screen	*s = ctx->s;
	struct grid	*gd = s->grid;
	struct tty_ctx	 ttyctx;
	u_int		 sx = screen_size_x(s), sy = screen_size_y(s);

#ifdef ENABLE_SIXEL
	if (image_check_line(s, s->cy, sy - s->cy) && ctx->wp != NULL)
		ctx->wp->flags |= PANE_REDRAW;
#endif

	screen_write_initctx(ctx, &ttyctx, 1);
	ttyctx.bg = bg;

	/* Scroll into history if it is enabled and clearing entire screen. */
	if (s->cx == 0 &&
	    s->cy == 0 &&
	    (gd->flags & GRID_HISTORY) &&
	    ctx->wp != NULL &&
	    options_get_number(ctx->wp->options, "scroll-on-clear"))
		grid_view_clear_history(gd, bg);
	else {
		if (s->cx <= sx - 1)
			grid_view_clear(gd, s->cx, s->cy, sx - s->cx, 1, bg);
		grid_view_clear(gd, 0, s->cy + 1, sx, sy - (s->cy + 1), bg);
	}

	screen_write_collect_clear(ctx, s->cy + 1, sy - (s->cy + 1));
	screen_write_collect_flush(ctx, 0, __func__);
	tty_write(tty_cmd_clearendofscreen, &ttyctx);
}

/* Clear to start of screen. */
void
screen_write_clearstartofscreen(struct screen_write_ctx *ctx, u_int bg)
{
	struct screen	*s = ctx->s;
	struct tty_ctx	 ttyctx;
	u_int		 sx = screen_size_x(s);

#ifdef ENABLE_SIXEL
	if (image_check_line(s, 0, s->cy - 1) && ctx->wp != NULL)
		ctx->wp->flags |= PANE_REDRAW;
#endif

	screen_write_initctx(ctx, &ttyctx, 1);
	ttyctx.bg = bg;

	if (s->cy > 0)
		grid_view_clear(s->grid, 0, 0, sx, s->cy, bg);
	if (s->cx > sx - 1)
		grid_view_clear(s->grid, 0, s->cy, sx, 1, bg);
	else
		grid_view_clear(s->grid, 0, s->cy, s->cx + 1, 1, bg);

	screen_write_collect_clear(ctx, 0, s->cy);
	screen_write_collect_flush(ctx, 0, __func__);
	tty_write(tty_cmd_clearstartofscreen, &ttyctx);
}

/* Clear entire screen. */
void
screen_write_clearscreen(struct screen_write_ctx *ctx, u_int bg)
{
	struct screen	*s = ctx->s;
	struct tty_ctx	 ttyctx;
	u_int		 sx = screen_size_x(s), sy = screen_size_y(s);

#ifdef ENABLE_SIXEL
	if (image_free_all(s) && ctx->wp != NULL)
		ctx->wp->flags |= PANE_REDRAW;
#endif

	screen_write_initctx(ctx, &ttyctx, 1);
	ttyctx.bg = bg;

	/* Scroll into history if it is enabled. */
	if ((s->grid->flags & GRID_HISTORY) &&
	    ctx->wp != NULL &&
	    options_get_number(ctx->wp->options, "scroll-on-clear"))
		grid_view_clear_history(s->grid, bg);
	else
		grid_view_clear(s->grid, 0, 0, sx, sy, bg);

	screen_write_collect_clear(ctx, 0, sy);
	tty_write(tty_cmd_clearscreen, &ttyctx);
}

/* Clear entire history. */
void
screen_write_clearhistory(struct screen_write_ctx *ctx)
{
	grid_clear_history(ctx->s->grid);
}

/* Force a full redraw. */
void
screen_write_fullredraw(struct screen_write_ctx *ctx)
{
	struct tty_ctx	 ttyctx;

	screen_write_collect_flush(ctx, 0, __func__);

	screen_write_initctx(ctx, &ttyctx, 1);
	if (ttyctx.redraw_cb != NULL)
		ttyctx.redraw_cb(&ttyctx);
}

/* Trim collected items. */
static struct screen_write_citem *
screen_write_collect_trim(struct screen_write_ctx *ctx, u_int y, u_int x,
    u_int used, int *wrapped)
{
	struct screen_write_cline	*cl = &ctx->s->write_list[y];
	struct screen_write_citem	*ci, *ci2, *tmp, *before = NULL;
	u_int				 sx = x, ex = x + used - 1;
	u_int				 csx, cex;

	if (TAILQ_EMPTY(&cl->items))
		return (NULL);
	TAILQ_FOREACH_SAFE(ci, &cl->items, entry, tmp) {
		csx = ci->x;
		cex = ci->x + ci->used - 1;

		/* Item is entirely before. */
		if (cex < sx) {
			log_debug("%s: %p %u-%u before %u-%u", __func__, ci,
			    csx, cex, sx, ex);
			continue;
		}

		/* Item is entirely after. */
		if (csx > ex) {
			log_debug("%s: %p %u-%u after %u-%u", __func__, ci,
			    csx, cex, sx, ex);
			before = ci;
			break;
		}

		/* Item is entirely inside. */
		if (csx >= sx && cex <= ex) {
			log_debug("%s: %p %u-%u inside %u-%u", __func__, ci,
			    csx, cex, sx, ex);
			TAILQ_REMOVE(&cl->items, ci, entry);
			screen_write_free_citem(ci);
			if (csx == 0 && ci->wrapped && wrapped != NULL)
				*wrapped = 1;
			continue;
		}

		/* Item under the start. */
		if (csx < sx && cex >= sx && cex <= ex) {
			log_debug("%s: %p %u-%u start %u-%u", __func__, ci,
			    csx, cex, sx, ex);
			ci->used = sx - csx;
			log_debug("%s: %p now %u-%u", __func__, ci, ci->x,
			    ci->x + ci->used + 1);
			continue;
		}

		/* Item covers the end. */
		if (cex > ex && csx >= sx && csx <= ex) {
			log_debug("%s: %p %u-%u end %u-%u", __func__, ci,
			    csx, cex, sx, ex);
			ci->x = ex + 1;
			ci->used = cex - ex;
			log_debug("%s: %p now %u-%u", __func__, ci, ci->x,
			    ci->x + ci->used + 1);
			before = ci;
			break;
		}

		/* Item must cover both sides. */
		log_debug("%s: %p %u-%u under %u-%u", __func__, ci,
		    csx, cex, sx, ex);
		ci2 = screen_write_get_citem();
		ci2->type = ci->type;
		ci2->bg = ci->bg;
		memcpy(&ci2->gc, &ci->gc, sizeof ci2->gc);
		TAILQ_INSERT_AFTER(&cl->items, ci, ci2, entry);

		ci->used = sx - csx;
		ci2->x = ex + 1;
		ci2->used = cex - ex;

		log_debug("%s: %p now %u-%u (%p) and %u-%u (%p)", __func__, ci,
		    ci->x, ci->x + ci->used - 1, ci, ci2->x,
		    ci2->x + ci2->used - 1, ci2);
		before = ci2;
		break;
	}
	return (before);
}

/* Clear collected lines. */
static void
screen_write_collect_clear(struct screen_write_ctx *ctx, u_int y, u_int n)
{
	struct screen_write_cline	*cl;
	u_int				 i;

	for (i = y; i < y + n; i++) {
		cl = &ctx->s->write_list[i];
		TAILQ_CONCAT(&screen_write_citem_freelist, &cl->items, entry);
	}
}

/* Scroll collected lines up. */
static void
screen_write_collect_scroll(struct screen_write_ctx *ctx, u_int bg)
{
	struct screen			*s = ctx->s;
	struct screen_write_cline	*cl;
	u_int				 y;
	char				*saved;
	struct screen_write_citem	*ci;

	log_debug("%s: at %u,%u (region %u-%u)", __func__, s->cx, s->cy,
	    s->rupper, s->rlower);

	screen_write_collect_clear(ctx, s->rupper, 1);
	saved = ctx->s->write_list[s->rupper].data;
	for (y = s->rupper; y < s->rlower; y++) {
		cl = &ctx->s->write_list[y + 1];
		TAILQ_CONCAT(&ctx->s->write_list[y].items, &cl->items, entry);
		ctx->s->write_list[y].data = cl->data;
	}
	ctx->s->write_list[s->rlower].data = saved;

	ci = screen_write_get_citem();
	ci->x = 0;
	ci->used = screen_size_x(s);
	ci->type = CLEAR;
	ci->bg = bg;
	TAILQ_INSERT_TAIL(&ctx->s->write_list[s->rlower].items, ci, entry);
}

/* Flush collected lines. */
static void
screen_write_collect_flush(struct screen_write_ctx *ctx, int scroll_only,
    const char *from)
{
	struct screen			*s = ctx->s;
	struct screen_write_citem	*ci, *tmp;
	struct screen_write_cline	*cl;
	u_int				 y, cx, cy, last, items = 0;
	struct tty_ctx			 ttyctx;

	if (ctx->scrolled != 0) {
		log_debug("%s: scrolled %u (region %u-%u)", __func__,
		    ctx->scrolled, s->rupper, s->rlower);
		if (ctx->scrolled > s->rlower - s->rupper + 1)
			ctx->scrolled = s->rlower - s->rupper + 1;

		screen_write_initctx(ctx, &ttyctx, 1);
		ttyctx.num = ctx->scrolled;
		ttyctx.bg = ctx->bg;
		tty_write(tty_cmd_scrollup, &ttyctx);

		if (ctx->wp != NULL)
			ctx->wp->flags |= PANE_REDRAWSCROLLBAR;
	}
	ctx->scrolled = 0;
	ctx->bg = 8;

	if (scroll_only)
		return;

	cx = s->cx; cy = s->cy;
	for (y = 0; y < screen_size_y(s); y++) {
		cl = &ctx->s->write_list[y];
		last = UINT_MAX;
		TAILQ_FOREACH_SAFE(ci, &cl->items, entry, tmp) {
			if (last != UINT_MAX && ci->x <= last) {
				fatalx("collect list not in order: %u <= %u",
				    ci->x, last);
			}
			screen_write_set_cursor(ctx, ci->x, y);
			if (ci->type == CLEAR) {
				screen_write_initctx(ctx, &ttyctx, 1);
				ttyctx.bg = ci->bg;
				ttyctx.num = ci->used;
				tty_write(tty_cmd_clearcharacter, &ttyctx);
			} else {
				screen_write_initctx(ctx, &ttyctx, 0);
				ttyctx.cell = &ci->gc;
				ttyctx.wrapped = ci->wrapped;
				ttyctx.ptr = cl->data + ci->x;
				ttyctx.num = ci->used;
				tty_write(tty_cmd_cells, &ttyctx);
			}
			items++;

			TAILQ_REMOVE(&cl->items, ci, entry);
			screen_write_free_citem(ci);
			last = ci->x;
		}
	}
	s->cx = cx; s->cy = cy;

	log_debug("%s: flushed %u items (%s)", __func__, items, from);
}

/* Finish and store collected cells. */
void
screen_write_collect_end(struct screen_write_ctx *ctx)
{
	struct screen			*s = ctx->s;
	struct screen_write_citem	*ci = ctx->item, *before;
	struct screen_write_cline	*cl = &s->write_list[s->cy];
	struct grid_cell		 gc;
	u_int				 xx;
	int				 wrapped = ci->wrapped;

	if (ci->used == 0)
		return;

	before = screen_write_collect_trim(ctx, s->cy, s->cx, ci->used,
	    &wrapped);
	ci->x = s->cx;
	ci->wrapped = wrapped;
	if (before == NULL)
		TAILQ_INSERT_TAIL(&cl->items, ci, entry);
	else
		TAILQ_INSERT_BEFORE(before, ci, entry);
	ctx->item = screen_write_get_citem();

	log_debug("%s: %u %.*s (at %u,%u)", __func__, ci->used,
	    (int)ci->used, cl->data + ci->x, s->cx, s->cy);

	if (s->cx != 0) {
		for (xx = s->cx; xx > 0; xx--) {
			grid_view_get_cell(s->grid, xx, s->cy, &gc);
			if (~gc.flags & GRID_FLAG_PADDING)
				break;
			grid_view_set_cell(s->grid, xx, s->cy,
			    &grid_default_cell);
		}
		if (gc.data.width > 1) {
			grid_view_set_cell(s->grid, xx, s->cy,
			    &grid_default_cell);
		}
	}

#ifdef ENABLE_SIXEL
	if (image_check_area(s, s->cx, s->cy, ci->used, 1) && ctx->wp != NULL)
		ctx->wp->flags |= PANE_REDRAW;
#endif

	grid_view_set_cells(s->grid, s->cx, s->cy, &ci->gc, cl->data + ci->x,
	    ci->used);
	screen_write_set_cursor(ctx, s->cx + ci->used, -1);

	for (xx = s->cx; xx < screen_size_x(s); xx++) {
		grid_view_get_cell(s->grid, xx, s->cy, &gc);
		if (~gc.flags & GRID_FLAG_PADDING)
			break;
		grid_view_set_cell(s->grid, xx, s->cy, &grid_default_cell);
	}
}

/* Write cell data, collecting if necessary. */
void
screen_write_collect_add(struct screen_write_ctx *ctx,
    const struct grid_cell *gc)
{
	struct screen			*s = ctx->s;
	struct screen_write_citem	*ci;
	u_int				 sx = screen_size_x(s);
	int				 collect;

	/*
	 * Don't need to check that the attributes and whatnot are still the
	 * same - input_parse will end the collection when anything that isn't
	 * a plain character is encountered.
	 */

	collect = 1;
	if (gc->data.width != 1 || gc->data.size != 1 || *gc->data.data >= 0x7f)
		collect = 0;
	else if (gc->flags & GRID_FLAG_TAB)
		collect = 0;
	else if (gc->attr & GRID_ATTR_CHARSET)
		collect = 0;
	else if (~s->mode & MODE_WRAP)
		collect = 0;
	else if (s->mode & MODE_INSERT)
		collect = 0;
	else if (s->sel != NULL)
		collect = 0;
	if (!collect) {
		screen_write_collect_end(ctx);
		screen_write_collect_flush(ctx, 0, __func__);
		screen_write_cell(ctx, gc);
		return;
	}

	if (s->cx > sx - 1 || ctx->item->used > sx - 1 - s->cx)
		screen_write_collect_end(ctx);
	ci = ctx->item; /* may have changed */

	if (s->cx > sx - 1) {
		log_debug("%s: wrapped at %u,%u", __func__, s->cx, s->cy);
		ci->wrapped = 1;
		screen_write_linefeed(ctx, 1, 8);
		screen_write_set_cursor(ctx, 0, -1);
	}

	if (ci->used == 0)
		memcpy(&ci->gc, gc, sizeof ci->gc);
	if (ctx->s->write_list[s->cy].data == NULL)
		ctx->s->write_list[s->cy].data = xmalloc(screen_size_x(ctx->s));
	ctx->s->write_list[s->cy].data[s->cx + ci->used++] = gc->data.data[0];
}

/* Write cell data. */
void
screen_write_cell(struct screen_write_ctx *ctx, const struct grid_cell *gc)
{
	struct screen		*s = ctx->s;
	struct grid		*gd = s->grid;
	const struct utf8_data	*ud = &gc->data;
	struct grid_line	*gl;
	struct grid_cell_entry	*gce;
	struct grid_cell 	 tmp_gc, now_gc;
	struct tty_ctx		 ttyctx;
	u_int			 sx = screen_size_x(s), sy = screen_size_y(s);
	u_int		 	 width = ud->width, xx, not_wrap;
	int			 selected, skip = 1;

	/* Ignore padding cells. */
	if (gc->flags & GRID_FLAG_PADDING)
		return;

	/* Get the previous cell to check for combining. */
	if (screen_write_combine(ctx, gc) != 0)
		return;

	/* Flush any existing scrolling. */
	screen_write_collect_flush(ctx, 1, __func__);

	/* If this character doesn't fit, ignore it. */
	if ((~s->mode & MODE_WRAP) &&
	    width > 1 &&
	    (width > sx || (s->cx != sx && s->cx > sx - width)))
		return;

	/* If in insert mode, make space for the cells. */
	if (s->mode & MODE_INSERT) {
		grid_view_insert_cells(s->grid, s->cx, s->cy, width, 8);
		skip = 0;
	}

	/* Check this will fit on the current line and wrap if not. */
	if ((s->mode & MODE_WRAP) && s->cx > sx - width) {
		log_debug("%s: wrapped at %u,%u", __func__, s->cx, s->cy);
		screen_write_linefeed(ctx, 1, 8);
		screen_write_set_cursor(ctx, 0, -1);
		screen_write_collect_flush(ctx, 1, __func__);
	}

	/* Sanity check cursor position. */
	if (s->cx > sx - width || s->cy > sy - 1)
		return;
	screen_write_initctx(ctx, &ttyctx, 0);

	/* Handle overwriting of UTF-8 characters. */
	gl = grid_get_line(s->grid, s->grid->hsize + s->cy);
	if (gl->flags & GRID_LINE_EXTENDED) {
		grid_view_get_cell(gd, s->cx, s->cy, &now_gc);
		if (screen_write_overwrite(ctx, &now_gc, width))
			skip = 0;
	}

	/*
	 * If the new character is UTF-8 wide, fill in padding cells. Have
	 * already ensured there is enough room.
	 */
	for (xx = s->cx + 1; xx < s->cx + width; xx++) {
		log_debug("%s: new padding at %u,%u", __func__, xx, s->cy);
		grid_view_set_padding(gd, xx, s->cy);
		skip = 0;
	}

	/* If no change, do not draw. */
	if (skip) {
		if (s->cx >= gl->cellsize)
			skip = grid_cells_equal(gc, &grid_default_cell);
		else {
			gce = &gl->celldata[s->cx];
			if (gce->flags & GRID_FLAG_EXTENDED)
				skip = 0;
			else if (gc->flags != gce->flags)
				skip = 0;
			else if (gc->attr != gce->data.attr)
				skip = 0;
			else if (gc->fg != gce->data.fg)
				skip = 0;
			else if (gc->bg != gce->data.bg)
				skip = 0;
			else if (gc->data.width != 1)
				skip = 0;
			else if (gc->data.size != 1)
				skip = 0;
			else if (gce->data.data != gc->data.data[0])
				skip = 0;
		}
	}

	/* Update the selected flag and set the cell. */
	selected = screen_check_selection(s, s->cx, s->cy);
	if (selected && (~gc->flags & GRID_FLAG_SELECTED)) {
		memcpy(&tmp_gc, gc, sizeof tmp_gc);
		tmp_gc.flags |= GRID_FLAG_SELECTED;
		grid_view_set_cell(gd, s->cx, s->cy, &tmp_gc);
	} else if (!selected && (gc->flags & GRID_FLAG_SELECTED)) {
		memcpy(&tmp_gc, gc, sizeof tmp_gc);
		tmp_gc.flags &= ~GRID_FLAG_SELECTED;
		grid_view_set_cell(gd, s->cx, s->cy, &tmp_gc);
	} else if (!skip)
		grid_view_set_cell(gd, s->cx, s->cy, gc);
	if (selected)
		skip = 0;

	/*
	 * Move the cursor. If not wrapping, stick at the last character and
	 * replace it.
	 */
	not_wrap = !(s->mode & MODE_WRAP);
	if (s->cx <= sx - not_wrap - width)
		screen_write_set_cursor(ctx, s->cx + width, -1);
	else
		screen_write_set_cursor(ctx,  sx - not_wrap, -1);

	/* Create space for character in insert mode. */
	if (s->mode & MODE_INSERT) {
		screen_write_collect_flush(ctx, 0, __func__);
		ttyctx.num = width;
		tty_write(tty_cmd_insertcharacter, &ttyctx);
	}

	/* Write to the screen. */
	if (!skip) {
		if (selected) {
			screen_select_cell(s, &tmp_gc, gc);
			ttyctx.cell = &tmp_gc;
		} else
			ttyctx.cell = gc;
		tty_write(tty_cmd_cell, &ttyctx);
	}
}

/* Combine a UTF-8 zero-width character onto the previous if necessary. */
static int
screen_write_combine(struct screen_write_ctx *ctx, const struct grid_cell *gc)
{
	struct screen		*s = ctx->s;
	struct grid		*gd = s->grid;
	const struct utf8_data	*ud = &gc->data;
	u_int			 n, cx = s->cx, cy = s->cy;
	struct grid_cell	 last;
	struct tty_ctx		 ttyctx;
	int			 force_wide = 0, zero_width = 0;

	/*
	 * Is this character which makes no sense without being combined? If
	 * this is true then flag it here and discard the character (return 1)
	 * if we cannot combine it.
	 */
	if (utf8_is_zwj(ud))
		zero_width = 1;
	else if (utf8_is_vs(ud))
		zero_width = force_wide = 1;
	else if (ud->width == 0)
		zero_width = 1;

	/* Cannot combine empty character or at left. */
	if (ud->size < 2 || cx == 0)
		return (zero_width);
	log_debug("%s: character %.*s at %u,%u (width %u)", __func__,
	    (int)ud->size, ud->data, cx, cy, ud->width);

	/* Find the cell to combine with. */
	n = 1;
	grid_view_get_cell(gd, cx - n, cy, &last);
	if (cx != 1 && (last.flags & GRID_FLAG_PADDING)) {
		n = 2;
		grid_view_get_cell(gd, cx - n, cy, &last);
	}
	if (n != last.data.width || (last.flags & GRID_FLAG_PADDING))
		return (zero_width);

	/*
	 * Check if we need to combine characters. This could be zero width
	 * (set above), a modifier character (with an existing Unicode
	 * character) or a previous ZWJ.
	 */
	if (!zero_width) {
		if (utf8_is_modifier(ud)) {
			if (last.data.size < 2)
				return (0);
			force_wide = 1;
		} else if (!utf8_has_zwj(&last.data))
			return (0);
	}

	/* Check if this combined character would be too long. */
	if (last.data.size + ud->size > sizeof last.data.data)
		return (0);

	/* Combining; flush any pending output. */
	screen_write_collect_flush(ctx, 0, __func__);

	log_debug("%s: %.*s -> %.*s at %u,%u (offset %u, width %u)", __func__,
	    (int)ud->size, ud->data, (int)last.data.size, last.data.data,
	    cx - n, cy, n, last.data.width);

	/* Append the data. */
	memcpy(last.data.data + last.data.size, ud->data, ud->size);
	last.data.size += ud->size;

	/* Force the width to 2 for modifiers and variation selector. */
	if (last.data.width == 1 && force_wide) {
		last.data.width = 2;
		n = 2;
		cx++;
	} else
		force_wide = 0;

	/* Set the new cell. */
	grid_view_set_cell(gd, cx - n, cy, &last);
	if (force_wide)
		grid_view_set_padding(gd, cx - 1, cy);

	/*
	 * Redraw the combined cell. If forcing the cell to width 2, reset the
	 * cached cursor position in the tty, since we don't really know
	 * whether the terminal thought the character was width 1 or width 2
	 * and what it is going to do now.
	 */
	screen_write_set_cursor(ctx, cx - n, cy);
	screen_write_initctx(ctx, &ttyctx, 0);
	ttyctx.cell = &last;
	ttyctx.num = force_wide; /* reset cached cursor position */
	tty_write(tty_cmd_cell, &ttyctx);
	screen_write_set_cursor(ctx, cx, cy);

	return (1);
}

/*
 * UTF-8 wide characters are a bit of an annoyance. They take up more than one
 * cell on the screen, so following cells must not be drawn by marking them as
 * padding.
 *
 * So far, so good. The problem is, when overwriting a padding cell, or a UTF-8
 * character, it is necessary to also overwrite any other cells which covered
 * by the same character.
 */
static int
screen_write_overwrite(struct screen_write_ctx *ctx, struct grid_cell *gc,
    u_int width)
{
	struct screen		*s = ctx->s;
	struct grid		*gd = s->grid;
	struct grid_cell	 tmp_gc;
	u_int			 xx;
	int			 done = 0;

	if (gc->flags & GRID_FLAG_PADDING) {
		/*
		 * A padding cell, so clear any following and leading padding
		 * cells back to the character. Don't overwrite the current
		 * cell as that happens later anyway.
		 */
		xx = s->cx + 1;
		while (--xx > 0) {
			grid_view_get_cell(gd, xx, s->cy, &tmp_gc);
			if (~tmp_gc.flags & GRID_FLAG_PADDING)
				break;
			log_debug("%s: padding at %u,%u", __func__, xx, s->cy);
			grid_view_set_cell(gd, xx, s->cy, &grid_default_cell);
		}

		/* Overwrite the character at the start of this padding. */
		log_debug("%s: character at %u,%u", __func__, xx, s->cy);
		grid_view_set_cell(gd, xx, s->cy, &grid_default_cell);
		done = 1;
	}

	/*
	 * Overwrite any padding cells that belong to any UTF-8 characters
	 * we'll be overwriting with the current character.
	 */
	if (width != 1 ||
	    gc->data.width != 1 ||
	    gc->flags & GRID_FLAG_PADDING) {
		xx = s->cx + width - 1;
		while (++xx < screen_size_x(s)) {
			grid_view_get_cell(gd, xx, s->cy, &tmp_gc);
			if (~tmp_gc.flags & GRID_FLAG_PADDING)
				break;
			log_debug("%s: overwrite at %u,%u", __func__, xx,
			    s->cy);
			if (gc->flags & GRID_FLAG_TAB) {
				memcpy(&tmp_gc, gc, sizeof tmp_gc);
				memset(tmp_gc.data.data, 0,
				    sizeof tmp_gc.data.data);
				*tmp_gc.data.data = ' ';
				tmp_gc.data.width = tmp_gc.data.size =
				    tmp_gc.data.have = 1;
				grid_view_set_cell(gd, xx, s->cy, &tmp_gc);
			} else
				grid_view_set_cell(gd, xx, s->cy,
				    &grid_default_cell);
			done = 1;
		}
	}

	return (done);
}

/* Set external clipboard. */
void
screen_write_setselection(struct screen_write_ctx *ctx, const char *flags,
    u_char *str, u_int len)
{
	struct tty_ctx	ttyctx;

	screen_write_initctx(ctx, &ttyctx, 0);
	ttyctx.ptr = str;
	ttyctx.ptr2 = (void *)flags;
	ttyctx.num = len;

	tty_write(tty_cmd_setselection, &ttyctx);
}

/* Write unmodified string. */
void
screen_write_rawstring(struct screen_write_ctx *ctx, u_char *str, u_int len,
    int allow_invisible_panes)
{
	struct tty_ctx	ttyctx;

	screen_write_initctx(ctx, &ttyctx, 0);
	ttyctx.ptr = str;
	ttyctx.num = len;
	ttyctx.allow_invisible_panes = allow_invisible_panes;

	tty_write(tty_cmd_rawstring, &ttyctx);
}

#ifdef ENABLE_SIXEL
/* Write a SIXEL image. */
void
screen_write_sixelimage(struct screen_write_ctx *ctx, struct sixel_image *si,
    u_int bg)
{
	struct screen		*s = ctx->s;
	struct grid		*gd = s->grid;
	struct tty_ctx		 ttyctx;
	u_int			 x, y, sx, sy, cx = s->cx, cy = s->cy, i, lines;
	struct sixel_image	*new;

	sixel_size_in_cells(si, &x, &y);
	if (x > screen_size_x(s) || y > screen_size_y(s)) {
		if (x > screen_size_x(s) - cx)
			sx = screen_size_x(s) - cx;
		else
			sx = x;
		if (y > screen_size_y(s) - 1)
			sy = screen_size_y(s) - 1;
		else
			sy = y;
		new = sixel_scale(si, 0, 0, 0, y - sy, sx, sy, 1);
		sixel_free(si);
		si = new;

		/* Bail out if the image cannot be scaled. */
		if (si == NULL)
			return;
		sixel_size_in_cells(si, &x, &y);
	}

	sy = screen_size_y(s) - cy;
	if (sy < y) {
		lines = y - sy + 1;
		if (image_scroll_up(s, lines) && ctx->wp != NULL)
			ctx->wp->flags |= PANE_REDRAW;
		for (i = 0; i < lines; i++) {
			grid_view_scroll_region_up(gd, 0, screen_size_y(s) - 1,
			    bg);
			screen_write_collect_scroll(ctx, bg);
		}
		ctx->scrolled += lines;
		if (lines > cy)
			screen_write_cursormove(ctx, -1, 0, 0);
		else
			screen_write_cursormove(ctx, -1, cy - lines, 0);
	}
	screen_write_collect_flush(ctx, 0, __func__);

	screen_write_initctx(ctx, &ttyctx, 0);
	ttyctx.ptr = image_store(s, si);

	tty_write(tty_cmd_sixelimage, &ttyctx);

	screen_write_cursormove(ctx, 0, cy + y, 0);
}
#endif

/* Turn alternate screen on. */
void
screen_write_alternateon(struct screen_write_ctx *ctx, struct grid_cell *gc,
    int cursor)
{
	struct tty_ctx		 ttyctx;
	struct window_pane	*wp = ctx->wp;

	if (wp != NULL && !options_get_number(wp->options, "alternate-screen"))
		return;

	screen_write_collect_flush(ctx, 0, __func__);
	screen_alternate_on(ctx->s, gc, cursor);

	if (wp != NULL)
		layout_fix_panes(wp->window, NULL);

	screen_write_initctx(ctx, &ttyctx, 1);
	if (ttyctx.redraw_cb != NULL)
		ttyctx.redraw_cb(&ttyctx);
}

/* Turn alternate screen off. */
void
screen_write_alternateoff(struct screen_write_ctx *ctx, struct grid_cell *gc,
    int cursor)
{
	struct tty_ctx		 ttyctx;
	struct window_pane	*wp = ctx->wp;

	if (wp != NULL && !options_get_number(wp->options, "alternate-screen"))
		return;

	screen_write_collect_flush(ctx, 0, __func__);
	screen_alternate_off(ctx->s, gc, cursor);

	if (wp != NULL)
		layout_fix_panes(wp->window, NULL);

	screen_write_initctx(ctx, &ttyctx, 1);
	if (ttyctx.redraw_cb != NULL)
		ttyctx.redraw_cb(&ttyctx);
}
