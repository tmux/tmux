/* $OpenBSD$ */

/*
 * Copyright (c) 2023 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <wchar.h>

#include "tmux.h"

enum hanguljamo_subclass {
	HANGULJAMO_SUBCLASS_NOT_HANGULJAMO,
	HANGULJAMO_SUBCLASS_CHOSEONG,			// U+1100 - U+1112
	HANGULJAMO_SUBCLASS_OLD_CHOSEONG,		// U+1113 - U+115E
	HANGULJAMO_SUBCLASS_CHOSEONG_FILLER,		// U+115F
	HANGULJAMO_SUBCLASS_JUNGSEONG_FILLER,		// U+1160
	HANGULJAMO_SUBCLASS_JUNGSEONG,			// U+1161 - U+1175
	HANGULJAMO_SUBCLASS_OLD_JUNGSEONG,		// U+1176 - U+11A7
	HANGULJAMO_SUBCLASS_JONGSEONG,			// U+11A8 - U+11C2
	HANGULJAMO_SUBCLASS_OLD_JONGSEONG,		// U+11C3 - U+11FF
	HANGULJAMO_SUBCLASS_EXTENDED_OLD_CHOSEONG,	// U+A960 - U+A97C
	HANGULJAMO_SUBCLASS_EXTENDED_OLD_JUNGSEONG,	// U+D7B0 - U+D7C6
	HANGULJAMO_SUBCLASS_EXTENDED_OLD_JONGSEONG	// U+D7CB - U+D7FB
};

enum hanguljamo_class {
	HANGULJAMO_CLASS_NOT_HANGULJAMO,
	HANGULJAMO_CLASS_CHOSEONG,
	HANGULJAMO_CLASS_JUNGSEONG,
	HANGULJAMO_CLASS_JONGSEONG
};

/* Has this got a zero width joiner at the end? */
int
utf8_has_zwj(const struct utf8_data *ud)
{
	if (ud->size < 3)
		return (0);
	return (memcmp(ud->data + ud->size - 3, "\342\200\215", 3) == 0);
}

/* Is this a zero width joiner? */
int
utf8_is_zwj(const struct utf8_data *ud)
{
	if (ud->size != 3)
		return (0);
	return (memcmp(ud->data, "\342\200\215", 3) == 0);
}

/* Is this a variation selector? */
int
utf8_is_vs(const struct utf8_data *ud)
{
	if (ud->size != 3)
		return (0);
	return (memcmp(ud->data, "\357\270\217", 3) == 0);
}

/* Is this in the modifier table? */
int
utf8_is_modifier(const struct utf8_data *ud)
{
	wchar_t	wc;

	if (utf8_towc(ud, &wc) != UTF8_DONE)
		return (0);
	switch (wc) {
	case 0x1F1E6:
	case 0x1F1E7:
	case 0x1F1E8:
	case 0x1F1E9:
	case 0x1F1EA:
	case 0x1F1EB:
	case 0x1F1EC:
	case 0x1F1ED:
	case 0x1F1EE:
	case 0x1F1EF:
	case 0x1F1F0:
	case 0x1F1F1:
	case 0x1F1F2:
	case 0x1F1F3:
	case 0x1F1F4:
	case 0x1F1F5:
	case 0x1F1F6:
	case 0x1F1F7:
	case 0x1F1F8:
	case 0x1F1F9:
	case 0x1F1FA:
	case 0x1F1FB:
	case 0x1F1FC:
	case 0x1F1FD:
	case 0x1F1FE:
	case 0x1F1FF:
	case 0x1F3FB:
	case 0x1F3FC:
	case 0x1F3FD:
	case 0x1F3FE:
	case 0x1F3FF:
		return (1);
	}
	return (0);
}

