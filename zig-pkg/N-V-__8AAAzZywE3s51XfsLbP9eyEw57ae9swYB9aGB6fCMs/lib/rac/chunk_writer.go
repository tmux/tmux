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
	"errors"
	"hash/crc32"
	"io"
	"sort"
)

func btoi(b bool) int {
	if b {
		return 1
	}
	return 0
}

func isZeroOrAPowerOf2(x uint64) bool {
	return (x & (x - 1)) == 0
}

func putU64LE(b []byte, v uint64) {
	_ = b[7] // Early bounds check to guarantee safety of writes below.
	b[0] = byte(v)
	b[1] = byte(v >> 8)
	b[2] = byte(v >> 16)
	b[3] = byte(v >> 24)
	b[4] = byte(v >> 32)
	b[5] = byte(v >> 40)
	b[6] = byte(v >> 48)
	b[7] = byte(v >> 56)
}

// OptResource is an option type, optionally holding a ChunkWriter-specific
// identifier for a shared resource.
//
// Zero means that the option is not taken: no shared resource is used.
type OptResource uint32

// ChunkWriter provides a relatively simple way to write a RAC file - one that
// is created starting from nothing, as opposed to incrementally modifying an
// existing RAC file.
//
// Other packages may provide a more flexible (and more complicated) way to
// write or append to RAC files, but that is out of scope of this package.
//
// Do not modify its exported fields after calling any of its methods.
type ChunkWriter struct {
	// Writer is where the RAC-encoded data is written to.
	//
	// Nil is an invalid value.
	Writer io.Writer

	// IndexLocation is whether the index is at the start or end of the RAC
	// file.
	//
	// See the RAC specification for further discussion.
	//
	// The zero value of this field is IndexLocationAtEnd: a one pass encoding.
	IndexLocation IndexLocation

	// TempFile is a temporary file to use for a two pass encoding. The field
	// name says "file" but it need not be a real file, in the operating system
	// sense.
	//
	// For IndexLocationAtEnd, this must be nil. For IndexLocationAtStart, this
	// must be non-nil.
	//
	// It does not need to implement io.Seeker, if it supports separate read
	// and write positions (e.g. it is a bytes.Buffer). If it does implement
	// io.Seeker (e.g. it is an os.File), its current position will be noted
	// (via SeekCurrent), then it will be written to (via the
	// ChunkWriter.AddXxx methods), then its position will be reset (via
	// SeekSet), then it will be read from (via the ChunkWriter.Close method).
	//
	// The ChunkWriter does not call TempFile.Close even if that method exists
	// (e.g. the TempFile is an os.File). In that case, closing the temporary
	// file (and deleting it) after the ChunkWriter is closed is the
	// responsibility of the ChunkWriter caller, not the ChunkWriter itself.
	TempFile io.ReadWriter

	// CPageSize guides padding the output to minimize the number of pages that
	// each chunk occupies (in what the RAC spec calls CSpace).
	//
	// It must either be zero (which means no padding is inserted) or a power
	// of 2, and no larger than MaxSize.
	//
	// For example, suppose that CSpace is partitioned into 1024-byte pages,
	// that 1000 bytes have already been written to the output, and the next
	// chunk is 1500 bytes long.
	//
	//   - With no padding (i.e. with CPageSize set to 0), this chunk will
	//     occupy the half-open range [1000, 2500) in CSpace, which straddles
	//     three 1024-byte pages: the page [0, 1024), the page [1024, 2048) and
	//     the page [2048, 3072). Call those pages p0, p1 and p2.
	//
	//   - With padding (i.e. with CPageSize set to 1024), 24 bytes of zeroes
	//     are first inserted so that this chunk occupies the half-open range
	//     [1024, 2524) in CSpace, which straddles only two pages (p1 and p2).
	//
	// This concept is similar, but not identical, to alignment. Even with a
	// non-zero CPageSize, chunk start offsets are not necessarily aligned to
	// page boundaries. For example, suppose that the chunk size was only 1040
	// bytes, not 1500 bytes. No padding will be inserted, as both [1000, 2040)
	// and [1024, 2064) straddle two pages: either pages p0 and p1, or pages p1
	// and p2.
	//
	// Nonetheless, if all chunks (or all but the final chunk) have a size
	// equal to (or just under) a multiple of the page size, then in practice,
	// each chunk's starting offset will be aligned to a page boundary.
	CPageSize uint64

	// initialized is set true after the first AddXxx call.
	initialized bool

	// codec is the compression codec for the RAC file.
	codec Codec

	// err is the first error encountered. It is sticky: once a non-nil error
	// occurs, all public methods will return that error.
	err error

	// tempFileSeekStart is the TempFile's starting position, if the TempFile
	// is an io.Seeker.
	tempFileSeekStart int64

	// dataSize is the total size (so far) of the data portion (as opposed to
	// the index portion) of the compressed file. For a two pass encoding, this
	// should equal the size of the TempFile.
	dataSize uint64

	// dFileSize is the total size (so far) of the decompressed file. It is the
	// sum of all the dRangeSize arguments passed to AddChunk.
	dFileSize uint64

	// resourcesCOffCLens is the COffset and CLength values for each shared
	// resource. Those values are for the compressed file (what the RAC spec
	// calls CSpace).
	//
	// The first element (if it exists) is an unused placeholder, as a zero
	// OptResource means unused.
	resourcesCOffCLens []uint64

	// leafNodes are the non-resource leaf nodes of the hierarchical index.
	leafNodes []wNode

	// log2CPageSize is the base-2 logarithm of CPageSize, or zero if CPageSize
	// is zero.
	log2CPageSize uint32

	// padding is a slice of zeroes to pad output to CPageSize boundaries.
	padding []byte
}

