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
#include <resolv.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "tmux.h"

#define KITTY_CHUNK_SIZE 3072

static const uint32_t kitty_diacritics[] = {
	0x0305, 0x030D, 0x030E, 0x0310, 0x0312, 0x033D, 0x033E, 0x033F,
	0x0346, 0x034A, 0x034B, 0x034C, 0x0350, 0x0351, 0x0352, 0x0357,
	0x035B, 0x0363, 0x0364, 0x0365, 0x0366, 0x0367, 0x0368, 0x0369,
	0x036A, 0x036B, 0x036C, 0x036D, 0x036E, 0x036F, 0x0483, 0x0484,
	0x0485, 0x0486, 0x0487, 0x0592, 0x0593, 0x0594, 0x0595, 0x0597,
	0x0598, 0x0599, 0x059C, 0x059D, 0x059E, 0x059F, 0x05A0, 0x05A1,
	0x05A8, 0x05A9, 0x05AB, 0x05AC, 0x05AF, 0x05C4, 0x0610, 0x0611,
	0x0612, 0x0613, 0x0614, 0x0615, 0x0616, 0x0617, 0x0657, 0x0658,
	0x0659, 0x065A, 0x065B, 0x065D, 0x065E, 0x06D6, 0x06D7, 0x06D8,
	0x06D9, 0x06DA, 0x06DB, 0x06DC, 0x06DF, 0x06E0, 0x06E1, 0x06E2,
	0x06E4, 0x06E7, 0x06E8, 0x06EB, 0x06EC, 0x0730, 0x0732, 0x0733,
	0x0735, 0x0736, 0x073A, 0x073D, 0x073F, 0x0740, 0x0741, 0x0743,
	0x0745, 0x0747, 0x0749, 0x074A, 0x07EB, 0x07EC, 0x07ED, 0x07EE,
	0x07EF, 0x07F0, 0x07F1, 0x07F3, 0x0816, 0x0817, 0x0818, 0x0819,
	0x081B, 0x081C, 0x081D, 0x081E, 0x081F, 0x0820, 0x0821, 0x0822,
	0x0823, 0x0825, 0x0826, 0x0827, 0x0829, 0x082A, 0x082B, 0x082C,
	0x082D, 0x0951, 0x0953, 0x0954, 0x0F82, 0x0F83, 0x0F86, 0x0F87,
	0x135D, 0x135E, 0x135F, 0x17DD, 0x193A, 0x1A17, 0x1A75, 0x1A76,
	0x1A77, 0x1A78, 0x1A79, 0x1A7A, 0x1A7B, 0x1A7C, 0x1B6B, 0x1B6D,
	0x1B6E, 0x1B6F, 0x1B70, 0x1B71, 0x1B72, 0x1B73, 0x1CD0, 0x1CD1,
	0x1CD2, 0x1CDA, 0x1CDB, 0x1CE0, 0x1DC0, 0x1DC1, 0x1DC3, 0x1DC4,
	0x1DC5, 0x1DC6, 0x1DC7, 0x1DC8, 0x1DC9, 0x1DCB, 0x1DCC, 0x1DD1,
	0x1DD2, 0x1DD3, 0x1DD4, 0x1DD5, 0x1DD6, 0x1DD7, 0x1DD8, 0x1DD9,
	0x1DDA, 0x1DDB, 0x1DDC, 0x1DDD, 0x1DDE, 0x1DDF, 0x1DE0, 0x1DE1,
	0x1DE2, 0x1DE3, 0x1DE4, 0x1DE5, 0x1DE6, 0x1DFE, 0x20D0, 0x20D1,
	0x20D4, 0x20D5, 0x20D6, 0x20D7, 0x20DB, 0x20DC, 0x20E1, 0x20E7,
	0x20E9, 0x20F0, 0x2CEF, 0x2CF0, 0x2CF1, 0x2DE0, 0x2DE1, 0x2DE2,
	0x2DE3, 0x2DE4, 0x2DE5, 0x2DE6, 0x2DE7, 0x2DE8, 0x2DE9, 0x2DEA,
	0x2DEB, 0x2DEC, 0x2DED, 0x2DEE, 0x2DEF, 0x2DF0, 0x2DF1, 0x2DF2,
	0x2DF3, 0x2DF4, 0x2DF5, 0x2DF6, 0x2DF7, 0x2DF8, 0x2DF9, 0x2DFA,
	0x2DFB, 0x2DFC, 0x2DFD, 0x2DFE, 0x2DFF, 0xA66F, 0xA67C, 0xA67D,
	0xA6F0, 0xA6F1, 0xA8E0, 0xA8E1, 0xA8E2, 0xA8E3, 0xA8E4, 0xA8E5,
	0xA8E6, 0xA8E7, 0xA8E8, 0xA8E9, 0xA8EA, 0xA8EB, 0xA8EC, 0xA8ED,
	0xA8EE, 0xA8EF, 0xA8F0, 0xA8F1, 0xAAB0, 0xAAB2, 0xAAB3, 0xAAB7,
	0xAAB8, 0xAABE, 0xAABF, 0xAAC1, 0xFE20, 0xFE21, 0xFE22, 0xFE23,
	0xFE24, 0xFE25, 0xFE26, 0x10A0F, 0x10A38, 0x1D185, 0x1D186, 0x1D187,
	0x1D188, 0x1D189, 0x1D1AA, 0x1D1AB, 0x1D1AC, 0x1D1AD, 0x1D242, 0x1D243,
	0x1D244
};

struct kitty_state {
	char	 action;
	char	 delete;
	u_int	 format;
	char	 medium;
	char	 compression;
	u_int	 width;
	u_int	 height;
	u_int	 source_x;
	u_int	 source_y;
	u_int	 x_offset;
	u_int	 y_offset;
	u_int	 source_width;
	u_int	 source_height;
	u_int	 columns;
	u_int	 rows;
	u_int	 image_id;
	u_int	 image_number;
	u_int	 placement_id;
	int32_t	 z;
	u_int	 quiet;
	int	 no_cursor;
	int	 virtual;
	u_int	 data_size;
	int	 more;
	int	 unsupported;

	u_char	*raw;
	size_t	 rawlen;
};

struct kitty_placement {
	u_int			 placement_id;
	u_int			 server_id;
	int32_t			 z;
	int			 virtual;
	struct kitty_placement	*next;
};

struct kitty_source {
	u_int			 app_id;
	u_int			 server_id;
	struct kitty_placement	*placements;
	struct kitty_source	*next;
};

