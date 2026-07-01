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

// Package cgozlib wraps the C "zlib" library.
//
// Unlike some other wrappers, this one supports dictionaries.
package cgozlib

/*
#cgo pkg-config: zlib
#include "zlib.h"

typedef struct {
	uInt ndst;
	uInt nsrc;
} advances;

int cgozlib_inflateInit(z_stream* z) {
	return inflateInit(z);
}

int cgozlib_inflateSetDictionary(z_stream* z,
		Bytef* dict_ptr,
		uInt dict_len) {
	return inflateSetDictionary(z, dict_ptr, dict_len);
}

int cgozlib_inflate(z_stream* z,
		advances* a,
		Bytef* next_out,
		uInt avail_out,
		Bytef* next_in,
		uInt avail_in) {
	z->next_out = next_out;
	z->avail_out = avail_out;
	z->next_in = next_in;
	z->avail_in = avail_in;

	int ret = inflate(z, Z_NO_FLUSH);

	a->ndst = avail_out - z->avail_out;
	a->nsrc = avail_in - z->avail_in;

	z->next_out = NULL;
	z->avail_out = 0;
	z->next_in = NULL;
	z->avail_in = 0;

	return ret;
}

int cgozlib_inflateEnd(z_stream* z) {
	return inflateEnd(z);
}
*/
import "C"

import (
	"errors"
	"io"
	"unsafe"
)

const cgoEnabled = true

var (
	errMissingResetCall = errors.New("cgozlib: missing Reset call")
	errNilIOReader      = errors.New("cgozlib: nil io.Reader")
	errNilReceiver      = errors.New("cgozlib: nil receiver")
)

const (
	errCodeStreamEnd = 1
	errCodeNeedDict  = 2
)

type errCode int32

func (e errCode) Error() string {
	switch e {
	case +1:
		return "cgozlib: Z_STREAM_END"
	case +2:
		return "cgozlib: Z_NEED_DICT"
	case -1:
		return "cgozlib: Z_ERRNO"
	case -2:
		return "cgozlib: Z_STREAM_ERROR"
	case -3:
		return "cgozlib: Z_DATA_ERROR"
	case -4:
		return "cgozlib: Z_MEM_ERROR"
	case -5:
		return "cgozlib: Z_BUF_ERROR"
	case -6:
		return "cgozlib: Z_VERSION_ERROR"
	}
	return "cgozlib: unknown zlib error"
}

// Reader is both a zlib.Resetter and an io.ReadCloser. Call Reset before
// calling Read.
//
// It is analogous to the value returned by zlib.NewReader in the Go standard
// library.
type Reader struct {
	buf  [4096]byte
	i, j uint32
	r    io.Reader
	dict []byte

	readErr error
	zlibErr error

	z C.z_stream
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

	if e := C.cgozlib_inflateInit(&r.z); e != 0 {
		return errCode(e)
	}

	r.r = reader
	r.dict = dictionary
	if n := len(r.dict); n > 32768 {
		r.dict = r.dict[n-32768:]
	}
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
	r.dict = nil
	r.readErr = nil
	r.zlibErr = nil
	if e := C.cgozlib_inflateEnd(&r.z); e != 0 {
		return errCode(e)
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

	const maxLen = 1 << 30
	if len(p) > maxLen {
		p = p[:maxLen]
	}

	for numRead := 0; ; {
		if r.zlibErr != nil {
			return numRead, r.zlibErr
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

		e := C.cgozlib_inflate(&r.z, &r.a,
			(*C.Bytef)(unsafe.Pointer(&p[0])),
			(C.uInt)(len(p)),
			(*C.Bytef)(unsafe.Pointer(&r.buf[r.i])),
			(C.uInt)(r.j-r.i),
		)

		numRead += int(r.a.ndst)
		p = p[int(r.a.ndst):]

		r.i += uint32(r.a.nsrc)

		if e == 0 {
			continue
		} else if e == errCodeStreamEnd {
			r.zlibErr = io.EOF
		} else if (e == errCodeNeedDict) && (len(r.dict) > 0) {
			e = C.cgozlib_inflateSetDictionary(&r.z,
				(*C.Bytef)(unsafe.Pointer(&r.dict[0])),
				(C.uInt)(len(r.dict)),
			)
			if e == 0 {
				continue
			}
			r.zlibErr = errCode(e)
		} else {
			r.zlibErr = errCode(e)
		}
		return numRead, r.zlibErr
	}
}
