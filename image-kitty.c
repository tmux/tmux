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

#include "tmux.h"

/*
 * Read Kitty graphics commands and draw images on Kitty graphics clients.
 *
 * Each input parser keeps one unfinished transfer and maps application image
 * and placement IDs to internal image IDs. Once an upload is completed, it is
 * stored as an RGBA image. Each placement then uses a child image.
 * Virtual placements appear through Unicode placeholder text cells.
 *
 * Output caches each image and placement separately for each client. Pixels
 * are scaled to the client's cell pixel size on upload and reused in redraws.
 *
 * When a redraw is started, old rectangles overlapping the redraw area are
 * marked for deletion, then new rectangles are queued during redraw. At the
 * end, old rectangles are removed, new images uploaded and new placements
 * sent. If an upload would change the drawing order of overlapping images,
 * all the affected images are deleted and uploaded again.
 */

#define KITTY_CHUNK_SIZE 3072

/* Unicode marks encoding placeholder rows, columns and image ID bits. */
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
	int				 virtual;
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
	int					 virtual;

	TAILQ_ENTRY(kitty_placement_map)	 entry;
};

/* Map Kitty image ID to image ID and placement views. */
struct kitty_image_map {
	u_int					 kitty_id;
	u_int					 image_id;
	TAILQ_HEAD(, kitty_placement_map)	 placements;

	TAILQ_ENTRY(kitty_image_map)		 entry;
};

/* An image uploaded to a terminal. */
struct kitty_image_cache {
	u_int					image_id;
	u_int					kitty_id;
	u_int					order_id;
	uint64_t				index;
	int					upload;
	u_int					xpixel;
	u_int					ypixel;
	u_int					next_placement;
	TAILQ_HEAD(, kitty_placement_cache)	placements;

	TAILQ_ENTRY(kitty_image_cache)		entry;
};

/* One grid row placed on a terminal, with positions and sizes in cells. */
struct kitty_placement_cache {
	u_int					id;
	u_int					x;
	u_int					y;
	u_int					w;
	u_int					h;
	u_int					src_x;
	u_int					src_y;
	int					pending_delete;

	TAILQ_ENTRY(kitty_placement_cache)	entry;
};

/* Image uploads and placements for one terminal. */
struct kitty_output {
	TAILQ_HEAD(, kitty_image_cache)	images;
	u_int				next_id;
	int				changed;
};

/* Select Kitty graphics output. */
int
kitty_select(struct tty *tty)
{
	if (tty->term != NULL && (tty->term->flags & TERM_KITTYGRAPHICS))
		return (1);
	return (0);
}

/* Get Kitty graphics output flags. */
int
kitty_get_flags(struct tty *tty)
{
	int	flags = IMAGE_BACKEND_GRAPHICAL;

	if (tty_term_flag(tty->term, TTYC_ISCRL))
		flags |= IMAGE_BACKEND_SCROLLS;
	return (flags);
}

/* Return the Kitty output state for a terminal. */
static struct kitty_output *
kitty_get_output(struct tty *tty)
{
	struct kitty_output	*ko = tty->image_data;

	if (ko == NULL) {
		ko = xcalloc(1, sizeof *ko);
		TAILQ_INIT(&ko->images);
		tty->image_data = ko;
	}
	return (ko);
}

/* Delete a Kitty image and its placements. */
static void
kitty_delete(struct tty *tty, u_int id)
{
	char	s[256];

	xsnprintf(s, sizeof s, "\033_Ga=d,d=I,i=%u,q=2\033\\", id);
	if (tty->flags & TTY_STARTED)
		tty_puts(tty, s);
	else
		tty_raw(tty, s);
}

/* Get a cached image in drawing order. */
static struct kitty_image_cache *
kitty_get_image(struct tty *tty, struct image *im, uint64_t index)
{
	struct kitty_output		*ko = kitty_get_output(tty);
	struct kitty_image_cache	*entry, *before;

	/* Look for this image and placement in the output cache. */
	TAILQ_FOREACH(entry, &ko->images, entry) {
		if (entry->image_id == image_get_id(im) &&
		    entry->index == index)
			return (entry);
	}

	/* Create a cache entry using this terminal's cell pixel size. */
	entry = xcalloc(1, sizeof *entry);
	entry->image_id = image_get_id(im);
	entry->order_id = image_get_order_id(im);
	entry->index = index;
	entry->upload = 1;
	entry->xpixel = tty->xpixel;
	if (entry->xpixel == 0)
		entry->xpixel = 8;
	entry->ypixel = tty->ypixel;
	if (entry->ypixel == 0)
		entry->ypixel = 16;
	TAILQ_INIT(&entry->placements);

	/* Insert the entry in image and placement drawing order. */
	TAILQ_FOREACH(before, &ko->images, entry) {
		if (entry->order_id < before->order_id ||
		    (entry->order_id == before->order_id &&
		    entry->index < before->index))
			break;
	}
	if (before != NULL)
		TAILQ_INSERT_BEFORE(before, entry, entry);
	else
		TAILQ_INSERT_TAIL(&ko->images, entry, entry);
	return (entry);
}

