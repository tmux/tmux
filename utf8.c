/* $OpenBSD$ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <vis.h>
#include <wchar.h>

#include "tmux.h"

struct utf8_item {
	RB_ENTRY(utf8_item)	index_entry;
	u_int			index;

	RB_ENTRY(utf8_item)	data_entry;
	char			data[UTF8_SIZE];
	u_char			size;
};

static int
utf8_data_cmp(struct utf8_item *ui1, struct utf8_item *ui2)
{
	if (ui1->size < ui2->size)
		return (-1);
	if (ui1->size > ui2->size)
		return (1);
	return (memcmp(ui1->data, ui2->data, ui1->size));
}
RB_HEAD(utf8_data_tree, utf8_item);
RB_GENERATE_STATIC(utf8_data_tree, utf8_item, data_entry, utf8_data_cmp);
static struct utf8_data_tree utf8_data_tree = RB_INITIALIZER(utf8_data_tree);

static int
utf8_index_cmp(struct utf8_item *ui1, struct utf8_item *ui2)
{
	if (ui1->index < ui2->index)
		return (-1);
	if (ui1->index > ui2->index)
		return (1);
	return (0);
}
RB_HEAD(utf8_index_tree, utf8_item);
RB_GENERATE_STATIC(utf8_index_tree, utf8_item, index_entry, utf8_index_cmp);
static struct utf8_index_tree utf8_index_tree = RB_INITIALIZER(utf8_index_tree);

static u_int utf8_next_index;

#define UTF8_GET_SIZE(uc) (((uc) >> 24) & 0x1f)
#define UTF8_GET_WIDTH(uc) (((uc) >> 29) - 1)

#define UTF8_SET_SIZE(size) (((utf8_char)(size)) << 24)
#define UTF8_SET_WIDTH(width) ((((utf8_char)(width)) + 1) << 29)

/* Get a UTF-8 item from data. */
static struct utf8_item *
utf8_item_by_data(const u_char *data, size_t size)
{
	struct utf8_item	ui;

	memcpy(ui.data, data, size);
	ui.size = size;

	return (RB_FIND(utf8_data_tree, &utf8_data_tree, &ui));
}

/* Get a UTF-8 item from data. */
static struct utf8_item *
utf8_item_by_index(u_int index)
{
	struct utf8_item	ui;

	ui.index = index;

	return (RB_FIND(utf8_index_tree, &utf8_index_tree, &ui));
}

/* Add a UTF-8 item. */
static int
utf8_put_item(const u_char *data, size_t size, u_int *index)
{
	struct utf8_item	*ui;

	ui = utf8_item_by_data(data, size);
	if (ui != NULL) {
		*index = ui->index;
		log_debug("%s: found %.*s = %u", __func__, (int)size, data,
		    *index);
		return (0);
	}

	if (utf8_next_index == 0xffffff + 1)
		return (-1);

	ui = xcalloc(1, sizeof *ui);
	ui->index = utf8_next_index++;
	RB_INSERT(utf8_index_tree, &utf8_index_tree, ui);

	memcpy(ui->data, data, size);
	ui->size = size;
	RB_INSERT(utf8_data_tree, &utf8_data_tree, ui);

	*index = ui->index;
	log_debug("%s: added %.*s = %u", __func__, (int)size, data, *index);
	return (0);
}

/* Get UTF-8 character from data. */
enum utf8_state
utf8_from_data(const struct utf8_data *ud, utf8_char *uc)
{
	u_int	index;

	if (ud->width > 2)
		fatalx("invalid UTF-8 width: %u", ud->width);

	if (ud->size > UTF8_SIZE)
		goto fail;
	if (ud->size <= 3) {
		index = (((utf8_char)ud->data[2] << 16)|
		          ((utf8_char)ud->data[1] << 8)|
		          ((utf8_char)ud->data[0]));
	} else if (utf8_put_item(ud->data, ud->size, &index) != 0)
		goto fail;
	*uc = UTF8_SET_SIZE(ud->size)|UTF8_SET_WIDTH(ud->width)|index;
	log_debug("%s: (%d %d %.*s) -> %08x", __func__, ud->width, ud->size,
	    (int)ud->size, ud->data, *uc);
	return (UTF8_DONE);

fail:
	if (ud->width == 0)
		*uc = UTF8_SET_SIZE(0)|UTF8_SET_WIDTH(0);
	else if (ud->width == 1)
		*uc = UTF8_SET_SIZE(1)|UTF8_SET_WIDTH(1)|0x20;
	else
		*uc = UTF8_SET_SIZE(1)|UTF8_SET_WIDTH(1)|0x2020;
	return (UTF8_ERROR);
}

