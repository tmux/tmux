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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

struct utf8_width_entry {
	u_int	first;
	u_int	last;

	int	width;

	struct utf8_width_entry	*left;
	struct utf8_width_entry	*right;
};

/* Sorted, then repeatedly split in the middle to balance the tree. */
static struct utf8_width_entry utf8_width_table[] = {
	{ 0x00b41, 0x00b44, 0, NULL, NULL },
	{ 0x008e4, 0x00902, 0, NULL, NULL },
	{ 0x006d6, 0x006dd, 0, NULL, NULL },
	{ 0x005c4, 0x005c5, 0, NULL, NULL },
	{ 0x00591, 0x005bd, 0, NULL, NULL },
	{ 0x00300, 0x0036f, 0, NULL, NULL },
	{ 0x00483, 0x00489, 0, NULL, NULL },
	{ 0x005bf, 0x005bf, 0, NULL, NULL },
	{ 0x005c1, 0x005c2, 0, NULL, NULL },
	{ 0x00610, 0x0061a, 0, NULL, NULL },
	{ 0x00600, 0x00605, 0, NULL, NULL },
	{ 0x005c7, 0x005c7, 0, NULL, NULL },
	{ 0x0064b, 0x0065f, 0, NULL, NULL },
	{ 0x0061c, 0x0061c, 0, NULL, NULL },
	{ 0x00670, 0x00670, 0, NULL, NULL },
	{ 0x007a6, 0x007b0, 0, NULL, NULL },
	{ 0x006ea, 0x006ed, 0, NULL, NULL },
	{ 0x006df, 0x006e4, 0, NULL, NULL },
	{ 0x006e7, 0x006e8, 0, NULL, NULL },
	{ 0x00711, 0x00711, 0, NULL, NULL },
	{ 0x0070f, 0x0070f, 0, NULL, NULL },
	{ 0x00730, 0x0074a, 0, NULL, NULL },
	{ 0x0081b, 0x00823, 0, NULL, NULL },
	{ 0x007eb, 0x007f3, 0, NULL, NULL },
	{ 0x00816, 0x00819, 0, NULL, NULL },
	{ 0x00829, 0x0082d, 0, NULL, NULL },
	{ 0x00825, 0x00827, 0, NULL, NULL },
	{ 0x00859, 0x0085b, 0, NULL, NULL },
	{ 0x00a41, 0x00a42, 0, NULL, NULL },
	{ 0x00981, 0x00981, 0, NULL, NULL },
	{ 0x00941, 0x00948, 0, NULL, NULL },
	{ 0x0093a, 0x0093a, 0, NULL, NULL },
	{ 0x0093c, 0x0093c, 0, NULL, NULL },
	{ 0x00951, 0x00957, 0, NULL, NULL },
	{ 0x0094d, 0x0094d, 0, NULL, NULL },
	{ 0x00962, 0x00963, 0, NULL, NULL },
	{ 0x009e2, 0x009e3, 0, NULL, NULL },
	{ 0x009c1, 0x009c4, 0, NULL, NULL },
	{ 0x009bc, 0x009bc, 0, NULL, NULL },
	{ 0x009cd, 0x009cd, 0, NULL, NULL },
	{ 0x00a01, 0x00a02, 0, NULL, NULL },
	{ 0x00a3c, 0x00a3c, 0, NULL, NULL },
	{ 0x00ac1, 0x00ac5, 0, NULL, NULL },
	{ 0x00a70, 0x00a71, 0, NULL, NULL },
	{ 0x00a4b, 0x00a4d, 0, NULL, NULL },
	{ 0x00a47, 0x00a48, 0, NULL, NULL },
	{ 0x00a51, 0x00a51, 0, NULL, NULL },
	{ 0x00a81, 0x00a82, 0, NULL, NULL },
	{ 0x00a75, 0x00a75, 0, NULL, NULL },
	{ 0x00abc, 0x00abc, 0, NULL, NULL },
	{ 0x00ae2, 0x00ae3, 0, NULL, NULL },
	{ 0x00ac7, 0x00ac8, 0, NULL, NULL },
	{ 0x00acd, 0x00acd, 0, NULL, NULL },
	{ 0x00b3c, 0x00b3c, 0, NULL, NULL },
	{ 0x00b01, 0x00b01, 0, NULL, NULL },
	{ 0x00b3f, 0x00b3f, 0, NULL, NULL },
	{ 0x03190, 0x031ba, 2, NULL, NULL },
	{ 0x017c9, 0x017d3, 0, NULL, NULL },
	{ 0x00ec8, 0x00ecd, 0, NULL, NULL },
	{ 0x00cc6, 0x00cc6, 0, NULL, NULL },
	{ 0x00c3e, 0x00c40, 0, NULL, NULL },
	{ 0x00b82, 0x00b82, 0, NULL, NULL },
	{ 0x00b56, 0x00b56, 0, NULL, NULL },
	{ 0x00b4d, 0x00b4d, 0, NULL, NULL },
	{ 0x00b62, 0x00b63, 0, NULL, NULL },
	{ 0x00bcd, 0x00bcd, 0, NULL, NULL },
	{ 0x00bc0, 0x00bc0, 0, NULL, NULL },
	{ 0x00c00, 0x00c00, 0, NULL, NULL },
	{ 0x00c62, 0x00c63, 0, NULL, NULL },
	{ 0x00c4a, 0x00c4d, 0, NULL, NULL },
	{ 0x00c46, 0x00c48, 0, NULL, NULL },
	{ 0x00c55, 0x00c56, 0, NULL, NULL },
	{ 0x00cbc, 0x00cbc, 0, NULL, NULL },
	{ 0x00c81, 0x00c81, 0, NULL, NULL },
	{ 0x00cbf, 0x00cbf, 0, NULL, NULL },
	{ 0x00dd2, 0x00dd4, 0, NULL, NULL },
	{ 0x00d41, 0x00d44, 0, NULL, NULL },
	{ 0x00ce2, 0x00ce3, 0, NULL, NULL },
	{ 0x00ccc, 0x00ccd, 0, NULL, NULL },
	{ 0x00d01, 0x00d01, 0, NULL, NULL },
	{ 0x00d62, 0x00d63, 0, NULL, NULL },
	{ 0x00d4d, 0x00d4d, 0, NULL, NULL },
	{ 0x00dca, 0x00dca, 0, NULL, NULL },
	{ 0x00e47, 0x00e4e, 0, NULL, NULL },
	{ 0x00e31, 0x00e31, 0, NULL, NULL },
	{ 0x00dd6, 0x00dd6, 0, NULL, NULL },
	{ 0x00e34, 0x00e3a, 0, NULL, NULL },
	{ 0x00eb4, 0x00eb9, 0, NULL, NULL },
	{ 0x00eb1, 0x00eb1, 0, NULL, NULL },
	{ 0x00ebb, 0x00ebc, 0, NULL, NULL },
	{ 0x0105e, 0x01060, 0, NULL, NULL },
	{ 0x00f8d, 0x00f97, 0, NULL, NULL },
	{ 0x00f39, 0x00f39, 0, NULL, NULL },
	{ 0x00f35, 0x00f35, 0, NULL, NULL },
	{ 0x00f18, 0x00f19, 0, NULL, NULL },
	{ 0x00f37, 0x00f37, 0, NULL, NULL },
	{ 0x00f80, 0x00f84, 0, NULL, NULL },
	{ 0x00f71, 0x00f7e, 0, NULL, NULL },
	{ 0x00f86, 0x00f87, 0, NULL, NULL },
	{ 0x01032, 0x01037, 0, NULL, NULL },
	{ 0x00fc6, 0x00fc6, 0, NULL, NULL },
	{ 0x00f99, 0x00fbc, 0, NULL, NULL },
	{ 0x0102d, 0x01030, 0, NULL, NULL },
	{ 0x0103d, 0x0103e, 0, NULL, NULL },
	{ 0x01039, 0x0103a, 0, NULL, NULL },
	{ 0x01058, 0x01059, 0, NULL, NULL },
	{ 0x0135d, 0x0135f, 0, NULL, NULL },
	{ 0x01085, 0x01086, 0, NULL, NULL },
	{ 0x01071, 0x01074, 0, NULL, NULL },
	{ 0x01082, 0x01082, 0, NULL, NULL },
	{ 0x0109d, 0x0109d, 0, NULL, NULL },
	{ 0x0108d, 0x0108d, 0, NULL, NULL },
	{ 0x01100, 0x011ff, 2, NULL, NULL },
	{ 0x01772, 0x01773, 0, NULL, NULL },
	{ 0x01732, 0x01734, 0, NULL, NULL },
	{ 0x01712, 0x01714, 0, NULL, NULL },
	{ 0x01752, 0x01753, 0, NULL, NULL },
	{ 0x017b7, 0x017bd, 0, NULL, NULL },
	{ 0x017b4, 0x017b5, 0, NULL, NULL },
	{ 0x017c6, 0x017c6, 0, NULL, NULL },
	{ 0x01c2c, 0x01c33, 0, NULL, NULL },
	{ 0x01a7f, 0x01a7f, 0, NULL, NULL },
	{ 0x01a17, 0x01a18, 0, NULL, NULL },
	{ 0x01920, 0x01922, 0, NULL, NULL },
	{ 0x0180b, 0x0180e, 0, NULL, NULL },
	{ 0x017dd, 0x017dd, 0, NULL, NULL },
	{ 0x018a9, 0x018a9, 0, NULL, NULL },
	{ 0x01932, 0x01932, 0, NULL, NULL },
	{ 0x01927, 0x01928, 0, NULL, NULL },
	{ 0x01939, 0x0193b, 0, NULL, NULL },
	{ 0x01a60, 0x01a60, 0, NULL, NULL },
	{ 0x01a56, 0x01a56, 0, NULL, NULL },
	{ 0x01a1b, 0x01a1b, 0, NULL, NULL },
	{ 0x01a58, 0x01a5e, 0, NULL, NULL },
	{ 0x01a65, 0x01a6c, 0, NULL, NULL },
	{ 0x01a62, 0x01a62, 0, NULL, NULL },
	{ 0x01a73, 0x01a7c, 0, NULL, NULL },
	{ 0x01b80, 0x01b81, 0, NULL, NULL },
	{ 0x01b36, 0x01b3a, 0, NULL, NULL },
	{ 0x01b00, 0x01b03, 0, NULL, NULL },
	{ 0x01ab0, 0x01abe, 0, NULL, NULL },
	{ 0x01b34, 0x01b34, 0, NULL, NULL },
	{ 0x01b42, 0x01b42, 0, NULL, NULL },
	{ 0x01b3c, 0x01b3c, 0, NULL, NULL },
	{ 0x01b6b, 0x01b73, 0, NULL, NULL },
	{ 0x01be6, 0x01be6, 0, NULL, NULL },
	{ 0x01ba8, 0x01ba9, 0, NULL, NULL },
	{ 0x01ba2, 0x01ba5, 0, NULL, NULL },
	{ 0x01bab, 0x01bad, 0, NULL, NULL },
	{ 0x01bed, 0x01bed, 0, NULL, NULL },
	{ 0x01be8, 0x01be9, 0, NULL, NULL },
	{ 0x01bef, 0x01bf1, 0, NULL, NULL },
	{ 0x02329, 0x0232a, 2, NULL, NULL },
	{ 0x01dc0, 0x01df5, 0, NULL, NULL },
	{ 0x01ce2, 0x01ce8, 0, NULL, NULL },
	{ 0x01cd0, 0x01cd2, 0, NULL, NULL },
	{ 0x01c36, 0x01c37, 0, NULL, NULL },
	{ 0x01cd4, 0x01ce0, 0, NULL, NULL },
	{ 0x01cf4, 0x01cf4, 0, NULL, NULL },
	{ 0x01ced, 0x01ced, 0, NULL, NULL },
	{ 0x01cf8, 0x01cf9, 0, NULL, NULL },
	{ 0x02060, 0x02064, 0, NULL, NULL },
	{ 0x0200b, 0x0200f, 0, NULL, NULL },
	{ 0x01dfc, 0x01dff, 0, NULL, NULL },
	{ 0x0202a, 0x0202e, 0, NULL, NULL },
	{ 0x02066, 0x0206f, 0, NULL, NULL },
	{ 0x020d0, 0x020f0, 0, NULL, NULL },
	{ 0x03001, 0x03029, 2, NULL, NULL },
	{ 0x02e80, 0x02e99, 2, NULL, NULL },
	{ 0x02d7f, 0x02d7f, 0, NULL, NULL },
	{ 0x02cef, 0x02cf1, 0, NULL, NULL },
	{ 0x02de0, 0x02dff, 0, NULL, NULL },
	{ 0x02f00, 0x02fd5, 2, NULL, NULL },
	{ 0x02e9b, 0x02ef3, 2, NULL, NULL },
	{ 0x02ff0, 0x02ffb, 2, NULL, NULL },
	{ 0x03099, 0x0309a, 0, NULL, NULL },
	{ 0x0302e, 0x0303e, 2, NULL, NULL },
	{ 0x0302a, 0x0302d, 0, NULL, NULL },
	{ 0x03041, 0x03096, 2, NULL, NULL },
	{ 0x03105, 0x0312d, 2, NULL, NULL },
	{ 0x0309b, 0x030ff, 2, NULL, NULL },
	{ 0x03131, 0x0318e, 2, NULL, NULL },
	{ 0x10a3f, 0x10a3f, 0, NULL, NULL },
	{ 0x0aa4c, 0x0aa4c, 0, NULL, NULL },
	{ 0x0a825, 0x0a826, 0, NULL, NULL },
	{ 0x0a490, 0x0a4c6, 2, NULL, NULL },
	{ 0x03250, 0x032fe, 2, NULL, NULL },
	{ 0x031f0, 0x0321e, 2, NULL, NULL },
	{ 0x031c0, 0x031e3, 2, NULL, NULL },
	{ 0x03220, 0x03247, 2, NULL, NULL },
	{ 0x04e00, 0x09fcc, 2, NULL, NULL },
	{ 0x03300, 0x04db5, 2, NULL, NULL },
	{ 0x0a000, 0x0a48c, 2, NULL, NULL },
	{ 0x0a6f0, 0x0a6f1, 0, NULL, NULL },
	{ 0x0a674, 0x0a67d, 0, NULL, NULL },
	{ 0x0a66f, 0x0a672, 0, NULL, NULL },
	{ 0x0a69f, 0x0a69f, 0, NULL, NULL },
	{ 0x0a806, 0x0a806, 0, NULL, NULL },
	{ 0x0a802, 0x0a802, 0, NULL, NULL },
	{ 0x0a80b, 0x0a80b, 0, NULL, NULL },
	{ 0x0a9b6, 0x0a9b9, 0, NULL, NULL },
	{ 0x0a947, 0x0a951, 0, NULL, NULL },
	{ 0x0a8e0, 0x0a8f1, 0, NULL, NULL },
	{ 0x0a8c4, 0x0a8c4, 0, NULL, NULL },
	{ 0x0a926, 0x0a92d, 0, NULL, NULL },
	{ 0x0a980, 0x0a982, 0, NULL, NULL },
	{ 0x0a960, 0x0a97c, 2, NULL, NULL },
	{ 0x0a9b3, 0x0a9b3, 0, NULL, NULL },
	{ 0x0aa29, 0x0aa2e, 0, NULL, NULL },
	{ 0x0a9bc, 0x0a9bc, 0, NULL, NULL },
	{ 0x0a9e5, 0x0a9e5, 0, NULL, NULL },
	{ 0x0aa35, 0x0aa36, 0, NULL, NULL },
	{ 0x0aa31, 0x0aa32, 0, NULL, NULL },
	{ 0x0aa43, 0x0aa43, 0, NULL, NULL },
	{ 0x0fb1e, 0x0fb1e, 0, NULL, NULL },
	{ 0x0aaf6, 0x0aaf6, 0, NULL, NULL },
	{ 0x0aab7, 0x0aab8, 0, NULL, NULL },
	{ 0x0aab0, 0x0aab0, 0, NULL, NULL },
	{ 0x0aa7c, 0x0aa7c, 0, NULL, NULL },
	{ 0x0aab2, 0x0aab4, 0, NULL, NULL },
	{ 0x0aac1, 0x0aac1, 0, NULL, NULL },
	{ 0x0aabe, 0x0aabf, 0, NULL, NULL },
	{ 0x0aaec, 0x0aaed, 0, NULL, NULL },
	{ 0x0ac00, 0x0d7a3, 2, NULL, NULL },
	{ 0x0abe8, 0x0abe8, 0, NULL, NULL },
	{ 0x0abe5, 0x0abe5, 0, NULL, NULL },
	{ 0x0abed, 0x0abed, 0, NULL, NULL },
	{ 0x0f900, 0x0fa6d, 2, NULL, NULL },
	{ 0x0d800, 0x0dfff, 0, NULL, NULL },
	{ 0x0fa70, 0x0fad9, 2, NULL, NULL },
	{ 0x0fff9, 0x0fffb, 0, NULL, NULL },
	{ 0x0fe30, 0x0fe52, 2, NULL, NULL },
	{ 0x0fe10, 0x0fe19, 2, NULL, NULL },
	{ 0x0fe00, 0x0fe0f, 0, NULL, NULL },
	{ 0x0fe20, 0x0fe2d, 0, NULL, NULL },
	{ 0x0fe68, 0x0fe6b, 2, NULL, NULL },
	{ 0x0fe54, 0x0fe66, 2, NULL, NULL },
	{ 0x0feff, 0x0feff, 0, NULL, NULL },
	{ 0x10a01, 0x10a03, 0, NULL, NULL },
	{ 0x102e0, 0x102e0, 0, NULL, NULL },
	{ 0x101fd, 0x101fd, 0, NULL, NULL },
	{ 0x10376, 0x1037a, 0, NULL, NULL },
	{ 0x10a0c, 0x10a0f, 0, NULL, NULL },
	{ 0x10a05, 0x10a06, 0, NULL, NULL },
	{ 0x10a38, 0x10a3a, 0, NULL, NULL },
	{ 0x11633, 0x1163a, 0, NULL, NULL },
	{ 0x11236, 0x11237, 0, NULL, NULL },
	{ 0x11100, 0x11102, 0, NULL, NULL },
	{ 0x1107f, 0x11081, 0, NULL, NULL },
	{ 0x11001, 0x11001, 0, NULL, NULL },
	{ 0x10ae5, 0x10ae6, 0, NULL, NULL },
	{ 0x11038, 0x11046, 0, NULL, NULL },
	{ 0x110b9, 0x110ba, 0, NULL, NULL },
	{ 0x110b3, 0x110b6, 0, NULL, NULL },
	{ 0x110bd, 0x110bd, 0, NULL, NULL },
	{ 0x11180, 0x11181, 0, NULL, NULL },
	{ 0x1112d, 0x11134, 0, NULL, NULL },
	{ 0x11127, 0x1112b, 0, NULL, NULL },
	{ 0x11173, 0x11173, 0, NULL, NULL },
	{ 0x1122f, 0x11231, 0, NULL, NULL },
	{ 0x111b6, 0x111be, 0, NULL, NULL },
	{ 0x11234, 0x11234, 0, NULL, NULL },
	{ 0x11370, 0x11374, 0, NULL, NULL },
	{ 0x11301, 0x11301, 0, NULL, NULL },
	{ 0x112df, 0x112df, 0, NULL, NULL },
	{ 0x112e3, 0x112ea, 0, NULL, NULL },
	{ 0x11340, 0x11340, 0, NULL, NULL },
	{ 0x1133c, 0x1133c, 0, NULL, NULL },
	{ 0x11366, 0x1136c, 0, NULL, NULL },
	{ 0x114c2, 0x114c3, 0, NULL, NULL },
	{ 0x114ba, 0x114ba, 0, NULL, NULL },
	{ 0x114b3, 0x114b8, 0, NULL, NULL },
	{ 0x114bf, 0x114c0, 0, NULL, NULL },
	{ 0x115bc, 0x115bd, 0, NULL, NULL },
	{ 0x115b2, 0x115b5, 0, NULL, NULL },
	{ 0x115bf, 0x115c0, 0, NULL, NULL },
	{ 0x1d1aa, 0x1d1ad, 0, NULL, NULL },
	{ 0x16b30, 0x16b36, 0, NULL, NULL },
	{ 0x116ad, 0x116ad, 0, NULL, NULL },
	{ 0x1163f, 0x11640, 0, NULL, NULL },
	{ 0x1163d, 0x1163d, 0, NULL, NULL },
	{ 0x116ab, 0x116ab, 0, NULL, NULL },
	{ 0x116b7, 0x116b7, 0, NULL, NULL },
	{ 0x116b0, 0x116b5, 0, NULL, NULL },
	{ 0x16af0, 0x16af4, 0, NULL, NULL },
	{ 0x1bca0, 0x1bca3, 0, NULL, NULL },
	{ 0x1b000, 0x1b001, 2, NULL, NULL },
	{ 0x16f8f, 0x16f92, 0, NULL, NULL },
	{ 0x1bc9d, 0x1bc9e, 0, NULL, NULL },
	{ 0x1d173, 0x1d182, 0, NULL, NULL },
	{ 0x1d167, 0x1d169, 0, NULL, NULL },
	{ 0x1d185, 0x1d18b, 0, NULL, NULL },
	{ 0x2a700, 0x2b734, 2, NULL, NULL },
	{ 0x1f210, 0x1f23a, 2, NULL, NULL },
	{ 0x1e8d0, 0x1e8d6, 0, NULL, NULL },
	{ 0x1d242, 0x1d244, 0, NULL, NULL },
	{ 0x1f200, 0x1f202, 2, NULL, NULL },
	{ 0x1f250, 0x1f251, 2, NULL, NULL },
	{ 0x1f240, 0x1f248, 2, NULL, NULL },
	{ 0x20000, 0x2a6d6, 2, NULL, NULL },
	{ 0xe0020, 0xe007f, 0, NULL, NULL },
	{ 0x2f800, 0x2fa1d, 2, NULL, NULL },
	{ 0x2b740, 0x2b81d, 2, NULL, NULL },
	{ 0xe0001, 0xe0001, 0, NULL, NULL },
	{ 0xf0000, 0xffffd, 0, NULL, NULL },
	{ 0xe0100, 0xe01ef, 0, NULL, NULL },
	{ 0x100000, 0x10fffd, 0, NULL, NULL },
};
static struct utf8_width_entry	*utf8_width_root = NULL;

