// Copyright 2020 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ---------------- Auxiliary - Base

// Auxiliary code is discussed at
// https://github.com/google/wuffs/blob/main/doc/note/auxiliary-code.md

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__AUX__BASE)

namespace wuffs_aux {

namespace sync_io {

// --------

DynIOBuffer::DynIOBuffer(uint64_t max_incl)
    : m_buf(wuffs_base__empty_io_buffer()), m_max_incl(max_incl) {}

DynIOBuffer::~DynIOBuffer() {
  free(m_buf.data.ptr);
}

void  //
DynIOBuffer::drop() {
  free(m_buf.data.ptr);
  m_buf = wuffs_base__empty_io_buffer();
}

DynIOBuffer::GrowResult  //
DynIOBuffer::grow(uint64_t min_incl) {
  uint64_t n = round_up(min_incl, m_max_incl);
  if (n == 0) {
    return ((min_incl == 0) && (m_max_incl == 0))
               ? DynIOBuffer::GrowResult::OK
               : DynIOBuffer::GrowResult::FailedMaxInclExceeded;
  } else if (n > SIZE_MAX) {
    return DynIOBuffer::GrowResult::FailedOutOfMemory;
  } else if (n > m_buf.data.len) {
    uint8_t* ptr =
        static_cast<uint8_t*>(realloc(m_buf.data.ptr, static_cast<size_t>(n)));
    if (!ptr) {
      return DynIOBuffer::GrowResult::FailedOutOfMemory;
    }
    m_buf.data.ptr = ptr;
    m_buf.data.len = static_cast<size_t>(n);
  }
  return DynIOBuffer::GrowResult::OK;
}

// round_up rounds min_incl up, returning the smallest value x satisfying
// (min_incl <= x) and (x <= max_incl) and some other constraints. It returns 0
// if there is no such x.
//
// When max_incl <= 4096, the other constraints are:
//  - (x == max_incl)
//
// When max_incl >  4096, the other constraints are:
//  - (x == max_incl) or (x is a power of 2)
//  - (x >= 4096)
uint64_t  //
DynIOBuffer::round_up(uint64_t min_incl, uint64_t max_incl) {
  if (min_incl > max_incl) {
    return 0;
  }
  uint64_t n = 4096;
  if (n >= max_incl) {
    return max_incl;
  }
  while (n < min_incl) {
    if (n >= (max_incl / 2)) {
      return max_incl;
    }
    n *= 2;
  }
  return n;
}

// --------

Input::~Input() {}

IOBuffer*  //
Input::BringsItsOwnIOBuffer() {
  return nullptr;
}

// --------

FileInput::FileInput(FILE* f) : m_f(f) {}

std::string  //
FileInput::CopyIn(IOBuffer* dst) {
  if (!m_f) {
    return "wuffs_aux::sync_io::FileInput: nullptr file";
  } else if (!dst) {
    return "wuffs_aux::sync_io::FileInput: nullptr IOBuffer";
  } else if (dst->meta.closed) {
    return "wuffs_aux::sync_io::FileInput: end of file";
  } else {
    dst->compact();
    size_t n = fread(dst->writer_pointer(), 1, dst->writer_length(), m_f);
    dst->meta.wi += n;
    dst->meta.closed = feof(m_f);
    if (ferror(m_f)) {
      return "wuffs_aux::sync_io::FileInput: error reading file";
    }
  }
  return "";
}

// --------

MemoryInput::MemoryInput(const char* ptr, size_t len)
    : m_io(wuffs_base__ptr_u8__reader(
          static_cast<uint8_t*>(static_cast<void*>(const_cast<char*>(ptr))),
          len,
          true)) {}

MemoryInput::MemoryInput(const uint8_t* ptr, size_t len)
    : m_io(wuffs_base__ptr_u8__reader(const_cast<uint8_t*>(ptr), len, true)) {}

IOBuffer*  //
MemoryInput::BringsItsOwnIOBuffer() {
  return &m_io;
}

std::string  //
MemoryInput::CopyIn(IOBuffer* dst) {
  if (!dst) {
    return "wuffs_aux::sync_io::MemoryInput: nullptr IOBuffer";
  } else if (dst->meta.closed) {
    return "wuffs_aux::sync_io::MemoryInput: end of file";
  } else if (wuffs_base__slice_u8__overlaps(dst->data, m_io.data)) {
    // Treat m_io's data as immutable, so don't compact dst or otherwise write
    // to it.
    return "wuffs_aux::sync_io::MemoryInput: overlapping buffers";
  } else {
    dst->compact();
    size_t nd = dst->writer_length();
    size_t ns = m_io.reader_length();
    size_t n = (nd < ns) ? nd : ns;
    memcpy(dst->writer_pointer(), m_io.reader_pointer(), n);
    m_io.meta.ri += n;
    dst->meta.wi += n;
    dst->meta.closed = m_io.reader_length() == 0;
  }
  return "";
}

// --------

}  // namespace sync_io

namespace private_impl {

struct ErrorMessages {
  const char* max_incl_metadata_length_exceeded;
  const char* out_of_memory;
  const char* unexpected_end_of_file;
  const char* unsupported_metadata;
  const char* unsupported_negative_advance;

  // If adding new "const char*" typed fields to this struct, either add them
  // after existing fields or, if re-ordering fields, make sure that you update
  // all of the "const private_impl::ErrorMessages FooBarErrorMessages" values
  // in all of the sibling *.cc files.

