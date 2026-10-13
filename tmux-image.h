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
struct image_rect;
struct image_backend;

#define IMAGE_SIZE_LIMIT (64 * 1024 * 1024)
#define IMAGE_BACKEND_GRAPHICAL 0x1
#define IMAGE_BACKEND_SCROLLS 0x2
#define IMAGE_BACKEND_CLIPPED 0x4

/* Private ACS keys used by the text image backend. */
#define TTY_ACS_IMAGE_SHADE_LIGHT 0x80
#define TTY_ACS_IMAGE_SHADE_MEDIUM 0x81
#define TTY_ACS_IMAGE_SHADE_DARK 0x82
#define TTY_ACS_IMAGE_BLOCK 0x83
#define TTY_ACS_IMAGE_HALF_UPPER 0x84
#define TTY_ACS_IMAGE_HALF_LOWER 0x85
#define TTY_ACS_IMAGE_HALF_LEFT 0x86
#define TTY_ACS_IMAGE_HALF_RIGHT 0x87
#define TTY_ACS_IMAGE_QUADRANT_LOWER_LEFT 0x88
#define TTY_ACS_IMAGE_QUADRANT_LOWER_RIGHT 0x89
#define TTY_ACS_IMAGE_QUADRANT_UPPER_LEFT 0x8a
#define TTY_ACS_IMAGE_QUADRANT_UPPER_LEFT_LOWER_LEFT_LOWER_RIGHT 0x8b
#define TTY_ACS_IMAGE_QUADRANT_UPPER_LEFT_LOWER_RIGHT 0x8c
#define TTY_ACS_IMAGE_QUADRANT_UPPER_LEFT_UPPER_RIGHT_LOWER_LEFT 0x8d
#define TTY_ACS_IMAGE_QUADRANT_UPPER_LEFT_UPPER_RIGHT_LOWER_RIGHT 0x8e
#define TTY_ACS_IMAGE_QUADRANT_UPPER_RIGHT 0x8f
#define TTY_ACS_IMAGE_QUADRANT_UPPER_RIGHT_LOWER_LEFT 0x90
#define TTY_ACS_IMAGE_QUADRANT_UPPER_RIGHT_LOWER_LEFT_LOWER_RIGHT 0x91
#define TTY_ACS_IMAGE_SEXTANT_FIRST 0x92
#define TTY_ACS_IMAGE_SEXTANT_LAST 0xcd

/* Averaged RGBA colour and brightness used by the text image backend. */
struct image_sample {
	u_char	red;
	u_char	green;
	u_char	blue;
	u_char	alpha;
	u_char	brightness;
};
#define IMAGE_SAMPLE_COLUMNS 2
#define IMAGE_SAMPLE_ROWS 6

/* Whole-cell and subcell samples generated lazily for one image cell. */
struct image_cell {
	struct image_sample whole;
	struct image_sample samples[IMAGE_SAMPLE_ROWS][IMAGE_SAMPLE_COLUMNS];
};

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
struct sixel_image *image_get_sixel(struct image *);
void		 image_set_sixel(struct image *, struct sixel_image *);
u_int		 image_get_source_id(struct image *);
u_int		 image_get_order_id(struct image *);
void		 image_set_order_id(struct image *, u_int);
void		*image_get_fallback_data(struct image *);
void		 image_set_fallback_data(struct image *, void *);
const struct image_cell *image_get_cell(struct image *, u_int, u_int);
void		 image_get_pixel_rect(struct image *, u_int, u_int, u_int,
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
int		 image_get_fallback_at(struct tty *, struct screen *, u_int, u_int,
		     const struct grid_cell *, struct grid_cell *);
struct image	*image_rect_get_image(const struct image_rect *);
void		 image_rect_get_coords(const struct image_rect *, u_int *, u_int *,
		     u_int *, u_int *, u_int *, u_int *);
int64_t		 image_rect_get_z(const struct image_rect *);
void		 image_draw_line(struct tty *, struct screen *, u_int, u_int, u_int,
		     u_int, u_int, int);
void		 image_redraw_start(struct tty *, u_int, u_int, u_int, u_int);
void		 image_redraw_finish(struct tty *);
void		 image_draw_flush(struct tty *);
int		 image_backend_flags(struct tty *);
int		 image_tty_update(struct tty *);
void		 image_tty_geometry_changed(struct tty *);
void		 image_tty_free(struct tty *, int);

/* image-fallback.c */
void		 fallback_get_cell(struct tty *, struct image *, u_int, u_int,
		     const struct grid_cell *, struct grid_cell *);
void		 fallback_free(struct image *);

/* image-kitty.c */
struct image	*kitty_parse_image(void **, const u_char *, size_t, u_int,
		     u_int, struct kitty_parse_result *);
int		 kitty_placeholder_to_image(void *, struct grid *,
		     struct grid_cell *, u_int, u_int, struct kitty_placeholder *);
int		 kitty_cell_is_placeholder(const struct grid_cell *);
void		 kitty_delete_images(void *, struct screen_write_ctx *,
		     const struct kitty_parse_result *);
void		 kitty_free_state(void *);
void		 kitty_draw_rect(struct tty *, const struct image_rect *);
void		 kitty_redraw_start(struct tty *, u_int, u_int, u_int, u_int);
void		 kitty_redraw_finish(struct tty *);
void		 kitty_free_output_state(struct tty *, int);

/* image-sixel.c */
#define SIXEL_COLOUR_REGISTERS 1024
struct sixel_image *sixel_parse(const char *, size_t, u_int, u_int, u_int,
		     u_int);
void		 sixel_write(struct screen_write_ctx *, struct sixel_image *,
		     u_int);
void		 sixel_draw_rect(struct tty *, const struct image_rect *);
void		 sixel_redraw_start(struct tty *, u_int, u_int, u_int, u_int);
void		 sixel_free_output(struct tty *, int);
void		 sixel_flush_output(struct tty *);
struct image	*sixel_to_image(struct sixel_image *);
void		 sixel_free(struct sixel_image *);
void		 sixel_log(struct sixel_image *);

#endif /* ENABLE_IMAGES */

#endif /* TMUX_IMAGE_H */
