// Copyright 2020 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ---------------- Auxiliary - JSON

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__AUX__JSON)

#include <utility>

namespace wuffs_aux {

DecodeJsonResult::DecodeJsonResult(std::string&& error_message0,
                                   uint64_t cursor_position0)
    : error_message(std::move(error_message0)),
      cursor_position(cursor_position0) {}

DecodeJsonCallbacks::~DecodeJsonCallbacks() {}

void  //
DecodeJsonCallbacks::Done(DecodeJsonResult& result,
                          sync_io::Input& input,
                          IOBuffer& buffer) {}

const char DecodeJson_BadJsonPointer[] =  //
    "wuffs_aux::DecodeJson: bad JSON Pointer";
const char DecodeJson_NoMatch[] =  //
    "wuffs_aux::DecodeJson: no match";

DecodeJsonArgQuirks::DecodeJsonArgQuirks(const QuirkKeyValuePair* ptr0,
                                         const size_t len0)
    : ptr(ptr0), len(len0) {}

DecodeJsonArgQuirks  //
DecodeJsonArgQuirks::DefaultValue() {
  return DecodeJsonArgQuirks(nullptr, 0);
}

DecodeJsonArgJsonPointer::DecodeJsonArgJsonPointer(std::string repr0)
    : repr(repr0) {}

DecodeJsonArgJsonPointer  //
DecodeJsonArgJsonPointer::DefaultValue() {
  return DecodeJsonArgJsonPointer(std::string());
}

// --------

#define WUFFS_AUX__DECODE_JSON__GET_THE_NEXT_TOKEN                          \
  while (tok_buf.meta.ri >= tok_buf.meta.wi) {                              \
    if (tok_status.repr == nullptr) {                                       \
      goto done;                                                            \
    } else if (tok_status.repr == wuffs_base__suspension__short_write) {    \
      tok_buf.compact();                                                    \
    } else if (tok_status.repr == wuffs_base__suspension__short_read) {     \
      if (!io_error_message.empty()) {                                      \
        ret_error_message = std::move(io_error_message);                    \
        goto done;                                                          \
      } else if (cursor_index != io_buf->meta.ri) {                         \
        ret_error_message =                                                 \
            "wuffs_aux::DecodeJson: internal error: bad cursor_index";      \
        goto done;                                                          \
      } else if (io_buf->meta.closed) {                                     \
        ret_error_message =                                                 \
            "wuffs_aux::DecodeJson: internal error: io_buf is closed";      \
        goto done;                                                          \
      }                                                                     \
      io_buf->compact();                                                    \
      if (io_buf->meta.wi >= io_buf->data.len) {                            \
        ret_error_message =                                                 \
            "wuffs_aux::DecodeJson: internal error: io_buf is full";        \
        goto done;                                                          \
      }                                                                     \
      cursor_index = io_buf->meta.ri;                                       \
      io_error_message = input.CopyIn(io_buf);                              \
    } else {                                                                \
      ret_error_message = tok_status.message();                             \
      goto done;                                                            \
    }                                                                       \
    tok_status =                                                            \
        dec->decode_tokens(&tok_buf, io_buf, wuffs_base__empty_slice_u8()); \
    if ((tok_buf.meta.ri > tok_buf.meta.wi) ||                              \
        (tok_buf.meta.wi > tok_buf.data.len) ||                             \
        (io_buf->meta.ri > io_buf->meta.wi) ||                              \
        (io_buf->meta.wi > io_buf->data.len)) {                             \
      ret_error_message =                                                   \
          "wuffs_aux::DecodeJson: internal error: bad buffer indexes";      \
      goto done;                                                            \
    }                                                                       \
  }                                                                         \
  wuffs_base__token token = tok_buf.data.ptr[tok_buf.meta.ri++];            \
  uint64_t token_len = token.length();                                      \
  if ((io_buf->meta.ri < cursor_index) ||                                   \
      ((io_buf->meta.ri - cursor_index) < token_len)) {                     \
    ret_error_message =                                                     \
        "wuffs_aux::DecodeJson: internal error: bad token indexes";         \
    goto done;                                                              \
  }                                                                         \
  uint8_t* token_ptr = io_buf->data.ptr + cursor_index;                     \
  (void)(token_ptr);                                                        \
  cursor_index += static_cast<size_t>(token_len)

// --------

namespace {

// DecodeJson_SplitJsonPointer returns ("bar", 8) for ("/foo/bar/b~1z/qux", 5,
// etc). It returns a 0 size_t when s has invalid JSON Pointer syntax or i is
// out of bounds.
//
// The string returned is unescaped. If calling it again, this time with i=8,
// the "b~1z" substring would be returned as "b/z".
std::pair<std::string, size_t>  //
DecodeJson_SplitJsonPointer(std::string& s,
                            size_t i,
                            bool allow_tilde_n_tilde_r_tilde_t) {
  std::string fragment;
  if (i > s.size()) {
    return std::make_pair(std::string(), 0);
  }
  while (i < s.size()) {
    char c = s[i];
    if (c == '/') {
      break;
    } else if (c != '~') {
      fragment.push_back(c);
      i++;
      continue;
    }
    i++;
    if (i >= s.size()) {
      return std::make_pair(std::string(), 0);
    }
    c = s[i];
    if (c == '0') {
      fragment.push_back('~');
      i++;
      continue;
    } else if (c == '1') {
      fragment.push_back('/');
      i++;
      continue;
    } else if (allow_tilde_n_tilde_r_tilde_t) {
      if (c == 'n') {
        fragment.push_back('\n');
        i++;
        continue;
      } else if (c == 'r') {
        fragment.push_back('\r');
        i++;
        continue;
      } else if (c == 't') {
        fragment.push_back('\t');
        i++;
        continue;
      }
    }
    return std::make_pair(std::string(), 0);
  }
  return std::make_pair(std::move(fragment), i);
}

// --------

std::string  //
DecodeJson_WalkJsonPointerFragment(wuffs_base__token_buffer& tok_buf,
                                   wuffs_base__status& tok_status,
                                   wuffs_json__decoder::unique_ptr& dec,
                                   wuffs_base__io_buffer* io_buf,
                                   std::string& io_error_message,
                                   size_t& cursor_index,
                                   sync_io::Input& input,
                                   std::string& json_pointer_fragment) {
  std::string ret_error_message;
  while (true) {
    WUFFS_AUX__DECODE_JSON__GET_THE_NEXT_TOKEN;

    int64_t vbc = token.value_base_category();
    uint64_t vbd = token.value_base_detail();
    if (vbc == WUFFS_BASE__TOKEN__VBC__FILLER) {
      continue;
    } else if ((vbc != WUFFS_BASE__TOKEN__VBC__STRUCTURE) ||
               !(vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__PUSH)) {
      return DecodeJson_NoMatch;
    } else if (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__TO_LIST) {
      goto do_list;
    }
    goto do_dict;
  }

do_dict:
  // Alternate between these two things:
  //  1. Decode the next dict key (a string). If it matches the fragment, we're
  //    done (success). If we've reached the dict's end (VBD__STRUCTURE__POP)
  //    so that there was no next dict key, we're done (failure).
  //  2. Otherwise, skip the next dict value.
  while (true) {
    for (std::string str; true;) {
      WUFFS_AUX__DECODE_JSON__GET_THE_NEXT_TOKEN;

      int64_t vbc = token.value_base_category();
      uint64_t vbd = token.value_base_detail();
      switch (vbc) {
        case WUFFS_BASE__TOKEN__VBC__FILLER:
          continue;

        case WUFFS_BASE__TOKEN__VBC__STRUCTURE:
          if (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__PUSH) {
            goto fail;
          }
          return DecodeJson_NoMatch;

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
          break;
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
          break;
        }

        default:
          goto fail;
      }

      if (token.continued()) {
        continue;
      }
      if (str == json_pointer_fragment) {
        return "";
      }
      goto skip_the_next_dict_value;
    }

  skip_the_next_dict_value:
    for (uint32_t skip_depth = 0; true;) {
      WUFFS_AUX__DECODE_JSON__GET_THE_NEXT_TOKEN;

      int64_t vbc = token.value_base_category();
      uint64_t vbd = token.value_base_detail();
      if (token.continued() || (vbc == WUFFS_BASE__TOKEN__VBC__FILLER)) {
        continue;
      } else if (vbc == WUFFS_BASE__TOKEN__VBC__STRUCTURE) {
        if (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__PUSH) {
          skip_depth++;
          continue;
        }
        skip_depth--;
      }

      if (skip_depth == 0) {
        break;
      }
    }  // skip_the_next_dict_value
  }    // do_dict

do_list:
  do {
    wuffs_base__result_u64 result_u64 = wuffs_base__parse_number_u64(
        wuffs_base__make_slice_u8(
            static_cast<uint8_t*>(static_cast<void*>(
                const_cast<char*>(json_pointer_fragment.data()))),
            json_pointer_fragment.size()),
        WUFFS_BASE__PARSE_NUMBER_XXX__DEFAULT_OPTIONS);
    if (!result_u64.status.is_ok()) {
      return DecodeJson_NoMatch;
    }
    uint64_t remaining = result_u64.value;
    if (remaining == 0) {
      goto check_that_a_value_follows;
    }
    for (uint32_t skip_depth = 0; true;) {
      WUFFS_AUX__DECODE_JSON__GET_THE_NEXT_TOKEN;

      int64_t vbc = token.value_base_category();
      uint64_t vbd = token.value_base_detail();
      if (token.continued() || (vbc == WUFFS_BASE__TOKEN__VBC__FILLER)) {
        continue;
      } else if (vbc == WUFFS_BASE__TOKEN__VBC__STRUCTURE) {
        if (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__PUSH) {
          skip_depth++;
          continue;
        }
        if (skip_depth == 0) {
          return DecodeJson_NoMatch;
        }
        skip_depth--;
      }

      if (skip_depth > 0) {
        continue;
      }
      remaining--;
      if (remaining == 0) {
        goto check_that_a_value_follows;
      }
    }
  } while (false);  // do_list

check_that_a_value_follows:
  while (true) {
    WUFFS_AUX__DECODE_JSON__GET_THE_NEXT_TOKEN;

    int64_t vbc = token.value_base_category();
    uint64_t vbd = token.value_base_detail();
    if (vbc == WUFFS_BASE__TOKEN__VBC__FILLER) {
      continue;
    }

    // Undo the last part of WUFFS_AUX__DECODE_JSON__GET_THE_NEXT_TOKEN, so
    // that we're only peeking at the next token.
    tok_buf.meta.ri--;
    cursor_index -= static_cast<size_t>(token_len);

    if ((vbc == WUFFS_BASE__TOKEN__VBC__STRUCTURE) &&
        (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__POP)) {
      return DecodeJson_NoMatch;
    }
    return "";
  }  // check_that_a_value_follows

fail:
  return "wuffs_aux::DecodeJson: internal error: unexpected token";
done:
  return ret_error_message;
}

}  // namespace