  static inline const char* resolve(const char* s) {
    return s ? s : "wuffs_aux::private_impl: unknown error";
  };
};

std::string  //
AdvanceIOBufferTo(const ErrorMessages& error_messages,
                  sync_io::Input& input,
                  IOBuffer& io_buf,
                  uint64_t absolute_position) {
  if (absolute_position < io_buf.reader_position()) {
    return error_messages.resolve(error_messages.unsupported_negative_advance);
  }
  while (true) {
    uint64_t relative_position = absolute_position - io_buf.reader_position();
    if (relative_position <= io_buf.reader_length()) {
      io_buf.meta.ri += (size_t)relative_position;
      break;
    } else if (io_buf.meta.closed) {
      return error_messages.resolve(error_messages.unexpected_end_of_file);
    }
    io_buf.meta.ri = io_buf.meta.wi;
    if (!input.BringsItsOwnIOBuffer()) {
      io_buf.compact();
    }
    std::string error_message = input.CopyIn(&io_buf);
    if (!error_message.empty()) {
      return error_message;
    }
  }
  return "";
}

std::string  //
HandleMetadata(
    const ErrorMessages& error_messages,
    sync_io::Input& input,
    wuffs_base__io_buffer& io_buf,
    sync_io::DynIOBuffer& raw,
    wuffs_base__status (*tell_me_more_func)(void*,
                                            wuffs_base__io_buffer*,
                                            wuffs_base__more_information*,
                                            wuffs_base__io_buffer*),
    void* tell_me_more_receiver,
    std::string (*handle_metadata_func)(void*,
                                        const wuffs_base__more_information*,
                                        wuffs_base__slice_u8),
    void* handle_metadata_receiver) {
  wuffs_base__more_information minfo = wuffs_base__empty_more_information();
  // Reset raw but keep its backing array (the raw.m_buf.data slice).
  raw.m_buf.meta = wuffs_base__empty_io_buffer_meta();

  while (true) {
    minfo = wuffs_base__empty_more_information();
    wuffs_base__status status = (*tell_me_more_func)(
        tell_me_more_receiver, &raw.m_buf, &minfo, &io_buf);
    switch (minfo.flavor) {
      case 0:
      case WUFFS_BASE__MORE_INFORMATION__FLAVOR__METADATA_RAW_TRANSFORM:
      case WUFFS_BASE__MORE_INFORMATION__FLAVOR__METADATA_PARSED:
        break;

      case WUFFS_BASE__MORE_INFORMATION__FLAVOR__METADATA_RAW_PASSTHROUGH: {
        wuffs_base__range_ie_u64 r = minfo.metadata_raw_passthrough__range();
        if (r.is_empty()) {
          break;
        }
        uint64_t num_to_copy = r.length();
        if (num_to_copy > (raw.m_max_incl - raw.m_buf.meta.wi)) {
          return error_messages.resolve(
              error_messages.max_incl_metadata_length_exceeded);
        } else if (num_to_copy > (raw.m_buf.data.len - raw.m_buf.meta.wi)) {
          switch (raw.grow(num_to_copy + raw.m_buf.meta.wi)) {
            case sync_io::DynIOBuffer::GrowResult::OK:
              break;
            case sync_io::DynIOBuffer::GrowResult::FailedMaxInclExceeded:
              return error_messages.resolve(
                  error_messages.max_incl_metadata_length_exceeded);
            case sync_io::DynIOBuffer::GrowResult::FailedOutOfMemory:
              return error_messages.resolve(error_messages.out_of_memory);
          }
        }

        if (io_buf.reader_position() > r.min_incl) {
          return error_messages.resolve(error_messages.unsupported_metadata);
        } else {
          std::string error_message =
              AdvanceIOBufferTo(error_messages, input, io_buf, r.min_incl);
          if (!error_message.empty()) {
            return error_message;
          }
        }

        while (true) {
          uint64_t n =
              wuffs_base__u64__min(num_to_copy, io_buf.reader_length());
          memcpy(raw.m_buf.writer_pointer(), io_buf.reader_pointer(), n);
          raw.m_buf.meta.wi += n;
          io_buf.meta.ri += n;
          num_to_copy -= n;
          if (num_to_copy == 0) {
            break;
          } else if (io_buf.meta.closed) {
            return error_messages.resolve(
                error_messages.unexpected_end_of_file);
          } else if (!input.BringsItsOwnIOBuffer()) {
            io_buf.compact();
          }
          std::string error_message = input.CopyIn(&io_buf);
          if (!error_message.empty()) {
            return error_message;
          }
        }
        break;
      }

      default:
        return error_messages.resolve(error_messages.unsupported_metadata);
    }

    if (status.repr == nullptr) {
      break;
    } else if (status.repr != wuffs_base__suspension__even_more_information) {
      if (status.repr != wuffs_base__suspension__short_write) {
        return status.message();
      }
      switch (raw.grow(wuffs_base__u64__sat_add(raw.m_buf.data.len, 1))) {
        case sync_io::DynIOBuffer::GrowResult::OK:
          break;
        case sync_io::DynIOBuffer::GrowResult::FailedMaxInclExceeded:
          return error_messages.resolve(
              error_messages.max_incl_metadata_length_exceeded);
        case sync_io::DynIOBuffer::GrowResult::FailedOutOfMemory:
          return error_messages.resolve(error_messages.out_of_memory);
      }
    }
  }

  return (*handle_metadata_func)(handle_metadata_receiver, &minfo,
                                 raw.m_buf.reader_slice());
}

}  // namespace private_impl

}  // namespace wuffs_aux

#endif  // !defined(WUFFS_CONFIG__MODULES) ||
        // defined(WUFFS_CONFIG__MODULE__AUX__BASE)
