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
#error image support is not ready yet

struct grid;
struct grid_line;
struct image;
struct image_line;
struct image_store;
struct screen;
struct screen_write_ctx;
struct sixel_image;

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

/* image-grid.c */
void		 image_grid_add(struct grid *, struct image *, u_int, u_int,
		     u_int, u_int, u_int, u_int);
void		 image_grid_remove(struct grid *, u_int, u_int, u_int, u_int);
void		 image_grid_free_line(struct grid *, struct grid_line *);
void		 image_grid_free(struct grid *);
void		 image_grid_move_cells(struct grid *, u_int, u_int, u_int,
		     u_int);
void		 image_grid_duplicate_lines(struct grid *, u_int, struct grid *,
		     u_int, u_int);
int		 image_grid_line_has_images(struct grid_line *);

/* image-sixel.c */
#define SIXEL_COLOUR_REGISTERS 1024
struct sixel_image *sixel_parse(const char *, size_t, u_int, u_int, u_int);
void		 sixel_free(struct sixel_image *);
void		 sixel_log(struct sixel_image *);
void		 sixel_size_in_cells(struct sixel_image *, u_int *, u_int *);
struct sixel_image *sixel_scale(struct sixel_image *, u_int, u_int, u_int,
		     u_int, u_int, u_int, int);
char		*sixel_print(struct sixel_image *, struct sixel_image *,
		     size_t *);
struct screen	*sixel_to_screen(struct sixel_image *);

#endif /* ENABLE_IMAGES */

#endif /* TMUX_IMAGE_H */