/* Get UTF-8 data from character. */
void
utf8_to_data(utf8_char uc, struct utf8_data *ud)
{
	struct utf8_item	*ui;
	u_int			 index;

	memset(ud, 0, sizeof *ud);
	ud->size = ud->have = UTF8_GET_SIZE(uc);
	ud->width = UTF8_GET_WIDTH(uc);

	if (ud->size <= 3) {
		ud->data[2] = (uc >> 16);
		ud->data[1] = ((uc >> 8) & 0xff);
		ud->data[0] = (uc & 0xff);
	} else {
		index = (uc & 0xffffff);
		if ((ui = utf8_item_by_index(index)) == NULL)
			memset(ud->data, ' ', ud->size);
		else
			memcpy(ud->data, ui->data, ud->size);
	}

	log_debug("%s: %08x -> (%d %d %.*s)", __func__, uc, ud->width, ud->size,
	    (int)ud->size, ud->data);
}

/* Get UTF-8 character from a single ASCII character. */
u_int
utf8_build_one(u_char ch)
{
	return (UTF8_SET_SIZE(1)|UTF8_SET_WIDTH(1)|ch);
}

/* Set a single character. */
void
utf8_set(struct utf8_data *ud, u_char ch)
{
	static const struct utf8_data empty = { { 0 }, 1, 1, 1 };

	memcpy(ud, &empty, sizeof *ud);
	*ud->data = ch;
}

/* Copy UTF-8 character. */
void
utf8_copy(struct utf8_data *to, const struct utf8_data *from)
{
	u_int	i;

	memcpy(to, from, sizeof *to);

	for (i = to->size; i < sizeof to->data; i++)
		to->data[i] = '\0';
}

/* Get width of Unicode character. */
static enum utf8_state
utf8_width(struct utf8_data *ud, int *width)
{
	wchar_t	wc;

	switch (mbtowc(&wc, ud->data, ud->size)) {
	case -1:
		log_debug("UTF-8 %.*s, mbtowc() %d", (int)ud->size, ud->data,
		    errno);
		mbtowc(NULL, NULL, MB_CUR_MAX);
		return (UTF8_ERROR);
	case 0:
		return (UTF8_ERROR);
	}
	log_debug("UTF-8 %.*s is %08X", (int)ud->size, ud->data, (u_int)wc);
	*width = wcwidth(wc);
	log_debug("wcwidth(%08X) returned %d", (u_int)wc, *width);
	if (*width < 0) {
		/*
		 * C1 control characters are nonprintable, so they are always
		 * zero width.
		 */
		*width = (wc >= 0x80 && wc <= 0x9f) ? 0 : 1;
	}
	if (*width >= 0 && *width <= 0xff)
		return (UTF8_DONE);
	return (UTF8_ERROR);
}

/*
 * Open UTF-8 sequence.
 *
 * 11000010-11011111 C2-DF start of 2-byte sequence
 * 11100000-11101111 E0-EF start of 3-byte sequence
 * 11110000-11110100 F0-F4 start of 4-byte sequence
 */
enum utf8_state
utf8_open(struct utf8_data *ud, u_char ch)
{
	memset(ud, 0, sizeof *ud);
	if (ch >= 0xc2 && ch <= 0xdf)
		ud->size = 2;
	else if (ch >= 0xe0 && ch <= 0xef)
		ud->size = 3;
	else if (ch >= 0xf0 && ch <= 0xf4)
		ud->size = 4;
	else
		return (UTF8_ERROR);
	utf8_append(ud, ch);
	return (UTF8_MORE);
}

