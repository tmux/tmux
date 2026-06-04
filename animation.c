/* $OpenBSD$ */

/*
 * Animated transitions for tmux. Frames are paced by an animation_timer
 * libevent; each tick advances state and forces a client redraw, which
 * invokes overlay_draw to paint the frame.
 */

#include <sys/types.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

enum animation_kind {
	ANIM_SLIDE_WINDOW,
	ANIM_PANE_LAYOUT,
};

enum pane_anim_phase {
	PANE_RESIZE,
	PANE_BORN,
	PANE_DYING,
};

struct pane_anim {
	int			 pane_id;
	enum pane_anim_phase	 phase;

	int			 src_x, src_y, src_w, src_h;
	int			 tgt_x, tgt_y, tgt_w, tgt_h;

	struct screen		*snapshot;	/* DYING only */
	struct colour_palette	 snapshot_palette;
};

struct pane_layout_capture {
	struct window		*w;
	struct pane_anim	*panes;
	size_t			 n;
	size_t			 cap;
	int			 dying_id;
	struct screen		*dying_snapshot;
	struct colour_palette	 dying_palette;
};

enum animation_easing {
	ANIM_EASE_SMOOTHDAMP,
	ANIM_EASE_LINEAR,
	ANIM_EASE_IN_OUT,
};

struct animation {
	struct client		*client;
	enum animation_kind	 kind;
	enum animation_easing	 easing;

	uint64_t		 start_ms;
	uint64_t		 last_ms;
	uint64_t		 duration_ms;
	uint64_t		 tau_ms;
	uint64_t		 frame_interval_ms;

	struct winlink		*src_wl;
	struct winlink		*tgt_wl;
	int			 axis;

	double			 pos;
	double			 vel;
	double			 start_pos;
	double			 target_pos;

	u_int			 sx;
	u_int			 sy;
	u_int			 pane_y0;
	u_int			 pane_h;

	int			 status_active;
	int			 status_y0;
	int			 src_idx;
	int			 tgt_idx;
	int			 src_row, src_start, src_end;
	int			 tgt_row, tgt_start, tgt_end;

	/* ANIM_PANE_LAYOUT */
	struct window		*pl_window;
	struct pane_anim	*pl_panes;
	size_t			 pl_n;

	struct visible_ranges	 vis;
};

static void animation_frame_cb(int, short, void *);
static struct visible_ranges *animation_check_cb(struct client *, void *,
		    u_int, u_int, u_int);
static void	animation_draw_cb(struct client *, void *,
		    struct screen_redraw_ctx *);
static int	animation_key_cb(struct client *, void *, struct key_event *);
static void	animation_free_cb(struct client *, void *);
static void	animation_resize_cb(struct client *, void *);
static void	animation_scan_status(struct client *, struct animation *);
static void	animation_free_capture(struct client *);
static void	animation_clone_screen(struct screen *, struct screen *);
static void	animation_pane_draw(struct client *, struct animation *,
		    double);

static enum animation_easing
animation_easing_lookup(struct session *s)
{
	int	n = options_get_number(s->options, "animation-easing");

	switch (n) {
	case 0: return (ANIM_EASE_SMOOTHDAMP);
	case 1: return (ANIM_EASE_LINEAR);
	case 2: return (ANIM_EASE_IN_OUT);
	}
	return (ANIM_EASE_SMOOTHDAMP);
}

void
animation_begin_window_switch(struct client *c, struct winlink *src,
    struct winlink *tgt)
{
	struct animation	*a;
	struct session		*s;
	int			 mode;
	uint64_t		 now;
	struct timeval		 tv;

	if (c == NULL || src == NULL || tgt == NULL || src == tgt)
		return;
	if ((s = c->session) == NULL)
		return;

	if (!options_get_number(s->options, "animation-enable"))
		return;
	mode = options_get_number(s->options, "animation-window-switch");
	if (mode == 0)
		return;

	if (c->overlay_draw != NULL && c->animation == NULL)
		return;

	if (c->animation != NULL) {
		animation_retarget(c, tgt);
		return;
	}

	a = xcalloc(1, sizeof *a);
	a->client = c;
	a->kind = ANIM_SLIDE_WINDOW;
	a->easing = animation_easing_lookup(s);
	a->duration_ms = options_get_number(s->options, "animation-duration");
	a->tau_ms = options_get_number(s->options, "animation-tau");
	a->frame_interval_ms =
	    options_get_number(s->options, "animation-frame-interval");

