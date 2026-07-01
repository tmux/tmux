// Copyright 2019 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package rac

import (
	"hash/crc32"
	"io"

	"github.com/google/wuffs/lib/readerat"
)

func u48LE(b []byte) int64 {
	_ = b[7] // Early bounds check to guarantee safety of reads below.
	u := uint64(b[0]) | uint64(b[1])<<8 | uint64(b[2])<<16 | uint64(b[3])<<24 |
		uint64(b[4])<<32 | uint64(b[5])<<40 | uint64(b[6])<<48 | uint64(b[7])<<56
	const mask = (1 << 48) - 1
	return int64(u & mask)
}

func u64LE(b []byte) uint64 {
	_ = b[7] // Early bounds check to guarantee safety of reads below.
	return uint64(b[0]) | uint64(b[1])<<8 | uint64(b[2])<<16 | uint64(b[3])<<24 |
		uint64(b[4])<<32 | uint64(b[5])<<40 | uint64(b[6])<<48 | uint64(b[7])<<56
}

// Range is the half-open range [low, high). It is invalid for low to be
// greater than high.
type Range [2]int64

func (r Range) Empty() bool { return r[0] == r[1] }
func (r Range) Size() int64 { return r[1] - r[0] }

func (r Range) Intersect(s Range) Range {
	if r[0] < s[0] {
		r[0] = s[0]
	}
	if r[1] > s[1] {
		r[1] = s[1]
	}
	if r[0] > r[1] {
		return Range{}
	}
	return r
}

// Chunk is a compressed chunk returned by a ChunkReader.
//
// See the RAC specification for further discussion.
type Chunk struct {
	DRange     Range
	CPrimary   Range
	CSecondary Range
	CTertiary  Range
	STag       uint8
	TTag       uint8
	Codec      Codec
}

// nodeSize returns the size (in CSpace) that a node with the given arity
// occupies.
func nodeSize(arity uint8) int {
	return (16 * int(arity)) + 16
}

// rNode is the ChunkReader's representation of a node.
//
// None of its methods, other than valid, should be called unless valid returns
// true.
type rNode [4096]byte

func (b *rNode) arity() int           { return int(b[3]) }
func (b *rNode) codecHasMixBit() bool { return b[(8*int(b[3]))+7]&0x40 != 0 }
func (b *rNode) cPtrMax() int64       { return u48LE(b[(16*int(b[3]))+8:]) }
func (b *rNode) dPtrMax() int64       { return u48LE(b[8*int(b[3]):]) }
func (b *rNode) version() uint8       { return b[(16*int(b[3]))+14] }

// codec returns the 1-byte (short) or 7-byte (long) codec as a valid Codec, or
// CodecInvalid.
//
// The (uint64) Codec value returned does not have the Mix Bit (the 62nd bit)
// set, regardless of whether it's set in the rNode's bytes.
func (b *rNode) codec() Codec {
	arity := int(b[3])
	cByte := b[(8*arity)+7]

	// Look for a short codec.
	if (cByte & 0x80) == 0 {
		return Codec(cByte&0x3F) << 56
	}
	cByte &= 0x3F

	// Look for a long codec.
	const highBit, low56Bits = 0x8000000000000000, 0x00FFFFFFFFFFFFFF
	for j := uint8(0); j < 4; j++ {
		i := int(cByte | (j << 6))
		if (i < arity) && (b.tTag(i) == 0xFD) {
			base := (8 * arity) + 8
			codec := Codec(u64LE(b[(8*i)+base:]))
			return (codec & low56Bits) | highBit
		}
	}

	return CodecInvalid
}

func (b *rNode) cLen(i int) uint8 {
	base := (8 * int(b[3])) + 14
	return b[(8*i)+base]
}

func (b *rNode) cOff(i int, cBias int64) int64 {
	base := (8 * int(b[3])) + 8
	return cBias + u48LE(b[(8*i)+base:])
}

