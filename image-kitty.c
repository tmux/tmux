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
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/* State attached to an input parser. */
struct kitty_state {
	char				 action;
	u_int				 format;
	char				 medium;
	char				 compression;
	u_int				 width;
	u_int				 height;
	u_int				 image_id;
	u_int				 image_number;
	u_int				 placement_id;
	u_int				 quiet;
	u_int				 data_size;
	int				 more;

	u_char				*data;
	size_t				 size;

	TAILQ_HEAD(, kitty_image_map)	 images;
};

/* Map Kitty image ID to image ID. */
struct kitty_image_map {
	u_int				 kitty_id;
	u_int				 image_id;

	TAILQ_ENTRY(kitty_image_map)	 entry;
};

/* Parse Kitty graphics control data. */
static int
kitty_parse_control_data(struct kitty_state *ks, const u_char *data,
    size_t size)
{
	char		*copy, *ptr, *next, *value;
	const char	*errstr;
	u_int		 number;
	char		 key;
	int		 error = -1;

	copy = ptr = xstrndup(data, size);
	while ((next = strsep(&ptr, ",")) != NULL) {
		if (*next == '\0') {
			if (ptr == NULL)
				break;
			goto out;
		}
		key = next[0];
		if (next[1] != '=' || next[2] == '\0')
			goto out;
		value = next + 2;

		switch (key) {
		case 'a':
		case 't':
		case 'o':
			if (value[1] != '\0')
				goto out;
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
			}
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
			number = strtonum(value, 0, UINT_MAX, &errstr);
			if (errstr != NULL)
				goto out;
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
			}
			break;
		}
	}
	error = 0;

out:
	free(copy);
	return (error);
}

/* Free Kitty graphics parser state. */
void
kitty_free_state(void *state)
{
	struct kitty_state	*ks = state;
	struct kitty_image_map	*map;

	if (ks == NULL)
		return;
	free(ks->data);
	while ((map = TAILQ_FIRST(&ks->images)) != NULL) {
		TAILQ_REMOVE(&ks->images, map, entry);
		image_free(image_find(map->image_id));
		free(map);
	}
	free(ks);
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
static void
kitty_image_map_set(struct kitty_state *ks, u_int id, struct image *im)
{
	struct kitty_image_map	*map;

	if (id == 0)
		return;
	map = kitty_image_map_find(ks, id);
	if (map != NULL)
		image_free(image_find(map->image_id));
	else {
		map = xcalloc(1, sizeof *map);
		map->kitty_id = id;
		TAILQ_INSERT_HEAD(&ks->images, map, entry);
	}
	image_ref(im);
	map->image_id = image_get_id(im);
}

/* Decode and append one base64-encoded Kitty payload chunk. */
static int
kitty_append_data(struct kitty_state *ks, const u_char *data, size_t size)
{
	u_char	*d;
	size_t	 dsize;

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

	ks->data = xrealloc(ks->data, ks->size + dsize);
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

/* Parse and reply to a Kitty graphics sequence. */
void
kitty_parse_input(struct input_ctx *ictx, struct window_pane *wp, void **state,
    const u_char *buf, size_t len)
{
	struct window		*w = wp->window;
	struct kitty_state	*ks = *state;
	struct kitty_state	 cks = { .action = 't' };
	const u_char		*found;
	const char		*m = "EINVAL";
	u_char			*p = NULL;
	size_t			 csize, psize;
	u_int			 sx, sy;
	uint64_t		 cw, ch;
	struct image		*im;
	int			 error;

	buf++;
	len--;

	if (ks == NULL) {
		ks = xcalloc(1, sizeof *ks);
		TAILQ_INIT(&ks->images);
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

	if (ks->more &&
	    kitty_parse_control_data(&cks, buf, csize) == 0 &&
	    cks.action == 'd') {
		free(ks->data);
		ks->data = NULL;
		ks->size = 0;
		ks->more = 0;
	}

	if (!ks->more) {
		ks->action = 't';
		ks->format = 32;
		ks->medium = 'd';
		ks->compression = 'o';
		ks->width = 0;
		ks->height = 0;
		ks->image_id = 0;
		ks->image_number = 0;
		ks->placement_id = 0;
		ks->quiet = 0;
		ks->data_size = 0;
	}
	ks->more = 0;

	error = kitty_parse_control_data(ks, buf, csize);
	if (error != 0)
		goto out;
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
	if (ks->action != 't' && ks->action != 'q') {
		m = "ENOTSUP";
		goto out;
	}
	if (ks->compression != 'o' && ks->compression != 'z')
		goto out;

	if (psize != 0 && kitty_append_data(ks, found + 1, psize) != 0)
		goto out;
	if (ks->more)
		return;

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
	im = image_create(IMAGE_TYPE_KITTY, ks->width, ks->height, cw, ch,
	    sx, sy, p);
	if (im == NULL) {
		free(p);
		goto out;
	}

	if (ks->action != 'q')
		kitty_image_map_set(ks, ks->image_id, im);
	image_free(im);
	m = "OK";

out:
	kitty_send_reply(ictx, ks, m);

	free(ks->data);
	ks->data = NULL;
	ks->size = 0;
	ks->more = 0;
}