	a->src_wl = src;
	a->tgt_wl = tgt;
	a->axis = (tgt->idx > src->idx) ? +1 : -1;

	a->sx = c->tty.sx;
	a->sy = c->tty.sy;
	if (a->sx == 0 || a->sy == 0) {
		free(a);
		return;
	}

	{
		struct window_pane	*wp;
		u_int			 y0 = a->sy;
		u_int			 y1 = 0;
		int			 any = 0;

		TAILQ_FOREACH(wp, &src->window->panes, entry) {
			if (wp->yoff >= 0 && (u_int)wp->yoff < y0)
				y0 = (u_int)wp->yoff;
			if (wp->yoff >= 0 &&
			    (u_int)wp->yoff + wp->sy > y1)
				y1 = (u_int)wp->yoff + wp->sy;
			any = 1;
		}
		TAILQ_FOREACH(wp, &tgt->window->panes, entry) {
			if (wp->yoff >= 0 && (u_int)wp->yoff < y0)
				y0 = (u_int)wp->yoff;
			if (wp->yoff >= 0 &&
			    (u_int)wp->yoff + wp->sy > y1)
				y1 = (u_int)wp->yoff + wp->sy;
			any = 1;
		}
		if (!any || y1 <= y0) {
			free(a);
			return;
		}
		a->pane_y0 = y0;
		a->pane_h = y1 - y0;
	}

	a->target_pos = 0;
	a->start_pos = (double)a->sx * a->axis;
	a->pos = a->start_pos;
	a->vel = 0;

	a->src_idx = src->idx;
	a->tgt_idx = tgt->idx;
	animation_scan_status(c, a);

	now = get_timer();
	a->start_ms = now;
	a->last_ms = now;

	c->animation = a;

	server_client_set_overlay(c, 0, animation_check_cb, NULL,
	    animation_draw_cb, animation_key_cb, animation_free_cb,
	    animation_resize_cb, a);

	evtimer_set(&c->animation_timer, animation_frame_cb, c);
	tv.tv_sec = 0;
	tv.tv_usec = (long)a->frame_interval_ms * 1000L;
	evtimer_add(&c->animation_timer, &tv);
}

void
animation_retarget(struct client *c, struct winlink *new_tgt)
{
	struct animation	*a;
	struct winlink		*new_src;
	uint64_t		 now;

	if (c == NULL || (a = c->animation) == NULL || new_tgt == NULL)
		return;
	if (new_tgt == a->tgt_wl)
		return;
	new_src = a->tgt_wl;
	if (new_tgt == new_src)
		return;

	now = get_timer();

	a->src_wl = new_src;
	a->tgt_wl = new_tgt;
	a->axis = (new_tgt->idx > new_src->idx) ? +1 : -1;

	a->start_pos = (double)a->sx * a->axis;
	a->pos = a->start_pos;
	a->target_pos = 0;
	a->vel = 0;
	a->start_ms = now;
	a->last_ms = now;

	a->src_idx = new_src->idx;
	a->tgt_idx = new_tgt->idx;
	animation_scan_status(c, a);
}

void
animation_cancel(struct client *c)
{
	if (c == NULL || c->animation == NULL)
		return;
	if (event_initialized(&c->animation_timer))
		evtimer_del(&c->animation_timer);
	server_client_clear_overlay(c);
}

int
animation_active(struct client *c)
{
	return (c != NULL && c->animation != NULL);
}

static struct visible_ranges *
animation_check_cb(struct client *c, void *data, u_int px, u_int py, u_int nx)
{
	struct animation	*a = data;
	struct visible_ranges	*r = &a->vis;

	(void)c;

	server_client_ensure_ranges(r, 1);
	r->used = 1;

	if (py < a->pane_y0 || py >= a->pane_y0 + a->pane_h) {
		r->ranges[0].px = px;
		r->ranges[0].nx = nx;
	} else {
		r->ranges[0].px = 0;
		r->ranges[0].nx = 0;
	}
	return (r);
}