struct kitty_context {
	struct kitty_state	*transfer;
	struct kitty_source	*sources;
};

struct kitty_image_cache {
	u_int				 server_id;
	u_int				 kitty_id;
	u_int				 xpixel;
	u_int				 ypixel;
	struct kitty_placement_cache	*placements;
	u_int				 next_placement;
	struct kitty_image_cache	*next;
};

struct kitty_placement_cache {
	u_int				 id;
	u_int				 x;
	u_int				 y;
	u_int				 width;
	u_int				 height;
	u_int				 source_x;
	u_int				 source_y;
	int32_t				 z;
	int				 pending_delete;
	struct kitty_placement_cache	*next;
};

struct kitty_output {
	struct kitty_image_cache	*images;
	u_int				 next_id;
};

/* Return the Kitty output state for a terminal. */
static struct kitty_output *
kitty_get_output(struct tty *tty)
{
	struct kitty_output	*ko = tty->image_data;

	if (ko == NULL) {
		ko = xcalloc(1, sizeof *ko);
		ko->next_id = arc4random_uniform(0xffff00) + 1;
		tty->image_data = ko;
	}
	return (ko);
}

/* Delete a Kitty image and its placements. */
static void
kitty_delete(struct tty *tty, u_int id)
{
	char	s[64];

	xsnprintf(s, sizeof s, "\033_Ga=d,d=I,i=%u,q=2\033\\", id);
	tty_puts(tty, s);
}

/* Free cached output placement records. */
static void
kitty_free_placements(struct kitty_image_cache *entry)
{
	struct kitty_placement_cache	*placement, *next;

	for (placement = entry->placements; placement != NULL;
	    placement = next) {
		next = placement->next;
		free(placement);
	}
	entry->placements = NULL;
}

static void	kitty_place(struct tty *, struct kitty_image_cache *,
		    struct image *, u_int, u_int, u_int, u_int, u_int, u_int,
		    int32_t);

/* Place one piece of an existing placement again as a new placement. */
static void
kitty_redraw_keep_piece(struct tty *tty, struct kitty_image_cache *entry,
    struct image *im, struct kitty_placement_cache *placement, u_int x,
    u_int y, u_int width, u_int height)
{
	if (width == 0 || height == 0)
		return;
	kitty_place(tty, entry, im, placement->source_x + (x - placement->x),
	    placement->source_y + (y - placement->y), width, height, x, y,
	    placement->z);
}

/*
 * Place the parts of a placement outside a redraw area again, since the
 * redraw will not replace them.
 */
static void
kitty_redraw_keep(struct tty *tty, struct kitty_image_cache *entry,
    struct kitty_placement_cache *placement, u_int x, u_int y, u_int width,
    u_int height)
{
	struct image	*im;
	u_int		 px0, px1, py0, py1, ix0, ix1, iy0, iy1;

	px0 = placement->x;
	px1 = placement->x + placement->width;
	py0 = placement->y;
	py1 = placement->y + placement->height;
	ix0 = (x > px0 ? x : px0);
	ix1 = (x + width < px1 ? x + width : px1);
	iy0 = (y > py0 ? y : py0);
	iy1 = (y + height < py1 ? y + height : py1);
	if (ix0 == px0 && ix1 == px1 && iy0 == py0 && iy1 == py1)
		return;

	im = image_find(entry->server_id);
	if (im == NULL)
		return;

	/* Callers have checked the areas intersect, so ix0 < ix1, iy0 < iy1. */
	kitty_redraw_keep_piece(tty, entry, im, placement, px0, py0,
	    px1 - px0, iy0 - py0);
	kitty_redraw_keep_piece(tty, entry, im, placement, px0, iy1,
	    px1 - px0, py1 - iy1);
	kitty_redraw_keep_piece(tty, entry, im, placement, px0, iy0,
	    ix0 - px0, iy1 - iy0);
	kitty_redraw_keep_piece(tty, entry, im, placement, ix1, iy0,
	    px1 - ix1, iy1 - iy0);
}

/*
 * Mark placements intersecting a redraw area as stale rather than deleting
 * them yet (see kitty_redraw_finish()) - some implementations free an
 * image's pixel data once its last placement is gone, so an image only
 * placed here would go blank before its replacement lands. A placement only
 * partly inside the area is deleted as a whole, so place its outside parts
 * again first or they would vanish from cells nothing redraws.
 */
void
kitty_redraw_start(struct tty *tty, u_int x, u_int y, u_int width,
    u_int height)
{
	struct kitty_output		*ko = tty->image_data;
	struct kitty_image_cache	*entry;
	struct kitty_placement_cache	*placement;

	if (ko == NULL)
		return;
	for (entry = ko->images; entry != NULL; entry = entry->next) {
		for (placement = entry->placements; placement != NULL;
		    placement = placement->next) {
			if (placement->pending_delete)
				continue;
			if (placement->x >= x + width ||
			    placement->x + placement->width <= x ||
			    placement->y >= y + height ||
			    placement->y + placement->height <= y)
				continue;
			placement->pending_delete = 1;
			kitty_redraw_keep(tty, entry, placement, x, y, width,
			    height);
		}
	}
}

/*
 * Delete placements marked stale by kitty_redraw_start() - called once any
 * replacement placements have already been created, so an image already
 * placed elsewhere in the same redraw is never left with none at all in
 * between the two.
 */
void
kitty_redraw_finish(struct tty *tty)
{
	struct kitty_output		*ko = tty->image_data;
	struct kitty_image_cache	*entry;
	struct kitty_placement_cache	**pp, *placement;
	char				  s[64];

	if (ko == NULL)
		return;
	for (entry = ko->images; entry != NULL; entry = entry->next) {
		for (pp = &entry->placements; (placement = *pp) != NULL; ) {
			if (!placement->pending_delete) {
				pp = &placement->next;
				continue;
			}
			xsnprintf(s, sizeof s,
			    "\033_Ga=d,d=i,i=%u,p=%u,q=2\033\\", entry->kitty_id,
			    placement->id);
			tty_puts(tty, s);
			*pp = placement->next;
			free(placement);
		}
	}
}

/* Free a cached Kitty image. */
static void
kitty_free_entry(struct tty *tty, struct kitty_image_cache *entry, int send)
{
	if (send)
		kitty_delete(tty, entry->kitty_id);
	kitty_free_placements(entry);
	free(entry);
}

