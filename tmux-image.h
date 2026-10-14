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
struct grid_line;
struct image;
struct image_line;
struct image_store;
struct screen;
struct screen_write_ctx;
struct sixel_image;
struct tty;
struct grid_cell;
struct image_backend;

/* Maximum image size in bytes. */
#define IMAGE_SIZE_LIMIT (64 * 1024 * 1024)
/* Backend draws images as graphics. */
#define IMAGE_BACKEND_GRAPHICAL 0x1
/* Backend images move with terminal scrolling. */
#define IMAGE_BACKEND_SCROLLS 0x2
#define IMAGE_BACKEND_CLIPPED 0x4

enum kitty_parse_status {
	KITTY_PARSE_ERROR = -1,
	KITTY_PARSE_OK,
	KITTY_PARSE_MORE,
	KITTY_PARSE_MISSING,
	KITTY_PARSE_UNSUPPORTED
};

/* Reply fields and placement actions returned by one Kitty command. */
struct kitty_parse_result {
	u_int	image_id;
	u_int	image_number;
	u_int	replace_id;
	u_int	placement_id;
	u_int	quiet;
	u_int	x;
	u_int	y;
	int32_t	z;
	char	action;
	char	delete;
	enum kitty_parse_status status;
};

/* Resolved source cell and application IDs of one Unicode placeholder. */
struct kitty_placeholder {
	struct image	*image;
	u_int		 source_x;
	u_int		 source_y;
	u_int		 image_id;
	u_int		 placement_id;
	int32_t	 z;
};

/* View of all or part of an image. */
struct image_view {
	/* Position and size in the image, in pixels. */
	u_int	 x;
	u_int	 y;
	u_int	 width;
	u_int	 height;

	/* Size after scaling, in pixels. */
	u_int	 scaled_width;
	u_int	 scaled_height;

	/* Destination canvas size and offset in pixels. */
	u_int	 canvas_width;
	u_int	 canvas_height;
	u_int	 canvas_xoff;
	u_int	 canvas_yoff;

	/* Destination size in terminal cells. */
	u_int	 sx;
	u_int	 sy;
};

