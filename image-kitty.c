/* $OpenBSD$ */

/*
 * Copyright (c) 2026 Michael Grant <mgrant@grant.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHOR
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH
 * THIS SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <sys/types.h>

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/* State attached to an input parser. */
struct kitty_state {
	char				 action;
	u_int				 format;
	char				 medium;
	char				 compression;
	char				 delete;
	u_int				 width;
	u_int				 height;
	u_int				 source_x;
	u_int				 source_y;
	u_int				 source_width;
	u_int				 source_height;
	u_int				 x_offset;
	u_int				 y_offset;
	u_int				 columns;
	u_int				 rows;
	u_int				 image_id;
	u_int				 image_number;
	u_int				 placement_id;
	u_int				 quiet;
	u_int				 data_size;
	int				 no_cursor;
	int				 more;

	u_char				*data;
	size_t				 size;
	size_t				 allocated;

	TAILQ_HEAD(, kitty_image_map)	 images;
	TAILQ_ENTRY(kitty_state)	 entry;
};
TAILQ_HEAD(kitty_states, kitty_state);

/* Kitty input states holding uploaded images. */
static struct kitty_states kitty_states = TAILQ_HEAD_INITIALIZER(kitty_states);

/* Map a Kitty placement ID to an image ID. */
struct kitty_placement_map {
	u_int					 kitty_id;
	u_int					 image_id;

	TAILQ_ENTRY(kitty_placement_map)	 entry;
};

/* Map Kitty image ID to image ID and placement views. */
struct kitty_image_map {
	u_int					 kitty_id;
	u_int					 image_id;
	TAILQ_HEAD(, kitty_placement_map)	 placements;

	TAILQ_ENTRY(kitty_image_map)		 entry;
};

/* Select Kitty output. */
int
kitty_select(__unused struct tty *tty)
{
	return (0);
}

/* Parse Kitty graphics control data. */
static int
kitty_parse_control_data(struct kitty_state *ks, const u_char *data,
    size_t size, const char **m)
{
	char		*copy, *ptr, *next, *value;
	const char	*errstr;
	u_int		 number;
	char		 key;
	int		 z;

	*m = NULL;

	copy = ptr = xstrndup(data, size);
	while ((next = strsep(&ptr, ",")) != NULL) {
		if (*next == '\0') {
			if (ptr == NULL)
				break;
			*m = (*m == NULL ? "EINVAL" : *m);
			continue;
		}
		key = next[0];
		if (next[1] != '=' || next[2] == '\0') {
			*m = (*m == NULL ? "EINVAL" : *m);
			continue;
		}
		value = next + 2;

		switch (key) {
		case 'a':
		case 't':
		case 'o':
		case 'd':
			if (value[1] != '\0') {
				*m = (*m == NULL ? "EINVAL" : *m);
				break;
			}
			switch (key) {
			case 'a':
				ks->action = value[0];
				break;
			case 't':
				ks->medium = value[0];
				break;
			case 'o':
				ks->compression = value[0];
				break;
			case 'd':
				ks->delete = value[0];
				break;
			}
			break;
		case 'z':
			z = strtonum(value, INT_MIN, INT_MAX, &errstr);
			if (errstr != NULL)
				*m = (*m == NULL ? "EINVAL" : *m);
			else if (z != 0)
				*m = (*m == NULL ? "ENOTSUP" : *m);
			break;
		case 'U':
		case 'P':
		case 'Q':
		case 'H':
		case 'V':
			if (strtonum(value, INT_MIN, UINT_MAX, &errstr) != 0)
				*m = (*m == NULL ? "ENOTSUP" : *m);
			else if (errstr != NULL)
				*m = (*m == NULL ? "ENOTSUP" : *m);
			break;
		case 'f':
		case 's':
		case 'v':
		case 'i':
		case 'I':
		case 'p':
		case 'q':
		case 'm':
		case 'S':
		case 'x':
		case 'y':
		case 'w':
		case 'h':
		case 'X':
		case 'Y':
		case 'c':
		case 'r':
		case 'C':
			number = strtonum(value, 0, UINT_MAX, &errstr);
			if (errstr != NULL) {
				*m = (*m == NULL ? "EINVAL" : *m);
				break;
			}
			switch (key) {
			case 'f':
				ks->format = number;
				break;
			case 's':
				ks->width = number;
				break;
			case 'v':
				ks->height = number;
				break;
			case 'i':
				ks->image_id = number;
				break;
			case 'I':
				ks->image_number = number;
				break;
			case 'p':
				ks->placement_id = number;
				break;
			case 'q':
				ks->quiet = number;
				break;
			case 'm':
				ks->more = (number != 0);
				break;
			case 'S':
				ks->data_size = number;
				break;
			case 'x':
				ks->source_x = number;
				break;
			case 'y':
				ks->source_y = number;
				break;
			case 'w':
				ks->source_width = number;
				break;
			case 'h':
				ks->source_height = number;
				break;
			case 'X':
				ks->x_offset = number;
				break;
			case 'Y':
				ks->y_offset = number;
				break;
			case 'c':
				ks->columns = number;
				break;
			case 'r':
				ks->rows = number;
				break;
			case 'C':
				ks->no_cursor = (number != 0);
				break;
			}
			break;
		}
	}
	free(copy);
	if (*m != NULL)
		return (-1);
	return (0);
}