func (b *rNode) cOffRange(i int, cBias int64) Range {
	m := cBias + b.cPtrMax()
	if i >= b.arity() {
		return Range{m, m}
	}
	cOff := b.cOff(i, cBias)
	if cLen := b.cLen(i); cLen != 0 {
		if n := cOff + (int64(cLen) * 1024); m > n {
			m = n
		}
	}
	return Range{cOff, m}
}

func (b *rNode) dOff(i int, dBias int64) int64 {
	if i == 0 {
		return dBias
	}
	return dBias + u48LE(b[8*i:])
}

func (b *rNode) dOffRange(i int, dBias int64) Range {
	return Range{b.dOff(i, dBias), b.dOff(i+1, dBias)}
}

func (b *rNode) dSize(i int) int64 {
	x := int64(0)
	if i > 0 {
		x = u48LE(b[8*i:])
	}
	return u48LE(b[(8*i)+8:]) - x
}

func (b *rNode) sTag(i int) uint8 {
	base := (8 * int(b[3])) + 15
	return b[(8*i)+base]
}

func (b *rNode) tTag(i int) uint8 {
	return b[(8*i)+7]
}

func (b *rNode) isLeaf(i int) bool {
	return b[(8*i)+7] != 0xFE
}

// findChunkContaining returns the largest i < arity such that the i'th DOff is
// less than or equal to the dOff argument.
//
// The dOff argument must be non-negative and less than DOffMax.
func (b *rNode) findChunkContaining(dOff int64, dBias int64) int {
	// Define f(x) to be whether the x'th DOff is greater than dOff, so that
	// f(-1) is false and f(arity) is true. The DOff values are non-decreasing
	// so that f(x) true implies f(x+1) true.
	//
	// Binary search with the invariant that f(lo-1) is false and f(hi) is true.
	// This finds the smallest x such that f(x) is true.
	lo, hi := 0, b.arity()
	for lo < hi {
		mid := int(uint(lo+hi) >> 1) // Avoid overflow when computing mid.
		// lo ≤ mid < hi
		if b.dOff(mid, dBias) <= dOff {
			lo = mid + 1 // Preserves f(lo-1) == false.
		} else {
			hi = mid // Preserves f(hi) == true.
		}

	}
	// lo == hi, f(lo-1) == false, and f(hi) (= f(lo)) == true. Therefore lo is
	// the smallest x such that f(x) is true, so lo-1 is the largest x such
	// that f(x) is false.
	if lo <= 0 {
		panic("rac: internal error: could not find containing chunk")
	}
	return lo - 1
}

func (b *rNode) chunk(i int, cBias int64, dBias int64) Chunk {
	sTag := b.sTag(i)
	tTag := b.tTag(i)
	return Chunk{
		DRange:     b.dOffRange(i, dBias),
		CPrimary:   b.cOffRange(i, cBias),
		CSecondary: b.cOffRange(int(sTag), cBias),
		CTertiary:  b.cOffRange(int(tTag), cBias),
		STag:       sTag,
		TTag:       tTag,
		Codec:      b.codec(),
	}
}