func (w *ChunkWriter) checkParameters() error {
	if w.Writer == nil {
		w.err = errInvalidWriter
		return w.err
	}
	if !isZeroOrAPowerOf2(w.CPageSize) || (w.CPageSize > MaxSize) {
		w.err = errInvalidCPageSize
		return w.err
	} else if w.CPageSize > 0 {
		for w.log2CPageSize = 1; w.CPageSize != (1 << w.log2CPageSize); w.log2CPageSize++ {
		}
	}
	return nil
}

func (w *ChunkWriter) initialize() error {
	if w.err != nil {
		return w.err
	}
	if w.initialized {
		return nil
	}
	w.initialized = true

	if err := w.checkParameters(); err != nil {
		return err
	}

	if s, ok := w.TempFile.(io.Seeker); ok {
		if n, err := s.Seek(0, io.SeekCurrent); err != nil {
			w.err = err
			return err
		} else {
			w.tempFileSeekStart = n
		}
	}

	switch w.IndexLocation {
	case IndexLocationAtEnd:
		if w.TempFile != nil {
			w.err = errILAEndTempFile
			return w.err
		}
		return w.write(indexLocationAtEndMagic)

	case IndexLocationAtStart:
		if w.TempFile == nil {
			w.err = errILAStartTempFile
			return w.err
		}
	}
	return nil
}

func (w *ChunkWriter) write(data []byte) error {
	ioWriter := w.Writer
	if w.TempFile != nil {
		ioWriter = w.TempFile
	}

	if uint64(len(data)) > MaxSize {
		w.err = errTooMuchInput
		return w.err
	}
	if w.CPageSize > 0 {
		if err := w.writePadding(ioWriter, uint64(len(data))); err != nil {
			return err
		}
	}
	if _, err := ioWriter.Write(data); err != nil {
		w.err = err
		return err
	}
	w.dataSize += uint64(len(data))
	if w.dataSize > MaxSize {
		w.err = errTooMuchInput
		return w.err
	}
	return nil
}

func (w *ChunkWriter) writePadding(ioWriter io.Writer, lenData uint64) error {
	if lenData == 0 {
		return nil
	}

	offset0 := w.dataSize & (w.CPageSize - 1)
	offset1WithPadding := (0 * offset0) + lenData
	offset1SansPadding := (1 * offset0) + lenData
	numPagesWithPadding := (offset1WithPadding + (w.CPageSize - 1)) >> w.log2CPageSize
	numPagesSansPadding := (offset1SansPadding + (w.CPageSize - 1)) >> w.log2CPageSize

	if numPagesSansPadding == numPagesWithPadding {
		return nil
	}
	return w.padToPageSize(ioWriter, w.dataSize)
}

