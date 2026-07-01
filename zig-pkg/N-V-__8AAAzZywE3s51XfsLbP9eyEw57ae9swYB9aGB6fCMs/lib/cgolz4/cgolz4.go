// Copyright 2019 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ----------------

// Package cgolz4 wraps the C "lz4" library.
//
// It speaks the LZ4 frame format, not the LZ4 block format.
package cgolz4

// TODO: dictionaries. See https://github.com/lz4/lz4/issues/791

/*
#cgo pkg-config: liblz4
#include "lz4.h"
#include "lz4frame.h"

#include <stdint.h>

#if (LZ4_VERSION_MAJOR < 1) || (LZ4_VERSION_MINOR < 8)
void LZ4F_resetDecompressionContext(LZ4F_decompressionContext_t d) {}
uint32_t cgolz4_have_lz4f_reset_decompression_context() { return 0; }
#else
uint32_t cgolz4_have_lz4f_reset_decompression_context() { return 1; }
#endif

typedef struct {
	uint32_t ndst;
	uint32_t nsrc;
	uint32_t eof;
} advances;

LZ4F_compressionContext_t cgolz4_compress_new() {
	LZ4F_compressionContext_t ret = NULL;
	if (LZ4F_isError(LZ4F_createCompressionContext(&ret, LZ4F_VERSION))) {
		return NULL;
	}
	return ret;
}

LZ4F_decompressionContext_t cgolz4_decompress_new() {
	LZ4F_decompressionContext_t ret = NULL;
	if (LZ4F_isError(LZ4F_createDecompressionContext(&ret, LZ4F_VERSION))) {
		return NULL;
	}
	return ret;
}

uint64_t cgolz4_compress_min_dst_len(uint32_t srcSize) {
	return LZ4F_compressBound(srcSize, NULL);
}

uint64_t cgolz4_compress_begin(LZ4F_compressionContext_t z,
		advances* a,
		uint8_t* dst_ptr,
		uint32_t dst_len) {
	size_t result = LZ4F_compressBegin(z, dst_ptr, dst_len, NULL);
	if (LZ4F_isError(result)) {
		a->ndst = 0;
		a->nsrc = 0;
		a->eof = 0;
		return result;
	}
	a->ndst = result;
	a->nsrc = 0;
	a->eof = 0;
	return 0;
}

uint64_t cgolz4_compress_update(LZ4F_compressionContext_t z,
		advances* a,
		uint8_t* dst_ptr,
		uint32_t dst_len,
		uint8_t* src_ptr,
		uint32_t src_len) {
	size_t result = LZ4F_compressUpdate(z, dst_ptr, dst_len, src_ptr, src_len, NULL);
	if (LZ4F_isError(result)) {
		a->ndst = 0;
		a->nsrc = 0;
		a->eof = 0;
		return result;
	}
	a->ndst = result;
	a->nsrc = src_len;
	a->eof = 0;
	return 0;
}

uint64_t cgolz4_compress_end(LZ4F_compressionContext_t z,
		advances* a,
		uint8_t* dst_ptr,
		uint32_t dst_len) {
	size_t result = LZ4F_compressEnd(z, dst_ptr, dst_len, NULL);
	if (LZ4F_isError(result)) {
		a->ndst = 0;
		a->nsrc = 0;
		a->eof = 0;
		return result;
	}
	a->ndst = result;
	a->nsrc = 0;
	a->eof = 1;
	return 0;
}

uint64_t cgolz4_decompress_update(LZ4F_decompressionContext_t z,
		advances* a,
		uint8_t* dst_ptr,
		uint32_t dst_len,
		uint8_t* src_ptr,
		uint32_t src_len) {
	size_t d = dst_len;
	size_t s = src_len;
	size_t result = LZ4F_decompress(z, dst_ptr, &d, src_ptr, &s, NULL);
	a->ndst = d;
	a->nsrc = s;
	a->eof = (result == 0) ? 1 : 0;
	return LZ4F_isError(result) ? result : 0;
}
*/
import "C"

import (
	"errors"
	"io"
	"unsafe"

	"github.com/google/wuffs/lib/compression"
)

const cgoEnabled = true

// maxLen avoids overflow concerns when converting C and Go integer types.
const maxLen = 1 << 30

// blockMaxLen is the size that Writer.Write's []byte argument is sliced into.
// LZ4 is fundamentally a block-based API, and compressing source data of size
// N requires O(N) memory. For Writer to have a fixed size buffer, we impose a
// cap on the source data size.
const blockMaxLen = 65536

var (
	errMissingResetCall           = errors.New("cgolz4: missing Reset call")
	errNilIOReader                = errors.New("cgolz4: nil io.Reader")
	errNilIOWriter                = errors.New("cgolz4: nil io.Writer")
	errNilReceiver                = errors.New("cgolz4: nil receiver")
	errOutOfMemoryVersionMismatch = errors.New("cgolz4: out of memory / version mismatch")
	errWriteBufferIsTooSmall      = errors.New("cgolz4: write buffer is too small")
)

