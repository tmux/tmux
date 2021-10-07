/* $OpenBSD$ */

/*
 * Copyright (c) 2020 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <sys/wait.h>

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

struct popup_data {
	struct client		 *c;
	struct cmdq_item	 *item;
	int			  flags;

	struct screen		  s;
	struct colour_palette	  palette;
	struct job		 *job;
	struct input_ctx	 *ictx;
	int			  status;
	popup_close_cb		  cb;
	void			 *arg;

	struct menu		 *menu;
	struct menu_data	 *md;
	int			  close;

	/* Current position and size. */
	u_int			  px;
	u_int			  py;
	u_int			  sx;
	u_int			  sy;

	/* Preferred position and size. */
	u_int			  ppx;
	u_int			  ppy;
	u_int			  psx;
	u_int			  psy;

	enum { OFF, MOVE, SIZE }  dragging;
	u_int			  dx;
	u_int			  dy;

	u_int			  lx;
	u_int			  ly;
	u_int			  lb;
};

struct popup_editor {
	char			*path;
	popup_finish_edit_cb	 cb;
	void			*arg;
};

static const struct menu_item popup_menu_items[] = {
	{ "Close", 'q', NULL },
	{ "#{?buffer_name,Paste #[underscore]#{buffer_name},}", 'p', NULL },
	{ "", KEYC_NONE, NULL },
	{ "Fill Space", 'F', NULL },
	{ "Centre", 'C', NULL },
	{ "", KEYC_NONE, NULL },
	{ "To Horizontal Pane", 'h', NULL },
	{ "To Vertical Pane", 'v', NULL },

	{ NULL, KEYC_NONE, NULL }
};

static const struct menu_item popup_internal_menu_items[] = {
	{ "Close", 'q', NULL },
	{ "", KEYC_NONE, NULL },
	{ "Fill Space", 'F', NULL },
	{ "Centre", 'C', NULL },

	{ NULL, KEYC_NONE, NULL }
};

static void
popup_redraw_cb(const struct tty_ctx *ttyctx)
{
	struct popup_data	*pd = ttyctx->arg;

	pd->c->flags |= CLIENT_REDRAWOVERLAY;
}

static int
popup_set_client_cb(struct tty_ctx *ttyctx, struct client *c)
{
	struct popup_data	*pd = ttyctx->arg;

	if (c != pd->c)
		return (0);
	if (pd->c->flags & CLIENT_REDRAWOVERLAY)
		return (0);

	ttyctx->bigger = 0;
	ttyctx->wox = 0;
	ttyctx->woy = 0;
	ttyctx->wsx = c->tty.sx;
	ttyctx->wsy = c->tty.sy;

	if (pd->flags & POPUP_NOBORDER) {
		ttyctx->xoff = ttyctx->rxoff = pd->px;
		ttyctx->yoff = ttyctx->ryoff = pd->py;
	} else {
		ttyctx->xoff = ttyctx->rxoff = pd->px + 1;
		ttyctx->yoff = ttyctx->ryoff = pd->py + 1;
	}

	return (1);
}

static void
popup_init_ctx_cb(struct screen_write_ctx *ctx, struct tty_ctx *ttyctx)
{
	struct popup_data	*pd = ctx->arg;

	ttyctx->palette = &pd->palette;
	ttyctx->redraw_cb = popup_redraw_cb;
	ttyctx->set_client_cb = popup_set_client_cb;
	ttyctx->arg = pd;
}

static struct screen *
popup_mode_cb(__unused struct client *c, void *data, u_int *cx, u_int *cy)
{
	struct popup_data	*pd = data;

	if (pd->md != NULL)
		return (menu_mode_cb(c, pd->md, cx, cy));

	if (pd->flags & POPUP_NOBORDER) {
		*cx = pd->px + pd->s.cx;
		*cy = pd->py + pd->s.cy;
	} else {
		*cx = pd->px + 1 + pd->s.cx;
		*cy = pd->py + 1 + pd->s.cy;
	}
	return (&pd->s);
}

static int
popup_check_cb(struct client *c, void *data, u_int px, u_int py)
{
	struct popup_data	*pd = data;

	if (pd->md != NULL && menu_check_cb(c, pd->md, px, py) == 0)
		return (0);
	if (px < pd->px || px > pd->px + pd->sx - 1)
		return (1);
	if (py < pd->py || py > pd->py + pd->sy - 1)
		return (1);
	return (0);
}

