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

// Package zlibcut produces zlib-formatted data subject to a maximum compressed
// size.
//
// The typical compression problem is to encode all of the given source data in
// some number of bytes. This package's problem is finding a reasonably long
// prefix of the source data that encodes in up to a given number of bytes.
package zlibcut

import (
	"errors"
	"hash/adler32"
	"io"

	"github.com/google/wuffs/lib/flatecut"
)

var (
	errMaxEncodedLenTooSmall            = errors.New("zlibcut: maxEncodedLen is too small")
	errUnsupportedZlibCompressionMethod = errors.New("zlibcut: unsupported zlib compression method")

	errInternalInconsistentDecodedLen = errors.New("zlibcut: internal: inconsistent decodedLen")

	errInvalidBadHeader     = errors.New("zlibcut: invalid input: bad header")
	errInvalidNotEnoughData = errors.New("zlibcut: invalid input: not enough data")
)

const (
	// SmallestValidMaxEncodedLen is the length in bytes of the smallest valid
	// zlib-encoded data.
	SmallestValidMaxEncodedLen = 8
)

// Cut modifies encoded's contents such that encoded[:encodedLen] is valid
// zlib-compressed data, assuming that encoded starts off containing valid
// zlib-compressed data.
//
// If a nil error is returned, then encodedLen <= maxEncodedLen will hold.
//
// Decompressing that modified, shorter byte slice produces a prefix (of length
// decodedLen) of the decompression of the original, longer byte slice.
//
// If w is non-nil, that prefix is also written to w. If a non-nil error is
// returned, incomplete data might still be written to w.
//
// It does not necessarily return the largest possible decodedLen.
func Cut(w io.Writer, encoded []byte, maxEncodedLen int) (encodedLen int, decodedLen int, retErr error) {
	if len(encoded) < 2 {
		return 0, 0, errInvalidNotEnoughData
	}
	header := uint32(encoded[0])<<8 | uint32(encoded[1])
	if header%31 != 0 {
		return 0, 0, errInvalidBadHeader
	}
	if (encoded[0] & 0x0F) != 0x08 {
		return 0, 0, errUnsupportedZlibCompressionMethod
	}

	payloadStart := 2
	if haveDict := (encoded[1] & 0x20) != 0; haveDict {
		if len(encoded) < 6 {
			return 0, 0, errInvalidNotEnoughData
		}
		payloadStart = 6
	}

	// Check that there's space for the trailing Adler-32 checksum.
	if len(encoded) < (payloadStart + 4) {
		return 0, 0, errInvalidNotEnoughData
	}

	if maxEncodedLen < (payloadStart + 4) {
		return 0, 0, errMaxEncodedLenTooSmall
	}

	hasher := adler32.New()
	if w == nil {
		w = hasher
	} else {
		w = io.MultiWriter(w, hasher)
	}

	encodedLen, decodedLen, err := flatecut.Cut(
		w,
		encoded[payloadStart:len(encoded)-4],
		maxEncodedLen-payloadStart-4,
	)
	if err != nil {
		return 0, 0, err
	}

	hash := hasher.Sum32()
	hashBytes := encoded[payloadStart+encodedLen : payloadStart+encodedLen+4]
	hashBytes[0] = uint8(hash >> 24)
	hashBytes[1] = uint8(hash >> 16)
	hashBytes[2] = uint8(hash >> 8)
	hashBytes[3] = uint8(hash >> 0)

	return payloadStart + encodedLen + 4, decodedLen, nil
}