func (w *ChunkWriter) padToPageSize(ioWriter io.Writer, offset uint64) error {
	offset &= w.CPageSize - 1
	if (offset == 0) || (w.CPageSize == 0) {
		return nil
	}

	if w.padding == nil {
		n := w.CPageSize
		if n > 4096 {
			n = 4096
		}
		w.padding = make([]byte, n)
	}

	for remaining := w.CPageSize - offset; remaining > 0; {
		padding := w.padding
		if remaining < uint64(len(padding)) {
			padding = padding[:remaining]
		}

		n, err := ioWriter.Write(padding)
		if err != nil {
			w.err = err
			return err
		}
		remaining -= uint64(n)
		w.dataSize += uint64(n)
		if w.dataSize > MaxSize {
			w.err = errTooMuchInput
			return w.err
		}
	}
	return nil
}

func (w *ChunkWriter) roundUpToCPageBoundary(x uint64) uint64 {
	if w.CPageSize == 0 {
		return x
	}
	return (x + w.CPageSize - 1) &^ (w.CPageSize - 1)
}

// AddResource adds a shared resource to the RAC file. It returns a non-zero
// OptResource that identifies the resource's bytes. Future calls to AddChunk
// can pass these identifiers to indicate that decompressing a chunk depends on
// up to two shared resources.
//
// The caller may modify resource's contents after this method returns.
func (w *ChunkWriter) AddResource(resource []byte) (OptResource, error) {
	if len(w.resourcesCOffCLens) >= (1 << 30) {
		w.err = errTooManyResources
		return 0, w.err
	}

	if err := w.initialize(); err != nil {
		return 0, err
	}
	if err := w.write(resource); err != nil {
		return 0, err
	}

	if len(w.resourcesCOffCLens) == 0 {
		w.resourcesCOffCLens = make([]uint64, 1, 8)
	}
	id := OptResource(len(w.resourcesCOffCLens))
	cOffset := w.dataSize - uint64(len(resource))
	cLength := calcCLength(len(resource))
	w.resourcesCOffCLens = append(w.resourcesCOffCLens, cOffset|(cLength<<48))
	return id, nil
}

// AddChunk adds a chunk of compressed data - the (primary, secondary,
// tertiary) tuple - to the RAC file. Decompressing that chunk should produce
// dRangeSize bytes, although the ChunkWriter does not attempt to verify that.
//
// The OptResource arguments may be zero, meaning that no resource is used. In
// that case, the corresponding STag or TTag (see the RAC specification for
// further discussion) will be 0xFF.
//
// The caller may modify primary's contents after this method returns.
func (w *ChunkWriter) AddChunk(
	dRangeSize uint64, codec Codec, primary []byte,
	secondary OptResource, tertiary OptResource) error {

	if w.err != nil {
		return w.err
	}
	if dRangeSize == 0 {
		return nil
	}
	if (dRangeSize > MaxSize) || ((w.dFileSize + dRangeSize) > MaxSize) {
		w.err = errTooMuchInput
		return w.err
	}
	if len(w.leafNodes) >= (1 << 30) {
		w.err = errTooManyChunks
		return w.err
	}

	if err := w.initialize(); err != nil {
		return err
	}
	if len(w.leafNodes) == 0 {
		if !codec.Valid() {
			return errInvalidCodec
		}
		w.codec = codec
	} else if w.codec != codec {
		w.err = errors.New("rac: TODO: support writing multiple Codecs")
		return w.err
	}
	if err := w.write(primary); err != nil {
		return err
	}

	cOffset := w.dataSize - uint64(len(primary))
	cLength := calcCLength(len(primary))
	w.dFileSize += dRangeSize
	w.leafNodes = append(w.leafNodes, wNode{
		dRangeSize:     dRangeSize,
		cOffsetCLength: cOffset | (cLength << 48),
		secondary:      secondary,
		tertiary:       tertiary,
		codec:          codec,
	})
	return nil
}

var emptyRACFile = [32]byte{
	0x72, 0xC3, 0x63, 0x01, 0x0D, 0xF8, 0x00, 0xFF,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xFF,
	0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
}

