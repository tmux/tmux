/* $OpenBSD$ */

/*
 * Copyright (c) 2026 Thomas Adam <thomas@xteddy.org>
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

#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * kitty_image stores the raw decoded pixel data and metadata from a kitty
 * graphics protocol APC sequence. It is used to re-emit the sequence to the
 * outer terminal on redraw.
 */
struct kitty_image {
	/* Control-data fields parsed from the APC sequence. */
	char		 action;      /* a=: 'T'=transmit+display, 't', 'p', 'd' */
	u_int		 format;      /* f=: 32=RGBA, 24=RGB, 100=PNG */
	char		 medium;      /* t=: 'd'=direct, 'f'=file, 't'=tmp, 's'=shm */
	u_int		 pixel_w;     /* s=: source image pixel width */
	u_int		 pixel_h;     /* v=: source image pixel height */
	u_int		 cols;        /* c=: display columns (0=auto) */
	u_int		 rows;        /* r=: display rows (0=auto) */
	u_int		 image_id;    /* i=: image id (0=unassigned) */
	u_int		 image_num;   /* I=: image number */
	u_int		 placement_id; /* p=: placement id */
	u_int		 more;        /* m=: 1=more chunks coming, 0=last */
	u_int		 quiet;       /* q=: suppress responses */
	int		 z_index;     /* z=: z-index */
	char		 compression; /* o=: 'z'=zlib, 0=none */
	char		 delete_what; /* d=: delete target (used with a=d) */

	/* Cell size at the time of parsing (from the owning window). */
	u_int		 xpixel;
	u_int		 ypixel;

	/* Original base64-encoded payload (concatenated across all chunks). */
	char		*encoded;
	size_t		 encodedlen;

	char		*ctrl;
	size_t		 ctrllen;
};

/*
 * Parse control-data key=value pairs from a kitty APC sequence.
 * Format: key=value,key=value,...
 */
static int
kitty_parse_control(const char *ctrl, size_t ctrllen, struct kitty_image *ki)
{
	const char	*p = ctrl, *end = ctrl + ctrllen, *errstr;
	char		 key[4], val[32];
	size_t		 klen, vlen;

	while (p < end) {
		klen = 0;
		while (p < end && *p != '=' && klen < sizeof(key) - 1)
			key[klen++] = *p++;
		key[klen] = '\0';
		if (p >= end || *p != '=')
			return (-1);
		p++;

		vlen = 0;
		while (p < end && *p != ',' && vlen < sizeof(val) - 1)
			val[vlen++] = *p++;
		val[vlen] = '\0';
		if (p < end && *p == ',')
			p++;

		if (klen != 1)
			continue;

		switch (key[0]) {
		case 'a':
			ki->action = val[0];
			break;
		case 'f':
			ki->format = strtonum(val, 0, UINT_MAX, &errstr);
			if (errstr != NULL)
				return (-1);
			break;
		case 't':
			ki->medium = val[0];
			break;
		case 's':
			ki->pixel_w = strtonum(val, 0, UINT_MAX, &errstr);
			if (errstr != NULL)
				return (-1);
			break;
		case 'v':
			ki->pixel_h = strtonum(val, 0, UINT_MAX, &errstr);
			if (errstr != NULL)
				return (-1);
			break;
		case 'c':
			ki->cols = strtonum(val, 0, UINT_MAX, &errstr);
			if (errstr != NULL)
				return (-1);
			break;
		case 'r':
			ki->rows = strtonum(val, 0, UINT_MAX, &errstr);
			if (errstr != NULL)
				return (-1);
			break;
		case 'i':
			ki->image_id = strtonum(val, 0, UINT_MAX, &errstr);
			if (errstr != NULL)
				return (-1);
			break;
		case 'I':
			ki->image_num = strtonum(val, 0, UINT_MAX, &errstr);
			if (errstr != NULL)
				return (-1);
			break;
		case 'p':
			ki->placement_id = strtonum(val, 0, UINT_MAX, &errstr);
			if (errstr != NULL)
				return (-1);
			break;
		case 'm':
			ki->more = strtonum(val, 0, UINT_MAX, &errstr);
			if (errstr != NULL)
				return (-1);
			break;
		case 'q':
			ki->quiet = strtonum(val, 0, UINT_MAX, &errstr);
			if (errstr != NULL)
				return (-1);
			break;
		case 'z':
			ki->z_index = strtonum(val, INT_MIN, INT_MAX, &errstr);
			if (errstr != NULL)
				return (-1);
			break;
		case 'o':
			ki->compression = val[0];
			break;
		case 'd':
			ki->delete_what = val[0];
			break;
		}
	}
	return (0);
}