type errCode uint64

func (e errCode) Error() string {
	if s := C.GoString(C.LZ4F_getErrorName(C.LZ4F_errorCode_t(e))); s != "" {
		return "cgolz4: " + s
	}
	return "cgolz4: unknown error"
}

// ReaderRecycler can lessen the new memory allocated when calling Reader.Reset
// on a bound Reader.
//
// It is not safe to use a ReaderRecycler and a Reader concurrently.
type ReaderRecycler struct {
	z      C.LZ4F_decompressionContext_t
	closed bool
}

// Bind lets r re-use the memory that is manually managed by c. Call c.Close to
// free that memory.
func (c *ReaderRecycler) Bind(r *Reader) {
	r.recycler = c
}

// Close implements io.Closer.
func (c *ReaderRecycler) Close() error {
	c.closed = true
	if c.z != nil {
		C.LZ4F_freeDecompressionContext(c.z)
		c.z = nil
	}
	return nil
}

// Reader decompresses from the lz4 format.
//
// The zero value is not usable until Reset is called.
type Reader struct {
	buf  [blockMaxLen]byte
	i, j uint32
	r    io.Reader

	readErr error
	lz4Err  error

	recycler *ReaderRecycler

	z C.LZ4F_decompressionContext_t
	a C.advances
}

// Reset implements compression.Reader.
func (r *Reader) Reset(reader io.Reader, dictionary []byte) error {
	if r == nil {
		return errNilReceiver
	}
	if err := r.Close(); err != nil {
		return err
	}
	if reader == nil {
		return errNilIOReader
	}
	r.r = reader
	return nil
}

// Close implements compression.Reader.
func (r *Reader) Close() error {
	if r == nil {
		return errNilReceiver
	}
	if r.r == nil {
		return nil
	}
	r.i = 0
	r.j = 0
	r.r = nil
	r.readErr = nil
	r.lz4Err = nil
	if r.z != nil {
		if (r.recycler != nil) && !r.recycler.closed && (r.recycler.z == nil) &&
			(C.cgolz4_have_lz4f_reset_decompression_context() != 0) {
			r.recycler.z, r.z = r.z, nil
		} else {
			C.LZ4F_freeDecompressionContext(r.z)
			r.z = nil
		}
	}
	return nil
}

// Read implements compression.Reader.
func (r *Reader) Read(p []byte) (int, error) {
	if r == nil {
		return 0, errNilReceiver
	}
	if r.r == nil {
		return 0, errMissingResetCall
	}

	if r.z == nil {
		if (r.recycler != nil) && !r.recycler.closed && (r.recycler.z != nil) &&
			(C.cgolz4_have_lz4f_reset_decompression_context() != 0) {
			r.z, r.recycler.z = r.recycler.z, nil
			C.LZ4F_resetDecompressionContext(r.z)
		} else {
			r.z = C.cgolz4_decompress_new()
			if r.z == nil {
				return 0, errOutOfMemoryVersionMismatch
			}
		}
	}

	if len(p) > maxLen {
		p = p[:maxLen]
	}

	for numRead := 0; ; {
		if r.lz4Err != nil {
			return numRead, r.lz4Err
		}
		if len(p) == 0 {
			return numRead, nil
		}

		if r.i >= r.j {
			if r.readErr != nil {
				return numRead, r.readErr
			}

			n, err := r.r.Read(r.buf[:])
			if err == io.EOF {
				err = io.ErrUnexpectedEOF
			}
			r.i, r.j, r.readErr = 0, uint32(n), err
			continue
		}

		e := errCode(C.cgolz4_decompress_update(r.z, &r.a,
			(*C.uint8_t)(unsafe.Pointer(&p[0])),
			(C.uint32_t)(len(p)),
			(*C.uint8_t)(unsafe.Pointer(&r.buf[r.i])),
			(C.uint32_t)(r.j-r.i),
		))

		numRead += int(r.a.ndst)
		p = p[int(r.a.ndst):]

		r.i += uint32(r.a.nsrc)

		if e == 0 {
			if r.a.eof == 0 {
				continue
			}
			r.lz4Err = io.EOF
		} else {
			r.lz4Err = e
		}
		return numRead, r.lz4Err
	}
}

// WriterRecycler can lessen the new memory allocated when calling Writer.Reset
// on a bound Writer.
//
// It is not safe to use a WriterRecycler and a Writer concurrently.
type WriterRecycler struct {
	z      C.LZ4F_compressionContext_t
	closed bool
}

// Bind lets w re-use the memory that is manually managed by c. Call c.Close to
// free that memory.
func (c *WriterRecycler) Bind(w *Writer) {
	w.recycler = c
}

// Close implements io.Closer.
func (c *WriterRecycler) Close() error {
	c.closed = true
	if c.z != nil {
		C.LZ4F_freeCompressionContext(c.z)
		c.z = nil
	}
	return nil
}

