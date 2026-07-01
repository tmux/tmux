/* ANSI-C code produced by gperf version 3.1 */
/* Command-line: gperf --pic -m 100 fcobjshash.gperf  */
/* Computed positions: -k'3,5' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gperf@gnu.org>."
#endif

#line 1 "fcobjshash.gperf"

#line 13 "fcobjshash.gperf"
struct FcObjectTypeInfo {
int name;
int id;
};
#include <string.h>
/* maximum key range = 59, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
FcObjectTypeHash (register const char *str, register size_t len)
{
  static const unsigned char asso_values[] =
    {
      66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
      66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
      66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
      66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
      66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
      66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
      66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
      66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
      66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
      66, 66, 66, 66, 66, 66, 66,  4, 10, 32,
      21, 29, 28, 49, 14,  4, 66, 66,  5, 31,
      18, 22, 27, 66, 15,  9,  8, 23, 23, 13,
      23, 16,  4, 66, 66, 66, 66, 66, 66, 66,
      66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
      66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
      66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
      66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
      66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
      66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
      66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
      66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
      66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
      66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
      66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
      66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
      66, 66, 66, 66, 66, 66
    };
  register unsigned int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[4]];
      /*FALLTHROUGH*/
      case 4:
      case 3:
        hval += asso_values[(unsigned char)str[2]];
        break;
    }
  return hval;
}

struct FcObjectTypeNamePool_t
  {
    char FcObjectTypeNamePool_str7[sizeof("dpi")];
    char FcObjectTypeNamePool_str8[sizeof("size")];
    char FcObjectTypeNamePool_str9[sizeof("file")];
    char FcObjectTypeNamePool_str13[sizeof("hash")];
    char FcObjectTypeNamePool_str14[sizeof("rgba")];
    char FcObjectTypeNamePool_str15[sizeof("spacing")];
    char FcObjectTypeNamePool_str16[sizeof("scalable")];
    char FcObjectTypeNamePool_str17[sizeof("slant")];
    char FcObjectTypeNamePool_str18[sizeof("matrix")];
    char FcObjectTypeNamePool_str19[sizeof("outline")];
    char FcObjectTypeNamePool_str20[sizeof("charset")];
    char FcObjectTypeNamePool_str21[sizeof("antialias")];
    char FcObjectTypeNamePool_str22[sizeof("lang")];
    char FcObjectTypeNamePool_str23[sizeof("embolden")];
    char FcObjectTypeNamePool_str24[sizeof("weight")];
    char FcObjectTypeNamePool_str25[sizeof("color")];
    char FcObjectTypeNamePool_str26[sizeof("charwidth")];
    char FcObjectTypeNamePool_str27[sizeof("variable")];
    char FcObjectTypeNamePool_str28[sizeof("charheight")];
    char FcObjectTypeNamePool_str29[sizeof("hinting")];
    char FcObjectTypeNamePool_str30[sizeof("autohint")];
    char FcObjectTypeNamePool_str31[sizeof("fullname")];
    char FcObjectTypeNamePool_str32[sizeof("postscriptname")];
    char FcObjectTypeNamePool_str33[sizeof("verticallayout")];
    char FcObjectTypeNamePool_str34[sizeof("lcdfilter")];
    char FcObjectTypeNamePool_str35[sizeof("fullnamelang")];
    char FcObjectTypeNamePool_str36[sizeof("hintstyle")];
    char FcObjectTypeNamePool_str37[sizeof("pixelsize")];
    char FcObjectTypeNamePool_str38[sizeof("scale")];
    char FcObjectTypeNamePool_str39[sizeof("globaladvance")];
    char FcObjectTypeNamePool_str40[sizeof("width")];
    char FcObjectTypeNamePool_str41[sizeof("order")];
    char FcObjectTypeNamePool_str42[sizeof("family")];
    char FcObjectTypeNamePool_str43[sizeof("fonthashint")];
    char FcObjectTypeNamePool_str44[sizeof("namelang")];
    char FcObjectTypeNamePool_str45[sizeof("embeddedbitmap")];
    char FcObjectTypeNamePool_str46[sizeof("familylang")];
    char FcObjectTypeNamePool_str47[sizeof("capability")];
    char FcObjectTypeNamePool_str48[sizeof("rasterizer")];
    char FcObjectTypeNamePool_str49[sizeof("index")];
    char FcObjectTypeNamePool_str50[sizeof("style")];
    char FcObjectTypeNamePool_str51[sizeof("foundry")];
    char FcObjectTypeNamePool_str52[sizeof("fontversion")];
    char FcObjectTypeNamePool_str53[sizeof("minspace")];
    char FcObjectTypeNamePool_str54[sizeof("stylelang")];
    char FcObjectTypeNamePool_str55[sizeof("fontvariations")];
    char FcObjectTypeNamePool_str56[sizeof("fontformat")];
    char FcObjectTypeNamePool_str57[sizeof("decorative")];
    char FcObjectTypeNamePool_str58[sizeof("fontfeatures")];
    char FcObjectTypeNamePool_str59[sizeof("symbol")];
    char FcObjectTypeNamePool_str60[sizeof("prgname")];
    char FcObjectTypeNamePool_str65[sizeof("aspect")];
  };