static void
popup_draw_cb(struct client *c, void *data, struct screen_redraw_ctx *rctx)
{
	struct popup_data	*pd = data;
	struct tty		*tty = &c->tty;
	struct screen		 s;
	struct screen_write_ctx	 ctx;
	u_int			 i, px = pd->px, py = pd->py;
	struct colour_palette	*palette = &pd->palette;
	struct grid_cell	 gc;

	screen_init(&s, pd->sx, pd->sy, 0);
	screen_write_start(&ctx, &s);
	screen_write_clearscreen(&ctx, 8);

	if (pd->flags & POPUP_NOBORDER) {
		screen_write_cursormove(&ctx, 0, 0, 0);
		screen_write_fast_copy(&ctx, &pd->s, 0, 0, pd->sx, pd->sy);
	} else if (pd->sx > 2 && pd->sy > 2) {
		screen_write_box(&ctx, pd->sx, pd->sy);
		screen_write_cursormove(&ctx, 1, 1, 0);
		screen_write_fast_copy(&ctx, &pd->s, 0, 0, pd->sx - 2,
		    pd->sy - 2);
	}
	screen_write_stop(&ctx);

	memcpy(&gc, &grid_default_cell, sizeof gc);
	gc.fg = pd->palette.fg;
	gc.bg = pd->palette.bg;

	if (pd->md != NULL) {
		c->overlay_check = menu_check_cb;
		c->overlay_data = pd->md;
	} else {
		c->overlay_check = NULL;
		c->overlay_data = NULL;
	}
	for (i = 0; i < pd->sy; i++)
		tty_draw_line(tty, &s, 0, i, pd->sx, px, py + i, &gc, palette);
	if (pd->md != NULL) {
		c->overlay_check = NULL;
		c->overlay_data = NULL;
		menu_draw_cb(c, pd->md, rctx);
	}
	c->overlay_check = popup_check_cb;
	c->overlay_data = pd;
}

static void
popup_free_cb(struct client *c, void *data)
{
	struct popup_data	*pd = data;
	struct cmdq_item	*item = pd->item;

	if (pd->md != NULL)
		menu_free_cb(c, pd->md);

	if (pd->cb != NULL)
		pd->cb(pd->status, pd->arg);

	if (item != NULL) {
		if (cmdq_get_client(item) != NULL &&
		    cmdq_get_client(item)->session == NULL)
			cmdq_get_client(item)->retval = pd->status;
		cmdq_continue(item);
	}
	server_client_unref(pd->c);

	if (pd->job != NULL)
		job_free(pd->job);
	input_free(pd->ictx);

	screen_free(&pd->s);
	colour_palette_free(&pd->palette);

	free(pd);
}

static void
popup_resize_cb(__unused struct client *c, void *data)
{
	struct popup_data	*pd = data;
	struct tty		*tty = &c->tty;

	if (pd == NULL)
		return;
	if (pd->md != NULL)
		menu_free_cb(c, pd->md);

	/* Adjust position and size. */
	if (pd->psy > tty->sy)
		pd->sy = tty->sy;
	else
		pd->sy = pd->psy;
	if (pd->psx > tty->sx)
		pd->sx = tty->sx;
	else
		pd->sx = pd->psx;
	if (pd->ppy + pd->sy > tty->sy)
		pd->py = tty->sy - pd->sy;
	else
		pd->py = pd->ppy;
	if (pd->ppx + pd->sx > tty->sx)
		pd->px = tty->sx - pd->sx;
	else
		pd->px = pd->ppx;

	/* Avoid zero size screens. */
	if (pd->flags & POPUP_NOBORDER) {
		screen_resize(&pd->s, pd->sx, pd->sy, 0);
		if (pd->job != NULL)
			job_resize(pd->job, pd->sx, pd->sy );
	} else if (pd->sx > 2 && pd->sy > 2) {
		screen_resize(&pd->s, pd->sx - 2, pd->sy - 2, 0);
		if (pd->job != NULL)
			job_resize(pd->job, pd->sx - 2, pd->sy - 2);
	}
}