static void	utf8_build(void);

/* Set a single character. */
void
utf8_set(struct utf8_data *ud, u_char ch)
{
	u_int	i;

	*ud->data = ch;
	ud->have = 1;
	ud->size = 1;

	ud->width = 1;

	for (i = ud->size; i < sizeof ud->data; i++)
		ud->data[i] = '\0';
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
	ud->width = utf8_width(utf8_combine(ud));
	return (UTF8_DONE);
}

/* Build UTF-8 width tree. */
static void
utf8_build(void)
{
	struct utf8_width_entry	**ptr, *item, *node;
	u_int			  i;

	for (i = 0; i < nitems(utf8_width_table); i++) {
		item = &utf8_width_table[i];

		ptr = &utf8_width_root;
		while (*ptr != NULL) {
			node = *ptr;
			if (item->last < node->first)
				ptr = &node->left;
			else if (item->first > node->last)
				ptr = &node->right;
		}
		*ptr = item;
	}
}

/* Lookup width of UTF-8 data in tree. */
u_int
utf8_width(u_int uc)
{
	struct utf8_width_entry	*item;

	if (utf8_width_root == NULL)
		utf8_build();

	item = utf8_width_root;
	while (item != NULL) {
		if (uc < item->first)
			item = item->left;
		else if (uc > item->last)
			item = item->right;
		else
			return (item->width);
	}
	return (1);
}