// Close writes the RAC index to w.Writer and marks that w accepts no further
// method calls.
//
// For a one pass encoding, no further action is taken. For a two pass encoding
// (i.e. IndexLocationAtStart), it then copies w.TempFile to w.Writer. Either
// way, if this method returns nil error, the entirety of what was written to
// w.Writer constitutes a valid RAC file.
//
// Closing the underlying w.Writer, w.TempFile or both is the responsibility of
// the ChunkWriter caller, not the ChunkWriter itself.
//
// It is not necessary to call Close, e.g. if an earlier AddXxx call returned a
// non-nil error. Unlike an os.File, failing to call ChunkWriter.Close will not
// leak resources such as file descriptors.
func (w *ChunkWriter) Close() error {
	if w.err != nil {
		return w.err
	}
	if !w.initialized {
		if err := w.checkParameters(); err != nil {
			return err
		}
	}

	if len(w.leafNodes) == 0 {
		_, err := w.Writer.Write(emptyRACFile[:])
		return err
	}
	rootNode := gather(w.leafNodes, w.codec.isLong())
	indexSize := rootNode.calcEncodedSize(0, w.IndexLocation == IndexLocationAtEnd)

	nw := &nodeWriter{
		w:                  w.Writer,
		resourcesCOffCLens: w.resourcesCOffCLens,
	}

	if w.IndexLocation == IndexLocationAtEnd {
		nw.indexCOffset = w.roundUpToCPageBoundary(w.dataSize)
		nw.cFileSize = nw.indexCOffset + indexSize
	} else {
		nw.dataCOffset = w.roundUpToCPageBoundary(indexSize)
		nw.cFileSize = nw.dataCOffset + w.dataSize
	}
	if nw.cFileSize > MaxSize {
		w.err = errTooMuchInput
		return w.err
	}

	if w.IndexLocation == IndexLocationAtEnd {
		// Write the align-to-CPageSize padding.
		if w.CPageSize > 0 {
			if err := w.padToPageSize(w.Writer, w.dataSize); err != nil {
				return err
			}
		}

		// Write the index. The compressed data has already been written.
		if err := nw.writeIndex(&rootNode, true); err != nil {
			w.err = err
			return err
		}

	} else {
		expectedTempFileSize := w.dataSize

		// Write the index.
		if err := nw.writeIndex(&rootNode, false); err != nil {
			w.err = err
			return err
		}

		// Write the align-to-CPageSize padding.
		if w.CPageSize > 0 {
			if err := w.padToPageSize(w.Writer, indexSize); err != nil {
				return err
			}
		}

		// Write the compressed data.
		if s, ok := w.TempFile.(io.Seeker); ok {
			if _, err := s.Seek(w.tempFileSeekStart, io.SeekStart); err != nil {
				w.err = err
				return err
			}
		}
		if n, err := io.Copy(w.Writer, w.TempFile); err != nil {
			w.err = err
			return err
		} else if uint64(n) != expectedTempFileSize {
			w.err = errInconsistentCompressedSize
			return w.err
		}
	}

	w.err = errAlreadyClosed
	return nil
}

// wNode is the ChunkWriter's representation of a node.
type wNode struct {
	dRangeSize uint64

	children  []wNode // Excludes resource-node chidren.
	resources []int

	cOffsetCLength uint64
	secondary      OptResource
	tertiary       OptResource
	codec          Codec
}

// calcEncodedSize accumulates the encoded size of n and its children,
// traversing the nodes in depth-first order.
//
// As a side effect, it also sets n.cOffsetCLength for branch nodes.
func (n *wNode) calcEncodedSize(accumulator uint64, rootAndIsAtEnd bool) (newAccumulator uint64) {
	arity := len(n.children) + len(n.resources)
	if arity == 0 {
		return accumulator
	}
	if n.codec.isLong() {
		arity++
	}
	size := (arity * 16) + 16
	cLength := calcCLength(size)

	if rootAndIsAtEnd {
		for i := range n.children {
			accumulator = n.children[i].calcEncodedSize(accumulator, false)
		}
	}

	n.cOffsetCLength = accumulator | (cLength << 48)
	accumulator += uint64(size)

	if !rootAndIsAtEnd {
		for i := range n.children {
			accumulator = n.children[i].calcEncodedSize(accumulator, false)
		}
	}

	return accumulator
}