/* Send a rectangle of the cached output pixels. */
static void
kitty_send_placement(struct tty *tty, struct kitty_image_cache *entry,
    struct kitty_placement_cache *kpl)
{
	char	s[256];
	u_int	px = kpl->src_x * entry->xpixel + 1;
	u_int	py = kpl->src_y * entry->ypixel + 1;
	u_int	w = kpl->w * entry->xpixel, h = kpl->h * entry->ypixel;

	do
		kpl->id = ++entry->next_placement;
	while (kpl->id == 0);
	xsnprintf(s, sizeof s,
	    "\033_Ga=p,i=%u,p=%u,x=%u,y=%u,w=%u,h=%u,c=%u,r=%u,C=1,q=2\033\\",
	    entry->kitty_id, kpl->id, px, py, w, h, kpl->w, kpl->h);

	tty_cursor(tty, kpl->x, kpl->y);
	tty_puts(tty, s);
}

/* Record a placement to send after the redraw. */
static void
kitty_place(struct kitty_image_cache *entry, u_int src_x, u_int src_y,
    u_int w, u_int h, u_int dst_x, u_int dst_y)
{
	struct kitty_placement_cache	*kpl;

	kpl = xcalloc(1, sizeof *kpl);
	kpl->x = dst_x;
	kpl->y = dst_y;
	kpl->w = w;
	kpl->h = h;
	kpl->src_x = src_x;
	kpl->src_y = src_y;
	TAILQ_INSERT_HEAD(&entry->placements, kpl, entry);
}

/* Place the part of an image outside the redraw area. */
static void
kitty_redraw_keep_piece(struct kitty_image_cache *entry,
    struct kitty_placement_cache *kpl, u_int x, u_int y, u_int w, u_int h)
{
	if (w == 0 || h == 0)
		return;
	kitty_place(entry, kpl->src_x + (x - kpl->x), kpl->src_y + (y - kpl->y),
	    w, h, x, y);
}

/* Preserve placement pieces outside the redraw area. */
static void
kitty_redraw_keep(struct kitty_image_cache *entry,
    struct kitty_placement_cache *kpl, u_int x, u_int y, u_int w, u_int h)
{
	u_int	px0 = kpl->x, px1 = kpl->x + kpl->w;
	u_int	py0 = kpl->y, py1 = kpl->y + kpl->h;
	u_int	ix0, ix1, iy0, iy1;

	ix0 = (x > px0 ? x : px0);
	ix1 = (x + w < px1 ? x + w : px1);
	iy0 = (y > py0 ? y : py0);
	iy1 = (y + h < py1 ? y + h : py1);
	if (ix0 == px0 && ix1 == px1 && iy0 == py0 && iy1 == py1)
		return;

	kitty_redraw_keep_piece(entry, kpl, px0, py0, px1 - px0, iy0 - py0);
	kitty_redraw_keep_piece(entry, kpl, px0, iy1, px1 - px0, py1 - iy1);
	kitty_redraw_keep_piece(entry, kpl, px0, iy0, ix0 - px0, iy1 - iy0);
	kitty_redraw_keep_piece(entry, kpl, ix1, iy0, px1 - ix1, iy1 - iy0);
}

/* Mark placements for deletion after the redraw. */
void
kitty_redraw_start(struct tty *tty, u_int x, u_int y, u_int w, u_int h)
{
	struct kitty_output		*ko = tty->image_data;
	struct kitty_image_cache	*entry;
	struct kitty_placement_cache	*kpl;

	if (ko == NULL)
		return;

	TAILQ_FOREACH(entry, &ko->images, entry) {
		TAILQ_FOREACH(kpl, &entry->placements, entry) {
			if (kpl->pending_delete)
				continue;
			if (kpl->x >= x + w ||
			    kpl->x + kpl->w <= x ||
			    kpl->y >= y + h ||
			    kpl->y + kpl->h <= y) {
				continue;
			}
			kpl->pending_delete = 1;
			ko->changed = 1;

			/* Preserve pieces outside the redraw area. */
			kitty_redraw_keep(entry, kpl, x, y, w, h);
		}
	}
}