/* Combine UTF-8 into 32-bit Unicode. */
u_int
utf8_combine(const struct utf8_data *ud)
{
	u_int	uc;

	uc = 0xfffd;
	switch (ud->size) {
	case 1:
		uc = ud->data[0];
		break;
	case 2:
		uc = ud->data[1] & 0x3f;
		uc |= (ud->data[0] & 0x1f) << 6;
		break;
	case 3:
		uc = ud->data[2] & 0x3f;
		uc |= (ud->data[1] & 0x3f) << 6;
		uc |= (ud->data[0] & 0xf) << 12;
		break;
	case 4:
		uc = ud->data[3] & 0x3f;
		uc |= (ud->data[2] & 0x3f) << 6;
		uc |= (ud->data[1] & 0x3f) << 12;
		uc |= (ud->data[0] & 0x7) << 18;
		break;
	}
	return (uc);
}

/* Split 32-bit Unicode into UTF-8. */
enum utf8_state
utf8_split(u_int uc, struct utf8_data *ud)
{
	if (uc < 0x7f) {
		ud->size = 1;
		ud->data[0] = uc;
	} else if (uc < 0x7ff) {
		ud->size = 2;
		ud->data[0] = 0xc0 | ((uc >> 6) & 0x1f);
		ud->data[1] = 0x80 | (uc & 0x3f);
	} else if (uc < 0xffff) {
		ud->size = 3;
		ud->data[0] = 0xe0 | ((uc >> 12) & 0xf);
		ud->data[1] = 0x80 | ((uc >> 6) & 0x3f);
		ud->data[2] = 0x80 | (uc & 0x3f);
	} else if (uc < 0x1fffff) {
		ud->size = 4;
		ud->data[0] = 0xf0 | ((uc >> 18) & 0x7);
		ud->data[1] = 0x80 | ((uc >> 12) & 0x3f);
		ud->data[2] = 0x80 | ((uc >> 6) & 0x3f);
		ud->data[3] = 0x80 | (uc & 0x3f);
	} else
		return (UTF8_ERROR);
	ud->width = utf8_width(uc);
	return (UTF8_DONE);
}