func (b *rNode) valid() bool {
	// Check the magic and arity.
	if (b[0] != magic[0]) || (b[1] != magic[1]) || (b[2] != magic[2]) || (b[3] == 0) {
		return false
	}
	arity := int(b[3])
	size := (16 * arity) + 16
	if b[3] != b[size-1] {
		return false
	}

	// Check that the "Reserved (0)" bytes are zero and that the TTag values
	// aren't in the reserved range [0xC0, 0xFD).
	hasChildren := false
	for i := 0; i < arity; i++ {
		if b[(8*i)+6] != 0 {
			return false
		}
		if tTag := b[(8*i)+7]; (0xC0 <= tTag) && (tTag < 0xFD) {
			return false
		} else if tTag != 0xFD {
			hasChildren = true
		}
	}
	if !hasChildren || (b[(8*arity)+6] != 0) {
		return false
	}

	// Check that the DPtr values are non-decreasing. The first DPtr value is
	// implicitly zero.
	prev := int64(0)
	for i := 1; i <= arity; i++ {
		curr := u48LE(b[8*i:])
		if curr < prev {
			return false
		} else if curr != prev {
			if tTag := b[(8*i)+7]; tTag == 0xFD {
				return false
			}
		}
		prev = curr
	}

	// Check that no CPtr value exceeds CPtrMax (the final CPtr value), other
	// than 0xFD Codec Entries.
	base := (8 * arity) + 8
	cPtrMax := u48LE(b[size-8:])
	for i := 0; i < arity; i++ {
		if cPtr := u48LE(b[(8*i)+base:]); cPtr <= cPtrMax {
			continue
		}
		if tTag := b[(8*i)+7]; tTag == 0xFD {
			continue
		}
		return false
	}

	// Check the the version is non-zero.
	if b[(16*arity)+14] == 0 {
		return false
	}

	// Check the checksum.
	checksum := crc32.ChecksumIEEE(b[6:size])
	checksum ^= checksum >> 16
	if (b[4] != uint8(checksum>>0)) || (b[5] != uint8(checksum>>8)) {
		return false
	}

	// Further checking of the codec, version, COffMax and DOffMax requires
	// more context, and is done in loadAndValidate.

	return b.codec().Valid()
}

// ChunkReader parses a RAC file.
//
// Do not modify its exported fields after calling any of its methods.
type ChunkReader struct {
	// ReadSeeker is where the RAC-encoded data is read from.
	//
	// It may optionally implement io.ReaderAt, in which case its ReadAt method
	// will be preferred and its Read and Seek methods will never be called.
	// The ReadAt method is safe to use concurrently, so that multiple
	// rac.Reader's can concurrently use the same source provided that the
	// source (this field, nominally an io.ReadSeeker) implements io.ReaderAt.
	//
	// In particular, if the source is a bytes.Reader or an os.File, multiple
	// rac.ChunkReader's can work in parallel, which can speed up decoding.
	//
	// This type itself only implements io.ReadSeeker, not io.ReaderAt, as it
	// is not safe for concurrent use.
	//
	// Nil is an invalid value.
	ReadSeeker io.ReadSeeker

	// CompressedSize is the size of the RAC file in CSpace.
	//
	// Zero is an invalid value. The smallest valid RAC file is 32 bytes long.
	CompressedSize int64

	// initialized is set true after the first call on this ChunkReader.
	initialized bool

	// rootNodeArity is the root node's arity.
	rootNodeArity uint8

	// needToResolveSeekPosition is whether NextChunk will need to resolve
	// seekPosition.
	needToResolveSeekPosition bool

	// readSeeker is either the same as the ReadSeeker field value, or it is
	// that field value wrapped by a readerat.ReadSeeker to be safe to use
	// concurrently.
	readSeeker io.ReadSeeker

	// err is the first error encountered. It is sticky: once a non-nil error
	// occurs, all public methods will return that error.
	err error

	// decompressedSize is the size of the RAC file in DSpace.
	decompressedSize int64

	// rootNodeCOffset is the position of the root node in the RAC file.
	rootNodeCOffset int64

	// seekPosition gives, if needToResolveSeekPosition is true, the position
	// in DSpace that NextChunk needs to find.
	seekPosition int64

	// The i (as in "the i'th child of currNode") that denotes the next chunk
	// to be returned by NextChunk, if a non-empty chunk. If nextChunk equals
	// currNode's arity, then currNode is exhausted and calling NextChunk will
	// seek to the next node.
	nextChunk int32

	// The CBias and DBias of currNode.
	currNodeCBias int64
	currNodeDBias int64

	// currNode is the 4096 byte buffer to hold the current node.
	currNode rNode
}

func (r *ChunkReader) checkParameters() error {
	if r.ReadSeeker == nil {
		r.err = errInvalidReadSeeker
		return r.err
	}
	// The smallest valid RAC file is 32 bytes long.
	if r.CompressedSize < 32 {
		r.err = errInvalidCompressedSize
		return r.err
	}
	return nil
}