/* Delete one terminal placement, retaining its uploaded pixels. */
static void
kitty_delete_placement(struct tty *tty, struct kitty_image_cache *entry,
    struct kitty_placement_cache *kpl)
{
	char	s[256];

	xsnprintf(s, sizeof s, "\033_Ga=d,d=i,i=%u,p=%u,q=2\033\\",
	    entry->kitty_id, kpl->id);
	if (entry->kitty_id != 0 && kpl->id != 0)
		tty_puts(tty, s);
	TAILQ_REMOVE(&entry->placements, kpl, entry);
	free(kpl);
}

/* Move cached grid rows with a terminal scroll region. */
void
kitty_scroll(struct tty *tty, u_int x, u_int y, u_int w, u_int h, int n)
{
	struct kitty_output		*ko = tty->image_data;
	struct kitty_image_cache	*entry;
	struct kitty_placement_cache	*kpl, *next;
	int				 py;

	if (ko == NULL || !tty_term_flag(tty->term, TTYC_ISCRL))
		return;
	TAILQ_FOREACH(entry, &ko->images, entry) {
		TAILQ_FOREACH_SAFE(kpl, &entry->placements, entry, next) {
			if (kpl->x < x ||
			    kpl->x + kpl->w > x + w ||
			    kpl->y < y ||
			    kpl->y >= y + h)
				continue;
			py = (int)kpl->y - n;
			if (py < (int)y || py >= (int)(y + h)) {
				kitty_delete_placement(tty, entry, kpl);
				ko->changed = 1;
			} else
				kpl->y = py;
		}
	}
}

/* Free a cached Kitty image. */
static void
kitty_free_entry(struct tty *tty, struct kitty_image_cache *entry, int send)
{
	struct kitty_placement_cache	*kpl, *next;

	if (send && entry->kitty_id != 0)
		kitty_delete(tty, entry->kitty_id);
	TAILQ_FOREACH_SAFE(kpl, &entry->placements, entry, next) {
		TAILQ_REMOVE(&entry->placements, kpl, entry);
		free(kpl);
	}
	free(entry);
}

/* Free Kitty output state for a terminal. */
void
kitty_free_output_state(struct tty *tty, int send)
{
	struct kitty_output		*ko = tty->image_data;
	struct kitty_image_cache	*entry, *next;

	if (ko != NULL) {
		TAILQ_FOREACH_SAFE(entry, &ko->images, entry, next) {
			TAILQ_REMOVE(&ko->images, entry, entry);
			kitty_free_entry(tty, entry, send);
		}
		free(ko);
		tty->image_data = NULL;
	}
}

/* Remove cached Kitty images no longer held by the server. */
static void
kitty_free_stale_images(struct tty *tty)
{
	struct kitty_output		*ko = tty->image_data;
	struct kitty_image_cache	*entry, *next;

	if (ko != NULL) {
		TAILQ_FOREACH_SAFE(entry, &ko->images, entry, next) {
			if (image_find(entry->image_id) == NULL) {
				TAILQ_REMOVE(&ko->images, entry, entry);
				kitty_free_entry(tty, entry, 1);
				ko->changed = 1;
			}
		}
	}
}

/* Check whether any placements of two images overlap. */
static int
kitty_images_overlap(struct kitty_image_cache *a, struct kitty_image_cache *b)
{
	struct kitty_placement_cache	*akpl, *bkpl;

	TAILQ_FOREACH(akpl, &a->placements, entry) {
		TAILQ_FOREACH(bkpl, &b->placements, entry) {
			if (akpl->x < bkpl->x + bkpl->w &&
			    bkpl->x < akpl->x + akpl->w &&
			    akpl->y < bkpl->y + bkpl->h &&
			    bkpl->y < akpl->y + akpl->h)
				return (1);
		}
	}
	return (0);
}