static void
animation_paint_window(struct client *c, struct window *w, int dx,
    u_int view_w, u_int view_y0, u_int view_h)
{
	struct window_pane	*wp;
	struct grid_cell	 defaults;
	int			 dst_x, src_px, nx;
	u_int			 sy, vy;

	if (w == NULL)
		return;

	memcpy(&defaults, &grid_default_cell, sizeof defaults);

	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (wp->screen == NULL)
			continue;

		dst_x = (int)wp->xoff + dx;
		src_px = 0;
		nx = (int)wp->sx;
		if (dst_x < 0) { src_px = -dst_x; nx -= src_px; dst_x = 0; }
		if (dst_x + nx > (int)view_w) nx = (int)view_w - dst_x;
		if (nx <= 0)
			continue;

		for (sy = 0; sy < wp->sy; sy++) {
			vy = wp->yoff + sy;
			if (vy < view_y0 || vy >= view_y0 + view_h)
				continue;
			tty_draw_line(&c->tty, wp->screen,
			    (u_int)src_px, sy, (u_int)nx,
			    (u_int)dst_x, vy,
			    &defaults, &wp->palette);
		}
	}
}

static struct window_pane *
animation_window_default_cell(struct window *w, struct grid_cell *out)
{
	struct window_pane	*wp;

	memcpy(out, &grid_default_cell, sizeof *out);
	if (w == NULL || (wp = w->active) == NULL)
		return (NULL);
	tty_default_colours(out, wp);
	return (wp);
}

/* Locate src/tgt window tab ranges in the current status entries. */
static void
animation_scan_status(struct client *c, struct animation *a)
{
	struct status_line	*sl = &c->status;
	struct style_range	*sr;
	int			 y, lines;
	int			 found_src = 0, found_tgt = 0;

	a->status_active = 0;
	if (!options_get_number(c->session->options,
	    "animation-status-highlight"))
		return;
	lines = status_line_size(c);
	if (lines <= 0)
		return;
	a->status_y0 = status_at_line(c);
	if (a->status_y0 < 0)
		return;

	for (y = 0; y < lines; y++) {
		TAILQ_FOREACH(sr, &sl->entries[y].ranges, entry) {
			if (sr->type != STYLE_RANGE_WINDOW)
				continue;
			if (!found_src &&
			    (int)sr->argument == a->src_idx) {
				a->src_row = y;
				a->src_start = sr->start;
				a->src_end = sr->end;
				found_src = 1;
			}
			if (!found_tgt &&
			    (int)sr->argument == a->tgt_idx) {
				a->tgt_row = y;
				a->tgt_start = sr->start;
				a->tgt_end = sr->end;
				found_tgt = 1;
			}
		}
	}
	if (found_src && found_tgt)
		a->status_active = 1;
}

static void
animation_paint_status_copy_cell(struct client *c, u_int sx, u_int sy,
    u_int tx, u_int ty)
{
	struct grid_cell	gc, defaults;

	grid_view_get_cell(c->status.screen.grid, sx, sy, &gc);
	if (gc.flags & GRID_FLAG_PADDING)
		return;
	memcpy(&defaults, &grid_default_cell, sizeof defaults);
	tty_attributes(&c->tty, &gc, &defaults, NULL, NULL);
	tty_cursor(&c->tty, tx, ty);
	if (gc.data.size == 0)
		tty_putc(&c->tty, ' ');
	else
		tty_putn(&c->tty, gc.data.data, gc.data.size, gc.data.width);
}

static void
animation_paint_status_cell(struct client *c, u_int sx, u_int sy, u_int tx,
    u_int ty, const struct grid_cell *style)
{
	struct grid_cell	gc, defaults;

	grid_view_get_cell(c->status.screen.grid, sx, sy, &gc);
	if (gc.flags & GRID_FLAG_PADDING)
		return;
	gc.fg = style->fg;
	gc.bg = style->bg;
	gc.attr = style->attr;
	memcpy(&defaults, &grid_default_cell, sizeof defaults);
	tty_attributes(&c->tty, &gc, &defaults, NULL, NULL);
	tty_cursor(&c->tty, tx, ty);
	if (gc.data.size == 0)
		tty_putc(&c->tty, ' ');
	else
		tty_putn(&c->tty, gc.data.data, gc.data.size, gc.data.width);
}