/* Free Kitty graphics parser state. */
void
kitty_free_state(void *state)
{
	struct kitty_state		*ks = state;
	struct kitty_image_map		*map;
	struct kitty_placement_map	*kpl;

	if (ks == NULL)
		return;
	TAILQ_REMOVE(&kitty_states, ks, entry);
	while ((map = TAILQ_FIRST(&ks->images)) != NULL) {
		TAILQ_REMOVE(&ks->images, map, entry);
		while ((kpl = TAILQ_FIRST(&map->placements)) != NULL) {
			TAILQ_REMOVE(&map->placements, kpl, entry);
			free(kpl);
		}
		image_free(image_find(map->image_id));
		free(map);
	}
	free(ks->data);
	free(ks);
}

/* Remove a Kitty image. */
void
kitty_remove_image(struct image *im)
{
	struct kitty_state		*ks;
	struct kitty_image_map		*map, *next;
	struct kitty_placement_map	*kpl, *kpl1;
	u_int				 id = image_get_id(im);

	TAILQ_FOREACH(ks, &kitty_states, entry) {
		TAILQ_FOREACH_SAFE(map, &ks->images, entry, next) {
			TAILQ_FOREACH_SAFE(kpl, &map->placements, entry, kpl1) {
				if (map->image_id != id && kpl->image_id != id)
					continue;
				TAILQ_REMOVE(&map->placements, kpl, entry);
				free(kpl);
			}
			if (map->image_id != id)
				continue;
			TAILQ_REMOVE(&ks->images, map, entry);
			image_free(im);
			free(map);
		}
	}
}

/* Find an image mapping by Kitty image ID. */
static struct kitty_image_map *
kitty_image_map_find(struct kitty_state *ks, u_int id)
{
	struct kitty_image_map	*map;

	TAILQ_FOREACH(map, &ks->images, entry) {
		if (map->kitty_id == id)
			return (map);
	}
	return (NULL);
}