/* Free Kitty output state for a terminal. */
void
kitty_free_output_state(struct tty *tty, int send)
{
	struct kitty_output		*ko = tty->image_data;
	struct kitty_image_cache	*entry, *next;

	if (ko == NULL)
		return;
	/* Free all cached image entries. */
	for (entry = ko->images; entry != NULL; entry = next) {
		next = entry->next;
		kitty_free_entry(tty, entry, send);
	}
	free(ko);
	tty->image_data = NULL;
}

/* Remove cached Kitty images no longer held by the server. */
static void
kitty_free_stale_images(struct tty *tty)
{
	struct kitty_output		*ko = tty->image_data;
	struct kitty_image_cache	*entry, *next, *previous;

	if (ko == NULL)
		return;
	previous = NULL;
	for (entry = ko->images; entry != NULL; entry = next) {
		/* Save the successor before this entry may be removed. */
		next = entry->next;
		if (image_find(entry->server_id) != NULL) {
			/* Retained entries become the predecessor of the next one. */
			previous = entry;
			continue;
		}

		/* Unlink stale entries, including the first entry in the list. */
		if (previous == NULL)
			ko->images = next;
		else
			previous->next = next;
		kitty_free_entry(tty, entry, 1);
	}
}

/* Place an image rectangle using the Kitty graphics protocol. */
static void
kitty_place(struct tty *tty, struct kitty_image_cache *entry,
    struct image *im, u_int source_x, u_int source_y, u_int width,
    u_int height, u_int destination_x, u_int destination_y, int32_t z)
{
	char				 control[192];
	u_int				 px, py, pwidth, pheight, sx, sy;
	u_int				 canvas_width, canvas_height;
	struct kitty_placement_cache	*placement;

	image_get_size_in_cells(im, &sx, &sy);
	image_get_canvas_size(im, &canvas_width, &canvas_height);
	px = (uint64_t)source_x * canvas_width / sx;
	py = (uint64_t)source_y * canvas_height / sy;
	pwidth = ((uint64_t)(source_x + width) * canvas_width + sx - 1) /
	    sx - px;
	pheight = ((uint64_t)(source_y + height) * canvas_height + sy - 1) /
	    sy - py;

	/* Account for the duplicate-pixel border added by kitty_upload(). */
	px++;
	py++;
	placement = xcalloc(1, sizeof *placement);
	do {
		placement->id = ++entry->next_placement;
	} while (placement->id == 0);

	placement->x = destination_x;
	placement->y = destination_y;
	placement->width = width;
	placement->height = height;
	placement->source_x = source_x;
	placement->source_y = source_y;
	placement->z = z;
	placement->next = entry->placements;
	entry->placements = placement;
	tty_cursor(tty, destination_x, destination_y);
	xsnprintf(control, sizeof control,
	    "\033_Ga=p,i=%u,p=%u,x=%u,y=%u,w=%u,h=%u,c=%u,r=%u,z=%d,"
	    "C=1,q=2\033\\", entry->kitty_id,
	    placement->id, px, py, pwidth, pheight, width,
	    height, z);
	tty_puts(tty, control);
}

/* Upload an image to Kitty and return its output cache entry. */
static struct kitty_image_cache *
kitty_upload(struct tty *tty, struct image *im)
{
	struct kitty_output		*ko = kitty_get_output(tty);
	struct kitty_image_cache	*entry;
	char				 control[128], encoded[4097];
	const u_char			*pixels;
	u_char				*padded;
	size_t				 offset, size, row, stride, image_size;
	int				 encodedlen;
	u_int				 id, width, height;
	u_int				 canvas_width, canvas_height;
	u_int				 upload_width, upload_height;

	image_get_size(im, &width, &height);
	image_get_canvas_size(im, &canvas_width, &canvas_height);
	if (canvas_width > UINT_MAX - 2 || canvas_height > UINT_MAX - 2)
		return (NULL);
	upload_width = canvas_width + 2;
	upload_height = canvas_height + 2;
	if ((uint64_t)upload_width * upload_height * 4 > IMAGE_SIZE_LIMIT)
		return (NULL);

	for (entry = ko->images; entry != NULL; entry = entry->next) {
		if (entry->server_id != image_get_id(im))
			continue;
		if (entry->xpixel == tty->xpixel &&
		    entry->ypixel == tty->ypixel)
			return (entry);
		kitty_delete(tty, entry->kitty_id);
		kitty_free_placements(entry);
		entry->server_id = 0;
		break;
	}
	if (entry == NULL) {
		entry = xcalloc(1, sizeof *entry);
		entry->next = ko->images;
		ko->images = entry;
	}
	do {
		id = ++ko->next_id & 0xffffff;
	} while (id == 0);
	entry->server_id = image_get_id(im);
	entry->kitty_id = id;
	entry->xpixel = tty->xpixel;
	entry->ypixel = tty->ypixel;
	entry->next_placement = 0;

	pixels = image_get_pixels(im, &stride, &image_size);
	/*
	 * Pad the upload with duplicate edge pixels. Kitty linearly filters scaled
	 * textures against transparent border pixels, which otherwise darkens the
	 * outermost pixels of an opaque image.
	 */
	padded = xcalloc((size_t)upload_width * upload_height, 4);
	for (row = 0; row < height; row++)
		memcpy(padded + ((size_t)(row + 1) * upload_width + 1) * 4,
		    pixels + row * stride, (size_t)width * 4);
	for (row = 1; row <= canvas_height; row++) {
		memcpy(padded + (size_t)row * upload_width * 4,
		    padded + ((size_t)row * upload_width + 1) * 4, 4);
		memcpy(padded + ((size_t)row * upload_width + upload_width - 1) * 4,
		    padded + ((size_t)row * upload_width + upload_width - 2) * 4,
		    4);
	}
	memcpy(padded, padded + (size_t)upload_width * 4,
	    (size_t)upload_width * 4);
	memcpy(padded + (size_t)(upload_height - 1) * upload_width * 4,
	    padded + (size_t)(upload_height - 2) * upload_width * 4,
	    (size_t)upload_width * 4);
	pixels = padded;
	width = upload_width;
	height = upload_height;
	image_size = (size_t)width * height * 4;
	for (offset = 0; offset < image_size; offset += size) {
		size = image_size - offset;
		if (size > KITTY_CHUNK_SIZE)
			size = KITTY_CHUNK_SIZE;
		encodedlen = b64_ntop(pixels + offset, size, encoded,
		    sizeof encoded);
		if (encodedlen < 0) {
			free(padded);
			return (NULL);
		}
		if (offset == 0) {
			xsnprintf(control, sizeof control,
			    "\033_Ga=t,f=32,s=%u,v=%u,i=%u,q=2,m=%d;",
			    width, height, id, offset + size < image_size);
		} else {
			xsnprintf(control, sizeof control, "\033_Gm=%d;",
			    offset + size < image_size);
		}
		tty_puts(tty, control);
		tty_putn(tty, encoded, encodedlen, 0);
		tty_puts(tty, "\033\\");
	}
	free(padded);
	return (entry);
}

