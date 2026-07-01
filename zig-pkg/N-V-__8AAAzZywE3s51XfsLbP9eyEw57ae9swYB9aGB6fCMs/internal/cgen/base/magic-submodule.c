// Copyright 2021 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ---------------- Magic Numbers

// ICO doesn't start with a magic identifier. Instead, see if the opening bytes
// are plausibly ICO.
//
// Callers should have already verified that (prefix_data.len >= 2) and the
// first two bytes are 0x00.
//
// See:
//  - https://docs.fileformat.com/image/ico/
static int32_t  //
wuffs_base__magic_number_guess_fourcc__maybe_ico(
    wuffs_base__slice_u8 prefix_data,
    bool prefix_closed) {
  // Allow-list for the Image Type field.
  if (prefix_data.len < 4) {
    return prefix_closed ? 0 : -1;
  } else if (prefix_data.ptr[3] != 0) {
    return 0;
  }
  switch (prefix_data.ptr[2]) {
    case 0x01:  // ICO
    case 0x02:  // CUR
      break;
    default:
      return 0;
  }

  // The Number Of Images should be positive.
  if (prefix_data.len < 6) {
    return prefix_closed ? 0 : -1;
  } else if ((prefix_data.ptr[4] == 0) && (prefix_data.ptr[5] == 0)) {
    return 0;
  }

  // The first ICONDIRENTRY's fourth byte should be zero.
  if (prefix_data.len < 10) {
    return prefix_closed ? 0 : -1;
  } else if (prefix_data.ptr[9] != 0) {
    return 0;
  }

  // TODO: have a separate FourCC for CUR?
  return 0x49434F20;  // 'ICO 'be
}

// TGA doesn't start with a magic identifier. Instead, see if the opening bytes
// are plausibly TGA.
//
// Callers should have already verified that (prefix_data.len >= 2) and the
// second byte (prefix_data.ptr[1], the Color Map Type byte), is either 0x00 or
// 0x01.
//
// See:
//  - https://docs.fileformat.com/image/tga/
//  - https://www.dca.fee.unicamp.br/~martino/disciplinas/ea978/tgaffs.pdf
static int32_t  //
wuffs_base__magic_number_guess_fourcc__maybe_tga(
    wuffs_base__slice_u8 prefix_data,
    bool prefix_closed) {
  // Allow-list for the Image Type field.
  if (prefix_data.len < 3) {
    return prefix_closed ? 0 : -1;
  }
  switch (prefix_data.ptr[2]) {
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x09:
    case 0x0A:
    case 0x0B:
      break;
    default:
      // TODO: 0x20 and 0x21 are invalid, according to the spec, but are
      // apparently unofficial extensions.
      return 0;
  }

  // Allow-list for the Color Map Entry Size field (if the Color Map Type field
  // is non-zero) or else all the Color Map fields should be zero.
  if (prefix_data.len < 8) {
    return prefix_closed ? 0 : -1;
  } else if (prefix_data.ptr[1] != 0x00) {
    switch (prefix_data.ptr[7]) {
      case 0x0F:
      case 0x10:
      case 0x18:
      case 0x20:
        break;
      default:
        return 0;
    }
  } else if ((prefix_data.ptr[3] | prefix_data.ptr[4] | prefix_data.ptr[5] |
              prefix_data.ptr[6] | prefix_data.ptr[7]) != 0x00) {
    return 0;
  }

  // Allow-list for the Pixel Depth field.
  if (prefix_data.len < 17) {
    return prefix_closed ? 0 : -1;
  }
  switch (prefix_data.ptr[16]) {
    case 0x01:
    case 0x08:
    case 0x0F:
    case 0x10:
    case 0x18:
    case 0x20:
      break;
    default:
      return 0;
  }

  return 0x54474120;  // 'TGA 'be
}