static void
animation_draw_status(struct client *c, struct animation *a, double t)
{
	struct grid_cell	inactive_style;
	double			 lstart;
	int			 lerp_x, lerp_w, tgt_w;
	int			 row, ty, x, i;

	if (!a->status_active)
		return;
	if (a->src_row != a->tgt_row)
		return;
	row = a->src_row;
	ty = a->status_y0 + row;

	tgt_w = a->tgt_end - a->tgt_start;
	if (tgt_w <= 0)
		return;
	lerp_w = tgt_w;
	lstart = (double)a->src_start + t *
	    ((double)a->tgt_start - a->src_start);
	lerp_x = (int)lround(lstart);

	/*
	 * Inactive sample taken from a column that is neither under the
	 * moving highlight nor at tgt's natural highlight - this prevents
	 * sampling our own paint output on a previous frame.
	 */
	{
		u_int imid = a->src_start + (a->src_end - a->src_start) / 2;
		if ((int)imid >= lerp_x && (int)imid < lerp_x + lerp_w)
			imid = a->src_end;
		grid_view_get_cell(c->status.screen.grid, imid, row,
		    &inactive_style);
	}

	/* Neutralize tgt's natural highlight where the lerp isn't covering. */
	for (x = a->tgt_start; x < a->tgt_end; x++) {
		if (x >= lerp_x && x < lerp_x + lerp_w)
			continue;
		animation_paint_status_cell(c, (u_int)x, (u_int)row,
		    (u_int)x, (u_int)ty, &inactive_style);
	}

	/* Copy tgt's styled cells, in order, to the lerp position. */
	for (i = 0; i < lerp_w; i++) {
		int dst_x = lerp_x + i;
		int src_col = a->tgt_start + i;

		if (dst_x < 0 || dst_x >= (int)c->tty.sx)
			continue;
		if (src_col < 0 || src_col >= a->tgt_end)
			continue;
		animation_paint_status_copy_cell(c, (u_int)src_col,
		    (u_int)row, (u_int)dst_x, (u_int)ty);
	}
}

static void
animation_draw_cb(struct client *c, void *data,
    struct screen_redraw_ctx *rctx)
{
	struct animation	*a = data;
	int			 src_dx, tgt_dx;
	overlay_check_cb	 saved_check;
	void			*saved_data;
	struct window_pane	*src_wp, *tgt_wp;
	struct grid_cell	 src_cell, tgt_cell;
	int			 src_l, src_r, tgt_l, tgt_r;
	u_int			 vy;

	(void)rctx;

	if (a->kind == ANIM_PANE_LAYOUT) {
		double t;
		if (a->duration_ms == 0)
			t = 1;
		else {
			uint64_t now = get_timer();
			t = (double)(now - a->start_ms) /
			    (double)a->duration_ms;
		}
		if (t < 0) t = 0;
		if (t > 1) t = 1;
		saved_check = c->overlay_check;
		saved_data = c->overlay_data;
		c->overlay_check = NULL;
		c->overlay_data = NULL;
		animation_pane_draw(c, a, t);
		c->overlay_check = saved_check;
		c->overlay_data = saved_data;
		return;
	}

	if (a->src_wl == NULL || a->tgt_wl == NULL)
		return;
	if (a->src_wl->window == NULL || a->tgt_wl->window == NULL)
		return;

	/*
	 * pos lerps from sx*axis toward 0. axis = +1 makes tgt enter from
	 * the right and src exit to the left; axis = -1 reverses both.
	 */
	tgt_dx = (int)lround(a->pos);
	src_dx = (int)lround(a->pos - (double)a->sx * a->axis);

	/* Suppress our own visibility report during nested paint calls. */
	saved_check = c->overlay_check;
	saved_data = c->overlay_data;
	c->overlay_check = NULL;
	c->overlay_data = NULL;

	src_wp = animation_window_default_cell(a->src_wl->window, &src_cell);
	tgt_wp = animation_window_default_cell(a->tgt_wl->window, &tgt_cell);

	src_l = src_dx > 0 ? src_dx : 0;
	src_r = src_dx + (int)a->sx;
	if (src_r > (int)a->sx) src_r = (int)a->sx;
	tgt_l = tgt_dx > 0 ? tgt_dx : 0;
	tgt_r = tgt_dx + (int)a->sx;
	if (tgt_r > (int)a->sx) tgt_r = (int)a->sx;