/* Upload a canvas prepared for this terminal's cell pixel size. */
static int
kitty_upload(struct tty *tty, struct kitty_image_cache *entry)
{
	struct kitty_output	*ko = tty->image_data;
	struct image		*im = image_find(entry->image_id);
	struct image_geom	*ig = image_get_geometry(im);
	char			 s[128], e[4097];
	u_char			*p, *row, *p0;
	size_t			 offset, size, total, limit, stride;
	u_int			 x, y, w, h;
	u_int			 cw = entry->xpixel, ch = entry->ypixel;
	uint64_t		 width = (uint64_t)ig->sx * cw;
	uint64_t		 height = (uint64_t)ig->sy * ch;
	int			 elen;

	/* Check the output size and allow for a one-pixel border. */
	limit = options_get_number(global_options, "image-size-limit");
	if (width > UINT_MAX - 2 ||
	    height > UINT_MAX - 2 ||
	    width * height > limit / 4)
		return (0);
	w = width + 2;
	h = height + 2;
	if ((uint64_t)w * h > SIZE_MAX / 4)
		return (0);
	stride = (size_t)w * 4;
	total = stride * h;

	/* Allocate and scale the output pixels. */
	p = xmalloc(total);
	for (y = 0; y < height; y++) {
		row = p + (y + 1) * stride;
		for (x = 0; x < width; x++) {
			p0 = image_get_pixel(im, x, y, width, height);
			memcpy(row + (x + 1) * 4, p0, 4);
		}

		/* Fill the side borders with copies of the edge pixels. */
		memcpy(row, row + 4, 4);
		memcpy(row + stride - 4, row + stride - 8, 4);
	}
	memcpy(p, p + stride, stride);
	memcpy(p + (h - 1) * stride, p + (h - 2) * stride, stride);

	entry->upload = 0;
	do
		entry->kitty_id = ++ko->next_id;
	while (entry->kitty_id == 0);

	/* Encode and send the pixels in base64 chunks. */
	for (offset = 0; offset < total; offset += size) {
		size = total - offset;
		if (size > KITTY_CHUNK_SIZE)
			size = KITTY_CHUNK_SIZE;
		elen = b64_ntop(p + offset, size, e, sizeof e);
		if (elen < 0)
			fatalx("base64 encode failed");
		if (offset == 0) {
			xsnprintf(s, sizeof s,
			    "\033_Ga=t,f=32,s=%u,v=%u,i=%u,q=2,m=%d;",
			    w, h, entry->kitty_id, offset + size < total);
		} else {
			if (offset + size < total)
				xsnprintf(s, sizeof s, "\033_Gm=1;");
			else
				xsnprintf(s, sizeof s, "\033_Gm=0;");
		}
		tty_puts(tty, s);
		tty_putn(tty, e, elen, 0);
		tty_puts(tty, "\033\\");
	}
	free(p);
	return (1);
}

/* Finish drawing, replacing overlapping images if needed. */
void
kitty_redraw_finish(struct tty *tty)
{
	struct kitty_output		*ko = tty->image_data;
	struct kitty_image_cache	*entry, *other, *next_entry;
	struct kitty_placement_cache	*kpl, *next;
	int				 changed;

	if (ko == NULL)
		return;
	kitty_free_stale_images(tty);

	if (!ko->changed)
		return;
	ko->changed = 0;

	/* Delete placements marked during the redraw. */
	TAILQ_FOREACH_SAFE(entry, &ko->images, entry, next_entry) {
		TAILQ_FOREACH_SAFE(kpl, &entry->placements, entry, next) {
			if (kpl->pending_delete)
				kitty_delete_placement(tty, entry, kpl);
		}
		if (TAILQ_EMPTY(&entry->placements)) {
			TAILQ_REMOVE(&ko->images, entry, entry);
			kitty_free_entry(tty, entry, 1);
			continue;
		}
		if (entry->kitty_id == 0)
			entry->upload = 1;
	}

	/* Include the overlap group if an upload needs to be replaced. */
	do {
		changed = 0;
		TAILQ_FOREACH(entry, &ko->images, entry) {
			other = entry;
			while ((other = TAILQ_NEXT(other, entry)) != NULL) {
				if (entry->upload && other->upload)
					continue;
				if (!entry->upload &&
				    !other->upload &&
				    entry->kitty_id < other->kitty_id)
					continue;
				if (!kitty_images_overlap(entry, other))
					continue;
				entry->upload = other->upload = 1;
				changed = 1;
			}
		}
	} while (changed);

	/* Delete affected uploads and reset their placement IDs. */
	TAILQ_FOREACH(entry, &ko->images, entry) {
		if (!entry->upload || entry->kitty_id == 0)
			continue;
		kitty_delete(tty, entry->kitty_id);
		entry->kitty_id = 0;
		TAILQ_FOREACH(kpl, &entry->placements, entry)
			kpl->id = 0;
	}

	/* Upload images in drawing order and send new placements. */
	TAILQ_FOREACH(entry, &ko->images, entry) {
		if (entry->upload && !kitty_upload(tty, entry))
			continue;
		TAILQ_FOREACH(kpl, &entry->placements, entry) {
			if (kpl->id == 0)
				kitty_send_placement(tty, entry, kpl);
		}
	}
}

