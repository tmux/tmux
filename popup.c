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

#include "tmux.h"

struct popup_data {
	struct client		 *c;
	struct cmdq_item	 *item;
	int			  flags;

	char			**lines;
	u_int			  nlines;

	char			 *cmd;
	struct cmd_find_state	  fs;
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

	if (pd->c->flags & CLIENT_REDRAWOVERLAY)
		return (-1);

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

static void
popup_write_screen(struct client *c, struct popup_data *pd)
{
	struct cmdq_item	*item = pd->item;
	struct screen_write_ctx	 ctx;
	char			*copy, *next, *loop, *tmp;
	struct format_tree	*ft;
	u_int			 i, y;

	ft = format_create(c, item, FORMAT_NONE, 0);
	if (cmd_find_valid_state(&pd->fs))
		format_defaults(ft, c, pd->fs.s, pd->fs.wl, pd->fs.wp);
	else
		format_defaults(ft, c, NULL, NULL, NULL);

	screen_write_start(&ctx, &pd->s);
	screen_write_clearscreen(&ctx, 8);

	y = 0;
	for (i = 0; i < pd->nlines; i++) {
		if (y == pd->sy - 2)
			break;
		copy = next = xstrdup(pd->lines[i]);
		while ((loop = strsep(&next, "\n")) != NULL) {
			if (y == pd->sy - 2)
				break;
			tmp = format_expand(ft, loop);
			screen_write_cursormove(&ctx, 0, y, 0);
			format_draw(&ctx, &grid_default_cell, pd->sx - 2, tmp,
			    NULL);
			free(tmp);
			y++;
		}
		free(copy);
	}

	format_free(ft);
	screen_write_cursormove(&ctx, 0, y, 0);
	screen_write_stop(&ctx);
}

static struct screen *
popup_mode_cb(struct client *c, u_int *cx, u_int *cy)
{
	struct popup_data	*pd = c->overlay_data;

	if (pd->ictx == NULL)
		return (0);
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
	u_int			 i;

	if (pd->cb != NULL)
		pd->cb(pd->status, pd->arg);

	if (item != NULL) {
		if (pd->ictx != NULL &&
		    cmdq_get_client(item) != NULL &&
		    cmdq_get_client(item)->session == NULL)
			cmdq_get_client(item)->retval = pd->status;
		cmdq_continue(item);
	}
	server_client_unref(pd->c);

	if (pd->job != NULL)
		job_free(pd->job);
	if (pd->ictx != NULL)
		input_free(pd->ictx);

	for (i = 0; i < pd->nlines; i++)
		free(pd->lines[i]);
	free(pd->lines);

	screen_free(&pd->s);
	free(pd->cmd);
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

		screen_resize(&pd->s, pd->sx, pd->sy, 0);
		if (pd->ictx == NULL)
			popup_write_screen(c, pd);
		else if (pd->job != NULL)
			job_resize(pd->job, pd->sx - 2, pd->sy - 2);
		server_redraw_client(c);
	}
}

