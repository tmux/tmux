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
	struct job		 *job;
	struct input_ctx	 *ictx;
	int			  status;
	popup_close_cb		  cb;
	void			 *arg;

	u_int			  px;
	u_int			  py;
	u_int			  sx;
	u_int			  sy;

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

	ttyctx->xoff = ttyctx->rxoff = pd->px + 1;
	ttyctx->yoff = ttyctx->ryoff = pd->py + 1;

	return (1);
}

static void
popup_init_ctx_cb(struct screen_write_ctx *ctx, struct tty_ctx *ttyctx)
{
	struct popup_data	*pd = ctx->arg;

	ttyctx->redraw_cb = popup_redraw_cb;
	ttyctx->set_client_cb = popup_set_client_cb;
	ttyctx->arg = pd;
}

static struct screen *
popup_mode_cb(struct client *c, u_int *cx, u_int *cy)
{
	struct popup_data	*pd = c->overlay_data;

	*cx = pd->px + 1 + pd->s.cx;
	*cy = pd->py + 1 + pd->s.cy;
	return (&pd->s);
}

static int
popup_check_cb(struct client *c, u_int px, u_int py)
{
	struct popup_data	*pd = c->overlay_data;

	if (px < pd->px || px > pd->px + pd->sx - 1)
		return (1);
	if (py < pd->py || py > pd->py + pd->sy - 1)
		return (1);
	return (0);
}

static void
popup_draw_cb(struct client *c, __unused struct screen_redraw_ctx *ctx0)
{
	struct popup_data	*pd = c->overlay_data;
	struct tty		*tty = &c->tty;
	struct screen		 s;
	struct screen_write_ctx	 ctx;
	u_int			 i, px = pd->px, py = pd->py;

	screen_init(&s, pd->sx, pd->sy, 0);
	screen_write_start(&ctx, &s);
	screen_write_clearscreen(&ctx, 8);
	screen_write_box(&ctx, pd->sx, pd->sy);
	screen_write_cursormove(&ctx, 1, 1, 0);
	screen_write_fast_copy(&ctx, &pd->s, 0, 0, pd->sx - 2, pd->sy - 2);
	screen_write_stop(&ctx);

	c->overlay_check = NULL;
	for (i = 0; i < pd->sy; i++){
		tty_draw_line(tty, &s, 0, i, pd->sx, px, py + i,
		    &grid_default_cell, NULL);
	}
	c->overlay_check = popup_check_cb;
}

static void
popup_free_cb(struct client *c)
{
	struct popup_data	*pd = c->overlay_data;
	struct cmdq_item	*item = pd->item;

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
	free(pd);
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
		server_redraw_client(c);
	} else if (pd->dragging == SIZE) {
		if (m->x < pd->px + 3)
			return;
		if (m->y < pd->py + 3)
			return;
		pd->sx = m->x - pd->px;
		pd->sy = m->y - pd->py;

		screen_resize(&pd->s, pd->sx - 2, pd->sy - 2, 0);
		if (pd->job != NULL)
			job_resize(pd->job, pd->sx - 2, pd->sy - 2);
		server_redraw_client(c);
	}
}

static int
popup_key_cb(struct client *c, struct key_event *event)
{
	struct popup_data	*pd = c->overlay_data;
	struct mouse_event	*m = &event->m;
	const char		*buf;
	size_t			 len;

	if (KEYC_IS_MOUSE(event->key)) {
		if (pd->dragging != OFF) {
			popup_handle_drag(c, pd, m);
			goto out;
		}
		if (m->x < pd->px ||
		    m->x > pd->px + pd->sx - 1 ||
		    m->y < pd->py ||
		    m->y > pd->py + pd->sy - 1) {
			if (MOUSE_BUTTONS (m->b) == 1)
				return (1);
			return (0);
		}
		if ((m->b & MOUSE_MASK_META) ||
		    m->x == pd->px ||
		    m->x == pd->px + pd->sx - 1 ||
		    m->y == pd->py ||
		    m->y == pd->py + pd->sy - 1) {
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
			if (!input_key_get_mouse(&pd->s, m, m->x - pd->px - 1,
			    m->y - pd->py - 1, &buf, &len))
				return (0);
			bufferevent_write(job_get_event(pd->job), buf, len);
			return (0);
		}
		input_key(&pd->s, job_get_event(pd->job), event->key);
	}
	return (0);

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

	c->overlay_check = NULL;
	c->tty.flags &= ~TTY_FREEZE;

	input_parse_screen(pd->ictx, s, popup_init_ctx_cb, pd, data, size);

	c->tty.flags |= TTY_FREEZE;
	c->overlay_check = popup_check_cb;

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

	if (sx < 3 || sy < 3)
		return (-1);
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

	pd->px = px;
	pd->py = py;
	pd->sx = sx;
	pd->sy = sy;

	pd->job = job_run(shellcmd, argc, argv, s, cwd,
	    popup_job_update_cb, popup_job_complete_cb, NULL, pd,
	    JOB_NOWAIT|JOB_PTY|JOB_KEEPWRITE, pd->sx - 2, pd->sy - 2);
	pd->ictx = input_init(NULL, job_get_event(pd->job));

	server_client_set_overlay(c, 0, popup_check_cb, popup_mode_cb,
	    popup_draw_cb, popup_key_cb, popup_free_cb, pd);
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
	if (popup_display(POPUP_CLOSEEXIT, NULL, px, py, sx, sy, cmd, 0, NULL,
	    _PATH_TMP, c, NULL, popup_editor_close_cb, pe) != 0) {
		popup_editor_free(pe);
		free(cmd);
		return (-1);
	}
	free(cmd);
	return (0);
}