/* Draw an image rectangle using the Kitty graphics protocol. */
void
kitty_draw_rect(struct tty *tty, struct image_rect *rect)
{
	struct kitty_image_cache	*entry;

	entry = kitty_get_image(tty, rect->image, rect->index);
	kitty_get_output(tty)->changed = 1;
	kitty_place(entry, rect->src_x, rect->src_y,
	    rect->sx, rect->sy, rect->dst_x, rect->dst_y);
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
		case 'U':
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
			case 'U':
				ks->virtual = (number != 0);
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
			if (kpl->virtual)
				image_free(image_find(kpl->image_id));
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
				if (kpl->virtual)
					image_free(image_find(kpl->image_id));
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
		image_clear(ctx, map->image_id);
		while ((kpl = TAILQ_FIRST(&map->placements)) != NULL) {
			TAILQ_REMOVE(&map->placements, kpl, entry);
			if (kpl->virtual)
				image_free(image_find(kpl->image_id));
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
    struct kitty_image_map *map, u_int id, struct image *im, int virtual)
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
	} else {
		if (kpl->virtual)
			image_free(image_find(kpl->image_id));
		else
			image_clear(ctx, kpl->image_id);
	}
	kpl->image_id = image_get_id(im);
	kpl->virtual = virtual;
	if (virtual)
		image_ref(im);
}