func minDstLenForBlockMaxLen() uint64 {
	return uint64(C.cgolz4_compress_min_dst_len(C.uint32_t(blockMaxLen)))
}

const writerBufLen = 2*blockMaxLen + 16

// Writer compresses to the lz4 format.
//
// Compressed bytes may be buffered and not sent to the underlying io.Writer
// until Close is called.
//
// The zero value is not usable until Reset is called.
type Writer struct {
	buf   [writerBufLen]byte
	j     uint32
	w     io.Writer
	begun bool

	writeErr error

	recycler *WriterRecycler

	z C.LZ4F_compressionContext_t
	a C.advances
}

// Reset implements compression.Writer.
func (w *Writer) Reset(writer io.Writer, dictionary []byte, level compression.Level) error {
	if w == nil {
		return errNilReceiver
	}
	w.close()
	if writer == nil {
		return errNilIOWriter
	}
	w.w = writer
	w.begun = false
	return nil
}

// Close implements compression.Writer.
func (w *Writer) Close() error {
	if w == nil {
		return errNilReceiver
	}
	err := w.flush(true)
	w.close()
	return err
}

func (w *Writer) flush(final bool) error {
	if w.w == nil {
		return nil
	}

	if final {
		if err := w.write(nil, true); err != nil {
			return err
		}
	}

	if w.j == 0 {
		return nil
	}
	_, err := w.w.Write(w.buf[:w.j])
	w.j = 0
	return err
}

func (w *Writer) close() {
	if w.w == nil {
		return
	}
	w.j = 0
	w.w = nil
	w.writeErr = nil
	if w.z != nil {
		if (w.recycler != nil) && !w.recycler.closed && (w.recycler.z == nil) {
			w.recycler.z, w.z = w.z, nil
		} else {
			C.LZ4F_freeCompressionContext(w.z)
			w.z = nil
		}
	}
}

// Write implements compression.Writer.
func (w *Writer) Write(p []byte) (int, error) {
	if w == nil {
		return 0, errNilReceiver
	}
	if w.w == nil {
		return 0, errMissingResetCall
	}
	if w.writeErr != nil {
		return 0, w.writeErr
	}

	originalLenP := len(p)
	for {
		remaining := []byte(nil)
		if blockMaxLen > maxLen {
			panic("unreachable")
		} else if len(p) > blockMaxLen {
			p, remaining = p[:blockMaxLen], p[blockMaxLen:]
		}

		if err := w.write(p, false); err != nil {
			return 0, err
		}

		p, remaining = remaining, nil
		if len(p) == 0 {
			return originalLenP, nil
		}
	}
}

func (w *Writer) write(p []byte, final bool) error {
	if n := len(p); (n > blockMaxLen) || (n > maxLen) {
		panic("unreachable")
	}

	if w.z == nil {
		if (w.recycler != nil) && !w.recycler.closed && (w.recycler.z != nil) {
			w.z, w.recycler.z = w.recycler.z, nil
		} else {
			w.z = C.cgolz4_compress_new()
			if w.z == nil {
				return errOutOfMemoryVersionMismatch
			}
		}
	}

	if !w.begun {
		w.begun = true
		e := errCode(C.cgolz4_compress_begin(w.z, &w.a,
			(*C.uint8_t)(unsafe.Pointer(&w.buf[w.j])),
			(C.uint32_t)(uint32(len(w.buf))-w.j),
		))

		w.j += uint32(w.a.ndst)
		p = p[uint32(w.a.nsrc):]

		if e != 0 {
			w.writeErr = e
			return w.writeErr
		}
	}

	for (len(p) > 0) || final {
		minDstLen := uint64(C.cgolz4_compress_min_dst_len(C.uint32_t(len(p))))
		for (uint64(len(w.buf)) - uint64(w.j)) < minDstLen {
			if w.j == 0 {
				w.writeErr = errWriteBufferIsTooSmall
				return w.writeErr
			}
			if err := w.flush(false); err != nil {
				w.writeErr = err
				return w.writeErr
			}
		}

		e := errCode(0)
		if final {
			e = errCode(C.cgolz4_compress_end(w.z, &w.a,
				(*C.uint8_t)(unsafe.Pointer(&w.buf[w.j])),
				(C.uint32_t)(uint32(len(w.buf))-w.j),
			))
			final = w.a.eof == 0
		} else {
			e = errCode(C.cgolz4_compress_update(w.z, &w.a,
				(*C.uint8_t)(unsafe.Pointer(&w.buf[w.j])),
				(C.uint32_t)(uint32(len(w.buf))-w.j),
				(*C.uint8_t)(unsafe.Pointer(&p[0])),
				(C.uint32_t)(len(p)),
			))
		}

		w.j += uint32(w.a.ndst)
		p = p[uint32(w.a.nsrc):]

		if e != 0 {
			w.writeErr = e
			return w.writeErr
		}
	}
	return nil
}