WUFFS_BASE__MAYBE_STATIC int32_t  //
wuffs_base__magic_number_guess_fourcc(wuffs_base__slice_u8 prefix_data,
                                      bool prefix_closed) {
  // This is similar to (but different from):
  //  - the magic/Magdir tables under https://github.com/file/file
  //  - the MIME Sniffing algorithm at https://mimesniff.spec.whatwg.org/

  // table holds the 'magic numbers' (which are actually variable length
  // strings). The strings may contain NUL bytes, so the "const char* magic"
  // value starts with the length-minus-1 of the 'magic number'.
  //
  // Keep it sorted by magic[1], then magic[0] descending (prioritizing longer
  // matches) and finally by magic[2:]. When multiple entries match, the
  // longest one wins.
  //
  // The fourcc field might be negated, in which case there's further
  // specialization (see § below).
  static struct {
    int32_t fourcc;
    const char* magic;
  } table[] = {
      {-0x30302020, "\x01\x00\x00"},                  // '00  'be
      {+0x41425852, "\x03\x03\x00\x08\x00"},          // ABXR
      {+0x475A2020, "\x02\x1F\x8B\x08"},              // GZ
      {+0x5A535444, "\x03\x28\xB5\x2F\xFD"},          // ZSTD
      {+0x584D4C20, "\x05\x3C\x3F\x78\x6D\x6C\x20"},  // XML
      {+0x41425853, "\x03\x41\x42\x58\x00"},          // ABXS
      {+0x425A3220, "\x02\x42\x5A\x68"},              // BZ2
      {+0x424D5020, "\x01\x42\x4D"},                  // BMP
      {+0x47494620, "\x03\x47\x49\x46\x38"},          // GIF
      {+0x54494646, "\x03\x49\x49\x2A\x00"},          // TIFF (little-endian)
      {+0x4C5A4950, "\x04\x4C\x5A\x49\x50\x01"},      // LZIP
      {+0x54494646, "\x03\x4D\x4D\x00\x2A"},          // TIFF (big-endian)
      {+0x45544332, "\x03\x50\x4B\x4D\x20"},          // ETC2 (*.pkm)
      {+0x4E50424D, "\x02\x50\x35\x0A"},              // NPBM (P5; *.pgm)
      {+0x4E50424D, "\x02\x50\x36\x0A"},              // NPBM (P6; *.ppm)
      {-0x52494646, "\x03\x52\x49\x46\x46"},          // RIFF
      {+0x4C5A4D41, "\x04\x5D\x00\x10\x00\x00"},      // LZMA
      {+0x4C5A4D41, "\x02\x5D\x00\x00"},              // LZMA
      {+0x4E494520, "\x02\x6E\xC3\xAF"},              // NIE
      {+0x514F4920, "\x03\x71\x6F\x69\x66"},          // QOI
      {+0x5A4C4942, "\x01\x78\x9C"},                  // ZLIB
      {+0x504E4720, "\x03\x89\x50\x4E\x47"},          // PNG
      {+0x54482020, "\x02\xC3\xBE\xFE"},              // TH
      {+0x585A2020, "\x04\xFD\x37\x7A\x58\x5A"},      // XZ
      {+0x4A504547, "\x01\xFF\xD8"},                  // JPEG
  };
  static const size_t table_len = sizeof(table) / sizeof(table[0]);

  if (prefix_data.len == 0) {
    return prefix_closed ? 0 : -1;
  }
  uint8_t pre_first_byte = prefix_data.ptr[0];

  int32_t fourcc = 0;
  size_t i;
  for (i = 0; i < table_len; i++) {
    uint8_t mag_first_byte = ((uint8_t)(table[i].magic[1]));
    if (pre_first_byte < mag_first_byte) {
      break;
    } else if (pre_first_byte > mag_first_byte) {
      continue;
    }
    fourcc = table[i].fourcc;

    uint8_t mag_remaining_len = ((uint8_t)(table[i].magic[0]));
    if (mag_remaining_len == 0) {
      goto match;
    }

    const char* mag_remaining_ptr = table[i].magic + 2;
    uint8_t* pre_remaining_ptr = prefix_data.ptr + 1;
    size_t pre_remaining_len = prefix_data.len - 1;
    if (pre_remaining_len < mag_remaining_len) {
      if (!memcmp(pre_remaining_ptr, mag_remaining_ptr, pre_remaining_len)) {
        return prefix_closed ? 0 : -1;
      }
    } else {
      if (!memcmp(pre_remaining_ptr, mag_remaining_ptr, mag_remaining_len)) {
        goto match;
      }
    }
  }

  if (prefix_data.len < 2) {
    return prefix_closed ? 0 : -1;
  } else if ((prefix_data.ptr[1] == 0x00) || (prefix_data.ptr[1] == 0x01)) {
    return wuffs_base__magic_number_guess_fourcc__maybe_tga(prefix_data,
                                                            prefix_closed);
  }

  return 0;

match:
  // Negative FourCC values (see § above) are further specialized.
  if (fourcc < 0) {
    fourcc = -fourcc;

    if (fourcc == 0x52494646) {  // 'RIFF'be
      if (prefix_data.len < 12) {
        return prefix_closed ? 0 : -1;
      }
      uint32_t x = wuffs_base__peek_u32be__no_bounds_check(prefix_data.ptr + 8);
      if (x == 0x57454250) {  // 'WEBP'be
        return 0x57454250;    // 'WEBP'be
      }

    } else if (fourcc == 0x30302020) {  // '00  'be
      // Binary data starting with multiple 0x00 NUL bytes is quite common.
      // Unfortunately, some file formats also don't start with a magic
      // identifier, so we have to use heuristics (where the order matters, the
      // same as /usr/bin/file's magic/Magdir tables) as best we can. Maybe
      // it's TGA, ICO/CUR, etc. Maybe it's something else.
      int32_t tga = wuffs_base__magic_number_guess_fourcc__maybe_tga(
          prefix_data, prefix_closed);
      if (tga != 0) {
        return tga;
      }
      int32_t ico = wuffs_base__magic_number_guess_fourcc__maybe_ico(
          prefix_data, prefix_closed);
      if (ico != 0) {
        return ico;
      }
      if (prefix_data.len < 4) {
        return prefix_closed ? 0 : -1;
      } else if ((prefix_data.ptr[2] != 0x00) &&
                 ((prefix_data.ptr[2] >= 0x80) ||
                  (prefix_data.ptr[3] != 0x00))) {
        // Roughly speaking, this could be a non-degenerate (non-0-width and
        // non-0-height) WBMP image.
        return 0x57424D50;  // 'WBMP'be
      }
      return 0;
    }
  }
  return fourcc;
}
