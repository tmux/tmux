/* $XTermId: charclass.c,v 1.28 2017/05/29 17:43:54 tom Exp $ */

/*
 * Compact and efficient reimplementation of the
 * xterm character class mechanism for large character sets
 *
 * Markus Kuhn -- mkuhn@acm.org -- 2000-07-03
 *
 * xterm allows users to select entire words with a double-click on the left
 * mouse button.  Opinions might differ on what type of characters are part of
 * separate words, therefore xterm allows users to configure a class code for
 * each 8-bit character.  Words are maximum length sequences of neighboring
 * characters with identical class code.  Extending this mechanism to Unicode
 * naively would create an at least 2^16 entries (128 kB) long class code
 * table.
 *
 * Instead, we transform the character class table into a list of intervals,
 * that will be accessed via a linear search.  Changes made to the table by the
 * user will be appended.  A special class code IDENT (default) marks
 * characters who have their code number as the class code.
 *
 * We could alternatively use a sorted table of non-overlapping intervals that
 * can be accessed via binary search, but merging in new intervals is
 * significantly more hassle and not worth the effort here.
 */

#include <xterm.h>
#include <charclass.h>

#if OPT_WIDE_CHARS

static struct classentry {
    int cclass;
    int first;
    int last;
} *classtab;

/*
 * Special convention for classtab[0]:
 * - classtab[0].cclass is the allocated number of entries in classtab
 * - classtab[0].first = 1 (first used entry in classtab)
 * - classtab[0].last is the last used entry in classtab
 */

int
SetCharacterClassRange(int low, int high, int value)
{
    TRACE(("...SetCharacterClassRange (%#x .. %#x) = %d\n", low, high, value));

    if (high < low)
	return -1;		/* nothing to do */

    /* make sure we have at least one free entry left at table end */
    if (classtab[0].last > classtab[0].cclass - 2) {
	classtab[0].cclass += 5 + classtab[0].cclass / 4;
	classtab = TypeRealloc(struct classentry,
			         (unsigned) classtab[0].cclass, classtab);
	if (!classtab)
	    abort();
    }

    /* simply append new interval to end of interval array */
    classtab[0].last++;
    classtab[classtab[0].last].first = low;
    classtab[classtab[0].last].last = high;
    classtab[classtab[0].last].cclass = value;

    return 0;
}

typedef enum {
    IDENT = -1,
    ALNUM = 48,
    CNTRL = 1,
    BLANK = 32,
    U_CJK = 0x4e00,
    U_SUP = 0x2070,
    U_SUB = 0x2080,
    U_HIR = 0x3040,
    U_KAT = 0x30a0,
    U_HAN = 0xac00
} Classes;