func (r *ChunkReader) initialize() error {
	if r.err != nil {
		return r.err
	}
	if r.initialized {
		return nil
	}
	r.initialized = true

	if err := r.checkParameters(); err != nil {
		return err
	}

	if ra, ok := r.ReadSeeker.(io.ReaderAt); ok {
		r.readSeeker = &readerat.ReadSeeker{
			ReaderAt: ra,
			Size:     r.CompressedSize,
		}
	} else {
		r.readSeeker = r.ReadSeeker
	}

	if err := r.findRootNode(); err != nil {
		return err
	}
	if r.currNode.version() != 1 {
		r.err = errUnsupportedRACFileVersion
		return r.err
	}
	return nil
}

func (r *ChunkReader) findRootNode() error {
	// Look at the start of the compressed file.
	if _, err := r.readSeeker.Seek(0, io.SeekStart); err != nil {
		r.err = err
		return err
	}
	if _, err := io.ReadFull(r.readSeeker, r.currNode[:4]); err != nil {
		r.err = err
		return err
	}
	if (r.currNode[0] != magic[0]) ||
		(r.currNode[1] != magic[1]) ||
		(r.currNode[2] != magic[2]) {
		r.err = errInvalidInputMissingMagicBytes
		return r.err
	}
	if found, err := r.tryRootNode(r.currNode[3], false); err != nil {
		return err
	} else if found {
		return nil
	}

	// Look at the end of the compressed file.
	if _, err := r.readSeeker.Seek(r.CompressedSize-1, io.SeekStart); err != nil {
		r.err = err
		return err
	}
	if _, err := io.ReadFull(r.readSeeker, r.currNode[:1]); err != nil {
		r.err = err
		return err
	}
	if found, err := r.tryRootNode(r.currNode[0], true); err != nil {
		return err
	} else if found {
		return nil
	}

	return errInvalidInputMissingRootNode
}

func (r *ChunkReader) tryRootNode(arity uint8, fromEnd bool) (found bool, ioErr error) {
	if arity == 0 {
		return false, nil
	}
	size := int64(nodeSize(arity))
	if r.CompressedSize < size {
		return false, nil
	}
	cOffset := int64(0)
	if fromEnd {
		cOffset = r.CompressedSize - size
	}
	if err := r.load(cOffset, arity); err != nil {
		return false, err
	}
	if !r.currNode.valid() {
		return false, nil
	}
	if r.currNode.cPtrMax() != r.CompressedSize {
		return false, nil
	}
	r.needToResolveSeekPosition = true
	r.rootNodeCOffset = cOffset
	r.rootNodeArity = arity
	r.decompressedSize = r.currNode.dPtrMax()
	return true, nil
}

// load loads a node from the RAC file into r.currNode. It does not check that
// the result is valid, and the caller should do so if it doesn't already know
// that it is valid.
func (r *ChunkReader) load(cOffset int64, arity uint8) error {
	if arity == 0 {
		r.err = errInternalInconsistentArity
		return r.err
	}
	size := nodeSize(arity)
	if _, err := r.readSeeker.Seek(cOffset, io.SeekStart); err != nil {
		r.err = err
		return err
	}
	if _, err := io.ReadFull(r.readSeeker, r.currNode[:size]); err != nil {
		r.err = err
		return err
	}
	return nil
}

func (r *ChunkReader) loadAndValidate(cOffset int64,
	parentCodec Codec, parentCodecHasMixBit bool, parentVersion uint8, parentCOffMax int64,
	childCBias int64, childDSize int64) error {

	if (cOffset < 0) || ((r.CompressedSize - 4) < cOffset) {
		r.err = errInvalidIndexNode
		return r.err
	}
	if _, err := r.readSeeker.Seek(cOffset, io.SeekStart); err != nil {
		r.err = err
		return err
	}
	if _, err := io.ReadFull(r.readSeeker, r.currNode[:4]); err != nil {
		r.err = err
		return err
	}
	arity := r.currNode[3]
	if arity == 0 {
		r.err = errInvalidIndexNode
		return r.err
	}
	size := int64(nodeSize(arity))
	if (r.CompressedSize < size) || ((r.CompressedSize - size) < cOffset) {
		r.err = errInvalidIndexNode
		return r.err
	}
	if err := r.load(cOffset, arity); err != nil {
		return err
	}

	if !r.currNode.valid() {
		r.err = errInvalidIndexNode
		return r.err
	}

	// Validate the parent and child codec, version, COffMax and DOffMax.
	childVersion := r.currNode.version()
	if !parentChildCodecsValid(parentCodec, r.currNode.codec(), parentCodecHasMixBit) ||
		(parentVersion < childVersion) ||
		(parentCOffMax < (childCBias + r.currNode.cPtrMax())) ||
		(childDSize != r.currNode.dPtrMax()) {
		r.err = errInvalidIndexNode
		return r.err
	}
	return nil
}