/* Replace the image associated with a Kitty image ID. */
static struct kitty_image_map *
kitty_image_map_set(struct kitty_state *ks, struct screen_write_ctx *ctx,
    u_int id, struct image *im)
{
	struct kitty_image_map		*map;
	struct kitty_placement_map	*kpl;
	struct image			*old;

	if (id == 0)
		return (NULL);
	map = kitty_image_map_find(ks, id);
	if (map != NULL) {
		old = image_find(map->image_id);
		image_set_order_id(im, image_get_order_id(old));
		while ((kpl = TAILQ_FIRST(&map->placements)) != NULL) {
			TAILQ_REMOVE(&map->placements, kpl, entry);
			image_clear(ctx, kpl->image_id);
			free(kpl);
		}
		image_free(image_find(map->image_id));
	} else {
		map = xcalloc(1, sizeof *map);
		map->kitty_id = id;
		TAILQ_INIT(&map->placements);
		TAILQ_INSERT_HEAD(&ks->images, map, entry);
	}
	image_ref(im);
	map->image_id = image_get_id(im);
	return (map);
}

/* Replace a named placement, or add a new anonymous placement. */
static void
kitty_placement_map_set(struct screen_write_ctx *ctx,
    struct kitty_image_map *map, u_int id, struct image *im)
{
	struct kitty_placement_map	*kpl, *kpl1;

	if (map == NULL)
		return;
	TAILQ_FOREACH_SAFE(kpl, &map->placements, entry, kpl1) {
		if (image_find(kpl->image_id) == NULL) {
			TAILQ_REMOVE(&map->placements, kpl, entry);
			free(kpl);
			continue;
		}
		if (id != 0 && kpl->kitty_id == id)
			break;
	}
	if (kpl == NULL) {
		kpl = xcalloc(1, sizeof *kpl);
		kpl->kitty_id = id;
		TAILQ_INSERT_TAIL(&map->placements, kpl, entry);
	} else
		image_clear(ctx, kpl->image_id);
	kpl->image_id = image_get_id(im);
}

/* Remove mappings for absent placements. */
static u_int
kitty_prune_placements(struct kitty_image_map *map, struct screen *s)
{
	struct kitty_placement_map	*kpl, *kpl1;
	u_int				 removed = 0;
	struct grid			*sgd = s->saved_grid;

	TAILQ_FOREACH_SAFE(kpl, &map->placements, entry, kpl1) {
		if (image_grid_has_image(s->grid, kpl->image_id))
			continue;
		if (sgd != NULL && image_grid_has_image(sgd, kpl->image_id))
			continue;
		TAILQ_REMOVE(&map->placements, kpl, entry);
		free(kpl);
		removed++;
	}
	return (removed);
}

/* Delete selected Kitty placements. */
static void
kitty_delete_images(struct kitty_state *ks, struct screen_write_ctx *ctx)
{
	struct screen			*s = ctx->s;
	struct grid			*gd = s->grid;
	struct kitty_image_map		*map, *map1;
	struct kitty_placement_map	*kpl;
	u_int				 removed;
	char				 how = ks->delete;
	int				 release;

	release = (how >= 'A' && how <= 'Z');
	if (release)
		how += 'a' - 'A';
	TAILQ_FOREACH(map, &ks->images, entry)
		kitty_prune_placements(map, s);
	if (how == 'a')
		image_grid_clear_visible(gd, IMAGE_TYPE_KITTY);

	TAILQ_FOREACH_SAFE(map, &ks->images, entry, map1) {
		removed = 0;
		if (how == 'i') {
			if (map->kitty_id != ks->image_id)
				continue;
			TAILQ_FOREACH(kpl, &map->placements, entry) {
				if (ks->placement_id != 0 &&
				    kpl->kitty_id != ks->placement_id)
					continue;
				removed += image_grid_clear(gd, kpl->image_id);
			}
			if (ks->placement_id == 0)
				removed++;
		}
		removed += kitty_prune_placements(map, s);
		if (!release || removed == 0 || !TAILQ_EMPTY(&map->placements))
			continue;
		TAILQ_REMOVE(&ks->images, map, entry);
		image_free(image_find(map->image_id));
		free(map);
	}
	if (ctx->wp != NULL)
		ctx->wp->flags |= PANE_REDRAW;
}