// --------

DecodeJsonResult  //
DecodeJson(DecodeJsonCallbacks& callbacks,
           sync_io::Input& input,
           DecodeJsonArgQuirks quirks,
           DecodeJsonArgJsonPointer json_pointer) {
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
    // Prepare the low-level JSON decoder.
    wuffs_json__decoder::unique_ptr dec = wuffs_json__decoder::alloc();
    if (!dec) {
      ret_error_message = "wuffs_aux::DecodeJson: out of memory";
      goto done;
    } else if (WUFFS_JSON__DECODER_WORKBUF_LEN_MAX_INCL_WORST_CASE != 0) {
      ret_error_message =
          "wuffs_aux::DecodeJson: internal error: bad WORKBUF_LEN";
      goto done;
    }
    bool allow_tilde_n_tilde_r_tilde_t = false;
    for (size_t i = 0; i < quirks.len; i++) {
      dec->set_quirk(quirks.ptr[i].first, quirks.ptr[i].second);
      if (quirks.ptr[i].first ==
          WUFFS_JSON__QUIRK_JSON_POINTER_ALLOW_TILDE_N_TILDE_R_TILDE_T) {
        allow_tilde_n_tilde_r_tilde_t = (quirks.ptr[i].second != 0);
      }
    }

    // Prepare the wuffs_base__tok_buffer. 256 tokens is 2KiB.
    wuffs_base__token tok_array[256];
    wuffs_base__token_buffer tok_buf =
        wuffs_base__slice_token__writer(wuffs_base__make_slice_token(
            &tok_array[0], (sizeof(tok_array) / sizeof(tok_array[0]))));
    wuffs_base__status tok_status =
        dec->decode_tokens(&tok_buf, io_buf, wuffs_base__empty_slice_u8());

    // Prepare other state.
    int32_t depth = 0;
    std::string str;

    // Walk the (optional) JSON Pointer.
    for (size_t i = 0; i < json_pointer.repr.size();) {
      if (json_pointer.repr[i] != '/') {
        ret_error_message = DecodeJson_BadJsonPointer;
        goto done;
      }
      std::pair<std::string, size_t> split = DecodeJson_SplitJsonPointer(
          json_pointer.repr, i + 1, allow_tilde_n_tilde_r_tilde_t);
      i = split.second;
      if (i == 0) {
        ret_error_message = DecodeJson_BadJsonPointer;
        goto done;
      }
      ret_error_message = DecodeJson_WalkJsonPointerFragment(
          tok_buf, tok_status, dec, io_buf, io_error_message, cursor_index,
          input, split.first);
      if (!ret_error_message.empty()) {
        goto done;
      }
    }

    // Loop, doing these two things:
    //  1. Get the next token.
    //  2. Process that token.
    while (true) {
      WUFFS_AUX__DECODE_JSON__GET_THE_NEXT_TOKEN;

      int64_t vbc = token.value_base_category();
      uint64_t vbd = token.value_base_detail();
      switch (vbc) {
        case WUFFS_BASE__TOKEN__VBC__FILLER:
          continue;

        case WUFFS_BASE__TOKEN__VBC__STRUCTURE: {
          if (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__PUSH) {
            ret_error_message = callbacks.Push(static_cast<uint32_t>(vbd));
            if (!ret_error_message.empty()) {
              goto done;
            }
            depth++;
            if (depth > (int32_t)WUFFS_JSON__DECODER_DEPTH_MAX_INCL) {
              ret_error_message =
                  "wuffs_aux::DecodeJson: internal error: bad depth";
              goto done;
            }
            continue;
          }
          ret_error_message = callbacks.Pop(static_cast<uint32_t>(vbd));
          depth--;
          if (depth < 0) {
            ret_error_message =
                "wuffs_aux::DecodeJson: internal error: bad depth";
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
          ret_error_message = callbacks.AppendTextString(std::move(str));
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
          ret_error_message =
              (vbd & WUFFS_BASE__TOKEN__VBD__LITERAL__NULL)
                  ? callbacks.AppendNull()
                  : callbacks.AppendBool(vbd &
                                         WUFFS_BASE__TOKEN__VBD__LITERAL__TRUE);
          goto parsed_a_value;
        }

        case WUFFS_BASE__TOKEN__VBC__NUMBER: {
          if (vbd & WUFFS_BASE__TOKEN__VBD__NUMBER__FORMAT_TEXT) {
            if (vbd & WUFFS_BASE__TOKEN__VBD__NUMBER__CONTENT_INTEGER_SIGNED) {
              wuffs_base__result_i64 r = wuffs_base__parse_number_i64(
                  wuffs_base__make_slice_u8(token_ptr,
                                            static_cast<size_t>(token_len)),
                  WUFFS_BASE__PARSE_NUMBER_XXX__DEFAULT_OPTIONS);
              if (r.status.is_ok()) {
                ret_error_message = callbacks.AppendI64(r.value);
                goto parsed_a_value;
              }
            }
            if (vbd & WUFFS_BASE__TOKEN__VBD__NUMBER__CONTENT_FLOATING_POINT) {
              wuffs_base__result_f64 r = wuffs_base__parse_number_f64(
                  wuffs_base__make_slice_u8(token_ptr,
                                            static_cast<size_t>(token_len)),
                  WUFFS_BASE__PARSE_NUMBER_XXX__DEFAULT_OPTIONS);
              if (r.status.is_ok()) {
                ret_error_message = callbacks.AppendF64(r.value);
                goto parsed_a_value;
              }
            }
          } else if (vbd & WUFFS_BASE__TOKEN__VBD__NUMBER__CONTENT_NEG_INF) {
            ret_error_message = callbacks.AppendF64(
                wuffs_base__ieee_754_bit_representation__from_u64_to_f64(
                    0xFFF0000000000000ul));
            goto parsed_a_value;
          } else if (vbd & WUFFS_BASE__TOKEN__VBD__NUMBER__CONTENT_POS_INF) {
            ret_error_message = callbacks.AppendF64(
                wuffs_base__ieee_754_bit_representation__from_u64_to_f64(
                    0x7FF0000000000000ul));
            goto parsed_a_value;
          } else if (vbd & WUFFS_BASE__TOKEN__VBD__NUMBER__CONTENT_NEG_NAN) {
            ret_error_message = callbacks.AppendF64(
                wuffs_base__ieee_754_bit_representation__from_u64_to_f64(
                    0xFFFFFFFFFFFFFFFFul));
            goto parsed_a_value;
          } else if (vbd & WUFFS_BASE__TOKEN__VBD__NUMBER__CONTENT_POS_NAN) {
            ret_error_message = callbacks.AppendF64(
                wuffs_base__ieee_754_bit_representation__from_u64_to_f64(
                    0x7FFFFFFFFFFFFFFFul));
            goto parsed_a_value;
          }
          goto fail;
        }
      }

    fail:
      ret_error_message =
          "wuffs_aux::DecodeJson: internal error: unexpected token";
      goto done;

    parsed_a_value:
      // If an error was encountered, we are done. Otherwise, (depth == 0)
      // after parsing a value is equivalent to having decoded the entire JSON
      // value (for an empty json_pointer query) or having decoded the
      // pointed-to JSON value (for a non-empty json_pointer query). In the
      // latter case, we are also done.
      //
      // However, if quirks like WUFFS_JSON__QUIRK_ALLOW_TRAILING_FILLER or
      // WUFFS_JSON__QUIRK_EXPECT_TRAILING_NEW_LINE_OR_EOF are passed, decoding
      // the entire JSON value should also consume any trailing filler, in case
      // the DecodeJson caller wants to subsequently check that the input is
      // completely exhausted (and otherwise raise "valid JSON followed by
      // further (unexpected) data"). We aren't done yet. Instead, keep the
      // loop running until WUFFS_AUX__DECODE_JSON__GET_THE_NEXT_TOKEN's
      // decode_tokens returns an ok status.
      if (!ret_error_message.empty() ||
          ((depth == 0) && !json_pointer.repr.empty())) {
        goto done;
      }
    }
  } while (false);

done:
  DecodeJsonResult result(
      std::move(ret_error_message),
      wuffs_base__u64__sat_add(io_buf->meta.pos, cursor_index));
  callbacks.Done(result, input, *io_buf);
  return result;
}

#undef WUFFS_AUX__DECODE_JSON__GET_THE_NEXT_TOKEN

}  // namespace wuffs_aux

#endif  // !defined(WUFFS_CONFIG__MODULES) ||
        // defined(WUFFS_CONFIG__MODULE__AUX__JSON)