/* A part of an image to draw. */
struct image_rect {
	struct image	*image;
	int64_t		 z;

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
struct image	*image_create(u_int, u_int, u_int, u_int, u_int, u_int,
		     u_char *);
struct image	*image_create_view(struct image *, struct image_view *);
struct image	*image_find(u_int);
u_int		 image_get_id(struct image *);
void		 image_ref(struct image *);
void		 image_free(struct image *);
void		 image_get_size(struct image *, u_int *, u_int *);
void		 image_get_canvas_size(struct image *, u_int *, u_int *);
void		 image_get_size_in_cells(struct image *, u_int *, u_int *);
const u_char	*image_get_pixels(struct image *, size_t *, size_t *);
void		 image_size_in_cells(u_int, u_int, u_int, u_int, u_int *,
		     u_int *);
void		 image_set_no_cursor(struct image *);
int		 image_has_no_cursor(struct image *);
u_int		 image_get_source_id(struct image *);
u_int		 image_get_order_id(struct image *);
void		 image_set_order_id(struct image *, u_int);
void		 image_get_pixel_area(struct image *, u_int, u_int, u_int,
		     u_int, u_int *, u_int *, u_int *, u_int *);
u_char		*image_base64_decode(const char *, size_t, size_t, size_t *);
u_char		*image_png_decode(const u_char *, size_t, size_t, u_int *,
		     u_int *);

/* image-grid.c */
void		 image_grid_add(struct grid *, struct image *, u_int, u_int,
		     u_int, u_int, u_int, u_int);
void		 image_grid_remove(struct grid *, u_int, u_int, u_int, u_int);
void		 image_grid_remove_overwritten_spans(struct grid *, u_int, u_int,
		     u_int, u_int);
void		 image_grid_free_line(struct grid *, struct grid_line *);
void		 image_grid_free(struct grid *);
void		 image_grid_move_cells(struct grid *, u_int, u_int, u_int,
		     u_int);
void		 image_grid_duplicate_lines(struct grid *, u_int, struct grid *,
		     u_int, u_int);
int		 image_grid_line_has_images(struct grid_line *);
int		 image_grid_area_has_images(struct grid *, u_int, u_int, u_int,
		     u_int);
void		 image_redraw_area(struct screen_write_ctx *, u_int, u_int,
		     u_int, u_int);
void		 image_redraw_all(struct screen_write_ctx *);
void		 image_redraw_scroll(struct screen_write_ctx *);
void		 image_damage_screen(struct screen_write_ctx *, u_int, u_int,
		     u_int, u_int);
void		 image_grid_copy_area(struct grid *, u_int, u_int, struct grid *,
		     u_int, u_int, u_int, u_int);
void		 image_grid_resize_width(struct grid *, u_int);
int		 image_grid_has_image(struct grid *, u_int);
int		 image_grid_get_placeholder(struct grid *, u_int, u_int,
		     struct kitty_placeholder *);
void		 image_place_cell_kitty(struct screen_write_ctx *, struct image *,
		     u_int, u_int, u_int, u_int, u_int, u_int, int32_t);
void		 image_write_sixel(struct screen_write_ctx *, struct image *, u_int);
void		 image_write_kitty(struct screen_write_ctx *, struct image *, u_int,
		     u_int, u_int, int32_t);
void		 image_clear(struct screen_write_ctx *, u_int);
void		 image_clear_kitty(struct screen_write_ctx *,
		     const struct kitty_parse_result *);
int		 image_grid_get_at(struct grid *, u_int, u_int, struct image **,
		     u_int *, u_int *);
const struct grid_cell *image_map_cell(struct tty *, struct grid *, u_int,
		     u_int, const struct grid_cell *, struct grid_cell *, int *);
struct image	*image_rect_get_image(const struct image_rect *);
void		 image_rect_get_coords(const struct image_rect *, u_int *, u_int *,
		     u_int *, u_int *, u_int *, u_int *);
int64_t		 image_rect_get_z(const struct image_rect *);
void		 image_draw_line(struct tty *, struct screen *, u_int, u_int, u_int,
		     u_int, u_int, int);
void		 image_redraw_start(struct tty *, u_int, u_int, u_int, u_int);
void		 image_redraw_finish(struct tty *);
void		 image_draw_flush(struct tty *);
int		 image_backend_get_flags(struct tty *);
int		 image_tty_update(struct tty *);
void		 image_tty_size_changed(struct tty *);
void		 image_tty_free(struct tty *, int);

/* image-fallback.c */
int		 fallback_select(struct tty *);
int		 fallback_get_flags(struct tty *);
const struct grid_cell *fallback_map_cell(struct tty *, struct grid *, u_int,
		     u_int, const struct grid_cell *, struct grid_cell *, int *);

/* image-kitty.c */
struct image	*kitty_parse_image(void **, const u_char *, size_t, u_int,
		     u_int, struct kitty_parse_result *);
int		 kitty_placeholder_to_image(void *, struct grid *,
		     struct grid_cell *, u_int, u_int, struct kitty_placeholder *);
int		 kitty_cell_is_placeholder(const struct grid_cell *);
void		 kitty_delete_images(void *, struct screen_write_ctx *,
		     const struct kitty_parse_result *);
void		 kitty_free_state(void *);
int		 kitty_select(struct tty *);
int		 kitty_get_flags(struct tty *);
void		 kitty_draw_rect(struct tty *, const struct image_rect *);
void		 kitty_redraw_start(struct tty *, u_int, u_int, u_int, u_int);
void		 kitty_redraw_finish(struct tty *);
void		 kitty_free_output_state(struct tty *, int);

/* image-sixel.c */
#define SIXEL_COLOUR_REGISTERS 1024
int		 sixel_select(struct tty *);
int		 sixel_get_flags(struct tty *);
struct sixel_image *sixel_parse(const char *, size_t, u_int, u_int, u_int);
void		 sixel_write(struct screen_write_ctx *, struct sixel_image *,
		     u_int);
void		 sixel_draw_rect(struct tty *, const struct image_rect *);
void		 sixel_start_draw(struct tty *, u_int, u_int, u_int, u_int);
void		 sixel_free_pending(struct tty *, int);
void		 sixel_flush_pending(struct tty *);

#endif /* ENABLE_IMAGES */

#endif /* TMUX_IMAGE_H */
