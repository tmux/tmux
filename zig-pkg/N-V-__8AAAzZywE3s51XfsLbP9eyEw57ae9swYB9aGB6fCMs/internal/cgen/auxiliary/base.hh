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

#include <stdio.h>

#include <string>
#include <utility>

namespace wuffs_aux {

using IOBuffer = wuffs_base__io_buffer;

// MemOwner represents ownership of some memory. Dynamically allocated memory
// (e.g. from malloc or new) is typically paired with free or delete, invoked
// when the std::unique_ptr is destroyed. Statically allocated memory might use
// MemOwner(nullptr, &free), even if that statically allocated memory is not
// nullptr, since calling free(nullptr) is a no-op.
using MemOwner = std::unique_ptr<void, decltype(&free)>;

using QuirkKeyValuePair = std::pair<uint32_t, uint64_t>;

namespace sync_io {

// --------

// DynIOBuffer is an IOBuffer that is backed by a dynamically sized byte array.
// It owns that backing array and will free it in its destructor.
//
// The array size can be explicitly extended (by calling the grow method) but,
// unlike a C++ std::vector, there is no implicit extension (e.g. by calling
// std::vector::insert) and its maximum size is capped by the max_incl
// constructor argument.
//
// It contains an IOBuffer-typed field whose reader side provides access to
// previously written bytes and whose writer side provides access to the
// allocated but not-yet-written-to slack space. For Go programmers, this slack
// space is roughly analogous to the s[len(s):cap(s)] space of a slice s.
class DynIOBuffer {
 public:
  enum GrowResult {
    OK = 0,
    FailedMaxInclExceeded = 1,
    FailedOutOfMemory = 2,
  };

  // m_buf holds the dynamically sized byte array and its read/write indexes:
  //  - m_buf.meta.wi  is roughly analogous to a Go slice's length.
  //  - m_buf.data.len is roughly analogous to a Go slice's capacity. It is
  //    also equal to the m_buf.data.ptr malloc/realloc size.
  //
  // Users should not modify the m_buf.data.ptr or m_buf.data.len fields (as
  // they are conceptually private to this class), but they can modify the
  // bytes referenced by that pointer-length pair (e.g. compactions).
  IOBuffer m_buf;

  // m_max_incl is an inclusive upper bound on the backing array size.
  const uint64_t m_max_incl;

  // Constructor and destructor.
  explicit DynIOBuffer(uint64_t max_incl);
  ~DynIOBuffer();

  // Drop frees the byte array and resets m_buf. The DynIOBuffer can still be
  // used after a drop call. It just restarts from zero.
  void drop();

  // grow ensures that the byte array size is at least min_incl and at most
  // max_incl. It returns FailedMaxInclExceeded if that would require
  // allocating more than max_incl bytes, including the case where (min_incl >
  // max_incl). It returns FailedOutOfMemory if memory allocation failed.
  GrowResult grow(uint64_t min_incl);

 private:
  // Delete the copy and assign constructors.
  DynIOBuffer(const DynIOBuffer&) = delete;
  DynIOBuffer& operator=(const DynIOBuffer&) = delete;

  static uint64_t round_up(uint64_t min_incl, uint64_t max_incl);
};

// --------

class Input {
 public:
  virtual ~Input();

  virtual IOBuffer* BringsItsOwnIOBuffer();
  virtual std::string CopyIn(IOBuffer* dst) = 0;
};

// --------

// FileInput is an Input that reads from a file source.
//
// It does not take responsibility for closing the file when done.
class FileInput : public Input {
 public:
  FileInput(FILE* f);

  virtual std::string CopyIn(IOBuffer* dst);

 private:
  FILE* m_f;

  // Delete the copy and assign constructors.
  FileInput(const FileInput&) = delete;
  FileInput& operator=(const FileInput&) = delete;
};

// --------

// MemoryInput is an Input that reads from an in-memory source.
//
// It does not take responsibility for freeing the memory when done.
class MemoryInput : public Input {
 public:
  MemoryInput(const char* ptr, size_t len);
  MemoryInput(const uint8_t* ptr, size_t len);

  virtual IOBuffer* BringsItsOwnIOBuffer();
  virtual std::string CopyIn(IOBuffer* dst);

 private:
  IOBuffer m_io;

  // Delete the copy and assign constructors.
  MemoryInput(const MemoryInput&) = delete;
  MemoryInput& operator=(const MemoryInput&) = delete;
};

// --------

}  // namespace sync_io

}  // namespace wuffs_aux