type nodeWriter struct {
	w io.Writer

	cFileSize          uint64
	dataCOffset        uint64
	indexCOffset       uint64
	resourcesCOffCLens []uint64

	// A wNode's maximum arity is 255, so a wNode's maximum size in bytes is
	// ((255 * 16) + 16), which is 4096.
	buffer [4096]byte
}

func (w *nodeWriter) writeIndex(n *wNode, rootAndIsAtEnd bool) error {
	const tagFF = 0xFF << 56

	if rootAndIsAtEnd {
		for i, o := range n.children {
			if len(o.children) != 0 {
				if err := w.writeIndex(&n.children[i], false); err != nil {
					return err
				}
			}
		}
	}

	if !n.codec.Valid() {
		return errors.New("rac: TODO: support writing multiple Codecs")
	}

	buf, dPtr := w.buffer[:], uint64(0)
	arity := uint64(len(n.children) + len(n.resources))
	if n.codec.isLong() {
		arity++
	}
	if arity > 0xFF {
		return errInternalArityIsTooLarge
	}

	// Codec Element 'DPtr|Reserved0|TTag', if present.
	if n.codec.isLong() {
		putU64LE(buf, 0xFD00000000000000)
		buf = buf[8:]
	}

	// DPtr's. We write resources before regular children (non-resources), so
	// that any TTag that refers to a resource always avoids the [0xC0, 0xFD]
	// reserved zone. The ratio of resources to regulars is at most 2:1, as
	// every chunk uses exactly 1 regular slot and up to 2 resource slots, and
	// that reserved zone is less than 33% of the [0x00, 0xFF] space.
	for i := range n.resources {
		putU64LE(buf[8*i:], dPtr|tagFF)
	}
	buf = buf[8*len(n.resources):]
	for i, o := range n.children {
		tag := uint64(0xFE << 56)
		if len(o.children) == 0 {
			tag = resourceToTag(n.resources, o.tertiary)
		}
		putU64LE(buf[8*i:], dPtr|tag)
		dPtr += o.dRangeSize
	}
	buf = buf[8*len(n.children):]

	// DPtrMax and the CodecByte. By construction, a Long Codec's 'c64' value
	// is always 0, as the Codec Element is always in position 0.
	codecHighByte := uint64(n.codec) & 0xFF00000000000000
	putU64LE(buf, dPtr|codecHighByte)
	buf = buf[8:]

	// Codec Element 'CPtr|CLen|STag', if present.
	if n.codec.isLong() {
		putU64LE(buf, uint64(n.codec)&0x00FFFFFFFFFFFFFF)
		buf = buf[8:]
	}

	// CPtr's. While the RAC format allows otherwise, this nodeWriter always
	// keeps the CBias at zero, so that a CPtr equals a COffset.
	for i, res := range n.resources {
		cOffsetCLength := w.resourcesCOffCLens[res] + w.dataCOffset
		putU64LE(buf[8*i:], cOffsetCLength|tagFF)
	}
	buf = buf[8*len(n.resources):]
	for i, o := range n.children {
		cOffsetCLength := o.cOffsetCLength
		if len(o.children) == 0 {
			cOffsetCLength += w.dataCOffset
		} else {
			cOffsetCLength += w.indexCOffset
		}
		putU64LE(buf[8*i:], cOffsetCLength|resourceToTag(n.resources, o.secondary))
	}
	buf = buf[8*len(n.children):]

	// CPtrMax.
	const version = 0x01
	putU64LE(buf, w.cFileSize|(version<<48)|(arity<<56))

	// Magic and Arity.
	w.buffer[0] = 0x72
	w.buffer[1] = 0xC3
	w.buffer[2] = 0x63
	w.buffer[3] = uint8(arity)

	// Checksum.
	size := (arity * 16) + 16
	checksum := crc32.ChecksumIEEE(w.buffer[6:size])
	checksum ^= checksum >> 16
	w.buffer[4] = uint8(checksum >> 0)
	w.buffer[5] = uint8(checksum >> 8)

	if _, err := w.w.Write(w.buffer[:size]); err != nil {
		return err
	}

	if !rootAndIsAtEnd {
		for i, o := range n.children {
			if len(o.children) != 0 {
				if err := w.writeIndex(&n.children[i], false); err != nil {
					return err
				}
			}
		}
	}

	return nil
}