static void
popup_make_pane(struct popup_data *pd, enum layout_type type)
{
	struct client		*c = pd->c;
	struct session		*s = c->session;
	struct window		*w = s->curw->window;
	struct layout_cell	*lc;
	struct window_pane	*wp = w->active, *new_wp;
	u_int			 hlimit;
	const char		*shell;

	window_unzoom(w);

	lc = layout_split_pane(wp, type, -1, 0);
	hlimit = options_get_number(s->options, "history-limit");
	new_wp = window_add_pane(wp->window, NULL, hlimit, 0);
	layout_assign_pane(lc, new_wp, 0);

	new_wp->fd = job_transfer(pd->job, &new_wp->pid, new_wp->tty,
	    sizeof new_wp->tty);
	pd->job = NULL;

	screen_set_title(&pd->s, new_wp->base.title);
	screen_free(&new_wp->base);
	memcpy(&new_wp->base, &pd->s, sizeof wp->base);
	screen_resize(&new_wp->base, new_wp->sx, new_wp->sy, 1);
	screen_init(&pd->s, 1, 1, 0);

	shell = options_get_string(s->options, "default-shell");
	if (!checkshell(shell))
		shell = _PATH_BSHELL;
	new_wp->shell = xstrdup(shell);

	window_pane_set_event(new_wp);
	window_set_active_pane(w, new_wp, 1);
	new_wp->flags |= PANE_CHANGED;

	pd->close = 1;
}

static void
popup_menu_done(__unused struct menu *menu, __unused u_int choice,
    key_code key, void *data)
{
	struct popup_data	*pd = data;
	struct client		*c = pd->c;
	struct paste_buffer	*pb;
	const char		*buf;
	size_t			 len;

	pd->md = NULL;
	pd->menu = NULL;
	server_redraw_client(pd->c);

	switch (key) {
	case 'p':
		pb = paste_get_top(NULL);
		if (pb != NULL) {
			buf = paste_buffer_data(pb, &len);
			bufferevent_write(job_get_event(pd->job), buf, len);
		}
		break;
	case 'F':
		pd->sx = c->tty.sx;
		pd->sy = c->tty.sy;
		pd->px = 0;
		pd->py = 0;
		server_redraw_client(c);
		break;
	case 'C':
		pd->px = c->tty.sx / 2 - pd->sx / 2;
		pd->py = c->tty.sy / 2 - pd->sy / 2;
		server_redraw_client(c);
		break;
	case 'h':
		popup_make_pane(pd, LAYOUT_LEFTRIGHT);
		break;
	case 'v':
		popup_make_pane(pd, LAYOUT_TOPBOTTOM);
		break;
	case 'q':
		pd->close = 1;
		break;
	}
}

static void
popup_handle_drag(struct client *c, struct popup_data *pd,
    struct mouse_event *m)
{
	u_int	px, py;

	if (!MOUSE_DRAG(m->b))
		pd->dragging = OFF;
	else if (pd->dragging == MOVE) {
		if (m->x < pd->dx)
			px = 0;
		else if (m->x - pd->dx + pd->sx > c->tty.sx)
			px = c->tty.sx - pd->sx;
		else
			px = m->x - pd->dx;
		if (m->y < pd->dy)
			py = 0;
		else if (m->y - pd->dy + pd->sy > c->tty.sy)
			py = c->tty.sy - pd->sy;
		else
			py = m->y - pd->dy;
		pd->px = px;
		pd->py = py;
		pd->dx = m->x - pd->px;
		pd->dy = m->y - pd->py;
		pd->ppx = px;
		pd->ppy = py;
		server_redraw_client(c);
	} else if (pd->dragging == SIZE) {
		if (pd->flags & POPUP_NOBORDER) {
			if (m->x < pd->px + 1)
				return;
			if (m->y < pd->py + 1)
				return;
		} else {
			if (m->x < pd->px + 3)
				return;
			if (m->y < pd->py + 3)
				return;
		}
		pd->sx = m->x - pd->px;
		pd->sy = m->y - pd->py;
		pd->psx = pd->sx;
		pd->psy = pd->sy;

		if (pd->flags & POPUP_NOBORDER) {
			screen_resize(&pd->s, pd->sx, pd->sy, 0);
			if (pd->job != NULL)
				job_resize(pd->job, pd->sx, pd->sy);
		} else {
			screen_resize(&pd->s, pd->sx - 2, pd->sy - 2, 0);
			if (pd->job != NULL)
				job_resize(pd->job, pd->sx - 2, pd->sy - 2);
		}
		server_redraw_client(c);
	}
}

