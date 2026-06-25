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

#include <resolv.h>
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

	/* Set after this image has been transmitted to a kitty terminal so
	 * we transmit the payload only once and let the terminal retain the
	 * placement, rather than re-transmitting on every redraw. */
	int			transmitted;

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
/*
 * For PNG images (f=100) the application often omits the pixel dimensions
 * (s=, v=) because they are embedded in the PNG itself. Without them the
 * cell-size computation below cannot work, so decode the IHDR from the
 * accumulated base64 payload to recover the real width and height. This is
 * only done once and only when the dimensions are missing.
 */
static void
kitty_ensure_png_size(struct kitty_image *ki)
{
	char		head[33];
	u_char		buf[24];
	u_int		 w, h, n, take;
	int		 decoded;

	if (ki->format != 100)
		return;
	if (ki->pixel_w != 0 && ki->pixel_h != 0)
		return;
	if (ki->encoded == NULL || ki->encodedlen < 32)
		return;

	/*
	 * The IHDR (width/height) is within the first 24 decoded bytes, i.e.
	 * the first 32 base64 characters. b64_pton() decodes its entire input, so
	 * feed it only that prefix - decoding the full payload into a tiny buffer
	 * fails with -1.
	 */
	take = sizeof(head) - 1;
	if (take > ki->encodedlen)
		take = ki->encodedlen;
	memcpy(head, ki->encoded, take);
	head[take] = '\0';

	/* b64_pton needs an input length that is a multiple of 4; trim back. */
	n = take - (take % 4);
	head[n] = '\0';
	decoded = b64_pton(head, buf, sizeof buf);
	if (decoded < 24)
		return;

	/* PNG: 8-byte signature, then IHDR chunk with 4-byte BE width/height. */
	if (memcmp(buf, "\x89PNG\r\n\x1a\n", 8) != 0)
		return;
	if (memcmp(buf + 12, "IHDR", 4) != 0)
		return;
	w = (buf[16] << 24) | (buf[17] << 16) | (buf[18] << 8) | buf[19];
	h = (buf[20] << 24) | (buf[21] << 16) | (buf[22] << 8) | buf[23];
	if (w == 0 || h == 0)
		return;
	if (ki->pixel_w == 0)
		ki->pixel_w = w;
	if (ki->pixel_h == 0)
		ki->pixel_h = h;
}

