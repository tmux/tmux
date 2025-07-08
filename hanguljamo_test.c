#include <sys/types.h>
#include <stdio.h>

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
		if (s[1] == 0x9F) {
			if (s[2] >= 0x80 && s[2] <= 0x86)
				return (HANGULJAMO_SUBCLASS_EXTENDED_OLD_JUNGSEONG);
			if (s[2] >= 0x8B && s[2] <= 0xBB)
				return (HANGULJAMO_SUBCLASS_EXTENDED_OLD_JONGSEONG);
		}
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

static void
print_3bytes(const u_char *s)
{
	printf("[\\x%02X\\x%02X\\x%02X]", s[0], s[1], s[2]);
}

static void
hanguljamo_test_subclass(const char *label, const u_char *utf8, enum hanguljamo_subclass expected)
{
	enum hanguljamo_subclass actual = hanguljamo_get_subclass(utf8);
	printf("%-40s ", label);
	print_3bytes(utf8);
	if (actual == expected) {
		printf(" OK\n");
	} else {
		printf(" FAILED (got %d, expected %d)\n", actual, expected);
	}
}

int
main(void)
{
	hanguljamo_test_subclass("Choseong start U+1100", (const u_char *)"\xE1\x84\x80", HANGULJAMO_SUBCLASS_CHOSEONG);
	hanguljamo_test_subclass("Choseong end U+1112",   (const u_char *)"\xE1\x84\x92", HANGULJAMO_SUBCLASS_CHOSEONG);

	hanguljamo_test_subclass("Old Choseong start U+1113", (const u_char *)"\xE1\x84\x93", HANGULJAMO_SUBCLASS_OLD_CHOSEONG);
	hanguljamo_test_subclass("Old Choseong end U+115E",   (const u_char *)"\xE1\x85\x9E", HANGULJAMO_SUBCLASS_OLD_CHOSEONG);

	hanguljamo_test_subclass("Choseong Filler U+115F", (const u_char *)"\xE1\x85\x9F", HANGULJAMO_SUBCLASS_CHOSEONG_FILLER);
	hanguljamo_test_subclass("Jungseong Filler U+1160", (const u_char *)"\xE1\x85\xA0", HANGULJAMO_SUBCLASS_JUNGSEONG_FILLER);

	hanguljamo_test_subclass("Jungseong start U+1161", (const u_char *)"\xE1\x85\xA1", HANGULJAMO_SUBCLASS_JUNGSEONG);
	hanguljamo_test_subclass("Jungseong end U+1175",   (const u_char *)"\xE1\x85\xB5", HANGULJAMO_SUBCLASS_JUNGSEONG);

	hanguljamo_test_subclass("Old Jungseong start U+1176", (const u_char *)"\xE1\x85\xB6", HANGULJAMO_SUBCLASS_OLD_JUNGSEONG);
	hanguljamo_test_subclass("Old Jungseong end U+11A7",   (const u_char *)"\xE1\x86\xA7", HANGULJAMO_SUBCLASS_OLD_JUNGSEONG);

	hanguljamo_test_subclass("Jongseong start U+11A8", (const u_char *)"\xE1\x86\xA8", HANGULJAMO_SUBCLASS_JONGSEONG);
	hanguljamo_test_subclass("Jongseong end U+11C2",   (const u_char *)"\xE1\x87\x82", HANGULJAMO_SUBCLASS_JONGSEONG);

	hanguljamo_test_subclass("Old Jongseong start U+11C3", (const u_char *)"\xE1\x87\x83", HANGULJAMO_SUBCLASS_OLD_JONGSEONG);
	hanguljamo_test_subclass("Old Jongseong end U+11FF",   (const u_char *)"\xE1\x87\xBF", HANGULJAMO_SUBCLASS_OLD_JONGSEONG);

	hanguljamo_test_subclass("Ext. Old Choseong start U+A960", (const u_char *)"\xEA\xA5\xA0", HANGULJAMO_SUBCLASS_EXTENDED_OLD_CHOSEONG);
	hanguljamo_test_subclass("Ext. Old Choseong end U+A97C",   (const u_char *)"\xEA\xA5\xBC", HANGULJAMO_SUBCLASS_EXTENDED_OLD_CHOSEONG);

	hanguljamo_test_subclass("Ext. Old Jungseong start U+D7B0", (const u_char *)"\xED\x9E\xB0", HANGULJAMO_SUBCLASS_EXTENDED_OLD_JUNGSEONG);
	hanguljamo_test_subclass("Ext. Old Jungseong end U+D7C6",   (const u_char *)"\xED\x9F\x86", HANGULJAMO_SUBCLASS_EXTENDED_OLD_JUNGSEONG);

	hanguljamo_test_subclass("Ext. Old Jongseong start U+D7CB", (const u_char *)"\xED\x9F\x8B", HANGULJAMO_SUBCLASS_EXTENDED_OLD_JONGSEONG);
	hanguljamo_test_subclass("Ext. Old Jongseong end U+D7FB",   (const u_char *)"\xED\x9F\xBB", HANGULJAMO_SUBCLASS_EXTENDED_OLD_JONGSEONG);
}