/* Draw an image rectangle using the Kitty graphics protocol. */
void
kitty_draw_rect(struct tty *tty, const struct image_rect *rectangle)
{
	struct kitty_image_cache	*entry;
	struct image			*im;
	u_int				 source_x, source_y;
	u_int				 width, height, destination_x, destination_y;
	int32_t				 z;

	im = image_rect_get_image(rectangle);
	kitty_free_stale_images(tty);
	entry = kitty_upload(tty, im);
	if (entry == NULL)
		return;
	image_rect_get_coords(rectangle, &source_x, &source_y, &width,
	    &height, &destination_x, &destination_y);
	z = image_rect_get_z(rectangle);
	kitty_place(tty, entry, im, source_x, source_y, width, height,
	    destination_x, destination_y, z);
}

/* Parse an unsigned Kitty graphics control value. */
static int
kitty_number(const char *s, size_t len, u_int *value)
{
	char		 copy[32];
	const char	*errstr;
	long long	 ll;

	if (len == 0 || len >= sizeof copy)
		return (-1);
	memcpy(copy, s, len);
	copy[len] = '\0';
	ll = strtonum(copy, 0, UINT_MAX, &errstr);
	if (errstr != NULL)
		return (-1);
	*value = ll;
	return (0);
}

/* Parse a signed Kitty graphics control value. */
static int
kitty_signed_number(const char *s, size_t len, int32_t *value)
{
	char		 copy[32];
	const char	*errstr;
	long long	 ll;

	if (len == 0 || len >= sizeof copy)
		return (-1);
	memcpy(copy, s, len);
	copy[len] = '\0';
	ll = strtonum(copy, INT32_MIN, INT32_MAX, &errstr);
	if (errstr != NULL)
		return (-1);
	*value = ll;
	return (0);
}

/* Parse a Kitty graphics control string. */
static int
kitty_control(struct kitty_state *ks, const u_char *buf, size_t len)
{
	const u_char	*value, *end = buf + len, *comma;
	size_t		 valuelen;
	u_int		 number;
	int32_t		 signed_number;
	char		 key;

	while (buf < end) {
		key = *buf++;
		if (buf == end || *buf++ != '=')
			return (-1);
		value = buf;
		comma = memchr(buf, ',', end - buf);
		if (comma == NULL) {
			valuelen = end - buf;
			buf = end;
		} else {
			valuelen = comma - buf;
			buf = comma + 1;
		}
		if (valuelen == 0)
			return (-1);

		switch (key) {
		case 'a':
			ks->action = value[0];
			break;
		case 'd':
			ks->delete = value[0];
			break;
		case 't':
			ks->medium = value[0];
			break;
		case 'o':
			ks->compression = value[0];
			break;
		case 'P': case 'Q':
			if (kitty_number((const char *)value, valuelen,
			    &number) != 0)
				return (-1);
			if (number != 0)
				ks->unsupported = 1;
			break;
		case 'H': case 'V':
			if (kitty_signed_number((const char *)value, valuelen,
			    &signed_number) != 0)
				return (-1);
			if (signed_number != 0)
				ks->unsupported = 1;
			break;
		case 'z':
			if (kitty_signed_number((const char *)value, valuelen,
			    &ks->z) != 0)
				return (-1);
			break;
		case 'f':
		case 's':
		case 'v':
		case 'x':
		case 'y':
		case 'w':
		case 'h':
		case 'c':
		case 'r':
		case 'i':
		case 'I':
		case 'p':
		case 'q':
		case 'm':
		case 'S':
		case 'C':
		case 'U':
		case 'X':
		case 'Y':
			if (kitty_number((const char *)value, valuelen,
			    &number) != 0)
				return (-1);
			switch (key) {
			case 'f': ks->format = number; break;
			case 's': ks->width = number; break;
			case 'v': ks->height = number; break;
			case 'x': ks->source_x = number; break;
			case 'y': ks->source_y = number; break;
			case 'w': ks->source_width = number; break;
			case 'h': ks->source_height = number; break;
			case 'c': ks->columns = number; break;
			case 'r': ks->rows = number; break;
			case 'i': ks->image_id = number; break;
			case 'I': ks->image_number = number; break;
			case 'p': ks->placement_id = number; break;
			case 'q': ks->quiet = number; break;
			case 'm': ks->more = (number != 0); break;
			case 'S': ks->data_size = number; break;
			case 'C': ks->no_cursor = (number != 0); break;
			case 'U': ks->virtual = (number != 0); break;
			case 'X': ks->x_offset = number; break;
			case 'Y': ks->y_offset = number; break;
			}
			break;
		}
	}
	return (0);
}

/* Free a partially parsed Kitty graphics command. */
static void
kitty_state_free(struct kitty_state *ks)
{
	if (ks == NULL)
		return;
	free(ks->raw);
	free(ks);
}

/* Free placement images belonging to a Kitty source image. */
static void
kitty_placements_free(struct kitty_source *source)
{
	struct kitty_placement	*placement, *next;

	for (placement = source->placements; placement != NULL;
	    placement = next) {
		next = placement->next;
		image_free(placement->server_id);
		free(placement);
	}
	source->placements = NULL;
}

/* Free a source image and its parser-side placements. */
static void
kitty_source_free(struct kitty_source *source)
{
	kitty_placements_free(source);
	image_free(source->server_id);
	free(source);
}

/* Free Kitty graphics parser state. */
void
kitty_free_state(void *state)
{
	struct kitty_context	*kc = state;
	struct kitty_source	*source, *next;

	if (kc == NULL)
		return;
	kitty_state_free(kc->transfer);
	for (source = kc->sources; source != NULL; source = next) {
		next = source->next;
		kitty_source_free(source);
	}
	free(kc);
}

/* Find a Kitty source image by application ID. */
static struct kitty_source *
kitty_source_find(struct kitty_context *kc, u_int id)
{
	struct kitty_source	*source;

	for (source = kc->sources; source != NULL; source = source->next) {
		if (source->app_id == id)
			return (source);
	}
	return (NULL);
}