/* Remove mappings for absent placements. */
static u_int
kitty_prune_placements(struct kitty_image_map *map, struct screen *s)
{
	struct kitty_placement_map	*kpl, *kpl1;
	u_int				 removed = 0;
	struct grid			*sgd = s->saved_grid;

	TAILQ_FOREACH_SAFE(kpl, &map->placements, entry, kpl1) {
		if (kpl->virtual)
			continue;
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
	struct kitty_placement_map	*kpl, *kpl1;
	u_int				 removed, n;
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
			TAILQ_FOREACH_SAFE(kpl, &map->placements, entry, kpl1) {
				if (ks->placement_id != 0 &&
				    kpl->kitty_id != ks->placement_id)
					continue;
				if (!kpl->virtual) {
					n = image_grid_clear(gd, kpl->image_id);
					removed += n;
					continue;
				}
				TAILQ_REMOVE(&map->placements, kpl, entry);
				image_free(image_find(kpl->image_id));
				free(kpl);
				removed++;
			}
			if (ks->placement_id == 0)
				removed++;
		}
		removed += kitty_prune_placements(map, s);
		if (!release || removed == 0 || !TAILQ_EMPTY(&map->placements))
			continue;
		if (image_grid_has_image(gd, map->image_id))
			continue;
		if (s->saved_grid != NULL &&
		    image_grid_has_image(s->saved_grid, map->image_id))
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
	size_t	 dsize, allocated, limit;

	limit = options_get_number(global_options, "image-size-limit");
	if (size > limit || ks->size > limit)
		return (-1);

	d = image_base64_decode(data, size, limit, &dsize);
	if (d == NULL)
		return (-1);
	if (dsize == 0) {
		free(d);
		return (0);
	}
	if (dsize > limit - ks->size) {
		free(d);
		return (-1);
	}

	if (ks->size + dsize > ks->allocated) {
		allocated = ks->allocated;
		if (allocated == 0)
			allocated = 4096;
		while (allocated < ks->size + dsize) {
			if (allocated > limit / 2)
				allocated = limit;
			else
				allocated *= 2;
		}
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
	size_t			 stride, limit;
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
		if (ks->virtual) {
			size = fmin(ig.scaled_width / ig.src_w,
			    ig.scaled_height / ig.src_h);
			ig.scaled_width = ig.src_w * size;
			ig.scaled_height = ig.src_h * size;
		}
	}
	limit = options_get_number(global_options, "image-size-limit");
	if (ig.sx > USHRT_MAX ||
	    ig.sy > USHRT_MAX ||
	    ig.sx * ig.sy > limit)
		return (NULL);

	/* Record geometry in the input terminal's pixel coordinates. */
	cw = (uint64_t)ig.sx * xpixel;
	ch = (uint64_t)ig.sy * ypixel;
	if (cw > UINT_MAX || ch > UINT_MAX)
		return (NULL);
	ig.canvas_width = cw;
	ig.canvas_height = ch;
	if (ks->virtual && ks->columns != 0 && ks->rows != 0) {
		ig.canvas_xoff += (cw - ig.canvas_xoff - ig.scaled_width) / 2;
		ig.canvas_yoff += (ch - ig.canvas_yoff - ig.scaled_height) / 2;
	}
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
	size_t			 csize, psize, limit;
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

	/* Reset the control fields when starting a new transfer. */
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
		ks->virtual = 0;
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

	/* Collect the image data until the last chunk arrives. */
	if (psize != 0 && kitty_append_data(ks, found + 1, psize) != 0)
		goto out;
	if (ks->more)
		return;

	/* Place an image which was uploaded earlier. */
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

	/* Decode the completed transfer into RGBA pixels. */
	if (ks->compression != 'o' && ks->compression != 'z')
		goto out;
	limit = options_get_number(global_options, "image-size-limit");
	switch (ks->format) {
	case 24:
	case 32:
		p = image_raw_decode(ks->data, ks->size, limit,
		    ks->format, ks->width, ks->height, ks->compression == 'z');
		break;
	case 100:
		p = image_png_decode(ks->data, ks->size, limit,
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

	if (ks->action != 'q' && (ks->action == 'T' || ks->virtual)) {
		im = kitty_place_image(src, ks, w->xpixel, w->ypixel);
		if (im == NULL)
			goto out;
	}
	if (ks->action != 'q')
		map = kitty_image_map_set(ks, ctx, ks->image_id, src);

place:
	if (im != NULL) {
		image_set_order_id(im, image_get_order_id(src));
		kitty_placement_map_set(ctx, map, ks->placement_id, im,
		    ks->virtual);
		if (!ks->virtual)
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

/* Decode one UTF-8 character from a Kitty placeholder. */
static int
kitty_placeholder_character(const u_char *data, size_t size, size_t *offset,
    uint32_t *value)
{
	u_char		ch;
	u_int		needed, i;
	uint32_t	result;

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
    u_int grid_x, u_int grid_y, struct kitty_placeholder *ph)
{
	struct kitty_state		*ks = state;
	struct kitty_image_map		*map;
	struct kitty_placement_map	*kpl;
	struct kitty_placeholder	 left;
	struct grid_cell		 left_cell;
	struct image			*im;
	struct image_geom		*ig;
	uint32_t			 value;
	size_t				 offset = 0;
	u_int				 values[3], nvalues = 0, id;
	u_int				 x = 0, y = 0, placement_id = 0;
	int				 inherit = 0;

	if (ks == NULL)
		return (0);

	/* Decode the base character and its row, column and image ID marks. */
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

	/* Inherit missing values from the matching placeholder to the left. */
	if (grid_x != 0) {
		grid_view_get_cell(gd, grid_x - 1, grid_y, &left_cell);
		if (left_cell.fg == gc->fg && left_cell.us == gc->us)
			inherit = image_grid_get_placeholder(gd, grid_x - 1,
			    gd->hsize + grid_y, &left);
	}
	if (inherit && ((nvalues >= 1 && left.src_y != y) ||
	    (nvalues >= 2 && left.src_x + 1 != x)))
		inherit = 0;
	if (inherit) {
		if (nvalues == 0)
			y = left.src_y;
		if (nvalues < 2)
			x = left.src_x + 1;
		if (nvalues < 3)
			id |= left.image_id & 0xff000000;
	}
	if (nvalues == 3) {
		if (values[2] > 255)
			return (0);
		id |= values[2] << 24;
	}

	/* Resolve the application image ID and its virtual placement. */
	map = kitty_image_map_find(ks, id);
	if (map == NULL)
		return (0);
	TAILQ_FOREACH(kpl, &map->placements, entry) {
		if (!kpl->virtual)
			continue;
		if (placement_id != 0 && kpl->kitty_id != placement_id)
			continue;
		break;
	}
	if (kpl == NULL)
		return (0);
	im = image_find(kpl->image_id);
	if (im == NULL)
		return (0);
	ig = image_get_geometry(im);
	if (x >= ig->sx || y >= ig->sy)
		return (0);
	ph->image = im;
	ph->src_x = x;
	ph->src_y = y;
	ph->image_id = id;
	return (1);
}