/* Decode and append one base64-encoded Kitty payload chunk. */
static int
kitty_append_data(struct kitty_state *ks, const u_char *data, size_t size)
{
	u_char	*d;
	size_t	 dsize, allocated;

	if (size > IMAGE_SIZE_LIMIT)
		return (-1);

	d = image_base64_decode(data, size, IMAGE_SIZE_LIMIT, &dsize);
	if (d == NULL)
		return (-1);
	if (dsize == 0) {
		free(d);
		return (0);
	}
	if (dsize > IMAGE_SIZE_LIMIT - ks->size) {
		free(d);
		return (-1);
	}

	if (ks->size + dsize > ks->allocated) {
		allocated = ks->allocated;
		if (allocated == 0)
			allocated = 4096;
		while (allocated < ks->size + dsize)
			allocated *= 2;
		ks->data = xrealloc(ks->data, allocated);
		ks->allocated = allocated;
	}
	memcpy(ks->data + ks->size, d, dsize);
	ks->size += dsize;

	free(d);
	return (0);
}

/* Reply to a Kitty graphics sequence. */
static void
kitty_send_reply(struct input_ctx *ictx, struct kitty_state *ks, const char *m)
{
	char	i[96] = "";
	char	I[96] = "";
	char	p[96] = "";

	if (ks->quiet >= 2)
		return;
	if (ks->action == 'd' && strcmp(m, "OK") == 0)
		return;
	if (ks->quiet == 1 && strcmp(m, "OK") == 0)
		return;
	if (ks->image_id == 0 && ks->image_number == 0 && ks->action != 'q')
		return;

	if (ks->image_id != 0 || ks->image_number == 0)
		xsnprintf(i, sizeof i, "i=%u", ks->image_id);
	if (ks->image_number != 0) {
		if (*i != '\0')
			strlcat(i, ",", sizeof i);
		xsnprintf(I, sizeof I, "I=%u", ks->image_number);
	}
	if (ks->placement_id != 0)
		xsnprintf(p, sizeof p, ",p=%u", ks->placement_id);
	input_reply(ictx, 0, "\033_G%s%s%s;%s\033\\", i, I, p, m);
}