void
kitty_size_in_cells(struct kitty_image *ki, u_int *sx, u_int *sy)
{
	kitty_ensure_png_size(ki);

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

void
kitty_set_image_id(struct kitty_image *ki, u_int image_id)
{
	ki->image_id = image_id;
}

/*
 * Override the image's display cell size. Used once the placeholder rectangle
 * has been fitted to the pane (aspect preserved) so that every consumer of
 * kitty_size_in_cells() - the grid cell loop and the per-client transmit -
 * agrees on the same dimensions.
 */
void
kitty_set_cells(struct kitty_image *ki, u_int cols, u_int rows)
{
	ki->cols = cols;
	ki->rows = rows;
}

u_int
kitty_get_rows(struct kitty_image *ki)
{
	return (ki->rows);
}

int
kitty_get_transmitted(struct kitty_image *ki)
{
	return (ki->transmitted);
}

u_int
kitty_get_more(struct kitty_image *ki)
{
	return (ki->more);
}

/* Append the base64 payload of "src" onto the accumulating "dst". */
void
kitty_append(struct kitty_image *dst, struct kitty_image *src)
{
	char	*new;
	size_t	 newlen;

	if (src->encoded == NULL || src->encodedlen == 0)
		return;
	newlen = dst->encodedlen + src->encodedlen;
	new = xmalloc(newlen + 1);
	if (dst->encoded != NULL && dst->encodedlen != 0)
		memcpy(new, dst->encoded, dst->encodedlen);
	memcpy(new + dst->encodedlen, src->encoded, src->encodedlen);
	new[newlen] = '\0';
	free(dst->encoded);
	dst->encoded = new;
	dst->encodedlen = newlen;
}

void
kitty_set_transmitted(struct kitty_image *ki, int transmitted)
{
	ki->transmitted = transmitted;
}

/*
 * Allocate a unique, terminal-wide image id for a captured image, so that
 * coexisting images (which applications frequently send with a recycled id
 * such as i=1) get distinct ids and thus distinct placeholder colours. ids
 * are kept below 256 so they fit in a 256-colour foreground escape, which is
 * how the terminal recovers the image id from each placeholder cell.
 */
u_int
kitty_alloc_id(void)
{
	static u_int	current = 0;

	if (current == 0)
		current = 1;
	return (current++ % 255 + 1);
}

/*
 * Serialize a kitty_image into a single complete APC for (re-)transmission
 * to a kitty terminal. The control string is rebuilt from the parsed fields
 * rather than echoed verbatim so that multi-chunk images (which arrived as
 * several a=T,m=1 fragments and were accumulated) are re-emitted as one
 * complete single-chunk transmit.
 */
char *
kitty_print(struct kitty_image *ki, size_t *outlen)
{
	char	ctrl[128];
	size_t	 ctrllen, total, pos;
	char	*out;

	if (ki == NULL)
		return (NULL);

	/* Rebuild a clean single-chunk control string from the fields. */
	ctrllen = xsnprintf(ctrl, sizeof ctrl, "a=%c", ki->action);
	if (ki->image_id != 0)
		ctrllen += xsnprintf(ctrl + ctrllen, sizeof ctrl - ctrllen,
		    ",i=%u", ki->image_id);
	if (ki->format != 0)
		ctrllen += xsnprintf(ctrl + ctrllen, sizeof ctrl - ctrllen,
		    ",f=%u", ki->format);
	if (ki->pixel_w != 0)
		ctrllen += xsnprintf(ctrl + ctrllen, sizeof ctrl - ctrllen,
		    ",s=%u", ki->pixel_w);
	if (ki->pixel_h != 0)
		ctrllen += xsnprintf(ctrl + ctrllen, sizeof ctrl - ctrllen,
		    ",v=%u", ki->pixel_h);
	if (ki->cols != 0)
		ctrllen += xsnprintf(ctrl + ctrllen, sizeof ctrl - ctrllen,
		    ",c=%u", ki->cols);
	if (ki->rows != 0)
		ctrllen += xsnprintf(ctrl + ctrllen, sizeof ctrl - ctrllen,
		    ",r=%u", ki->rows);
	if (ki->compression != 0)
		ctrllen += xsnprintf(ctrl + ctrllen, sizeof ctrl - ctrllen,
		    ",o=%c", ki->compression);

	total = 3 + ctrllen + 2;
	if (ki->encoded != NULL && ki->encodedlen > 0)
		total += 1 + ki->encodedlen;

	out = xmalloc(total + 1);
	*outlen = total;

	pos = 0;
	memcpy(out + pos, "\033_G", 3);
	pos += 3;
	memcpy(out + pos, ctrl, ctrllen);
	pos += ctrllen;
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

/*
 * Unicode placeholder support. Instead of re-emitting the image as a terminal
 * overlay (which tmux's grid-based, per-pane redraw model cannot clip or scroll),
 * the image is transmitted once with a=T,U=1,q=2 (transmit + create a virtual
 * placement, quiet) and then displayed by writing U+10EEEE placeholder cells
 * into tmux's own grid. Because those cells are ordinary text, tmux's existing
 * redraw/scroll/per-pane machinery handles clipping, scrolling and coexistence
 * for free -- the image becomes grid-resident, like sixel.
 */

/* Combining diacritics encoding row/column numbers 0..N, from kitty's
 * rowcolumn-diacritics.txt. */
static const u_int kitty_diacritics[] = {
	0x0305,0x030d,0x030e,0x0310,0x0312,0x033d,0x033e,0x033f,
	0x0346,0x034a,0x034b,0x034c,0x0350,0x0351,0x0352,0x0353,
	0x0357,0x035b,0x0363,0x0364,0x0365,0x0366,0x0367,0x0368,
	0x0369,0x036a,0x036b,0x036c,0x036d,0x036e,0x036f,0x0483,
	0x0484,0x0485,0x0486,0x0592,0x0593,0x0594,0x0595,0x0597,
	0x0598,0x0599,0x059c,0x059d,0x059e,0x059f,0x05a0,0x05a1,
	0x05a8,0x05a9,0x05ab,0x05ac,0x05af,0x05c4,0x0610,0x0611,
	0x0612,0x0613,0x0614,0x0615,0x0616,0x0617,0x0618,0x0619,
	0x061a,0x064b,0x064c,0x064d,0x064e,0x064f,0x0650,0x0651,
	0x0652,0x0653,0x0654,0x0655,0x0656,0x0657,0x0658,0x0659,
	0x065a,0x065b,0x065c,0x065d,0x065e,0x065f,0x0670,0x06d6,
	0x06d7,0x06d8,0x06d9,0x06da,0x06db,0x06dc,0x06df,0x06e0,
	0x06e1,0x06e2,0x06e3,0x06e4,0x06e7,0x06e8,0x06ea,0x06eb,
	0x06ec,0x06ed,
};
#define KITTY_NDIACRITICS (sizeof kitty_diacritics / sizeof kitty_diacritics[0])

static u_int
kitty_diacritic(u_int n)
{
	if (n < KITTY_NDIACRITICS)
		return (kitty_diacritics[n]);
	return (kitty_diacritics[0]);
}

/*
 * Build the UTF-8 bytes for one placeholder cell of image row `row`.
 * The first column of a row carries the row and column-0 diacritics; later
 * columns are bare U+10EEEE (the terminal auto-increments the column from the
 * left neighbour). Writes into buf (must be >= 8 bytes) and returns the length.
 */
size_t
kitty_placeholder_cell(u_int row, int first_column, char *buf, size_t buflen)
{
	static const u_char	ph[4] = { 0xf4, 0x8e, 0xbb, 0xae }; /* U+10EEEE */
	size_t			off = 0, i;
	u_int			cp;
	u_char			enc[4];

	/* Base placeholder character U+10EEEE (encoded directly so this does
	 * not depend on the locale/wctomb). */
	if (sizeof ph > buflen)
		return (0);
	memcpy(buf, ph, sizeof ph);
	off = sizeof ph;

	if (first_column) {
		/* Row diacritic, then column-0 diacritic, each a combining mark
	 * in the U+0300..U+07FF range: 2-byte UTF-8 (110xxxxx 10xxxxxx). */
		for (i = 0; i < 2; i++) {
			cp = kitty_diacritic(i == 0 ? row : 0);
			enc[0] = 0xc0 | (cp >> 6);
			enc[1] = 0x80 | (cp & 0x3f);
			if (off + 2 > buflen)
				return (0);
			buf[off++] = enc[0];
			buf[off++] = enc[1];
		}
	}
	return (off);
}

/*
 * Transmit a kitty image with a=T,U=1,q=2 (transmit data + create a virtual
 * placement for unicode placeholders, quiet) so the terminal stores the image
 * but does not display it directly. Display is done by placeholder cells in the
 * grid. Large payloads are split into 4096-byte base64 chunks via m=1/m=0.
 * Each chunk is emitted through `add` (the tty output callback).
 */
void
kitty_transmit(struct kitty_image *ki, u_int cols, u_int rows,
    void (*add)(void *, const char *, size_t), void *arg)
{
	char		 ctrl[160], chunkapc[4200], placeapc[128];
	const char	*p;
	size_t		 left, taken, ctrllen, n;
	int		 first = 1;
	u_int		 image_id;

	if (ki->encoded == NULL || ki->encodedlen == 0)
		return;

	image_id = ki->image_id == 0 ? 1 : ki->image_id;

	/*
	 * Step 1 — transmit the image data only, WITHOUT displaying it.
	 * a=t (lowercase) transmits without creating a placement, which is
	 * essential: a=T would also create a real placement at the cursor that
	 * ghosts when the placeholder cells later move on redraw.
	 */
	ctrllen = xsnprintf(ctrl, sizeof ctrl,
	    "a=t,q=2,i=%u,f=%u,s=%u,v=%u",
	    image_id, ki->format == 0 ? 32 : ki->format,
	    ki->pixel_w, ki->pixel_h);

	p = ki->encoded;
	left = ki->encodedlen;
	while (left > 0) {
		taken = left > 4096 ? 4096 : left;
		if (first && taken == left) {
			/* Single chunk: full control, no m=. */
			n = xsnprintf(chunkapc, sizeof chunkapc,
			    "\033_G%.*s;", (int)ctrllen, ctrl);
		} else if (first) {
			n = xsnprintf(chunkapc, sizeof chunkapc,
			    "\033_G%.*s,m=1;", (int)ctrllen, ctrl);
			first = 0;
		} else {
			n = xsnprintf(chunkapc, sizeof chunkapc,
			    "\033_Gm=%d;", taken == left ? 0 : 1);
		}
		memcpy(chunkapc + n, p, taken);
		n += taken;
		memcpy(chunkapc + n, "\033\\", 2);
		n += 2;
		add(arg, chunkapc, n);
		p += taken;
		left -= taken;
	}

	/*
	 * Step 2 — create an invisible *virtual* placement (U=1) for the
	 * transmitted image. This is the prototype that defines the render
	 * rectangle; the real, on-screen image is driven entirely by the
	 * U+10EEEE placeholder cells (grid-resident text) and moves with them.
	 */
	n = xsnprintf(placeapc, sizeof placeapc,
	    "\033_Ga=p,U=1,q=2,i=%u,c=%u,r=%u\033\\",
	    image_id, cols, rows);
	add(arg, placeapc, n);
}