static int
popup_key_cb(struct client *c, void *data, struct key_event *event)
{
	struct popup_data	*pd = data;
	struct mouse_event	*m = &event->m;
	const char		*buf;
	size_t			 len;
	u_int			 px, py, x;
	enum { NONE, LEFT, RIGHT, TOP, BOTTOM } border = NONE;

	if (pd->md != NULL) {
		if (menu_key_cb(c, pd->md, event) == 1) {
			pd->md = NULL;
			pd->menu = NULL;
			if (pd->close)
				server_client_clear_overlay(c);
			else
				server_redraw_client(c);
		}
		return (0);
	}

	if (KEYC_IS_MOUSE(event->key)) {
		if (pd->dragging != OFF) {
			popup_handle_drag(c, pd, m);
			goto out;
		}
		if (m->x < pd->px ||
		    m->x > pd->px + pd->sx - 1 ||
		    m->y < pd->py ||
		    m->y > pd->py + pd->sy - 1) {
			if (MOUSE_BUTTONS(m->b) == 2)
				goto menu;
			return (0);
		}
		if (~pd->flags & POPUP_NOBORDER) {
			if (m->x == pd->px)
				border = LEFT;
			else if (m->x == pd->px + pd->sx - 1)
				border = RIGHT;
			else if (m->y == pd->py)
				border = TOP;
			else if (m->y == pd->py + pd->sy - 1)
				border = BOTTOM;
		}
		if ((m->b & MOUSE_MASK_MODIFIERS) == 0 &&
		    MOUSE_BUTTONS(m->b) == 2 &&
		    (border == LEFT || border == TOP))
		    goto menu;
		if (((m->b & MOUSE_MASK_MODIFIERS) == MOUSE_MASK_META) ||
		    border != NONE) {
			if (!MOUSE_DRAG(m->b))
				goto out;
			if (MOUSE_BUTTONS(m->lb) == 0)
				pd->dragging = MOVE;
			else if (MOUSE_BUTTONS(m->lb) == 2)
				pd->dragging = SIZE;
			pd->dx = m->lx - pd->px;
			pd->dy = m->ly - pd->py;
			goto out;
		}
	}
	if ((((pd->flags & (POPUP_CLOSEEXIT|POPUP_CLOSEEXITZERO)) == 0) ||
	    pd->job == NULL) &&
	    (event->key == '\033' || event->key == '\003'))
		return (1);
	if (pd->job != NULL) {
		if (KEYC_IS_MOUSE(event->key)) {
			/* Must be inside, checked already. */
			if (pd->flags & POPUP_NOBORDER) {
				px = m->x - pd->px;
				py = m->y - pd->py;
			} else {
				px = m->x - pd->px - 1;
				py = m->y - pd->py - 1;
			}
			if (!input_key_get_mouse(&pd->s, m, px, py, &buf, &len))
				return (0);
			bufferevent_write(job_get_event(pd->job), buf, len);
			return (0);
		}
		input_key(&pd->s, job_get_event(pd->job), event->key);
	}
	return (0);

menu:
	pd->menu = menu_create("");
	if (pd->flags & POPUP_INTERNAL) {
		menu_add_items(pd->menu, popup_internal_menu_items, NULL, NULL,
		    NULL);
	} else
		menu_add_items(pd->menu, popup_menu_items, NULL, NULL, NULL);
	if (m->x >= (pd->menu->width + 4) / 2)
		x = m->x - (pd->menu->width + 4) / 2;
	else
		x = 0;
	pd->md = menu_prepare(pd->menu, 0, NULL, x, m->y, c, NULL,
	    popup_menu_done, pd);
	c->flags |= CLIENT_REDRAWOVERLAY;

out:
	pd->lx = m->x;
	pd->ly = m->y;
	pd->lb = m->b;
	return (0);
}

static void
popup_job_update_cb(struct job *job)
{
	struct popup_data	*pd = job_get_data(job);
	struct evbuffer		*evb = job_get_event(job)->input;
	struct client		*c = pd->c;
	struct screen		*s = &pd->s;
	void			*data = EVBUFFER_DATA(evb);
	size_t			 size = EVBUFFER_LENGTH(evb);

	if (size == 0)
		return;

	if (pd->md != NULL) {
		c->overlay_check = menu_check_cb;
		c->overlay_data = pd->md;
	} else {
		c->overlay_check = NULL;
		c->overlay_data = NULL;
	}
	input_parse_screen(pd->ictx, s, popup_init_ctx_cb, pd, data, size);
	c->overlay_check = popup_check_cb;
	c->overlay_data = pd;

	evbuffer_drain(evb, size);
}

static void
popup_job_complete_cb(struct job *job)
{
	struct popup_data	*pd = job_get_data(job);
	int			 status;

	status = job_get_status(pd->job);
	if (WIFEXITED(status))
		pd->status = WEXITSTATUS(status);
	else if (WIFSIGNALED(status))
		pd->status = WTERMSIG(status);
	else
		pd->status = 0;
	pd->job = NULL;

	if ((pd->flags & POPUP_CLOSEEXIT) ||
	    ((pd->flags & POPUP_CLOSEEXITZERO) && pd->status == 0))
		server_client_clear_overlay(pd->c);
}