/* Create a Kitty placement with shared source pixels and stored geometry. */
static struct image *
kitty_place_image(struct image *src, struct kitty_state *ks, u_int xpixel,
    u_int ypixel)
{
	struct image_geom	*sg = image_get_geometry(src);
	enum image_type		 type = image_get_type(src);
	struct image_geom	 ig = { 0 };
	u_char			*pixels;
	size_t			 stride;
	uint64_t		 cw, ch;
	double			 size;

	ig.src_x = ks->source_x;
	ig.src_y = ks->source_y;

	/* Clip the source rectangle. */
	if (ig.src_x >= sg->src_w || ig.src_y >= sg->src_h)
		return (NULL);
	ig.src_w = sg->src_w - ig.src_x;
	if (ks->source_width != 0 && ig.src_w > ks->source_width)
		ig.src_w = ks->source_width;
	ig.src_h = sg->src_h - ig.src_y;
	if (ks->source_height != 0 && ig.src_h > ks->source_height)
		ig.src_h = ks->source_height;

	/* Clamp offsets to the starting cell. */
	if (xpixel == 0)
		xpixel = 8;
	if (ypixel == 0)
		ypixel = 16;
	ig.canvas_xoff = ks->x_offset;
	if (ig.canvas_xoff >= xpixel)
		ig.canvas_xoff = xpixel - 1;
	ig.canvas_yoff = ks->y_offset;
	if (ig.canvas_yoff >= ypixel)
		ig.canvas_yoff = ypixel - 1;

	/* Work out the size. */
	ig.sx = ks->columns;
	ig.sy = ks->rows;
	ig.scaled_width = ig.src_w;
	ig.scaled_height = ig.src_h;
	if (ig.sx == 0 && ig.sy == 0) {
		if (ig.canvas_xoff > UINT_MAX - ig.src_w ||
		    ig.canvas_yoff > UINT_MAX - ig.src_h)
			return (NULL);
		image_size_in_cells(ig.src_w + ig.canvas_xoff,
		    ig.src_h + ig.canvas_yoff, xpixel, ypixel, &ig.sx, &ig.sy);
	} else if (ig.sy == 0) {
		ig.scaled_width = (double)ig.sx * xpixel - ig.canvas_xoff;
		ig.scaled_height = ig.src_h * ig.scaled_width / ig.src_w;

		size = ceil((ig.scaled_height + ig.canvas_yoff) / ypixel);
		if (size > USHRT_MAX)
			return (NULL);
		ig.sy = size;
	} else if (ig.sx == 0) {
		ig.scaled_height = (double)ig.sy * ypixel - ig.canvas_yoff;
		ig.scaled_width = ig.src_w * ig.scaled_height / ig.src_h;

		size = ceil((ig.scaled_width + ig.canvas_xoff) / xpixel);
		if (size > USHRT_MAX)
			return (NULL);
		ig.sx = size;
	} else {
		ig.scaled_width = (double)ig.sx * xpixel - ig.canvas_xoff;
		ig.scaled_height = (double)ig.sy * ypixel - ig.canvas_yoff;
	}
	if (ig.sx > USHRT_MAX ||
	    ig.sy > USHRT_MAX ||
	    ig.sx * ig.sy > IMAGE_SIZE_LIMIT)
		return (NULL);

	/* Record geometry in the input terminal's pixel coordinates. */
	cw = (uint64_t)ig.sx * xpixel;
	ch = (uint64_t)ig.sy * ypixel;
	if (cw > UINT_MAX || ch > UINT_MAX)
		return (NULL);
	ig.canvas_width = cw;
	ig.canvas_height = ch;
	pixels = image_get_pixels(src, &stride, NULL);
	return (image_create_with_parent(type, src, &ig, stride, pixels));
}