/* Replace the source image associated with a Kitty application ID. */
static u_int
kitty_source_set(struct kitty_context *kc, u_int id, struct image *im)
{
	struct kitty_source	*source;
	u_int			 old_id = 0;

	if (id == 0)
		return (0);
	source = kitty_source_find(kc, id);
	if (source == NULL) {
		source = xcalloc(1, sizeof *source);
		source->app_id = id;
		source->next = kc->sources;
		kc->sources = source;
	} else {
		old_id = source->server_id;
		kitty_placements_free(source);
		image_free(source->server_id);
	}
	image_ref(image_get_id(im));
	source->server_id = image_get_id(im);
	return (old_id);
}

/* Associate a placement ID with an image. */
static u_int
kitty_placement_set(struct kitty_context *kc, const struct kitty_state *ks,
    struct image *im)
{
	struct kitty_source	*source;
	struct kitty_placement	*placement;
	u_int			 old_id;

	source = kitty_source_find(kc, ks->image_id);
	if (source == NULL)
		return (0);
	for (placement = source->placements; placement != NULL;
	    placement = placement->next) {
		if (ks->placement_id != 0) {
			if (placement->placement_id == ks->placement_id)
				break;
		}
	}
	if (placement == NULL) {
		placement = xcalloc(1, sizeof *placement);
		placement->placement_id = ks->placement_id;
		placement->next = source->placements;
		source->placements = placement;
	}
	old_id = placement->server_id;
	placement->z = ks->z;
	placement->virtual = ks->virtual;
	image_ref(image_get_id(im));
	placement->server_id = image_get_id(im);
	if (old_id != 0)
		image_free(old_id);
	return (old_id);
}

/* Find and reference a Kitty source image. */
static struct image *
kitty_source_get(struct kitty_context *kc, u_int id)
{
	struct kitty_source	*source;
	struct image		*im;

	source = kitty_source_find(kc, id);
	if (source == NULL)
		return (NULL);
	im = image_find(source->server_id);
	if (im != NULL)
		image_ref(image_get_id(im));
	return (im);
}

/* Remove records for ordinary placements no longer present in the grid. */
static u_int
kitty_prune_placements(struct kitty_source *source, struct grid *gd)
{
	struct kitty_placement	**pp, *placement;
	u_int			 removed = 0;

	for (pp = &source->placements; (placement = *pp) != NULL; ) {
		if (placement->virtual)
			goto keep;
		if (image_grid_has_image(gd, placement->server_id))
			goto keep;
		*pp = placement->next;
		image_free(placement->server_id);
		free(placement);
		removed++;
		continue;
keep:
		pp = &placement->next;
	}
	return (removed);
}

/* Delete selected placements and release unreferenced image data. */
void
kitty_delete_images(void *state, struct screen_write_ctx *ctx,
    const struct kitty_parse_result *result)
{
	struct kitty_context	*kc = state;
	struct grid		*gd = ctx->s->grid;
	struct kitty_source	**sp, *source;
	struct kitty_placement	**pp, *placement;
	u_int			 removed;
	u_int			 placement_id = result->placement_id;
	char			 how = result->delete;
	int			 selected, release = 0;

	if (kc == NULL)
		return;
	if (how >= 'A') {
		if (how <= 'Z') {
			how += 'a' - 'A';
			release = 1;
		}
	}
	if (how == 'r')
		placement_id = 0;
	for (source = kc->sources; source != NULL; source = source->next)
		(void)kitty_prune_placements(source, gd);
	image_clear_kitty(ctx, result);
	for (sp = &kc->sources; (source = *sp) != NULL; ) {
		removed = kitty_prune_placements(source, gd);
		selected = 0;
		if (how == 'i') {
			if (source->app_id == result->image_id)
				selected = 1;
		} else if (how == 'r') {
			if (source->app_id >= result->x) {
				if (source->app_id <= result->y)
					selected = 1;
			}
		}
		if (selected) {
			for (pp = &source->placements;
			    (placement = *pp) != NULL; ) {
				if (!placement->virtual)
					goto keep_placement;
				if (placement_id != 0) {
					if (placement->placement_id != placement_id)
						goto keep_placement;
				}
				*pp = placement->next;
				image_free(placement->server_id);
				free(placement);
				removed++;
				continue;
keep_placement:
				pp = &placement->next;
			}
			if (placement_id == 0)
				removed++;
		}
		if (!release)
			goto keep_source;
		if (removed == 0)
			goto keep_source;
		if (source->placements != NULL)
			goto keep_source;
		if (image_grid_has_image(gd, source->server_id))
			goto keep_source;
		*sp = source->next;
		kitty_source_free(source);
		continue;
keep_source:
		sp = &source->next;
	}
}

/* Decode and append one base64-encoded Kitty payload chunk. */
static int
kitty_append(struct kitty_state *ks, const u_char *buf, size_t len)
{
	static const char base64[] =
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	u_char		*decoded;
	char		*copy = NULL;
	const char	*digit;
	size_t		 decodedlen, offset;
	u_int		 mask;

	if (len > IMAGE_SIZE_LIMIT)
		return (-1);
	/* Older Chafa chunks have padding with nonzero unused bits. */
	if (len >= 4 && len % 4 == 0 && buf[len - 1] == '=') {
		offset = len - 2;
		mask = 0x3c;
		if (buf[offset] == '=') {
			offset--;
			mask = 0x30;
		}
		digit = strchr(base64, buf[offset]);
		if (digit == NULL)
			return (-1);
		copy = xmalloc(len);
		memcpy(copy, buf, len);
		copy[offset] = base64[(digit - base64) & mask];
		buf = (const u_char *)copy;
	}

	decoded = image_base64_decode((const char *)buf, len,
	    IMAGE_SIZE_LIMIT, &decodedlen);
	free(copy);
	if (decoded == NULL)
		return (-1);
	if (decodedlen > IMAGE_SIZE_LIMIT - ks->rawlen) {
		free(decoded);
		return (-1);
	}
	ks->raw = xrealloc(ks->raw, ks->rawlen + decodedlen);
	memcpy(ks->raw + ks->rawlen, decoded, decodedlen);
	ks->rawlen += decodedlen;
	free(decoded);
	return (0);
}