	for (vy = a->pane_y0; vy < a->pane_y0 + a->pane_h; vy++) {
		if (src_wp != NULL && src_r > src_l) {
			tty_attributes(&c->tty, &src_cell, &src_cell,
			    &src_wp->palette, NULL);
			tty_cursor(&c->tty, (u_int)src_l, vy);
			tty_repeat_space(&c->tty, (u_int)(src_r - src_l));
		}
		if (tgt_wp != NULL && tgt_r > tgt_l) {
			tty_attributes(&c->tty, &tgt_cell, &tgt_cell,
			    &tgt_wp->palette, NULL);
			tty_cursor(&c->tty, (u_int)tgt_l, vy);
			tty_repeat_space(&c->tty, (u_int)(tgt_r - tgt_l));
		}
	}

	animation_paint_window(c, a->src_wl->window, src_dx, a->sx,
	    a->pane_y0, a->pane_h);
	animation_paint_window(c, a->tgt_wl->window, tgt_dx, a->sx,
	    a->pane_y0, a->pane_h);

	if (a->status_active && a->duration_ms > 0) {
		uint64_t now = get_timer();
		double	 t = (double)(now - a->start_ms) /
		    (double)a->duration_ms;
		if (t < 0) t = 0;
		if (t > 1) t = 1;
		animation_draw_status(c, a, t);
	}

	c->overlay_check = saved_check;
	c->overlay_data = saved_data;
}

/* Passthrough: stay alive, let the key reach normal dispatch. */
static int
animation_key_cb(__unused struct client *c, __unused void *data,
    __unused struct key_event *event)
{
	return (2);
}

static void
animation_free_cb(struct client *c, void *data)
{
	struct animation	*a = data;
	size_t			 i;

	if (c->animation == a)
		c->animation = NULL;
	for (i = 0; i < a->pl_n; i++) {
		if (a->pl_panes[i].snapshot != NULL) {
			screen_free(a->pl_panes[i].snapshot);
			free(a->pl_panes[i].snapshot);
		}
	}
	free(a->pl_panes);
	free(a->vis.ranges);
	free(a);
}

static void
animation_resize_cb(struct client *c, __unused void *data)
{
	animation_cancel(c);
}

/* pos += (1 - exp(-dt/tau)) * (target - pos) */
static void
animation_step_smoothdamp(struct animation *a, double dt_ms)
{
	double	tau = (double)a->tau_ms;
	double	alpha;
	double	prev = a->pos;

	if (tau <= 0)
		tau = 1;
	alpha = 1.0 - exp(-dt_ms / tau);
	a->pos += alpha * (a->target_pos - a->pos);
	if (dt_ms > 0)
		a->vel = (a->pos - prev) / dt_ms;
}

static void
animation_step_linear(struct animation *a, uint64_t now)
{
	double	t;

	if (a->duration_ms == 0) {
		a->pos = a->target_pos;
		return;
	}
	t = (double)(now - a->start_ms) / (double)a->duration_ms;
	if (t < 0) t = 0;
	if (t > 1) t = 1;
	a->pos = a->start_pos + t * (a->target_pos - a->start_pos);
}

static double
animation_ease_cubic_in_out(double t)
{
	if (t < 0.5)
		return (4.0 * t * t * t);
	t = 2.0 * t - 2.0;
	return (0.5 * t * t * t + 1.0);
}

static void
animation_step_ease_in_out(struct animation *a, uint64_t now)
{
	double	t, e;

	if (a->duration_ms == 0) {
		a->pos = a->target_pos;
		return;
	}
	t = (double)(now - a->start_ms) / (double)a->duration_ms;
	if (t < 0) t = 0;
	if (t > 1) t = 1;
	e = animation_ease_cubic_in_out(t);
	a->pos = a->start_pos + e * (a->target_pos - a->start_pos);
}

static int
animation_is_done(const struct animation *a, uint64_t now)
{
	return (now - a->start_ms >= a->duration_ms);
}

static void
animation_frame_cb(__unused int fd, __unused short ev, void *arg)
{
	struct client		*c = arg;
	struct animation	*a;
	uint64_t		 now;
	double			 dt_ms;
	struct timeval		 tv;

	if ((a = c->animation) == NULL)
		return;

	now = get_timer();
	dt_ms = (double)(now - a->last_ms);
	if (dt_ms < 0) dt_ms = 0;
	a->last_ms = now;

	if (a->kind == ANIM_PANE_LAYOUT)
		goto check_done;

	switch (a->easing) {
	case ANIM_EASE_SMOOTHDAMP:
		animation_step_smoothdamp(a, dt_ms);
		break;
	case ANIM_EASE_LINEAR:
		animation_step_linear(a, now);
		break;
	case ANIM_EASE_IN_OUT:
		animation_step_ease_in_out(a, now);
		break;
	}

check_done:
	if (animation_is_done(a, now)) {
		a->pos = a->target_pos;
		animation_cancel(c);
		return;
	}

	server_redraw_client(c);
	tv.tv_sec = 0;
	tv.tv_usec = (long)a->frame_interval_ms * 1000L;
	evtimer_add(&c->animation_timer, &tv);
}