/* Parse and reply to a Kitty graphics sequence. */
void
kitty_parse_input(struct input_ctx *ictx, struct window_pane *wp,
    struct screen_write_ctx *ctx, u_int bg, void **state, const u_char *buf,
    size_t len)
{
	struct window		*w = wp->window;
	struct kitty_state	*ks = *state;
	struct kitty_state	 cks = { .action = 't' };
	struct image_geom	 ig = { 0 };
	struct kitty_image_map	*map = NULL;
	const u_char		*found;
	const char		*m;
	u_char			*p = NULL;
	size_t			 csize, psize;
	u_int			 sx, sy;
	uint64_t		 cw, ch;
	struct image		*src = NULL, *im = NULL;
	int			 error;

	buf++;
	len--;

	if (ks == NULL) {
		ks = xcalloc(1, sizeof *ks);
		TAILQ_INIT(&ks->images);
		TAILQ_INSERT_TAIL(&kitty_states, ks, entry);
		*state = ks;
	}

	found = memchr(buf, ';', len);
	if (found == NULL) {
		csize = len;
		psize = 0;
	} else {
		csize = found - buf;
		psize = len - csize - 1;
	}

	if (ks->more) {
		kitty_parse_control_data(&cks, buf, csize, &m);
		if (cks.action == 'd') {
			free(ks->data);
			ks->data = NULL;
			ks->size = 0;
			ks->allocated = 0;
			ks->more = 0;
		}
	}

	if (!ks->more) {
		ks->action = 't';
		ks->format = 32;
		ks->medium = 'd';
		ks->compression = 'o';
		ks->delete = 'a';
		ks->width = 0;
		ks->height = 0;
		ks->source_x = 0;
		ks->source_y = 0;
		ks->source_width = 0;
		ks->source_height = 0;
		ks->x_offset = 0;
		ks->y_offset = 0;
		ks->columns = 0;
		ks->rows = 0;
		ks->image_id = 0;
		ks->image_number = 0;
		ks->placement_id = 0;
		ks->quiet = 0;
		ks->data_size = 0;
		ks->no_cursor = 0;
	}
	ks->more = 0;

	error = kitty_parse_control_data(ks, buf, csize, &m);
	if (error != 0)
		goto out;
	m = "EINVAL";
	if (ks->image_id == 0 && ks->image_number == 0)
		ks->placement_id = 0;
	if (ks->image_number != 0) {
		if (ks->image_id != 0)
			goto out;
		m = "ENOTSUP";
		goto out;
	}
	if (ks->medium != 'd') {
		m = "ENOTSUP";
		goto out;
	}
	if (ks->action != 't' && ks->action != 'T' &&
	    ks->action != 'q' && ks->action != 'p' && ks->action != 'd') {
		m = "ENOTSUP";
		goto out;
	}
	if (ks->action == 'd') {
		switch (ks->delete) {
		case 'a':
		case 'A':
		case 'i':
		case 'I':
			break;
		default:
			m = "ENOTSUP";
			goto out;
		}
		kitty_delete_images(ks, ctx);
		m = "OK";
		goto out;
	}
	if (psize != 0 && kitty_append_data(ks, found + 1, psize) != 0)
		goto out;
	if (ks->more)
		return;

	if (ks->action == 'p') {
		map = kitty_image_map_find(ks, ks->image_id);
		if (map == NULL) {
			m = "ENOENT";
			goto out;
		}
		src = image_find(map->image_id);
		image_ref(src);

		im = kitty_place_image(src, ks, w->xpixel, w->ypixel);
		if (im == NULL)
			goto out;
		goto place;
	}

	if (ks->compression != 'o' && ks->compression != 'z')
		goto out;
	switch (ks->format) {
	case 24:
	case 32:
		p = image_raw_decode(ks->data, ks->size, IMAGE_SIZE_LIMIT,
		    ks->format, ks->width, ks->height, ks->compression == 'z');
		break;
	case 100:
		p = image_png_decode(ks->data, ks->size, IMAGE_SIZE_LIMIT,
		    ks->compression == 'z', ks->data_size, &ks->width,
		    &ks->height);
		break;
	}
	if (p == NULL)
		goto out;

	cw = (w->xpixel == 0 ? 8 : w->xpixel);
	ch = (w->ypixel == 0 ? 16 : w->ypixel);
	image_size_in_cells(ks->width, ks->height, cw, ch, &sx, &sy);
	cw *= sx;
	ch *= sy;
	if (cw > UINT_MAX || ch > UINT_MAX) {
		free(p);
		goto out;
	}
	ig.src_w = ig.scaled_width = ks->width;
	ig.src_h = ig.scaled_height = ks->height;
	ig.canvas_width = cw;
	ig.canvas_height = ch;
	ig.sx = sx;
	ig.sy = sy;
	src = image_create(IMAGE_TYPE_KITTY, &ig, p);
	if (src == NULL) {
		free(p);
		goto out;
	}

	if (ks->action == 'T') {
		im = kitty_place_image(src, ks, w->xpixel, w->ypixel);
		if (im == NULL)
			goto out;
	}
	if (ks->action != 'q')
		map = kitty_image_map_set(ks, ctx, ks->image_id, src);

place:
	if (im != NULL) {
		image_set_order_id(im, image_get_order_id(src));
		kitty_placement_map_set(ctx, map, ks->placement_id, im);
		image_write(ctx, im, bg, ks->no_cursor);
	}
	m = "OK";

out:
	kitty_send_reply(ictx, ks, m);
	if (im != NULL)
		image_free(im);
	if (src != NULL)
		image_free(src);

	free(ks->data);
	ks->data = NULL;
	ks->size = 0;
	ks->allocated = 0;
	ks->more = 0;
}