/* Decode raw Kitty graphics data into RGBA pixels. */
static u_char *
kitty_raw(struct kitty_state *ks, u_char *data, size_t size)
{
	u_char		*raw, *pixels;
	size_t		 expected, i, j;
	uLongf		 rawlen;
	u_int		 bytes;

	bytes = (ks->format == 24 ? 3 : 4);
	if (ks->width == 0)
		return (NULL);
	if (ks->height == 0)
		return (NULL);
	if ((uint64_t)ks->width * ks->height > IMAGE_SIZE_LIMIT / 4)
		return (NULL);
	expected = (size_t)ks->width * ks->height * bytes;
	raw = data;
	if (ks->compression == 'z') {
		rawlen = expected;
		raw = xmalloc(expected);
		if (uncompress(raw, &rawlen, data, size) != Z_OK ||
		    rawlen != expected) {
			free(raw);
			return (NULL);
		}
		size = rawlen;
	} else if (ks->compression != '\0')
		return (NULL);
	if (size != expected) {
		if (raw != data)
			free(raw);
		return (NULL);
	}
	if (bytes == 4)
		return (raw);

	pixels = xmalloc((size_t)ks->width * ks->height * 4);
	for (i = j = 0; i < expected; i += 3, j += 4) {
		pixels[j] = raw[i];
		pixels[j + 1] = raw[i + 1];
		pixels[j + 2] = raw[i + 2];
		pixels[j + 3] = 255;
	}
	if (raw != data)
		free(raw);
	return (pixels);
}

/* Create an image view for a Kitty placement. */
static struct image *
kitty_place_image(struct image *source, struct kitty_state *ks, u_int xpixel,
    u_int ypixel)
{
	struct image_view view = { 0 };
	struct image	*im;
	u_int		 cell_width, cell_height, source_width, source_height;
	u_int		 x_offset, y_offset;
	uint64_t	 canvas_width, canvas_height;
	double		 width_scale, height_scale, scale = 1, units = 1;
	double		 value;
	int		 natural_size = 0;

	image_get_size(source, &source_width, &source_height);
	view.x = ks->source_x;
	view.y = ks->source_y;
	if (view.x >= source_width)
		return (NULL);
	if (view.y >= source_height)
		return (NULL);
	view.width = source_width - view.x;
	if (ks->source_width != 0) {
		if (view.width > ks->source_width)
			view.width = ks->source_width;
	}
	view.height = source_height - view.y;
	if (ks->source_height != 0) {
		if (view.height > ks->source_height)
			view.height = ks->source_height;
	}
	cell_width = (xpixel == 0 ? 8 : xpixel);
	cell_height = (ypixel == 0 ? 16 : ypixel);
	x_offset = ks->x_offset;
	y_offset = ks->y_offset;
	/* Match Kitty by clamping offsets to the starting cell. */
	if (x_offset >= cell_width)
		x_offset = cell_width - 1;
	if (y_offset >= cell_height)
		y_offset = cell_height - 1;
	view.sx = ks->columns;
	view.sy = ks->rows;
	if (view.sx > USHRT_MAX)
		return (NULL);
	if (view.sy > USHRT_MAX)
		return (NULL);
	if (view.sx == 0) {
		if (view.sy == 0)
			natural_size = 1;
	}
	if (natural_size) {
		if (x_offset > UINT_MAX - view.width)
			return (NULL);
		if (y_offset > UINT_MAX - view.height)
			return (NULL);
		image_size_in_cells(view.width + x_offset,
		    view.height + y_offset, cell_width, cell_height,
		    &view.sx, &view.sy);
	} else if (view.sy == 0) {
		scale = ((double)view.sx * cell_width - x_offset) / view.width;
		value = ceil((view.height * scale + y_offset) / cell_height);
		if (value > USHRT_MAX)
			return (NULL);
		view.sy = value;
	} else if (view.sx == 0) {
		scale = ((double)view.sy * cell_height - y_offset) /
		    view.height;
		value = ceil((view.width * scale + x_offset) / cell_width);
		if (value > USHRT_MAX)
			return (NULL);
		view.sx = value;
	} else {
		width_scale = ((double)view.sx * cell_width - x_offset) /
		    view.width;
		height_scale = ((double)view.sy * cell_height - y_offset) /
		    view.height;
		scale = width_scale;
		if (height_scale < scale)
			scale = height_scale;
	}
	canvas_width = (uint64_t)view.sx * cell_width;
	canvas_height = (uint64_t)view.sy * cell_height;
	/* Preserve source resolution when shrinking the image. */
	if (scale < 1)
		units = 1 / scale;
	value = ceil(canvas_width * units);
	if (value > UINT_MAX)
		return (NULL);
	view.canvas_width = value;
	value = ceil(canvas_height * units);
	if (value > UINT_MAX)
		return (NULL);
	view.canvas_height = value;
	view.scaled_width = floor(view.width * scale * units);
	if (view.scaled_width < view.width)
		view.scaled_width = view.width;
	view.scaled_height = floor(view.height * scale * units);
	if (view.scaled_height < view.height)
		view.scaled_height = view.height;
	view.x_offset = floor(x_offset * units);
	view.y_offset = floor(y_offset * units);
	if (view.x_offset > view.canvas_width)
		return (NULL);
	if (view.scaled_width > view.canvas_width - view.x_offset)
		return (NULL);
	if (view.y_offset > view.canvas_height)
		return (NULL);
	if (view.scaled_height > view.canvas_height - view.y_offset)
		return (NULL);
	if (ks->columns != 0) {
		if (ks->rows != 0) {
			view.x_offset += (view.canvas_width - view.x_offset -
			    view.scaled_width) / 2;
			view.y_offset += (view.canvas_height - view.y_offset -
			    view.scaled_height) / 2;
		}
	}
	/* A matching rectangle needs neither resampling nor padding. */
	if (x_offset == 0) {
		if (y_offset == 0) {
			if ((uint64_t)view.width * canvas_height ==
			    (uint64_t)view.height * canvas_width) {
				view.canvas_width = view.width;
				view.scaled_width = view.width;
				view.canvas_height = view.height;
				view.scaled_height = view.height;
				view.x_offset = 0;
				view.y_offset = 0;
			}
		}
	}
	im = image_create_view(source, &view);
	if (im != NULL) {
		if (ks->no_cursor)
			image_set_no_cursor(im);
	}
	return (im);
}

/*
 * Parse one Kitty graphics APC body (without the leading G). Only direct
 * static images are accepted. The returned image retains immutable RGBA
 * pixels for the lifetime of its placement.
 */
