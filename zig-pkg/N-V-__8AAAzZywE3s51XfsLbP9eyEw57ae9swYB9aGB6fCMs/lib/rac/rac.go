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

// Package rac provides access to RAC (Random Access Compression) files.
//
// RAC is just a container format, and this package ("rac") is a relatively
// low-level. Users will typically want to process a particular compression
// format wrapped in RAC, such as (RAC + Zlib). For that, look at e.g. the
// sibling "raczlib" package.
//
// The RAC specification is at
// https://github.com/google/wuffs/blob/main/doc/spec/rac-spec.md
package rac

import (
	"errors"
)

const (
	// MaxSize is the maximum RAC file size (in both CSpace and DSpace).
	MaxSize = (1 << 48) - 1

	// invalidCOffsetCLength is ((1 << 64) - 1).
	invalidCOffsetCLength = 0xFFFFFFFFFFFFFFFF
)

// Codec is the compression codec for the RAC file.
//
// For leaf nodes, there are two categories of valid Codecs: Short and Long. A
// Short Codec's uint64 value's high 2 bits and low 56 bits must be zero. A
// Long Codec's uint64 value's high 8 bits must be one and then 7 zeroes.
// Symbolically, Short and Long match:
//   - 0b00??????_00000000_00000000_00000000_00000000_00000000_00000000_00000000
//   - 0b10000000_????????_????????_????????_????????_????????_????????_????????
//
// In terms of the RAC file format, a Short Codec fits in a single byte: the
// Codec Byte in the middle of a branch node. A Long Codec uses that Codec Byte
// to locate 7 other bytes, a location which would otherwise form a "CPtr|CLen"
// value. When converting from the 7 bytes in the wire format to this Go type
// (a uint64 value), they are read little-endian: the "CPtr" bytes are the low
// 6 bytes, the "CLen" is the second-highest byte and the highest byte is
// hard-coded to 0x80.
//
// For example, the 7 bytes "mdo2\x00\x00\x00" would correspond to a Codec
// value of 0x8000_0000_326F_646D.
//
// The Mix Bit is not part of the uint64 representation. Neither is a Long
// Codec's 'c64' index. This package's exported API deals with leaf nodes.
// Branch nodes' wire formats are internal implementation details.
//
// See the RAC specification for further discussion.
type Codec uint64

func (c Codec) isLong() bool  { return int64(c) < 0 }
func (c Codec) isShort() bool { return int64(c) >= 0 }

// Valid returns whether c matches the Short or Long pattern.
func (c Codec) Valid() bool {
	if (c >> 63) == 0 {
		return ((c << 8) == 0) && ((c >> 62) == 0)
	}
	return (c >> 56) == 0x80
}

func (c Codec) name() string {
	if c.isShort() && c.Valid() {
		switch c >> 56 {
		case 0:
			return "Zeroes"
		case 1:
			return "Zlib"
		case 2:
			return "LZ4"
		case 3:
			return "Zstandard"
		}
	}
	return ""
}

func parentChildCodecsValid(parent Codec, child Codec, parentHasMixBit bool) bool {
	return (parent == child) || parentHasMixBit
}

const (
	CodecZeroes    = Codec(0x00 << 56)
	CodecZlib      = Codec(0x01 << 56)
	CodecLZ4       = Codec(0x02 << 56)
	CodecZstandard = Codec(0x03 << 56)

	codecMixBit     = Codec(1 << 62)
	codecLongZeroes = Codec(1 << 63)
	CodecInvalid    = Codec((1 << 64) - 1)
)

// IndexLocation is whether the index is at the start or end of the RAC file.
//
// See the RAC specification for further discussion.
type IndexLocation uint8

const magic = "\x72\xC3\x63"

const (
	IndexLocationAtEnd   = IndexLocation(0)
	IndexLocationAtStart = IndexLocation(1)
)

var indexLocationAtEndMagic = []byte("\x72\xC3\x63\x00")

var (
	ErrCodecWriterDoesNotSupportCChunkSize = errors.New("rac: CodecWriter does not support CChunkSize")

	errAlreadyClosed                 = errors.New("rac: already closed")
	errCChunkSizeIsTooSmall          = errors.New("rac: CChunkSize is too small")
	errILAEndTempFile                = errors.New("rac: IndexLocationAtEnd requires a nil TempFile")
	errILAStartTempFile              = errors.New("rac: IndexLocationAtStart requires a non-nil TempFile")
	errInconsistentCompressedSize    = errors.New("rac: inconsistent compressed size")
	errInvalidCPageSize              = errors.New("rac: invalid CPageSize")
	errInvalidChunk                  = errors.New("rac: invalid chunk")
	errInvalidChunkTooLarge          = errors.New("rac: invalid chunk (too large)")
	errInvalidChunkTruncated         = errors.New("rac: invalid chunk (truncated)")
	errInvalidCodec                  = errors.New("rac: invalid Codec")
	errInvalidCodecWriter            = errors.New("rac: invalid CodecWriter")
	errInvalidCompressedSize         = errors.New("rac: invalid CompressedSize")
	errInvalidIndexNode              = errors.New("rac: invalid index node")
	errInvalidInputMissingMagicBytes = errors.New("rac: invalid input: missing magic bytes")
	errInvalidInputMissingRootNode   = errors.New("rac: invalid input: missing root node")
	errInvalidReadSeeker             = errors.New("rac: invalid ReadSeeker")
	errInvalidWriter                 = errors.New("rac: invalid Writer")
	errSeekToInvalidWhence           = errors.New("rac: seek to invalid whence")
	errSeekToNegativePosition        = errors.New("rac: seek to negative position")
	errSeekToNegativeRange           = errors.New("rac: seek to negative range")
	errTooManyChunks                 = errors.New("rac: too many chunks")
	errTooManyResources              = errors.New("rac: too many resources")
	errTooMuchInput                  = errors.New("rac: too much input")
	errUnsupportedRACFileVersion     = errors.New("rac: unsupported RAC file version")

	errInternalArityIsTooLarge      = errors.New("rac: internal error: arity is too large")
	errInternalEmptyDRange          = errors.New("rac: internal error: empty DRange")
	errInternalInconsistentArity    = errors.New("rac: internal error: inconsistent arity")
	errInternalInconsistentPosition = errors.New("rac: internal error: inconsistent position")
	errInternalShortCSize           = errors.New("rac: internal error: short CSize")
)
