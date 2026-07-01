// Copyright 2020 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ---------------- Auxiliary - CBOR

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__AUX__CBOR)

#include <utility>

namespace wuffs_aux {

DecodeCborResult::DecodeCborResult(std::string&& error_message0,
                                   uint64_t cursor_position0)
    : error_message(std::move(error_message0)),
      cursor_position(cursor_position0) {}

DecodeCborCallbacks::~DecodeCborCallbacks() {}

void  //
DecodeCborCallbacks::Done(DecodeCborResult& result,
                          sync_io::Input& input,
                          IOBuffer& buffer) {}

DecodeCborArgQuirks::DecodeCborArgQuirks(const QuirkKeyValuePair* ptr0,
                                         const size_t len0)
    : ptr(ptr0), len(len0) {}

DecodeCborArgQuirks  //
DecodeCborArgQuirks::DefaultValue() {
  return DecodeCborArgQuirks(nullptr, 0);
}

DecodeCborResult  //
DecodeCbor(DecodeCborCallbacks& callbacks,
           sync_io::Input& input,
           DecodeCborArgQuirks quirks) {
  // Prepare the wuffs_base__io_buffer and the resultant error_message.
  wuffs_base__io_buffer* io_buf = input.BringsItsOwnIOBuffer();
  wuffs_base__io_buffer fallback_io_buf = wuffs_base__empty_io_buffer();
  std::unique_ptr<uint8_t[]> fallback_io_array(nullptr);
  if (!io_buf) {
    fallback_io_array = std::unique_ptr<uint8_t[]>(new uint8_t[4096]);
    fallback_io_buf = wuffs_base__ptr_u8__writer(fallback_io_array.get(), 4096);
    io_buf = &fallback_io_buf;
  }
  // cursor_index is discussed at
  // https://nigeltao.github.io/blog/2020/jsonptr.html#the-cursor-index
  size_t cursor_index = 0;
  std::string ret_error_message;
  std::string io_error_message;

  do {
    // Prepare the low-level CBOR decoder.
    wuffs_cbor__decoder::unique_ptr dec = wuffs_cbor__decoder::alloc();
    if (!dec) {
      ret_error_message = "wuffs_aux::DecodeCbor: out of memory";
      goto done;
    }
    for (size_t i = 0; i < quirks.len; i++) {
      dec->set_quirk(quirks.ptr[i].first, quirks.ptr[i].second);
    }

    // Prepare the wuffs_base__tok_buffer. 256 tokens is 2KiB.
    wuffs_base__token tok_array[256];
    wuffs_base__token_buffer tok_buf =
        wuffs_base__slice_token__writer(wuffs_base__make_slice_token(
            &tok_array[0], (sizeof(tok_array) / sizeof(tok_array[0]))));
    wuffs_base__status tok_status = wuffs_base__make_status(nullptr);

    // Prepare other state.
    int32_t depth = 0;
    std::string str;
    int64_t extension_category = 0;
    uint64_t extension_detail = 0;

    // Valid token's VBCs range in 0 ..= 15. Values over that are for tokens
    // from outside of the base package, such as the CBOR package.
    constexpr int64_t EXT_CAT__CBOR_TAG = 16;

    // Loop, doing these two things:
    //  1. Get the next token.
    //  2. Process that token.
    while (true) {
      // 1. Get the next token.

      while (tok_buf.meta.ri >= tok_buf.meta.wi) {
        if (tok_status.repr == nullptr) {
          // No-op.
        } else if (tok_status.repr == wuffs_base__suspension__short_write) {
          tok_buf.compact();
        } else if (tok_status.repr == wuffs_base__suspension__short_read) {
          // Read from input to io_buf.
          if (!io_error_message.empty()) {
            ret_error_message = std::move(io_error_message);
            goto done;
          } else if (cursor_index != io_buf->meta.ri) {
            ret_error_message =
                "wuffs_aux::DecodeCbor: internal error: bad cursor_index";
            goto done;
          } else if (io_buf->meta.closed) {
            ret_error_message =
                "wuffs_aux::DecodeCbor: internal error: io_buf is closed";
            goto done;
          }
          io_buf->compact();
          if (io_buf->meta.wi >= io_buf->data.len) {
            ret_error_message =
                "wuffs_aux::DecodeCbor: internal error: io_buf is full";
            goto done;
          }
          cursor_index = io_buf->meta.ri;
          io_error_message = input.CopyIn(io_buf);
        } else {
          ret_error_message = tok_status.message();
          goto done;
        }

        if (WUFFS_CBOR__DECODER_WORKBUF_LEN_MAX_INCL_WORST_CASE != 0) {
          ret_error_message =
              "wuffs_aux::DecodeCbor: internal error: bad WORKBUF_LEN";
          goto done;
        }
        wuffs_base__slice_u8 work_buf = wuffs_base__empty_slice_u8();
        tok_status = dec->decode_tokens(&tok_buf, io_buf, work_buf);
        if ((tok_buf.meta.ri > tok_buf.meta.wi) ||
            (tok_buf.meta.wi > tok_buf.data.len) ||
            (io_buf->meta.ri > io_buf->meta.wi) ||
            (io_buf->meta.wi > io_buf->data.len)) {
          ret_error_message =
              "wuffs_aux::DecodeCbor: internal error: bad buffer indexes";
          goto done;
        }
      }

      wuffs_base__token token = tok_buf.data.ptr[tok_buf.meta.ri++];
      uint64_t token_len = token.length();
      if ((io_buf->meta.ri < cursor_index) ||
          ((io_buf->meta.ri - cursor_index) < token_len)) {
        ret_error_message =
            "wuffs_aux::DecodeCbor: internal error: bad token indexes";
        goto done;
      }
      uint8_t* token_ptr = io_buf->data.ptr + cursor_index;
      cursor_index += static_cast<size_t>(token_len);

      // 2. Process that token.

      uint64_t vbd = token.value_base_detail();

      if (extension_category != 0) {
        int64_t ext = token.value_extension();
        if ((ext >= 0) && !token.continued()) {
          extension_detail = (extension_detail
                              << WUFFS_BASE__TOKEN__VALUE_EXTENSION__NUM_BITS) |
                             static_cast<uint64_t>(ext);
          switch (extension_category) {
            case WUFFS_BASE__TOKEN__VBC__INLINE_INTEGER_SIGNED:
              extension_category = 0;
              ret_error_message =
                  callbacks.AppendI64(static_cast<int64_t>(extension_detail));
              goto parsed_a_value;
            case WUFFS_BASE__TOKEN__VBC__INLINE_INTEGER_UNSIGNED:
              extension_category = 0;
              ret_error_message = callbacks.AppendU64(extension_detail);
              goto parsed_a_value;
            case EXT_CAT__CBOR_TAG:
              extension_category = 0;
              ret_error_message = callbacks.AppendCborTag(extension_detail);
              if (!ret_error_message.empty()) {
                goto done;
              }
              continue;
          }
        }
        ret_error_message =
            "wuffs_aux::DecodeCbor: internal error: bad extended token";
        goto done;
      }

      switch (token.value_base_category()) {
        case WUFFS_BASE__TOKEN__VBC__FILLER:
          continue;

        case WUFFS_BASE__TOKEN__VBC__STRUCTURE: {
          if (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__PUSH) {
            ret_error_message = callbacks.Push(static_cast<uint32_t>(vbd));
            if (!ret_error_message.empty()) {
              goto done;
            }
            depth++;
            if (depth > (int32_t)WUFFS_CBOR__DECODER_DEPTH_MAX_INCL) {
              ret_error_message =
                  "wuffs_aux::DecodeCbor: internal error: bad depth";
              goto done;
            }
            continue;
          }
          ret_error_message = callbacks.Pop(static_cast<uint32_t>(vbd));
          depth--;
          if (depth < 0) {
            ret_error_message =
                "wuffs_aux::DecodeCbor: internal error: bad depth";
            goto done;
          }
          goto parsed_a_value;
        }

        case WUFFS_BASE__TOKEN__VBC__STRING: {
          if (vbd & WUFFS_BASE__TOKEN__VBD__STRING__CONVERT_0_DST_1_SRC_DROP) {
            // No-op.
          } else if (vbd &
                     WUFFS_BASE__TOKEN__VBD__STRING__CONVERT_1_DST_1_SRC_COPY) {
            const char* ptr =  // Convert from (uint8_t*).
                static_cast<const char*>(static_cast<void*>(token_ptr));
            str.append(ptr, static_cast<size_t>(token_len));
          } else {
            goto fail;
          }
          if (token.continued()) {
            continue;
          }
          ret_error_message =
              (vbd & WUFFS_BASE__TOKEN__VBD__STRING__CHAIN_MUST_BE_UTF_8)
                  ? callbacks.AppendTextString(std::move(str))
                  : callbacks.AppendByteString(std::move(str));
          str.clear();
          goto parsed_a_value;
        }

        case WUFFS_BASE__TOKEN__VBC__UNICODE_CODE_POINT: {
          uint8_t u[WUFFS_BASE__UTF_8__BYTE_LENGTH__MAX_INCL];
          size_t n = wuffs_base__utf_8__encode(
              wuffs_base__make_slice_u8(
                  &u[0], WUFFS_BASE__UTF_8__BYTE_LENGTH__MAX_INCL),
              static_cast<uint32_t>(vbd));
          const char* ptr =  // Convert from (uint8_t*).
              static_cast<const char*>(static_cast<void*>(&u[0]));
          str.append(ptr, n);
          if (token.continued()) {
            continue;
          }
          goto fail;
        }

        case WUFFS_BASE__TOKEN__VBC__LITERAL: {
          if (vbd & WUFFS_BASE__TOKEN__VBD__LITERAL__NULL) {
            ret_error_message = callbacks.AppendNull();
          } else if (vbd & WUFFS_BASE__TOKEN__VBD__LITERAL__UNDEFINED) {
            ret_error_message = callbacks.AppendUndefined();
          } else {
            ret_error_message = callbacks.AppendBool(
                vbd & WUFFS_BASE__TOKEN__VBD__LITERAL__TRUE);
          }
          goto parsed_a_value;
        }

        case WUFFS_BASE__TOKEN__VBC__NUMBER: {
          const uint64_t cfp_fbbe_fifb =
              WUFFS_BASE__TOKEN__VBD__NUMBER__CONTENT_FLOATING_POINT |
              WUFFS_BASE__TOKEN__VBD__NUMBER__FORMAT_BINARY_BIG_ENDIAN |
              WUFFS_BASE__TOKEN__VBD__NUMBER__FORMAT_IGNORE_FIRST_BYTE;
          if ((vbd & cfp_fbbe_fifb) == cfp_fbbe_fifb) {
            double f;
            switch (token_len) {
              case 3:
                f = wuffs_base__ieee_754_bit_representation__from_u16_to_f64(
                    wuffs_base__peek_u16be__no_bounds_check(token_ptr + 1));
                break;
              case 5:
                f = wuffs_base__ieee_754_bit_representation__from_u32_to_f64(
                    wuffs_base__peek_u32be__no_bounds_check(token_ptr + 1));
                break;
              case 9:
                f = wuffs_base__ieee_754_bit_representation__from_u64_to_f64(
                    wuffs_base__peek_u64be__no_bounds_check(token_ptr + 1));
                break;
              default:
                goto fail;
            }
            ret_error_message = callbacks.AppendF64(f);
            goto parsed_a_value;
          }
          goto fail;
        }

        case WUFFS_BASE__TOKEN__VBC__INLINE_INTEGER_SIGNED: {
          if (token.continued()) {
            extension_category = WUFFS_BASE__TOKEN__VBC__INLINE_INTEGER_SIGNED;
            extension_detail =
                static_cast<uint64_t>(token.value_base_detail__sign_extended());
            continue;
          }
          ret_error_message =
              callbacks.AppendI64(token.value_base_detail__sign_extended());
          goto parsed_a_value;
        }

        case WUFFS_BASE__TOKEN__VBC__INLINE_INTEGER_UNSIGNED: {
          if (token.continued()) {
            extension_category =
                WUFFS_BASE__TOKEN__VBC__INLINE_INTEGER_UNSIGNED;
            extension_detail = vbd;
            continue;
          }
          ret_error_message = callbacks.AppendU64(vbd);
          goto parsed_a_value;
        }
      }

      if (token.value_major() == WUFFS_CBOR__TOKEN_VALUE_MAJOR) {
        uint64_t value_minor = token.value_minor();
        if (value_minor & WUFFS_CBOR__TOKEN_VALUE_MINOR__MINUS_1_MINUS_X) {
          if (token_len == 9) {
            ret_error_message = callbacks.AppendMinus1MinusX(
                wuffs_base__peek_u64be__no_bounds_check(token_ptr + 1));
            goto parsed_a_value;
          }
        } else if (value_minor & WUFFS_CBOR__TOKEN_VALUE_MINOR__SIMPLE_VALUE) {
          ret_error_message =
              callbacks.AppendCborSimpleValue(static_cast<uint8_t>(
                  value_minor & WUFFS_CBOR__TOKEN_VALUE_MINOR__DETAIL_MASK));
          goto parsed_a_value;
        } else if (value_minor & WUFFS_CBOR__TOKEN_VALUE_MINOR__TAG) {
          if (token.continued()) {
            extension_category = EXT_CAT__CBOR_TAG;
            extension_detail =
                value_minor & WUFFS_CBOR__TOKEN_VALUE_MINOR__DETAIL_MASK;
            continue;
          }
          ret_error_message = callbacks.AppendCborTag(
              value_minor & WUFFS_CBOR__TOKEN_VALUE_MINOR__DETAIL_MASK);
          if (!ret_error_message.empty()) {
            goto done;
          }
          continue;
        }
      }

    fail:
      ret_error_message =
          "wuffs_aux::DecodeCbor: internal error: unexpected token";
      goto done;

    parsed_a_value:
      if (!ret_error_message.empty() || (depth == 0)) {
        goto done;
      }
    }
  } while (false);

done:
  DecodeCborResult result(
      std::move(ret_error_message),
      wuffs_base__u64__sat_add(io_buf->meta.pos, cursor_index));
  callbacks.Done(result, input, *io_buf);
  return result;
}

}  // namespace wuffs_aux

#endif  // !defined(WUFFS_CONFIG__MODULES) ||
        // defined(WUFFS_CONFIG__MODULE__AUX__CBOR)