struct image *
kitty_parse_image(void **state, const u_char *buf, size_t len, u_int xpixel,
    u_int ypixel, struct kitty_parse_result *result)
{
	struct kitty_context	*kc = *state;
	struct kitty_state	*ks;
	struct kitty_state	 command = { .action = 't' };
	const u_char		*semi;
	u_char			*decoded, *pixels;
	u_char			*uncompressed;
	size_t			 controllen, payloadlen, decodedlen;
	uLongf			 uncompressedlen;
	u_int			 sx, sy, cell_width, cell_height;
	uint64_t		 canvas_width, canvas_height;
	struct image		*im = NULL, *source;
	int			 error;

	if (kc == NULL) {
		kc = xcalloc(1, sizeof *kc);
		*state = kc;
	}
	memset(result, 0, sizeof *result);
	result->status = KITTY_PARSE_ERROR;
	ks = kc->transfer;
	semi = memchr(buf, ';', len);
	controllen = (semi == NULL ? len : (size_t)(semi - buf));
	payloadlen = (semi == NULL ? 0 : len - controllen - 1);
	if (ks != NULL) {
		/* A delete command aborts an incomplete upload. */
		(void)kitty_control(&command, buf, controllen);
		if (command.action == 'd') {
			kitty_state_free(ks);
			kc->transfer = ks = NULL;
		}
	}
	if (ks == NULL) {
		ks = xcalloc(1, sizeof *ks);
		ks->action = 't';
		ks->delete = 'a';
		ks->format = 32;
		ks->medium = 'd';
	}
	ks->more = 0;
	error = kitty_control(ks, buf, controllen);
	if (ks->image_id == 0) {
		if (ks->image_number == 0)
			ks->placement_id = 0;
	}
	result->image_id = ks->image_id;
	result->image_number = ks->image_number;
	result->quiet = ks->quiet;
	result->action = ks->action;
	result->delete = ks->delete;
	result->placement_id = ks->placement_id;
	result->z = ks->z;
	result->x = ks->source_x;
	result->y = ks->source_y;
	if (error != 0)
		goto fail;
	if (ks->image_number != 0) {
		if (ks->image_id != 0)
			goto fail;
		result->status = KITTY_PARSE_UNSUPPORTED;
		goto fail;
	}
	if (ks->unsupported) {
		result->status = KITTY_PARSE_UNSUPPORTED;
		goto fail;
	}
	if (ks->medium != 'd') {
		result->status = KITTY_PARSE_UNSUPPORTED;
		goto fail;
	}
	if (payloadlen != 0) {
		if (kitty_append(ks, semi + 1, payloadlen) != 0)
			goto fail;
	}
	if (ks->more) {
		kc->transfer = ks;
		result->status = KITTY_PARSE_MORE;
		return (NULL);
	}
	kc->transfer = NULL;
	if (ks->action == 'p') {
		source = kitty_source_get(kc, ks->image_id);
		if (source == NULL) {
			result->status = KITTY_PARSE_MISSING;
			im = NULL;
		} else if (ks->virtual) {
			im = kitty_place_image(source, ks, xpixel, ypixel);
			image_free(image_get_id(source));
			if (im != NULL) {
				(void)kitty_placement_set(kc, ks, im);
				image_free(image_get_id(im));
				im = NULL;
				result->action = 'u';
				result->status = KITTY_PARSE_OK;
			}
		} else {
			im = kitty_place_image(source, ks, xpixel, ypixel);
			image_free(image_get_id(source));
			if (im != NULL) {
				result->replace_id =
				    kitty_placement_set(kc, ks, im);
				result->status = KITTY_PARSE_OK;
			}
		}
		kitty_state_free(ks);
		return (im);
	}
	if (ks->action == 'd') {
		switch (ks->delete) {
		case 'a': case 'A': case 'i': case 'I':
		case 'c': case 'C': case 'p': case 'P':
		case 'q': case 'Q': case 'r': case 'R':
		case 'x': case 'X': case 'y': case 'Y':
		case 'z': case 'Z':
			result->status = KITTY_PARSE_OK;
			break;
		default:
			result->status = KITTY_PARSE_UNSUPPORTED;
			break;
		}
		kitty_state_free(ks);
		return (NULL);
	}
	switch (ks->action) {
	case 'T': case 't': case 'q':
		break;
	case 'f': case 'a': case 'c':
		result->status = KITTY_PARSE_UNSUPPORTED;
		/* FALLTHROUGH */
	default:
		goto fail;
	}

	decoded = ks->raw;
	decodedlen = ks->rawlen;
	ks->raw = NULL;
	if (ks->format == 100) {
		if (ks->compression == 'z') {
			if (ks->data_size == 0 ||
			    ks->data_size > IMAGE_SIZE_LIMIT) {
				free(decoded);
				goto fail;
			}
			uncompressedlen = ks->data_size;
			uncompressed = xmalloc(uncompressedlen);
			if (uncompress(uncompressed, &uncompressedlen, decoded,
			    decodedlen) != Z_OK ||
			    uncompressedlen != ks->data_size) {
				free(decoded);
				free(uncompressed);
				goto fail;
			}
			free(decoded);
			decoded = uncompressed;
			decodedlen = uncompressedlen;
		} else if (ks->compression != '\0') {
			free(decoded);
			goto fail;
		}
		pixels = image_png_decode(decoded, decodedlen, IMAGE_SIZE_LIMIT,
		    &ks->width, &ks->height);
		free(decoded);
	} else if (ks->format == 24 || ks->format == 32) {
		pixels = kitty_raw(ks, decoded, decodedlen);
		if (pixels != decoded)
			free(decoded);
	} else {
		free(decoded);
		goto fail;
	}
	if (pixels == NULL)
		goto fail;

	cell_width = (xpixel == 0 ? 8 : xpixel);
	cell_height = (ypixel == 0 ? 16 : ypixel);
	image_size_in_cells(ks->width, ks->height, cell_width, cell_height,
	    &sx, &sy);
	canvas_width = (uint64_t)sx * cell_width;
	canvas_height = (uint64_t)sy * cell_height;
	if (canvas_width > UINT_MAX || canvas_height > UINT_MAX)
		source = NULL;
	else
		source = image_create(ks->width, ks->height, canvas_width,
		    canvas_height, sx, sy, pixels);
	if (source == NULL)
		free(pixels);
	else {
		result->status = KITTY_PARSE_OK;
		if (ks->action != 'q')
			result->replace_id = kitty_source_set(kc, ks->image_id, source);
		if (ks->action == 'q')
			im = NULL;
		else if (ks->action == 'T' && !ks->virtual) {
			im = kitty_place_image(source, ks, xpixel, ypixel);
			if (im == NULL)
				result->status = KITTY_PARSE_ERROR;
			else
				(void)kitty_placement_set(kc, ks, im);
		} else if (ks->virtual) {
			im = kitty_place_image(source, ks, xpixel, ypixel);
			if (im == NULL)
				result->status = KITTY_PARSE_ERROR;
			else {
				(void)kitty_placement_set(kc, ks, im);
				image_free(image_get_id(im));
				im = NULL;
				result->action = 'u';
			}
		} else {
			im = NULL;
		}
		image_free(image_get_id(source));
	}
	kitty_state_free(ks);
	return (im);

fail:
	kc->transfer = NULL;
	kitty_state_free(ks);
	return (NULL);
}