/* ---- pane layout animations ---- */

static void
animation_clone_screen(struct screen *dst, struct screen *src)
{
	struct screen_write_ctx	ctx;
	u_int			 sx = screen_size_x(src);
	u_int			 sy = screen_size_y(src);

	screen_init(dst, sx, sy, 0);
	screen_write_start(&ctx, dst);
	screen_write_cursormove(&ctx, 0, 0, 0);
	screen_write_fast_copy(&ctx, src, 0, 0, sx, sy);
	screen_write_stop(&ctx);
}

static void
animation_free_capture(struct client *c)
{
	struct pane_layout_capture	*cap = c->animation_capture;

	if (cap == NULL)
		return;
	if (cap->dying_snapshot != NULL) {
		screen_free(cap->dying_snapshot);
		free(cap->dying_snapshot);
	}
	free(cap->panes);
	free(cap);
	c->animation_capture = NULL;
}

void
animation_begin_pane_layout(struct client *c, struct window *w,
    struct window_pane *dying_wp)
{
	struct pane_layout_capture	*cap;
	struct window_pane		*wp;
	size_t				 i;

	if (c == NULL || w == NULL || c->session == NULL)
		return;
	if (!options_get_number(c->session->options, "animation-enable"))
		return;
	if (!options_get_number(c->session->options, "animation-pane-layout"))
		return;
	if (c->tty.sx == 0 || c->tty.sy == 0)
		return;
	if (c->overlay_draw != NULL && c->animation == NULL)
		return;

	if (c->animation_capture != NULL)
		animation_free_capture(c);

	cap = xcalloc(1, sizeof *cap);
	cap->w = w;
	cap->dying_id = (dying_wp != NULL) ? (int)dying_wp->id : -1;

	TAILQ_FOREACH(wp, &w->panes, entry)
		cap->cap++;
	if (cap->cap == 0) {
		free(cap);
		return;
	}
	cap->panes = xcalloc(cap->cap, sizeof *cap->panes);

	i = 0;
	TAILQ_FOREACH(wp, &w->panes, entry) {
		struct pane_anim *pa = &cap->panes[i++];
		pa->pane_id = wp->id;
		pa->src_x = wp->xoff;
		pa->src_y = wp->yoff;
		pa->src_w = wp->sx;
		pa->src_h = wp->sy;
	}
	cap->n = i;

	if (dying_wp != NULL && dying_wp->screen != NULL) {
		cap->dying_snapshot = xcalloc(1, sizeof *cap->dying_snapshot);
		animation_clone_screen(cap->dying_snapshot, dying_wp->screen);
		memcpy(&cap->dying_palette, &dying_wp->palette,
		    sizeof cap->dying_palette);
	}

	c->animation_capture = cap;
}

static struct pane_anim *
animation_capture_find_pane(struct pane_layout_capture *cap, int pane_id)
{
	size_t	i;
	for (i = 0; i < cap->n; i++)
		if (cap->panes[i].pane_id == pane_id)
			return (&cap->panes[i]);
	return (NULL);
}