static enum hanguljamo_subclass
hanguljamo_get_subclass(const u_char *s)
{
	switch (s[0]) {
	case 0xE1:
		switch (s[1]) {
		case 0x84:
			if (s[2] >= 0x80 && s[2] <= 0x92)
				return (HANGULJAMO_SUBCLASS_CHOSEONG);
			if (s[2] >= 0x93 && s[2] <= 0xBF)
				return (HANGULJAMO_SUBCLASS_OLD_CHOSEONG);
			break;
		case 0x85:
			if (s[2] == 0x9F)
				return (HANGULJAMO_SUBCLASS_CHOSEONG_FILLER);
			if (s[2] == 0xA0)
				return (HANGULJAMO_SUBCLASS_JUNGSEONG_FILLER);
			if (s[2] >= 0x80 && s[2] <= 0x9E)
				return (HANGULJAMO_SUBCLASS_OLD_CHOSEONG);
			if (s[2] >= 0xA1 && s[2] <= 0xB5)
				return (HANGULJAMO_SUBCLASS_JUNGSEONG);
			if (s[2] >= 0xB6 && s[2] <= 0xBF)
				return (HANGULJAMO_SUBCLASS_OLD_JUNGSEONG);
			break;
		case 0x86:
			if (s[2] >= 0x80 && s[2] <= 0xA7)
				return (HANGULJAMO_SUBCLASS_OLD_JUNGSEONG);
			if (s[2] >= 0xA8 && s[2] <= 0xBF)
				return (HANGULJAMO_SUBCLASS_JONGSEONG);
			break;
		case 0x87:
			if (s[2] >= 0x80 && s[2] <= 0x82)
				return (HANGULJAMO_SUBCLASS_JONGSEONG);
			if (s[2] >= 0x83 && s[2] <= 0xBF)
				return (HANGULJAMO_SUBCLASS_OLD_JONGSEONG);
			break;
		}
		break;
	case 0xEA:
		if (s[1] == 0xA5 && s[2] >= 0xA0 && s[2] <= 0xBC)
			return (HANGULJAMO_SUBCLASS_EXTENDED_OLD_CHOSEONG);
		break;
	case 0xED:
		if (s[1] == 0x9E && s[2] >= 0xB0 && s[2] <= 0xBF)
			return (HANGULJAMO_SUBCLASS_EXTENDED_OLD_JUNGSEONG);
		if (s[1] != 0x9F)
			break;
		if (s[2] >= 0x80 && s[2] <= 0x86)
			return (HANGULJAMO_SUBCLASS_EXTENDED_OLD_JUNGSEONG);
		if (s[2] >= 0x8B && s[2] <= 0xBB)
			return (HANGULJAMO_SUBCLASS_EXTENDED_OLD_JONGSEONG);
		break;
	}
	return (HANGULJAMO_SUBCLASS_NOT_HANGULJAMO);
}

static enum hanguljamo_class
hanguljamo_get_class(const u_char *s)
{
	switch (hanguljamo_get_subclass(s)) {
	case HANGULJAMO_SUBCLASS_CHOSEONG:
	case HANGULJAMO_SUBCLASS_CHOSEONG_FILLER:
	case HANGULJAMO_SUBCLASS_OLD_CHOSEONG:
	case HANGULJAMO_SUBCLASS_EXTENDED_OLD_CHOSEONG:
		return (HANGULJAMO_CLASS_CHOSEONG);
	case HANGULJAMO_SUBCLASS_JUNGSEONG:
	case HANGULJAMO_SUBCLASS_JUNGSEONG_FILLER:
	case HANGULJAMO_SUBCLASS_OLD_JUNGSEONG:
	case HANGULJAMO_SUBCLASS_EXTENDED_OLD_JUNGSEONG:
		return (HANGULJAMO_CLASS_JUNGSEONG);
	case HANGULJAMO_SUBCLASS_JONGSEONG:
	case HANGULJAMO_SUBCLASS_OLD_JONGSEONG:
	case HANGULJAMO_SUBCLASS_EXTENDED_OLD_JONGSEONG:
		return (HANGULJAMO_CLASS_JONGSEONG);
	case HANGULJAMO_SUBCLASS_NOT_HANGULJAMO:
		return (HANGULJAMO_CLASS_NOT_HANGULJAMO);
	}
	return (HANGULJAMO_CLASS_NOT_HANGULJAMO);
}

enum hanguljamo_state
hanguljamo_check_state(const struct utf8_data *p_ud, const struct utf8_data *ud)
{
	const u_char	*s;

	if (ud->size != 3)
		return (HANGULJAMO_STATE_NOT_HANGULJAMO);

	switch (hanguljamo_get_class(ud->data)) {
	case HANGULJAMO_CLASS_CHOSEONG:
		return (HANGULJAMO_STATE_CHOSEONG);
	case HANGULJAMO_CLASS_JUNGSEONG:
		if (p_ud->size < 3)
			return (HANGULJAMO_STATE_NOT_COMPOSABLE);
		s = p_ud->data + p_ud->size - 3;
		if (hanguljamo_get_class(s) == HANGULJAMO_CLASS_CHOSEONG)
			return (HANGULJAMO_STATE_COMPOSABLE);
		return (HANGULJAMO_STATE_NOT_COMPOSABLE);
	case HANGULJAMO_CLASS_JONGSEONG:
		if (p_ud->size < 3)
			return (HANGULJAMO_STATE_NOT_COMPOSABLE);
		s = p_ud->data + p_ud->size - 3;
		if (hanguljamo_get_class(s) == HANGULJAMO_CLASS_JUNGSEONG)
			return (HANGULJAMO_STATE_COMPOSABLE);
		return (HANGULJAMO_STATE_NOT_COMPOSABLE);
	case HANGULJAMO_CLASS_NOT_HANGULJAMO:
		return (HANGULJAMO_STATE_NOT_HANGULJAMO);
	}
	return (HANGULJAMO_STATE_NOT_HANGULJAMO);
}