void
init_classtab(void)
{
    const int size = 50;

    TRACE(("init_classtab {{\n"));

    classtab = TypeMallocN(struct classentry, (unsigned) size);
    if (!classtab)
	abort();
    classtab[0].cclass = size;
    classtab[0].first = 1;
    classtab[0].last = 0;

    /* old xterm default classes */
    SetCharacterClassRange(0, 0, BLANK);
    SetCharacterClassRange(1, 31, CNTRL);
    SetCharacterClassRange('\t', '\t', BLANK);
    SetCharacterClassRange('0', '9', ALNUM);
    SetCharacterClassRange('A', 'Z', ALNUM);
    SetCharacterClassRange('_', '_', ALNUM);
    SetCharacterClassRange('a', 'z', ALNUM);
    SetCharacterClassRange(127, 159, CNTRL);
    SetCharacterClassRange(160, 191, IDENT);
    SetCharacterClassRange(192, 255, ALNUM);
    SetCharacterClassRange(215, 215, IDENT);
    SetCharacterClassRange(247, 247, IDENT);

    /* added Unicode classes */
    SetCharacterClassRange(0x0100, 0xffdf, ALNUM);	/* mostly characters */
    SetCharacterClassRange(0x037e, 0x037e, IDENT);	/* Greek question mark */
    SetCharacterClassRange(0x0387, 0x0387, IDENT);	/* Greek ano teleia */
    SetCharacterClassRange(0x055a, 0x055f, IDENT);	/* Armenian punctuation */
    SetCharacterClassRange(0x0589, 0x0589, IDENT);	/* Armenian full stop */
    SetCharacterClassRange(0x0700, 0x070d, IDENT);	/* Syriac punctuation */
    SetCharacterClassRange(0x104a, 0x104f, IDENT);	/* Myanmar punctuation */
    SetCharacterClassRange(0x10fb, 0x10fb, IDENT);	/* Georgian punctuation */
    SetCharacterClassRange(0x1361, 0x1368, IDENT);	/* Ethiopic punctuation */
    SetCharacterClassRange(0x166d, 0x166e, IDENT);	/* Canadian Syl. punctuation */
    SetCharacterClassRange(0x17d4, 0x17dc, IDENT);	/* Khmer punctuation */
    SetCharacterClassRange(0x1800, 0x180a, IDENT);	/* Mongolian punctuation */
    SetCharacterClassRange(0x2000, 0x200a, BLANK);	/* spaces */
    SetCharacterClassRange(0x200b, 0x27ff, IDENT);	/* punctuation and symbols */
    SetCharacterClassRange(0x2070, 0x207f, U_SUP);	/* superscript */
    SetCharacterClassRange(0x2080, 0x208f, U_SUB);	/* subscript */
    SetCharacterClassRange(0x3000, 0x3000, BLANK);	/* ideographic space */
    SetCharacterClassRange(0x3001, 0x3020, IDENT);	/* ideographic punctuation */
    SetCharacterClassRange(0x3040, 0x309f, U_HIR);	/* Hiragana */
    SetCharacterClassRange(0x30a0, 0x30ff, U_KAT);	/* Katakana */
    SetCharacterClassRange(0x3300, 0x9fff, U_CJK);	/* CJK Ideographs */
    SetCharacterClassRange(0xac00, 0xd7a3, U_HAN);	/* Hangul Syllables */
    SetCharacterClassRange(0xf900, 0xfaff, U_CJK);	/* CJK Ideographs */
    SetCharacterClassRange(0xfe30, 0xfe6b, IDENT);	/* punctuation forms */
    SetCharacterClassRange(0xff00, 0xff0f, IDENT);	/* half/fullwidth ASCII */
    SetCharacterClassRange(0xff1a, 0xff20, IDENT);	/* half/fullwidth ASCII */
    SetCharacterClassRange(0xff3b, 0xff40, IDENT);	/* half/fullwidth ASCII */
    SetCharacterClassRange(0xff5b, 0xff64, IDENT);	/* half/fullwidth ASCII */

    TRACE(("}} init_classtab\n"));
    return;
}

int
CharacterClass(int c)
{
    int i, cclass = IDENT;

    for (i = classtab[0].first; i <= classtab[0].last; i++)
	if (classtab[i].first <= c && classtab[i].last >= c)
	    cclass = classtab[i].cclass;

    if (cclass < 0)
	cclass = c;

    return cclass;
}

#if OPT_REPORT_CCLASS
#define charFormat(code) ((code) > 255 ? "0x%04X" : "%d")
static const char *
class_name(Classes code)
{
    static char buffer[80];
    const char *result = "?";
    switch (code) {
    case IDENT:
	result = "IDENT";
	break;
    case ALNUM:
	result = "ALNUM";
	break;
    case CNTRL:
	result = "CNTRL";
	break;
    case BLANK:
	result = "BLANK";
	break;
    case U_SUP:
	result = "superscript";
	break;
    case U_SUB:
	result = "subscript";
	break;
    case U_CJK:
	result = "CJK Ideographs";
	break;
    case U_HIR:
	result = "Hiragana";
	break;
    case U_KAT:
	result = "Katakana";
	break;
    case U_HAN:
	result = "Hangul Syllables";
	break;
    default:
	sprintf(buffer, charFormat(code), code);
	result = buffer;
	break;
    }
    return result;
}

void
report_wide_char_class(void)
{
    static const Classes known_classes[] =
    {IDENT, ALNUM, CNTRL, BLANK, U_SUP, U_SUB, U_HIR, U_KAT, U_CJK, U_HAN};
    int i;

    printf("\n");
    printf("Unicode charClass data uses the last match\n");
    printf("from these overlapping intervals of character codes:\n");
    for (i = classtab[0].first; i <= classtab[0].last; i++) {
	printf("\tU+%04X .. U+%04X %s\n",
	       classtab[i].first,
	       classtab[i].last,
	       class_name(classtab[i].cclass));
    }
    printf("\n");
    printf("These class-names are used internally (the first character code in a class):\n");
    for (i = 0; i < (int) XtNumber(known_classes); ++i) {
	printf("\t");
	printf(charFormat(known_classes[i]), known_classes[i]);
	printf(" = %s\n", class_name(known_classes[i]));
    }
}
#endif /* OPT_REPORT_CCLASS */

#ifdef NO_LEAKS
void
noleaks_CharacterClass(void)
{
    if (classtab != 0) {
	free(classtab);
	classtab = 0;
    }
}
#endif

#endif /* OPT_WIDE_CHARS */