void
animation_commit_pane_layout(struct client *c, struct window *w)
{
	struct pane_layout_capture	*cap;
	struct window_pane		*wp;
	struct animation		*a;
	struct timeval			 tv;
	struct pane_anim		*pa;
	size_t				 i;
	int				 dirty = 0;
	int				 y0, y1;

	if (c == NULL)
		return;
	cap = c->animation_capture;
	if (cap == NULL)
		return;
	if (cap->w != w || c->animation != NULL) {
		animation_free_capture(c);
		return;
	}

	/* Fill tgt for surviving panes; mark dying for those gone. */
	for (i = 0; i < cap->n; i++) {
		struct window_pane	*found = NULL;
		pa = &cap->panes[i];
		TAILQ_FOREACH(wp, &w->panes, entry) {
			if ((int)wp->id == pa->pane_id) {
				found = wp;
				break;
			}
		}
		if (found != NULL) {
			pa->tgt_x = found->xoff;
			pa->tgt_y = found->yoff;
			pa->tgt_w = found->sx;
			pa->tgt_h = found->sy;
			pa->phase = PANE_RESIZE;
		} else {
			pa->phase = PANE_DYING;
			pa->tgt_x = pa->src_x + pa->src_w / 2;
			pa->tgt_y = pa->src_y + pa->src_h / 2;
			pa->tgt_w = 0;
			pa->tgt_h = 0;
			if (pa->pane_id == cap->dying_id) {
				pa->snapshot = cap->dying_snapshot;
				memcpy(&pa->snapshot_palette,
				    &cap->dying_palette,
				    sizeof pa->snapshot_palette);
				cap->dying_snapshot = NULL;
			}
		}
		if (pa->src_x != pa->tgt_x || pa->src_y != pa->tgt_y ||
		    pa->src_w != pa->tgt_w || pa->src_h != pa->tgt_h)
			dirty = 1;
	}

	/* Newborn panes: in w but not in capture. */
	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (animation_capture_find_pane(cap, wp->id) != NULL)
			continue;
		if (cap->n >= cap->cap) {
			cap->cap = cap->cap * 2 + 1;
			cap->panes = xreallocarray(cap->panes, cap->cap,
			    sizeof *cap->panes);
		}
		pa = &cap->panes[cap->n++];
		memset(pa, 0, sizeof *pa);
		pa->pane_id = wp->id;
		pa->phase = PANE_BORN;
		pa->src_x = wp->xoff + wp->sx / 2;
		pa->src_y = wp->yoff + wp->sy / 2;
		pa->src_w = 0;
		pa->src_h = 0;
		pa->tgt_x = wp->xoff;
		pa->tgt_y = wp->yoff;
		pa->tgt_w = wp->sx;
		pa->tgt_h = wp->sy;
		dirty = 1;
	}

	if (!dirty) {
		animation_free_capture(c);
		return;
	}

	a = xcalloc(1, sizeof *a);
	a->client = c;
	a->kind = ANIM_PANE_LAYOUT;
	a->easing = animation_easing_lookup(c->session);
	a->duration_ms = options_get_number(c->session->options,
	    "animation-pane-duration");
	a->frame_interval_ms = options_get_number(c->session->options,
	    "animation-frame-interval");
	a->sx = c->tty.sx;
	a->sy = c->tty.sy;

	y0 = a->sy;
	y1 = 0;
	for (i = 0; i < cap->n; i++) {
		pa = &cap->panes[i];
		if (pa->src_y < y0) y0 = pa->src_y;
		if (pa->src_y + pa->src_h > y1) y1 = pa->src_y + pa->src_h;
		if (pa->tgt_y < y0) y0 = pa->tgt_y;
		if (pa->tgt_y + pa->tgt_h > y1) y1 = pa->tgt_y + pa->tgt_h;
	}
	if (y1 <= y0) {
		free(a);
		animation_free_capture(c);
		return;
	}
	a->pane_y0 = (u_int)y0;
	a->pane_h = (u_int)(y1 - y0);

	a->pl_window = w;
	a->pl_panes = cap->panes;
	a->pl_n = cap->n;
	cap->panes = NULL;
	cap->n = 0;

	a->start_ms = get_timer();
	a->last_ms = a->start_ms;

	c->animation = a;

	server_client_set_overlay(c, 0, animation_check_cb, NULL,
	    animation_draw_cb, animation_key_cb, animation_free_cb,
	    animation_resize_cb, a);

	evtimer_set(&c->animation_timer, animation_frame_cb, c);
	tv.tv_sec = 0;
	tv.tv_usec = (long)a->frame_interval_ms * 1000L;
	evtimer_add(&c->animation_timer, &tv);

	animation_free_capture(c);
}

static struct window_pane *
animation_find_pane(struct window *w, int id)
{
	struct window_pane	*wp;

	if (w == NULL)
		return (NULL);
	TAILQ_FOREACH(wp, &w->panes, entry) {
		if ((int)wp->id == id)
			return (wp);
	}
	return (NULL);
}