/* Append character to UTF-8, closing if finished. */
enum utf8_state
utf8_append(struct utf8_data *ud, u_char ch)
{
	int	width;

	if (ud->have >= ud->size)
		fatalx("UTF-8 character overflow");
	if (ud->size > sizeof ud->data)
		fatalx("UTF-8 character size too large");

	if (ud->have != 0 && (ch & 0xc0) != 0x80)
		ud->width = 0xff;

	ud->data[ud->have++] = ch;
	if (ud->have != ud->size)
		return (UTF8_MORE);

	if (ud->width == 0xff)
		return (UTF8_ERROR);
	if (utf8_width(ud, &width) != UTF8_DONE)
		return (UTF8_ERROR);
	ud->width = width;

	return (UTF8_DONE);
}

/*
 * Encode len characters from src into dst, which is guaranteed to have four
 * bytes available for each character from src (for \abc or UTF-8) plus space
 * for \0.
 */
int
utf8_strvis(char *dst, const char *src, size_t len, int flag)
{
	struct utf8_data	 ud;
	const char		*start = dst, *end = src + len;
	enum utf8_state		 more;
	size_t			 i;

	while (src < end) {
		if ((more = utf8_open(&ud, *src)) == UTF8_MORE) {
			while (++src < end && more == UTF8_MORE)
				more = utf8_append(&ud, *src);
			if (more == UTF8_DONE) {
				/* UTF-8 character finished. */
				for (i = 0; i < ud.size; i++)
					*dst++ = ud.data[i];
				continue;
			}
			/* Not a complete, valid UTF-8 character. */
			src -= ud.have;
		}
		if (src[0] == '$' && src < end - 1) {
			if (isalpha((u_char)src[1]) ||
			    src[1] == '_' ||
			    src[1] == '{')
				*dst++ = '\\';
			*dst++ = '$';
		} else if (src < end - 1)
			dst = vis(dst, src[0], flag, src[1]);
		else if (src < end)
			dst = vis(dst, src[0], flag, '\0');
		src++;
	}
	*dst = '\0';
	return (dst - start);
}

/* Same as utf8_strvis but allocate the buffer. */
int
utf8_stravis(char **dst, const char *src, int flag)
{
	char	*buf;
	int	 len;

	buf = xreallocarray(NULL, 4, strlen(src) + 1);
	len = utf8_strvis(buf, src, strlen(src), flag);

	*dst = xrealloc(buf, len + 1);
	return (len);
}

/* Same as utf8_strvis but allocate the buffer. */
int
utf8_stravisx(char **dst, const char *src, size_t srclen, int flag)
{
	char	*buf;
	int	 len;

	buf = xreallocarray(NULL, 4, srclen + 1);
	len = utf8_strvis(buf, src, srclen, flag);

	*dst = xrealloc(buf, len + 1);
	return (len);
}

/* Does this string contain anything that isn't valid UTF-8? */
int
utf8_isvalid(const char *s)
{
	struct utf8_data ud;
	const char	*end;
	enum utf8_state	 more;

	end = s + strlen(s);
	while (s < end) {
		if ((more = utf8_open(&ud, *s)) == UTF8_MORE) {
			while (++s < end && more == UTF8_MORE)
				more = utf8_append(&ud, *s);
			if (more == UTF8_DONE)
				continue;
			return (0);
		}
		if (*s < 0x20 || *s > 0x7e)
			return (0);
		s++;
	}
	return (1);
}

/*
 * Sanitize a string, changing any UTF-8 characters to '_'. Caller should free
 * the returned string. Anything not valid printable ASCII or UTF-8 is
 * stripped.
 */
char *
utf8_sanitize(const char *src)
{
	char		*dst = NULL;
	size_t		 n = 0;
	enum utf8_state	 more;
	struct utf8_data ud;
	u_int		 i;

	while (*src != '\0') {
		dst = xreallocarray(dst, n + 1, sizeof *dst);
		if ((more = utf8_open(&ud, *src)) == UTF8_MORE) {
			while (*++src != '\0' && more == UTF8_MORE)
				more = utf8_append(&ud, *src);
			if (more == UTF8_DONE) {
				dst = xreallocarray(dst, n + ud.width,
				    sizeof *dst);
				for (i = 0; i < ud.width; i++)
					dst[n++] = '_';
				continue;
			}
			src -= ud.have;
		}
		if (*src > 0x1f && *src < 0x7f)
			dst[n++] = *src;
		else
			dst[n++] = '_';
		src++;
	}
	dst = xreallocarray(dst, n + 1, sizeof *dst);
	dst[n] = '\0';
	return (dst);
}