int
popup_display(int flags, struct cmdq_item *item, u_int px, u_int py, u_int sx,
    u_int sy, const char *shellcmd, int argc, char **argv, const char *cwd,
    struct client *c, struct session *s, popup_close_cb cb, void *arg)
{
	struct popup_data	*pd;
	u_int			 jx, jy;

	if (flags & POPUP_NOBORDER) {
		if (sx < 1 || sy < 1)
			return (-1);
		jx = sx;
		jy = sy;
	} else {
		if (sx < 3 || sy < 3)
			return (-1);
		jx = sx - 2;
		jy = sy - 2;
	}
	if (c->tty.sx < sx || c->tty.sy < sy)
		return (-1);

	pd = xcalloc(1, sizeof *pd);
	pd->item = item;
	pd->flags = flags;

	pd->c = c;
	pd->c->references++;

	pd->cb = cb;
	pd->arg = arg;
	pd->status = 128 + SIGHUP;

	screen_init(&pd->s, sx - 2, sy - 2, 0);
	colour_palette_init(&pd->palette);
	colour_palette_from_option(&pd->palette, global_w_options);

	pd->px = px;
	pd->py = py;
	pd->sx = sx;
	pd->sy = sy;

	pd->ppx = px;
	pd->ppy = py;
	pd->psx = sx;
	pd->psy = sy;

	pd->job = job_run(shellcmd, argc, argv, s, cwd,
	    popup_job_update_cb, popup_job_complete_cb, NULL, pd,
	    JOB_NOWAIT|JOB_PTY|JOB_KEEPWRITE, jx, jy);
	pd->ictx = input_init(NULL, job_get_event(pd->job), &pd->palette);

	server_client_set_overlay(c, 0, popup_check_cb, popup_mode_cb,
	    popup_draw_cb, popup_key_cb, popup_free_cb, popup_resize_cb, pd);
	return (0);
}

static void
popup_editor_free(struct popup_editor *pe)
{
	unlink(pe->path);
	free(pe->path);
	free(pe);
}

static void
popup_editor_close_cb(int status, void *arg)
{
	struct popup_editor	*pe = arg;
	FILE			*f;
	char			*buf = NULL;
	off_t			 len = 0;

	if (status != 0) {
		pe->cb(NULL, 0, pe->arg);
		popup_editor_free(pe);
		return;
	}

	f = fopen(pe->path, "r");
	if (f != NULL) {
		fseeko(f, 0, SEEK_END);
		len = ftello(f);
		fseeko(f, 0, SEEK_SET);

		if (len == 0 ||
		    (uintmax_t)len > (uintmax_t)SIZE_MAX ||
		    (buf = malloc(len)) == NULL ||
		    fread(buf, len, 1, f) != 1) {
			free(buf);
			buf = NULL;
			len = 0;
		}
		fclose(f);
	}
	pe->cb(buf, len, pe->arg); /* callback now owns buffer */
	popup_editor_free(pe);
}

int
popup_editor(struct client *c, const char *buf, size_t len,
    popup_finish_edit_cb cb, void *arg)
{
	struct popup_editor	*pe;
	int			 fd;
	FILE			*f;
	char			*cmd;
	char			 path[] = _PATH_TMP "tmux.XXXXXXXX";
	const char		*editor;
	u_int			 px, py, sx, sy;

	editor = options_get_string(global_options, "editor");
	if (*editor == '\0')
		return (-1);

	fd = mkstemp(path);
	if (fd == -1)
		return (-1);
	f = fdopen(fd, "w");
	if (fwrite(buf, len, 1, f) != 1) {
		fclose(f);
		return (-1);
	}
	fclose(f);

	pe = xcalloc(1, sizeof *pe);
	pe->path = xstrdup(path);
	pe->cb = cb;
	pe->arg = arg;

	sx = c->tty.sx * 9 / 10;
	sy = c->tty.sy * 9 / 10;
	px = (c->tty.sx / 2) - (sx / 2);
	py = (c->tty.sy / 2) - (sy / 2);

	xasprintf(&cmd, "%s %s", editor, path);
	if (popup_display(POPUP_INTERNAL|POPUP_CLOSEEXIT, NULL, px, py, sx, sy,
	    cmd, 0, NULL, _PATH_TMP, c, NULL, popup_editor_close_cb, pe) != 0) {
		popup_editor_free(pe);
		free(cmd);
		return (-1);
	}
	free(cmd);
	return (0);
}
