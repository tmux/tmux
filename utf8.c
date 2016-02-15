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
};

/* Notice: this list must be sorted to allow binary search. */
/* Generated from EastAsianWidth-8.0.0.txt */
static struct utf8_width_entry utf8_width_table[] = {
	{ 0x0000a1, 0x0000a1, 1 },
	{ 0x0000a4, 0x0000a4, 1 },
	{ 0x0000a7, 0x0000a8, 1 },
	{ 0x0000aa, 0x0000aa, 1 },
	{ 0x0000ad, 0x0000ae, 1 },
	{ 0x0000b0, 0x0000b4, 1 },
	{ 0x0000b6, 0x0000ba, 1 },
	{ 0x0000bc, 0x0000bf, 1 },
	{ 0x0000c6, 0x0000c6, 1 },
	{ 0x0000d0, 0x0000d0, 1 },
	{ 0x0000d7, 0x0000d8, 1 },
	{ 0x0000de, 0x0000e1, 1 },
	{ 0x0000e6, 0x0000e6, 1 },
	{ 0x0000e8, 0x0000ea, 1 },
	{ 0x0000ec, 0x0000ed, 1 },
	{ 0x0000f0, 0x0000f0, 1 },
	{ 0x0000f2, 0x0000f3, 1 },
	{ 0x0000f7, 0x0000fa, 1 },
	{ 0x0000fc, 0x0000fc, 1 },
	{ 0x0000fe, 0x0000fe, 1 },
	{ 0x000101, 0x000101, 1 },
	{ 0x000111, 0x000111, 1 },
	{ 0x000113, 0x000113, 1 },
	{ 0x00011b, 0x00011b, 1 },
	{ 0x000126, 0x000127, 1 },
	{ 0x00012b, 0x00012b, 1 },
	{ 0x000131, 0x000133, 1 },
	{ 0x000138, 0x000138, 1 },
	{ 0x00013f, 0x000142, 1 },
	{ 0x000144, 0x000144, 1 },
	{ 0x000148, 0x00014b, 1 },
	{ 0x00014d, 0x00014d, 1 },
	{ 0x000152, 0x000153, 1 },
	{ 0x000166, 0x000167, 1 },
	{ 0x00016b, 0x00016b, 1 },
	{ 0x0001ce, 0x0001ce, 1 },
	{ 0x0001d0, 0x0001d0, 1 },
	{ 0x0001d2, 0x0001d2, 1 },
	{ 0x0001d4, 0x0001d4, 1 },
	{ 0x0001d6, 0x0001d6, 1 },
	{ 0x0001d8, 0x0001d8, 1 },
	{ 0x0001da, 0x0001da, 1 },
	{ 0x0001dc, 0x0001dc, 1 },
	{ 0x000251, 0x000251, 1 },
	{ 0x000261, 0x000261, 1 },
	{ 0x0002c4, 0x0002c4, 1 },
	{ 0x0002c7, 0x0002c7, 1 },
	{ 0x0002c9, 0x0002cb, 1 },
	{ 0x0002cd, 0x0002cd, 1 },
	{ 0x0002d0, 0x0002d0, 1 },
	{ 0x0002d8, 0x0002db, 1 },
	{ 0x0002dd, 0x0002dd, 1 },
	{ 0x0002df, 0x0002df, 1 },
	{ 0x000300, 0x00036f, 0 },
	{ 0x000391, 0x0003a1, 1 },
	{ 0x0003a3, 0x0003a9, 1 },
	{ 0x0003b1, 0x0003c1, 1 },
	{ 0x0003c3, 0x0003c9, 1 },
	{ 0x000401, 0x000401, 1 },
	{ 0x000410, 0x00044f, 1 },
	{ 0x000451, 0x000451, 1 },
	{ 0x000483, 0x000489, 0 },
	{ 0x000591, 0x0005bd, 0 },
	{ 0x0005bf, 0x0005bf, 0 },
	{ 0x0005c1, 0x0005c2, 0 },
	{ 0x0005c4, 0x0005c5, 0 },
	{ 0x0005c7, 0x0005c7, 0 },
	{ 0x000600, 0x000605, 0 },
	{ 0x000610, 0x00061a, 0 },
	{ 0x00061c, 0x00061c, 0 },
	{ 0x00064b, 0x00065f, 0 },
	{ 0x000670, 0x000670, 0 },
	{ 0x0006d6, 0x0006dd, 0 },
	{ 0x0006df, 0x0006e4, 0 },
	{ 0x0006e7, 0x0006e8, 0 },
	{ 0x0006ea, 0x0006ed, 0 },
	{ 0x00070f, 0x00070f, 0 },
	{ 0x000711, 0x000711, 0 },
	{ 0x000730, 0x00074a, 0 },
	{ 0x0007a6, 0x0007b0, 0 },
	{ 0x0007eb, 0x0007f3, 0 },
	{ 0x000816, 0x000819, 0 },
	{ 0x00081b, 0x000823, 0 },
	{ 0x000825, 0x000827, 0 },
	{ 0x000829, 0x00082d, 0 },
	{ 0x000859, 0x00085b, 0 },
	{ 0x0008e4, 0x000902, 0 },
	{ 0x00093a, 0x00093a, 0 },
	{ 0x00093c, 0x00093c, 0 },
	{ 0x000941, 0x000948, 0 },
	{ 0x00094d, 0x00094d, 0 },
	{ 0x000951, 0x000957, 0 },
	{ 0x000962, 0x000963, 0 },
	{ 0x000981, 0x000981, 0 },
	{ 0x0009bc, 0x0009bc, 0 },
	{ 0x0009c1, 0x0009c4, 0 },
	{ 0x0009cd, 0x0009cd, 0 },
	{ 0x0009e2, 0x0009e3, 0 },
	{ 0x000a01, 0x000a02, 0 },
	{ 0x000a3c, 0x000a3c, 0 },
	{ 0x000a41, 0x000a42, 0 },
	{ 0x000a47, 0x000a48, 0 },
	{ 0x000a4b, 0x000a4d, 0 },
	{ 0x000a51, 0x000a51, 0 },
	{ 0x000a70, 0x000a71, 0 },
	{ 0x000a75, 0x000a75, 0 },
	{ 0x000a81, 0x000a82, 0 },
	{ 0x000abc, 0x000abc, 0 },
	{ 0x000ac1, 0x000ac5, 0 },
	{ 0x000ac7, 0x000ac8, 0 },
	{ 0x000acd, 0x000acd, 0 },
	{ 0x000ae2, 0x000ae3, 0 },
	{ 0x000b01, 0x000b01, 0 },
	{ 0x000b3c, 0x000b3c, 0 },
	{ 0x000b3f, 0x000b3f, 0 },
	{ 0x000b41, 0x000b44, 0 },
	{ 0x000b4d, 0x000b4d, 0 },
	{ 0x000b56, 0x000b56, 0 },
	{ 0x000b62, 0x000b63, 0 },
	{ 0x000b82, 0x000b82, 0 },
	{ 0x000bc0, 0x000bc0, 0 },
	{ 0x000bcd, 0x000bcd, 0 },
	{ 0x000c00, 0x000c00, 0 },
	{ 0x000c3e, 0x000c40, 0 },
	{ 0x000c46, 0x000c48, 0 },
	{ 0x000c4a, 0x000c4d, 0 },
	{ 0x000c55, 0x000c56, 0 },
	{ 0x000c62, 0x000c63, 0 },
	{ 0x000c81, 0x000c81, 0 },
	{ 0x000cbc, 0x000cbc, 0 },
	{ 0x000cbf, 0x000cbf, 0 },
	{ 0x000cc6, 0x000cc6, 0 },
	{ 0x000ccc, 0x000ccd, 0 },
	{ 0x000ce2, 0x000ce3, 0 },
	{ 0x000d01, 0x000d01, 0 },
	{ 0x000d41, 0x000d44, 0 },
	{ 0x000d4d, 0x000d4d, 0 },
	{ 0x000d62, 0x000d63, 0 },
	{ 0x000dca, 0x000dca, 0 },
	{ 0x000dd2, 0x000dd4, 0 },
	{ 0x000dd6, 0x000dd6, 0 },
	{ 0x000e31, 0x000e31, 0 },
	{ 0x000e34, 0x000e3a, 0 },
	{ 0x000e47, 0x000e4e, 0 },
	{ 0x000eb1, 0x000eb1, 0 },
	{ 0x000eb4, 0x000eb9, 0 },
	{ 0x000ebb, 0x000ebc, 0 },
	{ 0x000ec8, 0x000ecd, 0 },
	{ 0x000f18, 0x000f19, 0 },
	{ 0x000f35, 0x000f35, 0 },
	{ 0x000f37, 0x000f37, 0 },
	{ 0x000f39, 0x000f39, 0 },
	{ 0x000f71, 0x000f7e, 0 },
	{ 0x000f80, 0x000f84, 0 },
	{ 0x000f86, 0x000f87, 0 },
	{ 0x000f8d, 0x000f97, 0 },
	{ 0x000f99, 0x000fbc, 0 },
	{ 0x000fc6, 0x000fc6, 0 },
	{ 0x00102d, 0x001030, 0 },
	{ 0x001032, 0x001037, 0 },
	{ 0x001039, 0x00103a, 0 },
	{ 0x00103d, 0x00103e, 0 },
	{ 0x001058, 0x001059, 0 },
	{ 0x00105e, 0x001060, 0 },
	{ 0x001071, 0x001074, 0 },
	{ 0x001082, 0x001082, 0 },
	{ 0x001085, 0x001086, 0 },
	{ 0x00108d, 0x00108d, 0 },
	{ 0x00109d, 0x00109d, 0 },
	{ 0x001100, 0x0011ff, 2 },
	{ 0x00135d, 0x00135f, 0 },
	{ 0x001712, 0x001714, 0 },
	{ 0x001732, 0x001734, 0 },
	{ 0x001752, 0x001753, 0 },
	{ 0x001772, 0x001773, 0 },
	{ 0x0017b4, 0x0017b5, 0 },
	{ 0x0017b7, 0x0017bd, 0 },
	{ 0x0017c6, 0x0017c6, 0 },
	{ 0x0017c9, 0x0017d3, 0 },
	{ 0x0017dd, 0x0017dd, 0 },
	{ 0x00180b, 0x00180e, 0 },
	{ 0x0018a9, 0x0018a9, 0 },
	{ 0x001920, 0x001922, 0 },
	{ 0x001927, 0x001928, 0 },
	{ 0x001932, 0x001932, 0 },
	{ 0x001939, 0x00193b, 0 },
	{ 0x001a17, 0x001a18, 0 },
	{ 0x001a1b, 0x001a1b, 0 },
	{ 0x001a56, 0x001a56, 0 },
	{ 0x001a58, 0x001a5e, 0 },
	{ 0x001a60, 0x001a60, 0 },
	{ 0x001a62, 0x001a62, 0 },
	{ 0x001a65, 0x001a6c, 0 },
	{ 0x001a73, 0x001a7c, 0 },
	{ 0x001a7f, 0x001a7f, 0 },
	{ 0x001ab0, 0x001abe, 0 },
	{ 0x001b00, 0x001b03, 0 },
	{ 0x001b34, 0x001b34, 0 },
	{ 0x001b36, 0x001b3a, 0 },
	{ 0x001b3c, 0x001b3c, 0 },
	{ 0x001b42, 0x001b42, 0 },
	{ 0x001b6b, 0x001b73, 0 },
	{ 0x001b80, 0x001b81, 0 },
	{ 0x001ba2, 0x001ba5, 0 },
	{ 0x001ba8, 0x001ba9, 0 },
	{ 0x001bab, 0x001bad, 0 },
	{ 0x001be6, 0x001be6, 0 },
	{ 0x001be8, 0x001be9, 0 },
	{ 0x001bed, 0x001bed, 0 },
	{ 0x001bef, 0x001bf1, 0 },
	{ 0x001c2c, 0x001c33, 0 },
	{ 0x001c36, 0x001c37, 0 },
	{ 0x001cd0, 0x001cd2, 0 },
	{ 0x001cd4, 0x001ce0, 0 },
	{ 0x001ce2, 0x001ce8, 0 },
	{ 0x001ced, 0x001ced, 0 },
	{ 0x001cf4, 0x001cf4, 0 },
	{ 0x001cf8, 0x001cf9, 0 },
	{ 0x001dc0, 0x001df5, 0 },
	{ 0x001dfc, 0x001dff, 0 },
	{ 0x00200b, 0x00200f, 0 },
	{ 0x002010, 0x002010, 1 },
	{ 0x002013, 0x002016, 1 },
	{ 0x002018, 0x002019, 1 },
	{ 0x00201c, 0x00201d, 1 },
	{ 0x002020, 0x002022, 1 },
	{ 0x002024, 0x002027, 1 },
	{ 0x00202a, 0x00202e, 0 },
	{ 0x002030, 0x002030, 1 },
	{ 0x002032, 0x002033, 1 },
	{ 0x002035, 0x002035, 1 },
	{ 0x00203b, 0x00203b, 1 },
	{ 0x00203e, 0x00203e, 1 },
	{ 0x002060, 0x002064, 0 },
	{ 0x002066, 0x00206f, 0 },
	{ 0x002074, 0x002074, 1 },
	{ 0x00207f, 0x00207f, 1 },
	{ 0x002081, 0x002084, 1 },
	{ 0x0020ac, 0x0020ac, 1 },
	{ 0x0020d0, 0x0020f0, 0 },
	{ 0x002103, 0x002103, 1 },
	{ 0x002105, 0x002105, 1 },
	{ 0x002109, 0x002109, 1 },
	{ 0x002113, 0x002113, 1 },
	{ 0x002116, 0x002116, 1 },
	{ 0x002121, 0x002122, 1 },
	{ 0x002126, 0x002126, 1 },
	{ 0x00212b, 0x00212b, 1 },
	{ 0x002153, 0x002154, 1 },
	{ 0x00215b, 0x00215e, 1 },
	{ 0x002160, 0x00216b, 1 },
	{ 0x002170, 0x002179, 1 },
	{ 0x002189, 0x002189, 1 },
	{ 0x002190, 0x002199, 1 },
	{ 0x0021b8, 0x0021b9, 1 },
	{ 0x0021d2, 0x0021d2, 1 },
	{ 0x0021d4, 0x0021d4, 1 },
	{ 0x0021e7, 0x0021e7, 1 },
	{ 0x002200, 0x002200, 1 },
	{ 0x002202, 0x002203, 1 },
	{ 0x002207, 0x002208, 1 },
	{ 0x00220b, 0x00220b, 1 },
	{ 0x00220f, 0x00220f, 1 },
	{ 0x002211, 0x002211, 1 },
	{ 0x002215, 0x002215, 1 },
	{ 0x00221a, 0x00221a, 1 },
	{ 0x00221d, 0x002220, 1 },
	{ 0x002223, 0x002223, 1 },
	{ 0x002225, 0x002225, 1 },
	{ 0x002227, 0x00222c, 1 },
	{ 0x00222e, 0x00222e, 1 },
	{ 0x002234, 0x002237, 1 },
	{ 0x00223c, 0x00223d, 1 },
	{ 0x002248, 0x002248, 1 },
	{ 0x00224c, 0x00224c, 1 },
	{ 0x002252, 0x002252, 1 },
	{ 0x002260, 0x002261, 1 },
	{ 0x002264, 0x002267, 1 },
	{ 0x00226a, 0x00226b, 1 },
	{ 0x00226e, 0x00226f, 1 },
	{ 0x002282, 0x002283, 1 },
	{ 0x002286, 0x002287, 1 },
	{ 0x002295, 0x002295, 1 },
	{ 0x002299, 0x002299, 1 },
	{ 0x0022a5, 0x0022a5, 1 },
	{ 0x0022bf, 0x0022bf, 1 },
	{ 0x002312, 0x002312, 1 },
	{ 0x002329, 0x00232a, 2 },
	{ 0x002460, 0x0024e9, 1 },
	{ 0x0024eb, 0x00254b, 1 },
	{ 0x002550, 0x002573, 1 },
	{ 0x002580, 0x00258f, 1 },
	{ 0x002592, 0x002595, 1 },
	{ 0x0025a0, 0x0025a1, 1 },
	{ 0x0025a3, 0x0025a9, 1 },
	{ 0x0025b2, 0x0025b3, 1 },
	{ 0x0025b6, 0x0025b7, 1 },
	{ 0x0025bc, 0x0025bd, 1 },
	{ 0x0025c0, 0x0025c1, 1 },
	{ 0x0025c6, 0x0025c8, 1 },
	{ 0x0025cb, 0x0025cb, 1 },
	{ 0x0025ce, 0x0025d1, 1 },
	{ 0x0025e2, 0x0025e5, 1 },
	{ 0x0025ef, 0x0025ef, 1 },
	{ 0x002605, 0x002606, 1 },
	{ 0x002609, 0x002609, 1 },
	{ 0x00260e, 0x00260f, 1 },
	{ 0x002614, 0x002615, 1 },
	{ 0x00261c, 0x00261c, 1 },
	{ 0x00261e, 0x00261e, 1 },
	{ 0x002640, 0x002640, 1 },
	{ 0x002642, 0x002642, 1 },
	{ 0x002660, 0x002661, 1 },
	{ 0x002663, 0x002665, 1 },
	{ 0x002667, 0x00266a, 1 },
	{ 0x00266c, 0x00266d, 1 },
	{ 0x00266f, 0x00266f, 1 },
	{ 0x00269e, 0x00269f, 1 },
	{ 0x0026be, 0x0026bf, 1 },
	{ 0x0026c4, 0x0026cd, 1 },
	{ 0x0026cf, 0x0026e1, 1 },
	{ 0x0026e3, 0x0026e3, 1 },
	{ 0x0026e8, 0x0026ff, 1 },
	{ 0x00273d, 0x00273d, 1 },
	{ 0x002757, 0x002757, 1 },
	{ 0x002776, 0x00277f, 1 },
	{ 0x002b55, 0x002b59, 1 },
	{ 0x002cef, 0x002cf1, 0 },
	{ 0x002d7f, 0x002d7f, 0 },
	{ 0x002de0, 0x002dff, 0 },
	{ 0x002e80, 0x002e99, 2 },
	{ 0x002e9b, 0x002ef3, 2 },
	{ 0x002f00, 0x002fd5, 2 },
	{ 0x002ff0, 0x002ffb, 2 },
	{ 0x003000, 0x003029, 2 },
	{ 0x00302a, 0x00302d, 0 },
	{ 0x00302e, 0x00303e, 2 },
	{ 0x003041, 0x003096, 2 },
	{ 0x003099, 0x00309a, 0 },
	{ 0x00309b, 0x0030ff, 2 },
	{ 0x003105, 0x00312d, 2 },
	{ 0x003131, 0x00318e, 2 },
	{ 0x003190, 0x0031ba, 2 },
	{ 0x0031c0, 0x0031e3, 2 },
	{ 0x0031f0, 0x00321e, 2 },
	{ 0x003220, 0x003247, 2 },
	{ 0x003248, 0x00324f, 1 },
	{ 0x003250, 0x0032fe, 2 },
	{ 0x003300, 0x004dbf, 2 },
	{ 0x004e00, 0x00a48c, 2 },
	{ 0x00a490, 0x00a4c6, 2 },
	{ 0x00a66f, 0x00a672, 0 },
	{ 0x00a674, 0x00a67d, 0 },
	{ 0x00a69f, 0x00a69f, 0 },
	{ 0x00a6f0, 0x00a6f1, 0 },
	{ 0x00a802, 0x00a802, 0 },
	{ 0x00a806, 0x00a806, 0 },
	{ 0x00a80b, 0x00a80b, 0 },
	{ 0x00a825, 0x00a826, 0 },
	{ 0x00a8c4, 0x00a8c4, 0 },
	{ 0x00a8e0, 0x00a8f1, 0 },
	{ 0x00a926, 0x00a92d, 0 },
	{ 0x00a947, 0x00a951, 0 },
	{ 0x00a960, 0x00a97c, 2 },
	{ 0x00a980, 0x00a982, 0 },
	{ 0x00a9b3, 0x00a9b3, 0 },
	{ 0x00a9b6, 0x00a9b9, 0 },
	{ 0x00a9bc, 0x00a9bc, 0 },
	{ 0x00a9e5, 0x00a9e5, 0 },
	{ 0x00aa29, 0x00aa2e, 0 },
	{ 0x00aa31, 0x00aa32, 0 },
	{ 0x00aa35, 0x00aa36, 0 },
	{ 0x00aa43, 0x00aa43, 0 },
	{ 0x00aa4c, 0x00aa4c, 0 },
	{ 0x00aa7c, 0x00aa7c, 0 },
	{ 0x00aab0, 0x00aab0, 0 },
	{ 0x00aab2, 0x00aab4, 0 },
	{ 0x00aab7, 0x00aab8, 0 },
	{ 0x00aabe, 0x00aabf, 0 },
	{ 0x00aac1, 0x00aac1, 0 },
	{ 0x00aaec, 0x00aaed, 0 },
	{ 0x00aaf6, 0x00aaf6, 0 },
	{ 0x00abe5, 0x00abe5, 0 },
	{ 0x00abe8, 0x00abe8, 0 },
	{ 0x00abed, 0x00abed, 0 },
	{ 0x00ac00, 0x00d7a3, 2 },
	{ 0x00d800, 0x00dfff, 0 },
	{ 0x00e000, 0x00f8ff, 1 },
	{ 0x00f900, 0x00faff, 2 },
	{ 0x00fb1e, 0x00fb1e, 0 },
	{ 0x00fe00, 0x00fe0f, 0 },
	{ 0x00fe10, 0x00fe19, 2 },
	{ 0x00fe20, 0x00fe2d, 0 },
	{ 0x00fe30, 0x00fe52, 2 },
	{ 0x00fe54, 0x00fe66, 2 },
	{ 0x00fe68, 0x00fe6b, 2 },
	{ 0x00feff, 0x00feff, 0 },
	{ 0x00ff01, 0x00ff60, 2 },
	{ 0x00ffe0, 0x00ffe6, 2 },
	{ 0x00fff9, 0x00fffb, 0 },
	{ 0x00fffd, 0x00fffd, 1 },
	{ 0x0101fd, 0x0101fd, 0 },
	{ 0x0102e0, 0x0102e0, 0 },
	{ 0x010376, 0x01037a, 0 },
	{ 0x010a01, 0x010a03, 0 },
	{ 0x010a05, 0x010a06, 0 },
	{ 0x010a0c, 0x010a0f, 0 },
	{ 0x010a38, 0x010a3a, 0 },
	{ 0x010a3f, 0x010a3f, 0 },
	{ 0x010ae5, 0x010ae6, 0 },
	{ 0x011001, 0x011001, 0 },
	{ 0x011038, 0x011046, 0 },
	{ 0x01107f, 0x011081, 0 },
	{ 0x0110b3, 0x0110b6, 0 },
	{ 0x0110b9, 0x0110ba, 0 },
	{ 0x0110bd, 0x0110bd, 0 },
	{ 0x011100, 0x011102, 0 },
	{ 0x011127, 0x01112b, 0 },
	{ 0x01112d, 0x011134, 0 },
	{ 0x011173, 0x011173, 0 },
	{ 0x011180, 0x011181, 0 },
	{ 0x0111b6, 0x0111be, 0 },
	{ 0x01122f, 0x011231, 0 },
	{ 0x011234, 0x011234, 0 },
	{ 0x011236, 0x011237, 0 },
	{ 0x0112df, 0x0112df, 0 },
	{ 0x0112e3, 0x0112ea, 0 },
	{ 0x011301, 0x011301, 0 },
	{ 0x01133c, 0x01133c, 0 },
	{ 0x011340, 0x011340, 0 },
	{ 0x011366, 0x01136c, 0 },
	{ 0x011370, 0x011374, 0 },
	{ 0x0114b3, 0x0114b8, 0 },
	{ 0x0114ba, 0x0114ba, 0 },
	{ 0x0114bf, 0x0114c0, 0 },
	{ 0x0114c2, 0x0114c3, 0 },
	{ 0x0115b2, 0x0115b5, 0 },
	{ 0x0115bc, 0x0115bd, 0 },
	{ 0x0115bf, 0x0115c0, 0 },
	{ 0x011633, 0x01163a, 0 },
	{ 0x01163d, 0x01163d, 0 },
	{ 0x01163f, 0x011640, 0 },
	{ 0x0116ab, 0x0116ab, 0 },
	{ 0x0116ad, 0x0116ad, 0 },
	{ 0x0116b0, 0x0116b5, 0 },
	{ 0x0116b7, 0x0116b7, 0 },
	{ 0x016af0, 0x016af4, 0 },
	{ 0x016b30, 0x016b36, 0 },
	{ 0x016f8f, 0x016f92, 0 },
	{ 0x01b000, 0x01b001, 2 },
	{ 0x01bc9d, 0x01bc9e, 0 },
	{ 0x01bca0, 0x01bca3, 0 },
	{ 0x01d167, 0x01d169, 0 },
	{ 0x01d173, 0x01d182, 0 },
	{ 0x01d185, 0x01d18b, 0 },
	{ 0x01d1aa, 0x01d1ad, 0 },
	{ 0x01d242, 0x01d244, 0 },
	{ 0x01e8d0, 0x01e8d6, 0 },
	{ 0x01f100, 0x01f10a, 1 },
	{ 0x01f110, 0x01f12d, 1 },
	{ 0x01f130, 0x01f169, 1 },
	{ 0x01f170, 0x01f19a, 1 },
	{ 0x01f200, 0x01f202, 2 },
	{ 0x01f210, 0x01f23a, 2 },
	{ 0x01f240, 0x01f248, 2 },
	{ 0x01f250, 0x01f251, 2 },
	{ 0x020000, 0x02fffd, 2 },
	{ 0x030000, 0x03fffd, 2 },
	{ 0x0e0001, 0x0e0001, 0 },
	{ 0x0e0020, 0x0e007f, 0 },
	{ 0x0e0100, 0x0e01ef, 0 },
	{ 0x0f0000, 0x0ffffd, 0 },
	{ 0x100000, 0x10fffd, 0 },
};

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

/* Lookup width of UTF-8 data in tree. */
u_int
utf8_width(u_int uc)
{
	struct utf8_width_entry	*item;
	int	i, i0, i1;

	/* Perform binary search. */
	i0 = 0;
	i1 = sizeof utf8_width_table / sizeof (struct utf8_width_entry);
	while (i0 < i1) {
		i = (i0 + i1)/2;
		item = &utf8_width_table[i];
		if (uc < item->first)
			i1 = i;
		else if (uc > item->last)
			i0 = i+1;
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