/*
 * Parse a kitty APC body (after the leading 'G').
 * Stores the original control string and base64 payload verbatim for
 * pass-through re-emission to the outer terminal.
 */
struct kitty_image *
kitty_parse(const u_char *buf, size_t len, u_int xpixel, u_int ypixel)
{
	struct kitty_image	*ki;
	const u_char		*semi;
	const char		*ctrl;
	size_t			 ctrllen, paylen;

	if (len == 0)
		return (NULL);

	semi = memchr(buf, ';', len);
	if (semi != NULL) {
		ctrl = (const char *)buf;
		ctrllen = semi - buf;
		paylen = len - ctrllen - 1;
	} else {
		ctrl = (const char *)buf;
		ctrllen = len;
		paylen = 0;
	}

	ki = xcalloc(1, sizeof *ki);
	ki->xpixel = xpixel;
	ki->ypixel = ypixel;
	ki->action = 'T';
	ki->format = 32;
	ki->medium = 'd';

	if (kitty_parse_control(ctrl, ctrllen, ki) != 0) {
		free(ki);
		return (NULL);
	}

	if (paylen > 0) {
		ki->encoded = xmalloc(paylen + 1);
		memcpy(ki->encoded, semi + 1, paylen);
		ki->encoded[paylen] = '\0';
		ki->encodedlen = paylen;
	}

	ki->ctrl = xmalloc(ctrllen + 1);
	memcpy(ki->ctrl, ctrl, ctrllen);
	ki->ctrl[ctrllen] = '\0';
	ki->ctrllen = ctrllen;

	return (ki);
}

void
kitty_free(struct kitty_image *ki)
{
	if (ki == NULL)
		return;
	free(ki->encoded);
	free(ki->ctrl);
	free(ki);
}

/*
 * Get the size in cells of a kitty image. If cols/rows are 0 (auto),
 * calculate from pixel dimensions. Returns size via sx/sy pointers.
 */
void
kitty_size_in_cells(struct kitty_image *ki, u_int *sx, u_int *sy)
{
	*sx = ki->cols;
	*sy = ki->rows;

	/*
	 * If cols/rows are 0, they mean "auto" - calculate from
	 * pixel dimensions.
	 */
	if (*sx == 0 && ki->pixel_w > 0 && ki->xpixel > 0) {
		*sx = (ki->pixel_w + ki->xpixel - 1) / ki->xpixel;
	}
	if (*sy == 0 && ki->pixel_h > 0 && ki->ypixel > 0) {
		*sy = (ki->pixel_h + ki->ypixel - 1) / ki->ypixel;
	}

	/* If still 0, use a reasonable default */
	if (*sx == 0)
		*sx = 10;
	if (*sy == 0)
		*sy = 10;
}

char
kitty_get_action(struct kitty_image *ki)
{
	return (ki->action);
}

u_int
kitty_get_image_id(struct kitty_image *ki)
{
	return (ki->image_id);
}

u_int
kitty_get_rows(struct kitty_image *ki)
{
	return (ki->rows);
}

/*
 * Serialize a kitty_image back into an APC escape sequence for transmission
 * to the terminal. This recreates the original command that was parsed.
 */
char *
kitty_print(struct kitty_image *ki, size_t *outlen)
{
	char	*out;
	size_t	 total, pos;

	if (ki == NULL || ki->ctrl == NULL)
		return (NULL);

	/* Calculate total length: ESC _ G + ctrl + ; + encoded + ESC \ */
	total = 3 + ki->ctrllen;  /* \033_G + ctrl */
	if (ki->encoded != NULL && ki->encodedlen > 0) {
		total += 1 + ki->encodedlen;  /* ; + encoded */
	}
	total += 2;  /* \033\\ */

	out = xmalloc(total + 1);
	*outlen = total;

	/* Build the sequence */
	pos = 0;
	memcpy(out + pos, "\033_G", 3);
	pos += 3;
	memcpy(out + pos, ki->ctrl, ki->ctrllen);
	pos += ki->ctrllen;

	if (ki->encoded != NULL && ki->encodedlen > 0) {
		out[pos++] = ';';
		memcpy(out + pos, ki->encoded, ki->encodedlen);
		pos += ki->encodedlen;
	}

	memcpy(out + pos, "\033\\", 2);
	pos += 2;
	out[pos] = '\0';

	return (out);
}

char *
kitty_delete_all(size_t *outlen)
{
	char	*out;

	out = xstrdup("\033_Ga=d,d=a\033\\");
	*outlen = strlen(out);
	return (out);
}