static int
popup_key_cb(struct client *c, struct key_event *event)
{
	struct popup_data	*pd = c->overlay_data;
	struct mouse_event	*m = &event->m;
	struct cmd_find_state	*fs = &pd->fs;
	struct format_tree	*ft;
	const char		*cmd, *buf;
	size_t			 len;
	struct cmdq_state	*state;
	enum cmd_parse_status	 status;
	char			*error;

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

	if (pd->ictx != NULL && (pd->flags & POPUP_WRITEKEYS)) {
		if (((pd->flags & (POPUP_CLOSEEXIT|POPUP_CLOSEEXITZERO)) == 0 ||
		    pd->job == NULL) &&
		    (event->key == '\033' || event->key == '\003'))
			return (1);
		if (pd->job == NULL)
			return (0);
		if (KEYC_IS_MOUSE(event->key)) {
			/* Must be inside, checked already. */
			if (!input_key_get_mouse(&pd->s, m, m->x - pd->px,
			    m->y - pd->py, &buf, &len))
				return (0);
			bufferevent_write(job_get_event(pd->job), buf, len);
			return (0);
		}
		input_key(NULL, &pd->s, job_get_event(pd->job), event->key);
		return (0);
	}

	if (pd->cmd == NULL)
		return (1);

	ft = format_create(NULL, pd->item, FORMAT_NONE, 0);
	if (cmd_find_valid_state(fs))
		format_defaults(ft, c, fs->s, fs->wl, fs->wp);
	else
		format_defaults(ft, c, NULL, NULL, NULL);
	format_add(ft, "popup_key", "%s", key_string_lookup_key(event->key));
	if (KEYC_IS_MOUSE(event->key)) {
		format_add(ft, "popup_mouse", "1");
		format_add(ft, "popup_mouse_x", "%u", m->x - pd->px);
		format_add(ft, "popup_mouse_y", "%u", m->y - pd->py);
	}
	cmd = format_expand(ft, pd->cmd);
	format_free(ft);

	if (pd->item != NULL)
		event = cmdq_get_event(pd->item);
	else
		event = NULL;
	state = cmdq_new_state(&pd->fs, event, 0);

	status = cmd_parse_and_append(cmd, NULL, c, state, &error);
	if (status == CMD_PARSE_ERROR) {
		cmdq_append(c, cmdq_get_error(error));
		free(error);
	}
	cmdq_free_state(state);

	return (1);

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

u_int
popup_height(u_int nlines, const char **lines)
{
	char	*copy, *next, *loop;
	u_int	 i, height = 0;

	for (i = 0; i < nlines; i++) {
		copy = next = xstrdup(lines[i]);
		while ((loop = strsep(&next, "\n")) != NULL)
			height++;
		free(copy);
	}

	return (height);
}

u_int
popup_width(struct cmdq_item *item, u_int nlines, const char **lines,
    struct client *c, struct cmd_find_state *fs)
{
	char			*copy, *next, *loop, *tmp;
	struct format_tree	*ft;
	u_int			 i, width = 0, tmpwidth;

	ft = format_create(cmdq_get_client(item), item, FORMAT_NONE, 0);
	if (fs != NULL && cmd_find_valid_state(fs))
		format_defaults(ft, c, fs->s, fs->wl, fs->wp);
	else
		format_defaults(ft, c, NULL, NULL, NULL);

	for (i = 0; i < nlines; i++) {
		copy = next = xstrdup(lines[i]);
		while ((loop = strsep(&next, "\n")) != NULL) {
			tmp = format_expand(ft, loop);
			tmpwidth = format_width(tmp);
			if (tmpwidth > width)
				width = tmpwidth;
			free(tmp);
		}
		free(copy);
	}

	format_free(ft);
	return (width);
}

int
popup_display(int flags, struct cmdq_item *item, u_int px, u_int py, u_int sx,
    u_int sy, u_int nlines, const char **lines, const char *shellcmd,
    const char *cmd, const char *cwd, struct client *c,
    struct cmd_find_state *fs, popup_close_cb cb, void *arg)
{
	struct popup_data	*pd;
	u_int			 i;
	struct session		*s;
	int			 jobflags;

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

	if (fs != NULL)
		cmd_find_copy_state(&pd->fs, fs);
	screen_init(&pd->s, sx - 2, sy - 2, 0);

	if (cmd != NULL)
		pd->cmd = xstrdup(cmd);

	pd->px = px;
	pd->py = py;
	pd->sx = sx;
	pd->sy = sy;

	pd->nlines = nlines;
	if (pd->nlines != 0)
		pd->lines = xreallocarray(NULL, pd->nlines, sizeof *pd->lines);

	for (i = 0; i < pd->nlines; i++)
		pd->lines[i] = xstrdup(lines[i]);
	popup_write_screen(c, pd);

	if (shellcmd != NULL) {
		if (fs != NULL)
			s = fs->s;
		else
			s = NULL;
		jobflags = JOB_NOWAIT|JOB_PTY;
		if (flags & POPUP_WRITEKEYS)
		    jobflags |= JOB_KEEPWRITE;
		pd->job = job_run(shellcmd, s, cwd, popup_job_update_cb,
		    popup_job_complete_cb, NULL, pd, jobflags, pd->sx - 2,
		    pd->sy - 2);
		pd->ictx = input_init(NULL, job_get_event(pd->job));
	}

	server_client_set_overlay(c, 0, popup_check_cb, popup_mode_cb,
	    popup_draw_cb, popup_key_cb, popup_free_cb, pd);
	return (0);
}