func calcCLength(primarySize int) uint64 {
	if primarySize <= 0 {
		return 1
	}

	// Divide by 1024, rounding up.
	primarySize = ((primarySize - 1) >> 10) + 1

	if primarySize > 255 {
		return 0
	}
	return uint64(primarySize)
}

func resourceToTag(resources []int, r OptResource) uint64 {
	if r != 0 {
		for i, res := range resources {
			if res == int(r) {
				return uint64(i) << 56
			}
		}
	}
	return 0xFF << 56
}

// gather brings the given nodes into a tree, such that every branch node's
// arity (the count of its resource and non-resource child nodes) is at most
// 0xFF. It returns the root of that tree.
//
// TODO: it currently builds a tree where all leaf nodes have equal depth. For
// small RAC files (with only one branch node: the mandatory root node), this
// doesn't matter. For larger RAC files, it might be better to build an uneven
// tree to minimize average leaf node depth, with some branch nodes having both
// branch node children and leaf node children. It might even be worth ensuring
// that the root node always directly contains the first and last leaf nodes as
// children, as those leaves presumably contain the most commonly accessed
// parts of the decompressed file.
//
// If doing this TODO, we'd also have to change the "codec = codecMixBit |
// CodecZeroes" line below, as it assumes that no branch nodes have both branch
// node children and leaf node children.
func gather(nodes []wNode, codecIsLong bool) wNode {
	if len(nodes) == 0 {
		panic("gather: no nodes")
	}

	resources := map[OptResource]bool{}

	arityBudget := 0xFF
	if codecIsLong {
		arityBudget = 0xFE
	}

	for {
		i, j, arity, newNodes := 0, 0, 0, []wNode(nil)
		for ; j < len(nodes); j++ {
			o := &nodes[j]

			new2 := (o.secondary != 0) && !resources[o.secondary]
			new3 := (o.tertiary != 0) && !resources[o.tertiary]
			arity += 1 + btoi(new2) + btoi(new3)
			if arity <= arityBudget {
				if new2 {
					resources[o.secondary] = true
				}
				if new3 {
					resources[o.tertiary] = true
				}
				continue
			}

			newNodes = append(newNodes, makeBranch(nodes[i:j], resources))
			if len(resources) != 0 {
				resources = map[OptResource]bool{}
			}

			i = j
			arity = 1
			if o.secondary != 0 {
				resources[o.secondary] = true
				arity++
			}
			if o.tertiary != 0 {
				resources[o.tertiary] = true
				arity++
			}
		}

		if i == 0 {
			return makeBranch(nodes, resources)
		}

		newNodes = append(newNodes, makeBranch(nodes[i:], resources))
		if len(resources) != 0 {
			resources = map[OptResource]bool{}
		}

		nodes, newNodes = newNodes, nil
	}
}

func makeBranch(children []wNode, resMap map[OptResource]bool) wNode {
	dRangeSize, codec := uint64(0), Codec(0)
	for i, c := range children {
		dRangeSize += c.dRangeSize
		if i == 0 {
			codec = c.codec
		} else if codec != c.codec {
			// We construct the node tree so that a branch node's children are
			// either all branch nodes or all leaf nodes. If they are all leaf
			// nodes (with the same Codec), or all branch nodes with the same
			// Codec, the parent uses the same Codec. If they are all branch
			// nodes, with different Codecs, we set the Mix bit and since it
			// has no leaf nodes, we might as well use CodecZeroes.
			codec = codecMixBit | CodecZeroes
		}
	}

	resList := []int(nil)
	if len(resMap) != 0 {
		resList = make([]int, 0, len(resMap))
		for res := range resMap {
			resList = append(resList, int(res))
		}
		sort.Ints(resList)
	}

	return wNode{
		dRangeSize:     dRangeSize,
		children:       children,
		resources:      resList,
		cOffsetCLength: invalidCOffsetCLength,
		codec:          codec,
	}
}
