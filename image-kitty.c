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
#include <resolv.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "tmux.h"

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
	u_int	 placement_id;
	int32_t	 z;
	u_int	 quiet;
	int	 no_cursor;
	int	 virtual;
	u_int	 data_size;
	int	 more;

	char	*encoded;
	size_t	 encodedlen;
};

struct kitty_placement {
	u_int			 placement_id;
	u_int			 server_id;
	int32_t			 z;
	struct kitty_placement	*next;
};

struct kitty_source {
	u_int			 app_id;
	u_int			 server_id;
	u_int			 virtual_id;
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

/* Delete Kitty placements intersecting a redraw area. */
void
kitty_redraw_start(struct tty *tty, u_int x, u_int y, u_int width,
    u_int height)
{
	struct kitty_output		 *ko = tty->image_data;
	struct kitty_image_cache	 *entry;
	struct kitty_placement_cache	**pp, *placement;
	char				  s[64];

	if (ko == NULL)
		return;
	for (entry = ko->images; entry != NULL; entry = entry->next) {
		for (pp = &entry->placements; (placement = *pp) != NULL; ) {
			if (placement->x >= x + width ||
			    placement->x + placement->width <= x ||
			    placement->y >= y + height ||
			    placement->y + placement->height <= y) {
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
	placement->next = entry->placements;
	entry->placements = placement;
	tty_cursor(tty, destination_x, destination_y);
	xsnprintf(control, sizeof control,
	    "\033_Ga=p,i=%u,p=%llu,x=%u,y=%u,w=%u,h=%u,c=%u,r=%u,z=%d,"
	    "C=1,q=2\033\\", entry->kitty_id,
	    (unsigned long long)placement->id, px, py, pwidth, pheight, width,
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
	u_char				 raw[3072];
	const u_char			*pixels;
	u_char				*padded;
	size_t				 offset, size, copied, row, column;
	size_t				 available, stride, image_size;
	int				 encodedlen;
	u_int				 id, width, height;
	u_int				 canvas_width, canvas_height;
	u_int				 upload_width, upload_height;

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
	image_get_size(im, &width, &height);
	image_get_canvas_size(im, &canvas_width, &canvas_height);
	if (canvas_width > UINT_MAX - 2 || canvas_height > UINT_MAX - 2)
		return (NULL);
	upload_width = canvas_width + 2;
	upload_height = canvas_height + 2;
	if ((uint64_t)upload_width * upload_height * 4 > IMAGE_SIZE_LIMIT)
		return (NULL);
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
	stride = (size_t)width * 4;
	for (offset = 0; offset < image_size; offset += size) {
		size = image_size - offset;
		if (size > sizeof raw)
			size = sizeof raw;
		for (copied = 0; copied < size; copied += available) {
			row = (offset + copied) / ((size_t)width * 4);
			column = (offset + copied) % ((size_t)width * 4);
			available = (size_t)width * 4 - column;
			if (available > size - copied)
				available = size - copied;
			memcpy(raw + copied, pixels + row * stride +
			    column, available);
		}
		encodedlen = b64_ntop(raw, size, encoded,
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
kitty_draw_rect(struct tty *tty, const struct image_rect *rectangle,
    __unused const struct tty_style_ctx *style_ctx)
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
	free(ks->encoded);
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

/* Free placement images belonging to all Kitty source images. */
static void
kitty_placements_free_all(struct kitty_context *kc)
{
	struct kitty_source	*source;

	for (source = kc->sources; source != NULL; source = source->next)
		kitty_placements_free(source);
}

/* Remove all parser-side placements at a Kitty z-index. */
static void
kitty_placements_remove_z(struct kitty_context *kc, int32_t z)
{
	struct kitty_source	*source;
	struct kitty_placement	**pp, *placement;

	for (source = kc->sources; source != NULL; source = source->next) {
		for (pp = &source->placements; (placement = *pp) != NULL; ) {
			if (placement->z != z) {
				pp = &placement->next;
				continue;
			}
			*pp = placement->next;
			image_free(placement->server_id);
			free(placement);
		}
	}
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
		kitty_placements_free(source);
		if (source->virtual_id != 0)
			image_free(source->virtual_id);
		image_free(source->server_id);
		free(source);
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
		if (source->virtual_id != 0) {
			image_free(source->virtual_id);
			source->virtual_id = 0;
		}
		image_free(source->server_id);
	}
	image_ref(image_get_id(im));
	source->server_id = image_get_id(im);
	return (old_id);
}

/* Associate a placement ID with an image. */
static u_int
kitty_placement_set(struct kitty_context *kc, u_int image_id,
    u_int placement_id, int32_t z, struct image *im)
{
	struct kitty_source	*source;
	struct kitty_placement	*placement;
	u_int			 old_id;

	if (placement_id == 0)
		return (0);
	source = kitty_source_find(kc, image_id);
	if (source == NULL)
		return (0);
	for (placement = source->placements; placement != NULL;
	    placement = placement->next) {
		if (placement->placement_id == placement_id)
			break;
	}
	if (placement == NULL) {
		placement = xcalloc(1, sizeof *placement);
		placement->placement_id = placement_id;
		placement->next = source->placements;
		source->placements = placement;
	}
	old_id = placement->server_id;
	placement->z = z;
	image_ref(image_get_id(im));
	placement->server_id = image_get_id(im);
	if (old_id != 0)
		image_free(old_id);
	return (old_id);
}

/* Remove and return the image associated with a Kitty placement ID. */
static struct image *
kitty_placement_remove(struct kitty_context *kc, u_int image_id,
    u_int placement_id)
{
	struct kitty_source	*source;
	struct kitty_placement	**pp, *placement;
	struct image		*im;

	source = kitty_source_find(kc, image_id);
	if (source == NULL)
		return (NULL);
	for (pp = &source->placements; (placement = *pp) != NULL;
	    pp = &placement->next) {
		if (placement->placement_id != placement_id)
			continue;
		im = image_find(placement->server_id);
		if (im != NULL)
			image_ref(image_get_id(im));
		*pp = placement->next;
		image_free(placement->server_id);
		free(placement);
		return (im);
	}
	return (NULL);
}

/* Replace the virtual image associated with a Kitty source image. */
static void
kitty_virtual_set(struct kitty_context *kc, u_int id, struct image *im)
{
	struct kitty_source	*source;

	source = kitty_source_find(kc, id);
	if (source == NULL)
		return;
	if (source->virtual_id != 0)
		image_free(source->virtual_id);
	image_ref(image_get_id(im));
	source->virtual_id = image_get_id(im);
}

/* Remove and return a Kitty source image. */
static struct image *
kitty_source_remove(struct kitty_context *kc, u_int id)
{
	struct kitty_source	**pp, *source;
	struct image		*im;

	for (pp = &kc->sources; (source = *pp) != NULL; pp = &source->next) {
		if (source->app_id != id)
			continue;
		im = image_find(source->server_id);
		if (im != NULL)
			image_ref(image_get_id(im));
		*pp = source->next;
		kitty_placements_free(source);
		if (source->virtual_id != 0)
			image_free(source->virtual_id);
		image_free(source->server_id);
		free(source);
		return (im);
	}
	return (NULL);
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

/* Append encoded payload data to a Kitty graphics command. */
static int
kitty_append(struct kitty_state *ks, const u_char *buf, size_t len)
{
	if (len > IMAGE_SIZE_LIMIT ||
	    ks->encodedlen > IMAGE_SIZE_LIMIT - len)
		return (-1);
	ks->encoded = xrealloc(ks->encoded, ks->encodedlen + len + 1);
	memcpy(ks->encoded + ks->encodedlen, buf, len);
	ks->encodedlen += len;
	ks->encoded[ks->encodedlen] = '\0';
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
	if (ks->width == 0 || ks->height == 0 ||
	    (uint64_t)ks->width * ks->height * bytes > IMAGE_SIZE_LIMIT)
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
	uint64_t	 numerator, denominator, value;
	u_int		 x, y, width, height, sx, sy, canvas_width;
	u_int		 canvas_height, cell_width, cell_height, source_width;
	u_int		 source_height, display_width, display_height;
	struct image	*im;

	x = ks->source_x;
	y = ks->source_y;
	image_get_size(source, &source_width, &source_height);
	if (x >= source_width || y >= source_height)
		return (NULL);
	width = ks->source_width;
	if (width == 0 || width > source_width - x)
		width = source_width - x;
	height = ks->source_height;
	if (height == 0 || height > source_height - y)
		height = source_height - y;
	if (ks->x_offset > UINT_MAX - width ||
	    ks->y_offset > UINT_MAX - height)
		return (NULL);
	display_width = width + ks->x_offset;
	display_height = height + ks->y_offset;

	cell_width = (xpixel == 0 ? 8 : xpixel);
	cell_height = (ypixel == 0 ? 16 : ypixel);
	if (ks->columns == 0 && ks->rows == 0) {
		image_size_in_cells(display_width, display_height, cell_width,
		    cell_height,
		    &sx, &sy);
		value = (uint64_t)sx * cell_width;
		if (value > UINT_MAX)
			return (NULL);
		canvas_width = value;
		value = (uint64_t)sy * cell_height;
		if (value > UINT_MAX)
			return (NULL);
		canvas_height = value;
	} else if (ks->columns != 0 && ks->rows != 0) {
		sx = ks->columns;
		sy = ks->rows;
		canvas_width = display_width;
		canvas_height = display_height;
	} else if (ks->columns != 0) {
		sx = ks->columns;
		numerator = (uint64_t)display_height * sx * cell_width;
		denominator = (uint64_t)display_width * cell_height;
		value = (numerator + denominator - 1) / denominator;
		if (value == 0 || value > UINT_MAX)
			return (NULL);
		sy = value;
		canvas_width = display_width;
		numerator = (uint64_t)sy * cell_height * display_width;
		denominator = (uint64_t)sx * cell_width;
		value = (numerator + denominator - 1) / denominator;
		if (value < display_height)
			value = display_height;
		if (value > UINT_MAX)
			return (NULL);
		canvas_height = value;
	} else {
		sy = ks->rows;
		numerator = (uint64_t)display_width * sy * cell_height;
		denominator = (uint64_t)display_height * cell_width;
		value = (numerator + denominator - 1) / denominator;
		if (value == 0 || value > UINT_MAX)
			return (NULL);
		sx = value;
		canvas_height = display_height;
		numerator = (uint64_t)sx * cell_width * display_height;
		denominator = (uint64_t)sy * cell_height;
		value = (numerator + denominator - 1) / denominator;
		if (value < display_width)
			value = display_width;
		if (value > UINT_MAX)
			return (NULL);
		canvas_width = value;
	}

	im = image_create_view(source, x, y, width, height, canvas_width,
	    canvas_height, sx, sy, ks->x_offset, ks->y_offset);
	if (im != NULL && ks->no_cursor)
		image_set_no_cursor(im);
	return (im);
}

/*
 * Parse one Kitty graphics APC body (without the leading G). Only direct
 * static images are accepted. The returned image retains immutable RGBA
 * pixels for the lifetime of its placement.
 */
struct image *
kitty_parse_image(void **state, const u_char *buf, size_t len, u_int xpixel,
    u_int ypixel, u_int *image_id, u_int *replace_id, u_int *quiet,
    char *action, char *delete, u_int *placement_id, int32_t *z, int *status)
{
	struct kitty_context	*kc = *state;
	struct kitty_state	*ks;
	const u_char		*semi;
	u_char			*decoded, *pixels;
	u_char			*uncompressed;
	size_t			 controllen, payloadlen, decodedlen;
	uLongf			 uncompressedlen;
	u_int			 sx, sy, cell_width, cell_height;
	uint64_t		 canvas_width, canvas_height;
	struct image		*im = NULL, *source;
	struct kitty_source	*stored;

	if (kc == NULL) {
		kc = xcalloc(1, sizeof *kc);
		*state = kc;
	}
	*image_id = 0;
	*replace_id = 0;
	*quiet = 0;
	*action = '\0';
	*delete = '\0';
	*placement_id = 0;
	*z = 0;
	*status = KITTY_PARSE_ERROR;
	ks = kc->transfer;
	semi = memchr(buf, ';', len);
	controllen = (semi == NULL ? len : (size_t)(semi - buf));
	payloadlen = (semi == NULL ? 0 : len - controllen - 1);
	if (ks == NULL) {
		ks = xcalloc(1, sizeof *ks);
		ks->action = 't';
		ks->delete = 'a';
		ks->format = 32;
		ks->medium = 'd';
	}
	ks->more = 0;
	*image_id = ks->image_id;
	*quiet = ks->quiet;
	*action = ks->action;
	*delete = ks->delete;
	*placement_id = ks->placement_id;
	*z = ks->z;
	if (kitty_control(ks, buf, controllen) != 0 ||
	    ks->medium != 'd' ||
	    (payloadlen != 0 &&
	    kitty_append(ks, semi + 1, payloadlen) != 0))
		goto fail;

	*image_id = ks->image_id;
	*quiet = ks->quiet;
	*action = ks->action;
	*delete = ks->delete;
	*placement_id = ks->placement_id;
	*z = ks->z;
	if (ks->more) {
		kc->transfer = ks;
		*status = KITTY_PARSE_MORE;
		return (NULL);
	}
	kc->transfer = NULL;
	if (ks->action == 'p') {
		source = kitty_source_get(kc, ks->image_id);
		if (source == NULL) {
			*status = KITTY_PARSE_MISSING;
			im = NULL;
		} else if (ks->virtual) {
			im = kitty_place_image(source, ks, xpixel, ypixel);
			image_free(image_get_id(source));
			if (im != NULL) {
				kitty_virtual_set(kc, ks->image_id, im);
				image_free(image_get_id(im));
				im = NULL;
				*action = 'u';
				*status = KITTY_PARSE_OK;
			}
		} else {
			im = kitty_place_image(source, ks, xpixel, ypixel);
			image_free(image_get_id(source));
			if (im != NULL) {
				*replace_id = kitty_placement_set(kc, ks->image_id,
				    ks->placement_id, ks->z, im);
				*status = KITTY_PARSE_OK;
			}
		}
		kitty_state_free(ks);
		return (im);
	}
	if (ks->action == 'd') {
		if (ks->delete == 'a' || ks->delete == 'A') {
			kitty_placements_free_all(kc);
			im = NULL;
		} else if (ks->delete == 'i') {
			if (ks->placement_id != 0)
				im = kitty_placement_remove(kc, ks->image_id,
				    ks->placement_id);
			else {
				source = kitty_source_get(kc, ks->image_id);
				if (source != NULL) {
					stored = kitty_source_find(kc,
					    ks->image_id);
					kitty_placements_free(stored);
				}
				im = source;
			}
		} else if (ks->delete == 'I')
			im = kitty_source_remove(kc, ks->image_id);
		else if (ks->delete == 'z' || ks->delete == 'Z') {
			kitty_placements_remove_z(kc, ks->z);
			im = NULL;
		} else
			goto fail;
		if ((ks->delete == 'i' || ks->delete == 'I') &&
		    im == NULL)
			*status = KITTY_PARSE_MISSING;
		else
			*status = KITTY_PARSE_OK;
		kitty_state_free(ks);
		return (im);
	}
	if (ks->action != 'T' && ks->action != 't' && ks->action != 'q')
		goto fail;

	decoded = image_base64_decode(ks->encoded, ks->encodedlen,
	    IMAGE_SIZE_LIMIT, &decodedlen);
	if (decoded == NULL)
		goto fail;
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
		*status = KITTY_PARSE_OK;
		if (ks->action != 'q')
			*replace_id = kitty_source_set(kc, ks->image_id, source);
		if (ks->action == 'T' && !ks->virtual) {
			im = kitty_place_image(source, ks, xpixel, ypixel);
			if (im == NULL)
				*status = KITTY_PARSE_ERROR;
			else if (ks->placement_id != 0)
				(void)kitty_placement_set(kc, ks->image_id,
				    ks->placement_id, ks->z, im);
		} else if (ks->virtual) {
			im = kitty_place_image(source, ks, xpixel, ypixel);
			if (im == NULL)
				*status = KITTY_PARSE_ERROR;
			else {
				kitty_virtual_set(kc, ks->image_id, im);
				image_free(image_get_id(im));
				im = NULL;
				*action = 'u';
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

/* Resolve a Kitty Unicode placeholder to an image and source cell. */
int
kitty_placeholder_to_image(void *state, struct grid *gd, struct grid_cell *gc,
    u_int grid_x, u_int grid_y, struct image **image, u_int *source_x,
    u_int *source_y, u_int *image_id, u_int *placement_id, int32_t *z)
{
	struct kitty_context	*kc = state;
	struct kitty_source	*source;
	struct image		*im;
	uint32_t		 value;
	size_t			 offset = 0;
	u_int			 values[3], nvalues = 0, id, x, y, sx, sy;
	u_int			 left_x, left_y;

	if (kc == NULL ||
	    !kitty_placeholder_character(gc->data.data, gc->data.size, &offset,
	    &value) || value != 0x10eeee)
		return (0);
	while (offset < gc->data.size && nvalues < nitems(values)) {
		if (!kitty_placeholder_character(gc->data.data, gc->data.size,
		    &offset, &value) ||
		    !kitty_placeholder_index(value, &values[nvalues]))
			return (0);
		nvalues++;
	}
	if (offset != gc->data.size)
		return (0);

	if (gc->fg & COLOUR_FLAG_RGB)
		id = gc->fg & 0xffffff;
	else if (gc->fg >= 0 && gc->fg <= 255)
		id = gc->fg;
	else
		return (0);
	if (nvalues == 3)
		id |= values[2] << 24;
	source = kitty_source_find(kc, id);
	if (source == NULL)
		return (0);
	im = image_find(source->virtual_id != 0 ? source->virtual_id :
	    source->server_id);
	if (im == NULL)
		return (0);
	image_get_size_in_cells(im, &sx, &sy);

	if (nvalues >= 1)
		y = values[0];
	else {
		if (grid_x == 0 || !image_grid_get_source(gd, grid_x - 1,
		    gd->hsize + grid_y, im, &left_x, &left_y))
			return (0);
		y = left_y;
	}
	if (nvalues >= 2)
		x = values[1];
	else {
		if (grid_x == 0 || !image_grid_get_source(gd, grid_x - 1,
		    gd->hsize + grid_y, im, &left_x, &left_y) ||
		    left_x == UINT_MAX)
			return (0);
		x = left_x + 1;
	}
	if (x >= sx || y >= sy)
		return (0);

	*image = im;
	*source_x = x;
	*source_y = y;
	*image_id = id;
	if (gc->us & COLOUR_FLAG_RGB)
		*placement_id = gc->us & 0xffffff;
	else
		*placement_id = 0;
	*z = 0;
	utf8_set(&gc->data, ' ');
	return (1);
}