static void
animation_paint_pane_clipped(struct client *c, struct screen *s,
    struct colour_palette *palette,
    int dst_x, int dst_y, int dst_w, int dst_h)
{
	struct grid_cell	defaults;
	u_int			sy, src_sx, src_sy;
	int			nx;

	if (s == NULL || dst_w <= 0 || dst_h <= 0)
		return;
	memcpy(&defaults, &grid_default_cell, sizeof defaults);

	src_sx = screen_size_x(s);
	src_sy = screen_size_y(s);
	nx = dst_w;
	if (nx > (int)src_sx) nx = (int)src_sx;
	if (dst_x + nx > (int)c->tty.sx) nx = (int)c->tty.sx - dst_x;
	if (nx <= 0)
		return;

	for (sy = 0; sy < (u_int)dst_h && sy < src_sy; sy++) {
		int vy = dst_y + sy;
		if (vy < 0 || vy >= (int)c->tty.sy)
			continue;
		tty_draw_line(&c->tty, s, 0, sy, (u_int)nx,
		    (u_int)dst_x, (u_int)vy, &defaults, palette);
	}
}

static void
animation_pane_draw(struct client *c, struct animation *a, double t)
{
	struct pane_anim	*pa;
	struct grid_cell	 fill, defaults;
	struct window_pane	*wp_live;
	int			 lx, ly, lw, lh;
	int			 vy;
	size_t			 i;

	(void)defaults;

	/* Background fill across the union pane area using window default. */
	{
		struct grid_cell	cell;
		struct window_pane	*wp;
		memcpy(&cell, &grid_default_cell, sizeof cell);
		wp = (a->pl_window != NULL) ? a->pl_window->active : NULL;
		if (wp != NULL)
			tty_default_colours(&cell, wp);
		memcpy(&fill, &cell, sizeof fill);
		for (vy = a->pane_y0; vy < (int)(a->pane_y0 + a->pane_h);
		    vy++) {
			tty_attributes(&c->tty, &fill, &fill,
			    wp ? &wp->palette : NULL, NULL);
			tty_cursor(&c->tty, 0, (u_int)vy);
			tty_repeat_space(&c->tty, a->sx);
		}
	}

	for (i = 0; i < a->pl_n; i++) {
		pa = &a->pl_panes[i];
		lx = (int)lround(pa->src_x + t * (pa->tgt_x - pa->src_x));
		ly = (int)lround(pa->src_y + t * (pa->tgt_y - pa->src_y));
		lw = (int)lround(pa->src_w + t * (pa->tgt_w - pa->src_w));
		lh = (int)lround(pa->src_h + t * (pa->tgt_h - pa->src_h));

		if (pa->phase == PANE_DYING) {
			animation_paint_pane_clipped(c, pa->snapshot,
			    NULL, lx, ly, lw, lh);
		} else {
			wp_live = animation_find_pane(a->pl_window,
			    pa->pane_id);
			if (wp_live == NULL || wp_live->screen == NULL)
				continue;
			animation_paint_pane_clipped(c, wp_live->screen,
			    &wp_live->palette, lx, ly, lw, lh);
		}
	}
}

/*
 * Begin/commit pane-layout animations for every client whose currently
 * displayed window is `w`. Used from non-command paths like
 * server_destroy_pane (Ctrl-D shell exit) that don't have a single
 * triggering client.
 */
void
animation_window_pane_layout_begin(struct window *w,
    struct window_pane *dying_wp)
{
	struct client	*c;

	TAILQ_FOREACH(c, &clients, entry) {
		if (c->session == NULL || c->session->curw == NULL)
			continue;
		if (c->session->curw->window != w)
			continue;
		animation_begin_pane_layout(c, w, dying_wp);
	}
}

void
animation_window_pane_layout_commit(struct window *w)
{
	struct client	*c;

	TAILQ_FOREACH(c, &clients, entry) {
		if (c->session == NULL || c->session->curw == NULL)
			continue;
		if (c->session->curw->window != w)
			continue;
		animation_commit_pane_layout(c, w);
	}
}

/* Hook used from window_pane_destroy paths to snapshot a dying pane. */
void
animation_capture_dying_pane(struct window_pane *wp)
{
	/*
	 * Implemented by the command-site hooks for now (cmd-kill-pane.c
	 * calls animation_begin_pane_layout(c, w, wp) before destroy).
	 * The detour stays as a placeholder so callers can be added later
	 * without API churn.
	 */
	(void)wp;
}