/* Decode one UTF-8 character from a Kitty placeholder. */
static int
kitty_placeholder_character(const u_char *data, size_t size, size_t *offset,
    uint32_t *value)
{
	u_char	 ch;
	u_int	 needed, i;
	uint32_t result;

	if (*offset >= size)
		return (0);
	ch = data[(*offset)++];
	if (ch < 0x80) {
		*value = ch;
		return (1);
	}
	if ((ch & 0xe0) == 0xc0) {
		needed = 1;
		result = ch & 0x1f;
	} else if ((ch & 0xf0) == 0xe0) {
		needed = 2;
		result = ch & 0x0f;
	} else if ((ch & 0xf8) == 0xf0) {
		needed = 3;
		result = ch & 0x07;
	} else
		return (0);
	if (needed > size - *offset)
		return (0);
	for (i = 0; i < needed; i++) {
		ch = data[(*offset)++];
		if ((ch & 0xc0) != 0x80)
			return (0);
		result = (result << 6)|(ch & 0x3f);
	}
	*value = result;
	return (1);
}

/* Return the Kitty placeholder diacritic index for a character. */
static int
kitty_placeholder_index(uint32_t value, u_int *index)
{
	u_int	i;

	for (i = 0; i < nitems(kitty_diacritics); i++) {
		if (kitty_diacritics[i] == value) {
			*index = i;
			return (1);
		}
	}
	return (0);
}

/* Return whether a cell starts with a Kitty Unicode placeholder. */
int
kitty_cell_is_placeholder(const struct grid_cell *gc)
{
	if (gc->data.size < 4)
		return (0);
	if (gc->data.data[0] != 0xf4)
		return (0);
	if (gc->data.data[1] != 0x8e)
		return (0);
	if (gc->data.data[2] != 0xbb)
		return (0);
	if (gc->data.data[3] != 0xae)
		return (0);
	return (1);
}

/* Decode an image or placement ID from a placeholder colour. */
static int
kitty_colour_id(int colour, u_int *id)
{
	if (colour < 0)
		return (0);
	if (colour & COLOUR_FLAG_RGB)
		*id = colour & 0xffffff;
	else if (colour & COLOUR_FLAG_256)
		*id = colour & 0xff;
	else if (colour < 8)
		*id = colour;
	else if (colour >= 90) {
		if (colour >= 98)
			return (0);
		*id = colour - 90 + 8;
	} else
		return (0);
	return (1);
}

/* Resolve a Kitty Unicode placeholder to an image and source cell. */
int
kitty_placeholder_to_image(void *state, struct grid *gd, struct grid_cell *gc,
    u_int grid_x, u_int grid_y, struct kitty_placeholder *placeholder)
{
	struct kitty_context	*kc = state;
	struct kitty_source	*source;
	struct kitty_placement	*placement;
	struct kitty_placeholder	 left;
	struct grid_cell		 left_cell;
	struct image		*im;
	uint32_t		 value;
	size_t			 offset = 0;
	u_int			 values[3], nvalues = 0, id, x = 0, y = 0;
	u_int			 sx, sy, placement_id = 0;
	int			 inherit = 0;

	if (kc == NULL)
		return (0);
	if (!kitty_placeholder_character(gc->data.data, gc->data.size, &offset,
	    &value))
		return (0);
	if (value != 0x10eeee)
		return (0);
	while (offset < gc->data.size && nvalues < nitems(values)) {
		if (!kitty_placeholder_character(gc->data.data, gc->data.size,
		    &offset, &value))
			return (0);
		if (!kitty_placeholder_index(value, &values[nvalues]))
			return (0);
		nvalues++;
	}
	if (offset != gc->data.size)
		return (0);

	if (!kitty_colour_id(gc->fg, &id))
		return (0);
	(void)kitty_colour_id(gc->us, &placement_id);
	if (nvalues >= 1)
		y = values[0];
	if (nvalues >= 2)
		x = values[1];
	if (grid_x != 0) {
		grid_view_get_cell(gd, grid_x - 1, grid_y, &left_cell);
		if (left_cell.fg == gc->fg) {
			if (left_cell.us == gc->us) {
				inherit = image_grid_get_placeholder(gd,
				    grid_x - 1, gd->hsize + grid_y, &left);
			}
		}
	}
	if (inherit) {
		if (nvalues >= 1) {
			if (left.source_y != y)
				inherit = 0;
		}
		if (nvalues >= 2) {
			if (left.source_x + 1 != x)
				inherit = 0;
		}
	}
	if (inherit) {
		if (nvalues == 0)
			y = left.source_y;
		if (nvalues < 2)
			x = left.source_x + 1;
		if (nvalues < 3)
			id |= left.image_id & 0xff000000;
	}
	if (nvalues == 3) {
		if (values[2] > 255)
			return (0);
		id |= values[2] << 24;
	}
	source = kitty_source_find(kc, id);
	if (source == NULL)
		return (0);
	for (placement = source->placements; placement != NULL;
	    placement = placement->next) {
		if (!placement->virtual)
			continue;
		if (placement_id != 0) {
			if (placement->placement_id != placement_id)
				continue;
		}
		break;
	}
	if (placement != NULL) {
		im = image_find(placement->server_id);
		placeholder->z = placement->z;
		placement_id = placement->placement_id;
	} else
		return (0);
	if (im == NULL)
		return (0);
	image_get_size_in_cells(im, &sx, &sy);
	if (x >= sx || y >= sy)
		return (0);

	placeholder->image = im;
	placeholder->source_x = x;
	placeholder->source_y = y;
	placeholder->image_id = id;
	placeholder->placement_id = placement_id;
	return (1);
}