static const struct FcObjectTypeNamePool_t FcObjectTypeNamePool_contents =
  {
    "dpi",
    "size",
    "file",
    "hash",
    "rgba",
    "spacing",
    "scalable",
    "slant",
    "matrix",
    "outline",
    "charset",
    "antialias",
    "lang",
    "embolden",
    "weight",
    "color",
    "charwidth",
    "variable",
    "charheight",
    "hinting",
    "autohint",
    "fullname",
    "postscriptname",
    "verticallayout",
    "lcdfilter",
    "fullnamelang",
    "hintstyle",
    "pixelsize",
    "scale",
    "globaladvance",
    "width",
    "order",
    "family",
    "fonthashint",
    "namelang",
    "embeddedbitmap",
    "familylang",
    "capability",
    "rasterizer",
    "index",
    "style",
    "foundry",
    "fontversion",
    "minspace",
    "stylelang",
    "fontvariations",
    "fontformat",
    "decorative",
    "fontfeatures",
    "symbol",
    "prgname",
    "aspect"
  };
#define FcObjectTypeNamePool ((const char *) &FcObjectTypeNamePool_contents)
const struct FcObjectTypeInfo *
FcObjectTypeLookup (register const char *str, register size_t len)
{
  enum
    {
      TOTAL_KEYWORDS = 52,
      MIN_WORD_LENGTH = 3,
      MAX_WORD_LENGTH = 14,
      MIN_HASH_VALUE = 7,
      MAX_HASH_VALUE = 65
    };

  static const struct FcObjectTypeInfo wordlist[] =
    {
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 43 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str7,FC_DPI_OBJECT},
#line 27 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str8,FC_SIZE_OBJECT},
#line 38 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str9,FC_FILE_OBJECT},
      {-1}, {-1}, {-1},
#line 62 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str13,FC_HASH_OBJECT},
#line 44 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str14,FC_RGBA_OBJECT},
#line 30 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str15,FC_SPACING_OBJECT},
#line 42 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str16,FC_SCALABLE_OBJECT},
#line 24 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str17,FC_SLANT_OBJECT},
#line 49 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str18,FC_MATRIX_OBJECT},
#line 41 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str19,FC_OUTLINE_OBJECT},
#line 50 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str20,FC_CHARSET_OBJECT},
#line 32 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str21,FC_ANTIALIAS_OBJECT},
#line 51 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str22,FC_LANG_OBJECT},
#line 55 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str23,FC_EMBOLDEN_OBJECT},
#line 25 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str24,FC_WEIGHT_OBJECT},
#line 64 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str25,FC_COLOR_OBJECT},
#line 47 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str26,FC_CHARWIDTH_OBJECT},
#line 67 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str27,FC_VARIABLE_OBJECT},
#line 48 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str28,FC_CHAR_HEIGHT_OBJECT},
#line 34 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str29,FC_HINTING_OBJECT},
#line 36 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str30,FC_AUTOHINT_OBJECT},
#line 22 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str31,FC_FULLNAME_OBJECT},
#line 63 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str32,FC_POSTSCRIPT_NAME_OBJECT},
#line 35 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str33,FC_VERTICAL_LAYOUT_OBJECT},
#line 58 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str34,FC_LCD_FILTER_OBJECT},
#line 23 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str35,FC_FULLNAMELANG_OBJECT},
#line 33 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str36,FC_HINT_STYLE_OBJECT},
#line 29 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str37,FC_PIXEL_SIZE_OBJECT},
#line 45 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str38,FC_SCALE_OBJECT},
#line 37 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str39,FC_GLOBAL_ADVANCE_OBJECT},
#line 26 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str40,FC_WIDTH_OBJECT},
#line 69 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str41,FC_ORDER_OBJECT},
#line 18 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str42,FC_FAMILY_OBJECT},
#line 68 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str43,FC_FONT_HAS_HINT_OBJECT},
#line 59 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str44,FC_NAMELANG_OBJECT},
#line 56 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str45,FC_EMBEDDED_BITMAP_OBJECT},
#line 19 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str46,FC_FAMILYLANG_OBJECT},
#line 53 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str47,FC_CAPABILITY_OBJECT},
#line 40 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str48,FC_RASTERIZER_OBJECT},
#line 39 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str49,FC_INDEX_OBJECT},
#line 20 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str50,FC_STYLE_OBJECT},
#line 31 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str51,FC_FOUNDRY_OBJECT},
#line 52 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str52,FC_FONTVERSION_OBJECT},
#line 46 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str53,FC_MINSPACE_OBJECT},
#line 21 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str54,FC_STYLELANG_OBJECT},
#line 66 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str55,FC_FONT_VARIATIONS_OBJECT},
#line 54 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str56,FC_FONTFORMAT_OBJECT},
#line 57 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str57,FC_DECORATIVE_OBJECT},
#line 60 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str58,FC_FONT_FEATURES_OBJECT},
#line 65 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str59,FC_SYMBOL_OBJECT},
#line 61 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str60,FC_PRGNAME_OBJECT},
      {-1}, {-1}, {-1}, {-1},
#line 28 "fcobjshash.gperf"
      {(int)(size_t)&((struct FcObjectTypeNamePool_t *)0)->FcObjectTypeNamePool_str65,FC_ASPECT_OBJECT}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register unsigned int key = FcObjectTypeHash (str, len);

      if (key <= MAX_HASH_VALUE)
        {
          register int o = wordlist[key].name;
          if (o >= 0)
            {
              register const char *s = o + FcObjectTypeNamePool;

              if (*str == *s && !strcmp (str + 1, s + 1))
                return &wordlist[key];
            }
        }
    }
  return 0;
}
