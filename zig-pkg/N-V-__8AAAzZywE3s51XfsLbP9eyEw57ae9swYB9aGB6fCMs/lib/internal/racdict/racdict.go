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

// Package racdict provides support for the RAC (Random Access Compression)
// common dictionary format.
//
// See
// https://github.com/google/wuffs/blob/main/doc/spec/rac-spec.md#common-dictionary-format
package racdict

import (
	"errors"
	"hash/crc32"
	"io"

	"github.com/google/wuffs/lib/rac"
)

const (
	// MaxInclLength is the maximum (inclusive) length of a dictionary, not
	// including the length prefix and the checksum suffix.
	//
	// Its value is (1 GiB - 1), or 1_073_741_823 bytes.
	MaxInclLength = (1 << 30) - 1
)

var (
	errDictionaryIsTooLong = errors.New("racdict: dictionary is too long")
	errInvalidDictionary   = errors.New("racdict: invalid dictionary")
)

func u32LE(b []byte) uint32 {
	_ = b[3] // Early bounds check to guarantee safety of reads below.
	return (uint32(b[0]) << 0) |
		(uint32(b[1]) << 8) |
		(uint32(b[2]) << 16) |
		(uint32(b[3]) << 24)
}

// Loader loads a dictionary wrapped in the RAC common dictionary format.
//
// It can cache previously loaded values, keyed by a rac.Chunk.
type Loader struct {
	cachedBytes []byte
	cachedRange rac.Range
	buf         [4]byte
}

// Load reads and unwraps a wrapped dictionary.
func (r *Loader) Load(rs io.ReadSeeker, chunk rac.Chunk) (dictionary []byte, retErr error) {
	if !chunk.CTertiary.Empty() {
		return nil, errInvalidDictionary
	}
	if chunk.CSecondary.Empty() {
		return nil, nil
	}
	cRange := chunk.CSecondary

	// Load from the MRU cache, if it was loaded from the same cRange.
	if (cRange == r.cachedRange) && !cRange.Empty() {
		return r.cachedBytes, nil
	}

	// Check the cRange size and the tTag.
	if (cRange.Size() < 8) || (chunk.TTag != 0xFF) {
		return nil, errInvalidDictionary
	}

	// Read the dictionary size.
	if _, err := rs.Seek(cRange[0], io.SeekStart); err != nil {
		return nil, err
	}
	if _, err := io.ReadFull(rs, r.buf[:4]); err != nil {
		return nil, err
	}

	// Check that the high 2 bits are zero (reserved). In other words, we
	// enforce that the maximum (inclusive) length is MaxInclLength.
	dictSize := int64(u32LE(r.buf[:4]))
	if (dictSize >> 30) != 0 {
		return nil, errInvalidDictionary
	}

	// Check the overall size. The +8 is for the 4 byte prefix (dictionary
	// size) and the 4 byte suffix (checksum).
	if (dictSize + 8) > cRange.Size() {
		return nil, errInvalidDictionary
	}

	// Allocate or re-use the cachedBytes buffer.
	buffer := []byte(nil)
	if n := dictSize + 4; int64(cap(r.cachedBytes)) >= n {
		buffer = r.cachedBytes[:n]
		// Invalidate the cached dictionary, as we are re-using its memory.
		r.cachedRange = rac.Range{}
	} else {
		buffer = make([]byte, n)
	}

	// Read the dictionary and checksum.
	if _, err := io.ReadFull(rs, buffer); err != nil {
		return nil, err
	}

	// Verify the checksum.
	dict, checksum := buffer[:dictSize], buffer[dictSize:]
	if u32LE(checksum) != crc32.ChecksumIEEE(dict) {
		return nil, errInvalidDictionary
	}

	// Save to the MRU cache and return.
	r.cachedBytes = dict
	r.cachedRange = cRange
	return dict, nil
}

// Saver saves a dictionary wrapped in the RAC common dictionary format.
type Saver struct {
	stash []byte
}

// WrapResource wraps a dictionary.
func (w *Saver) WrapResource(
	raw []byte,
	refineResourceData func([]byte) []byte,
) ([]byte, error) {
	refined := refineResourceData(raw)
	if len(refined) > MaxInclLength {
		return nil, errDictionaryIsTooLong
	}
	wrapped := make([]byte, len(refined)+8)
	wrapped[0] = uint8(len(refined) >> 0)
	wrapped[1] = uint8(len(refined) >> 8)
	wrapped[2] = uint8(len(refined) >> 16)
	wrapped[3] = uint8(len(refined) >> 24)
	copy(wrapped[4:], refined)
	checksum := crc32.ChecksumIEEE(refined)
	wrapped[len(wrapped)-4] = uint8(checksum >> 0)
	wrapped[len(wrapped)-3] = uint8(checksum >> 8)
	wrapped[len(wrapped)-2] = uint8(checksum >> 16)
	wrapped[len(wrapped)-1] = uint8(checksum >> 24)
	return wrapped, nil
}

// Compress helps implement rac.CodecWriter.
func (w *Saver) Compress(
	p []byte,
	q []byte,
	resourcesData [][]byte,
	codec rac.Codec,
	compress func(p []byte, q []byte, dict []byte) ([]byte, error),
	refineResourceData func([]byte) []byte,
) (
	retCodec rac.Codec,
	compressed []byte,
	secondaryResource int,
	tertiaryResource int,
	retErr error,
) {
	secondaryResource = rac.NoResourceUsed

	// Compress p+q without any shared dictionary.
	baseline, err := compress(p, q, nil)
	if err != nil {
		return 0, nil, 0, 0, err
	}
	// If there aren't any potential shared dictionaries, or if the baseline
	// compressed form is small, just return the baseline.
	const minBaselineSizeToConsiderDictionaries = 256
	if (len(resourcesData) == 0) || (len(baseline) < minBaselineSizeToConsiderDictionaries) {
		return codec, baseline, secondaryResource, rac.NoResourceUsed, nil
	}

	// w.stash keeps a copy of the best compression so far. Every call to
	// compress can clobber the bytes previously returned by compress.
	w.stash = append(w.stash[:0], baseline...)
	compressed = w.stash

	// Only use a shared dictionary if it results in something smaller than
	// 98.4375% (an arbitrary heuristic threshold) of the baseline. Otherwise,
	// it's arguably not worth the additional complexity.
	threshold := (len(baseline) / 64) * 63

	for i, resourceData := range resourcesData {
		refined := refineResourceData(resourceData)
		if len(refined) > MaxInclLength {
			return 0, nil, 0, 0, errDictionaryIsTooLong
		}

		candidate, err := compress(p, q, refined)
		if err != nil {
			return 0, nil, 0, 0, err
		}
		if n := len(candidate); (n >= threshold) || (n >= len(compressed)) {
			continue
		}
		secondaryResource = i

		// As an optimization, if the final candidate is the winner, we don't
		// have to copy its contents to w.stash.
		if i == len(resourcesData)-1 {
			compressed = candidate
		} else {
			w.stash = append(w.stash[:0], candidate...)
			compressed = w.stash
		}
	}
	return codec, compressed, secondaryResource, rac.NoResourceUsed, nil
}