/* Split a two-byte UTF-8 character. */
u_int
utf8_split2(u_int uc, u_char *ptr)
{
	if (uc > 0x7f) {
		ptr[0] = (uc >> 6) | 0xc0;
		ptr[1] = (uc & 0x3f) | 0x80;
		return (2);
	}
	ptr[0] = uc;
	return (1);
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
	const char		*start, *end;
	enum utf8_state		 more;
	size_t			 i;

	start = dst;
	end = src + len;

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
		if (src < end - 1)
			dst = vis(dst, src[0], flag, src[1]);
		else if (src < end)
			dst = vis(dst, src[0], flag, '\0');
		src++;
	}

	*dst = '\0';
	return (dst - start);
}

/*
 * Sanitize a string, changing any UTF-8 characters to '_'. Caller should free
 * the returned string. Anything not valid printable ASCII or UTF-8 is
 * stripped.
 */
char *
utf8_sanitize(const char *src)
{
	char			*dst;
	size_t			 n;
	enum utf8_state		 more;
	struct utf8_data	 ud;
	u_int			 i;

	dst = NULL;

	n = 0;
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

/*
 * Convert a string into a buffer of UTF-8 characters. Terminated by size == 0.
 * Caller frees.
 */
struct utf8_data *
utf8_fromcstr(const char *src)
{
	struct utf8_data	*dst;
	size_t			 n;
	enum utf8_state		 more;

	dst = NULL;

	n = 0;
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
	char	*dst;
	size_t	 n;

	dst = NULL;

	n = 0;
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

/* Trim UTF-8 string to width. Caller frees. */
char *
utf8_trimcstr(const char *s, u_int width)
{
	struct utf8_data	*tmp, *next;
	char			*out;
	u_int			 at;

	tmp = utf8_fromcstr(s);

	at = 0;
	for (next = tmp; next->size != 0; next++) {
		if (at + next->width > width) {
			next->size = 0;
			break;
		}
		at += next->width;
	}

	out = utf8_tocstr(tmp);
	free(tmp);
	return (out);
}

/* Trim UTF-8 string to width. Caller frees. */
char *
utf8_rtrimcstr(const char *s, u_int width)
{
	struct utf8_data	*tmp, *next, *end;
	char			*out;
	u_int			 at;

	tmp = utf8_fromcstr(s);

	for (end = tmp; end->size != 0; end++)
		/* nothing */;
	if (end == tmp) {
		free(tmp);
		return (xstrdup(""));
	}
	next = end - 1;

	at = 0;
	for (;;)
	{
		if (at + next->width > width) {
			next++;
			break;
		}
		at += next->width;

		if (next == tmp)
			break;
		next--;
	}

	out = utf8_tocstr(next);
	free(tmp);
	return (out);
}

/* Pad UTF-8 string to width. Caller frees. */
char *
utf8_padcstr(const char *s, u_int width)
{
	size_t	 slen;
	char	*out;
	u_int	  n, i;

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