/* Get UTF-8 buffer length. */
size_t
utf8_strlen(const struct utf8_data *s)
{
	size_t	i;

	for (i = 0; s[i].size != 0; i++)
		/* nothing */;
	return (i);
}

/* Get UTF-8 string width. */
u_int
utf8_strwidth(const struct utf8_data *s, ssize_t n)
{
	ssize_t	i;
	u_int	width = 0;

	for (i = 0; s[i].size != 0; i++) {
		if (n != -1 && n == i)
			break;
		width += s[i].width;
	}
	return (width);
}

/*
 * Convert a string into a buffer of UTF-8 characters. Terminated by size == 0.
 * Caller frees.
 */
struct utf8_data *
utf8_fromcstr(const char *src)
{
	struct utf8_data	*dst = NULL;
	size_t			 n = 0;
	enum utf8_state		 more;

	while (*src != '\0') {
		dst = xreallocarray(dst, n + 1, sizeof *dst);
		if ((more = utf8_open(&dst[n], *src)) == UTF8_MORE) {
			while (*++src != '\0' && more == UTF8_MORE)
				more = utf8_append(&dst[n], *src);
			if (more == UTF8_DONE) {
				n++;
				continue;
			}
			src -= dst[n].have;
		}
		utf8_set(&dst[n], *src);
		n++;
		src++;
	}
	dst = xreallocarray(dst, n + 1, sizeof *dst);
	dst[n].size = 0;
	return (dst);
}

/* Convert from a buffer of UTF-8 characters into a string. Caller frees. */
char *
utf8_tocstr(struct utf8_data *src)
{
	char	*dst = NULL;
	size_t	 n = 0;

	for(; src->size != 0; src++) {
		dst = xreallocarray(dst, n + src->size, 1);
		memcpy(dst + n, src->data, src->size);
		n += src->size;
	}
	dst = xreallocarray(dst, n + 1, 1);
	dst[n] = '\0';
	return (dst);
}

/* Get width of UTF-8 string. */
u_int
utf8_cstrwidth(const char *s)
{
	struct utf8_data	tmp;
	u_int			width;
	enum utf8_state		more;

	width = 0;
	while (*s != '\0') {
		if ((more = utf8_open(&tmp, *s)) == UTF8_MORE) {
			while (*++s != '\0' && more == UTF8_MORE)
				more = utf8_append(&tmp, *s);
			if (more == UTF8_DONE) {
				width += tmp.width;
				continue;
			}
			s -= tmp.have;
		}
		if (*s > 0x1f && *s != 0x7f)
			width++;
		s++;
	}
	return (width);
}

/* Pad UTF-8 string to width on the left. Caller frees. */
char *
utf8_padcstr(const char *s, u_int width)
{
	size_t	 slen;
	char	*out;
	u_int	 n, i;

	n = utf8_cstrwidth(s);
	if (n >= width)
		return (xstrdup(s));

	slen = strlen(s);
	out = xmalloc(slen + 1 + (width - n));
	memcpy(out, s, slen);
	for (i = n; i < width; i++)
		out[slen++] = ' ';
	out[slen] = '\0';
	return (out);
}

/* Pad UTF-8 string to width on the right. Caller frees. */
char *
utf8_rpadcstr(const char *s, u_int width)
{
	size_t	 slen;
	char	*out;
	u_int	 n, i;

	n = utf8_cstrwidth(s);
	if (n >= width)
		return (xstrdup(s));

	slen = strlen(s);
	out = xmalloc(slen + 1 + (width - n));
	for (i = 0; i < width - n; i++)
		out[i] = ' ';
	memcpy(out + i, s, slen);
	out[i + slen] = '\0';
	return (out);
}

int
utf8_cstrhas(const char *s, const struct utf8_data *ud)
{
	struct utf8_data	*copy, *loop;
	int			 found = 0;

	copy = utf8_fromcstr(s);
	for (loop = copy; loop->size != 0; loop++) {
		if (loop->size != ud->size)
			continue;
		if (memcmp(loop->data, ud->data, loop->size) == 0) {
			found = 1;
			break;
		}
	}
	free(copy);

	return (found);
}
