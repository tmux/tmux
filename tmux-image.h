/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
 * Copyright (c) 2026 Michael Grant <mgrant@grant.org>
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

#ifndef TMUX_IMAGE_H
#define TMUX_IMAGE_H

#ifdef ENABLE_IMAGES

struct grid;
struct grid_cell;
struct grid_line;
struct image;
struct image_backend;
struct image_line;
struct image_saved;
struct image_store;
struct input_ctx;
struct screen;
struct screen_write_ctx;
struct sixel_image;
struct tty;
struct window_pane;

/* Maximum image size in bytes. */
#define IMAGE_SIZE_LIMIT (64 * 1024 * 1024)

/* Backend draws images as graphics. */
#define IMAGE_BACKEND_GRAPHICAL 0x1

/* Backend images move with terminal scrolling. */
#define IMAGE_BACKEND_SCROLLS 0x2

/* Type of image. */
enum image_type {
	IMAGE_TYPE_SIXEL,
	IMAGE_TYPE_KITTY
};

/* Source image and placement geometry. */
struct image_geom {
	/* Position and size within the source RGBA pixels. */
	u_int	 src_x;
	u_int	 src_y;
	u_int	 src_w;
	u_int	 src_h;

	/* Actual image size in pixels, including padding. */
	u_int	 canvas_width;
	u_int	 canvas_height;

	/* Content offset within the canvas, in pixels. */
	u_int	 canvas_xoff;
	u_int	 canvas_yoff;

	/* Scaled source size in pixels, excluding padding. */
	double	 scaled_width;
	double	 scaled_height;

	/* Actual image size in cells, including padding. */
	u_int	 sx;
	u_int	 sy;
};

/* Bounds within a source image, in pixels. */
struct image_bounds {
	double	 left;
	double	 top;
	double	 right;
	double	 bottom;
	double	 area;
};

/* A part of an image to draw. */
struct image_rect {
	struct image	*image;

	/* Size in cells. */
	u_int		 sx;
	u_int		 sy;

	/* Source in image in cells. */
	u_int		 src_x;
	u_int		 src_y;

	/* Destination on terminal in cells. */
	u_int		 dst_x;
	u_int		 dst_y;
};

/* image.c */
struct image	*image_create(enum image_type, struct image_geom *, u_char *);
struct image	*image_create_with_parent(enum image_type, struct image *,
		     struct image_geom *, size_t, u_char *);
struct image	*image_find(u_int);
u_int		 image_get_id(struct image *);
u_int		 image_get_order_id(struct image *);
void		 image_set_order_id(struct image *, u_int);
enum image_type	 image_get_type(struct image *);
void		 image_ref(struct image *);
void		 image_free(struct image *);
void		 image_touch(struct image *);
struct image_geom *image_get_geometry(struct image *);
u_char		*image_get_pixels(struct image *, size_t *, size_t *);
int		 image_get_bounds(struct image_geom *, u_int, u_int, u_int,
		     u_int, u_int, u_int, struct image_bounds *);
u_char		*image_get_pixel(struct image *, u_int, u_int, u_int, u_int);
void		 image_size_in_cells(u_int, u_int, u_int, u_int, u_int *,
		     u_int *);
void		 image_get_pixel_area(struct image *, u_int, u_int, u_int,
		     u_int, u_int *, u_int *, u_int *, u_int *);

/* image-decode.c */
u_char		*image_base64_decode(const char *, size_t, size_t, size_t *);
u_char		*image_png_decode(const u_char *, size_t, size_t, int, size_t,
		     u_int *, u_int *);
u_char		*image_raw_decode(const u_char *, size_t, size_t, u_int, u_int,
		     u_int, int);

/* image-grid.c */
void		 image_grid_add(struct grid *, struct image *, u_int, u_int,
		     u_int, u_int, u_int, u_int);
void		 image_grid_overwrite(struct grid *, u_int, u_int, u_int,
		     u_int);
void		 image_grid_free_line(struct grid *, struct grid_line *);
void		 image_grid_free(struct grid *);
void		 image_grid_remove_all(struct image *);
void		 image_grid_move_cells(struct grid *, u_int, u_int, u_int,
		     u_int);
struct image_saved *image_grid_save_lines(struct grid *, u_int, u_int, u_int,
		     int);
void		 image_grid_restore_lines(struct grid *, struct image_saved *);
void		 image_grid_duplicate_lines(struct grid *, u_int, struct grid *,
		     u_int, u_int);
void		 image_grid_copy_area(struct grid *, u_int, u_int,
		     struct grid *, u_int, u_int, u_int, u_int);
void		 image_grid_resize_width(struct grid *, u_int);
int		 image_grid_line_has_images(struct grid_line *);
int		 image_grid_has_image(struct grid *, u_int);
u_int		 image_grid_clear(struct grid *, u_int);
u_int		 image_grid_clear_visible(struct grid *, enum image_type);
int		 image_grid_area_has_images(struct grid *, u_int, u_int, u_int,
		     u_int);
void		 image_redraw_area(struct screen_write_ctx *, u_int, u_int,
		     u_int, u_int);
void		 image_redraw_all(struct screen_write_ctx *);
void		 image_redraw_scroll(struct screen_write_ctx *);
void		 image_damage_screen(struct screen_write_ctx *, u_int, u_int,
		     u_int, u_int);
void		 image_write(struct screen_write_ctx *, struct image *, u_int,
		     int);
void		 image_clear(struct screen_write_ctx *, u_int);
int		 image_grid_get_at(struct grid *, u_int, u_int, struct image **,
		     u_int *, u_int *);
const struct grid_cell *image_map_cell(struct tty *, struct grid *, u_int,
		     u_int, const struct grid_cell *, struct grid_cell *, int *);
void		 image_draw_line(struct tty *, struct screen *, u_int, u_int,
		     u_int, u_int, u_int);
void		 image_redraw_start(struct tty *, u_int, u_int, u_int, u_int);
void		 image_draw_flush(struct tty *);
int		 image_tty_update(struct tty *);
int		 image_backend_get_flags(struct tty *);
void		 image_tty_size_changed(struct tty *);
void		 image_tty_free(struct tty *);

/* image-fallback.c */
int		 fallback_select(struct tty *);
int		 fallback_get_flags(struct tty *);
const struct grid_cell *fallback_map_cell(struct tty *, struct grid *, u_int,
		     u_int, const struct grid_cell *, struct grid_cell *, int *);

/* image-sixel.c */
#define SIXEL_COLOUR_REGISTERS 1024
int		 sixel_select(struct tty *);
int		 sixel_get_flags(struct tty *);
struct sixel_image *sixel_parse(const char *, size_t, u_int, u_int, u_int);
void		 sixel_write(struct screen_write_ctx *, struct sixel_image *,
		     u_int);
void		 sixel_draw_rect(struct tty *, struct image_rect *);
void		 sixel_start_draw(struct tty *, u_int, u_int, u_int, u_int);
void		 sixel_free_pending(struct tty *);
void		 sixel_flush_pending(struct tty *);

/* image-kitty.c */
void		 kitty_parse_input(struct input_ctx *, struct window_pane *,
		     struct screen_write_ctx *, u_int, void **, const u_char *,
		     size_t);
void		 kitty_free_state(void *);

#endif /* ENABLE_IMAGES */

#endif /* TMUX_IMAGE_H */