// DecompressedSize returns the total size of the decompressed data.
func (r *ChunkReader) DecompressedSize() (int64, error) {
	if err := r.initialize(); err != nil {
		return 0, err
	}
	return r.decompressedSize, nil
}

// SeekToChunkContaining sets up NextChunk to return the chunk containing
// dSpaceOffset. That chunk does not necessarily start at dSpaceOffset.
//
// It is an error to seek to a negative value.
func (r *ChunkReader) SeekToChunkContaining(dSpaceOffset int64) error {
	if err := r.initialize(); err != nil {
		return err
	}
	if dSpaceOffset < 0 {
		r.err = errSeekToNegativePosition
		return r.err
	}
	r.needToResolveSeekPosition = true
	r.seekPosition = dSpaceOffset
	return nil
}

// NextChunk returns the next independently compressed chunk, or io.EOF if
// there are no more chunks.
//
// Empty chunks (those that contain no decompressed data, only metadata) are
// skipped.
func (r *ChunkReader) NextChunk() (Chunk, error) {
	if err := r.initialize(); err != nil {
		return Chunk{}, err
	}
	for {
		if r.needToResolveSeekPosition {
			if r.seekPosition >= r.decompressedSize {
				return Chunk{}, io.EOF
			}
			r.needToResolveSeekPosition = false
			if err := r.resolveSeekPosition(); err != nil {
				return Chunk{}, err
			}
		}
		for n := int32(r.currNode.arity()); r.nextChunk < n; {
			c := r.currNode.chunk(int(r.nextChunk), r.currNodeCBias, r.currNodeDBias)
			r.nextChunk++
			r.seekPosition = c.DRange[1]
			if !c.DRange.Empty() {
				return c, nil
			}
		}
		r.needToResolveSeekPosition = true
	}
}

func (r *ChunkReader) resolveSeekPosition() error {
	// Load the root node. It has already been validated, during initialize.
	if err := r.load(r.rootNodeCOffset, r.rootNodeArity); err != nil {
		return err
	}

	// Walk the branch nodes until we find the leaf node containing the
	// seekPosition.
	cBias := int64(0)
	dBias := int64(0)
	for {
		i := r.currNode.findChunkContaining(r.seekPosition, dBias)
		if r.currNode.isLeaf(i) {
			r.nextChunk = int32(i)
			r.currNodeCBias = cBias
			r.currNodeDBias = dBias
			return nil
		}

		parentCodec := r.currNode.codec()
		parentCodecHasMixBit := r.currNode.codecHasMixBit()
		parentVersion := r.currNode.version()
		parentCOffMax := cBias + r.currNode.cPtrMax()
		childCOffset := r.currNode.cOff(i, cBias)
		childCBias := cBias
		if sTag := int(r.currNode.sTag(i)); sTag < r.currNode.arity() {
			childCBias = r.currNode.cOff(sTag, cBias)
		}
		childDBias := r.currNode.dOff(i, dBias)
		childDSize := r.currNode.dSize(i)

		if err := r.loadAndValidate(childCOffset,
			parentCodec, parentCodecHasMixBit, parentVersion, parentCOffMax,
			childCBias, childDSize); err != nil {
			return err
		}

		cBias = childCBias
		dBias = childDBias
	}
}
