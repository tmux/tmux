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
	"io"
)

const (
	defaultDChunkSize = 65536 // 64 KiB.

	maxCChunkSize       = 1 << 30 // 1 GiB.
	maxTargetDChunkSize = 1 << 31 // 2 GiB.
)

func startingTargetDChunkSize(cChunkSize uint64) uint64 { return 2 * cChunkSize }

// stripTrailingZeroes returns b shorn of any trailing '\x00' bytes. The RAC
// file format will implicitly set any trailing zeroes.
func stripTrailingZeroes(b []byte) []byte {
	n := len(b)
	for (n > 0) && (b[n-1] == 0) {
		n--
	}
	return b[:n]
}

// writeBuffer is a sequence of bytes, split into two slices: prev[p:] and
// curr. Together, they hold the tail (so far) of the stream of bytes given to
// an io.Writer.
//
// Conceptually, it is the concatenation of those two slices, but the struct
// holds two slices, not one, to minimize copying and memory allocation.
//
// The first slice, prev[p:], is those bytes from all previous Write calls that
// have been copied to an internal buffer but not yet fully processed.
//
// The second slice, curr, is those bytes from the current Write call that have
// not been processed.
//
// The underlying array that backs the prev slice is owned by the writeBuffer,
// and is re-used to minimize memory allocations. The curr slice is a sub-slice
// of the []byte passed to the Write method, and is owned by the caller, not by
// the writeBuffer.
//
// As bytes are fully processed, prev[p:] shrinks (by incrementing "p += etc")
// and after len(prev[p:]) hits zero, curr shrinks (by re-assigning "curr =
// curr[etc:]"). The two mechanisms differ (e.g. there is a p int that offsets
// prev but not a corresponding c int that offsets curr) because of the
// difference in backing-array memory ownership, mentioned above.
//
// The intended usage is for an io.Writer's Write(data) method to first call
// writeBuffer.extend(data), then a mixture of multiple writeBuffer.peek(n) and
// writeBuffer.advance(n), and finally writeBuffer.compact(). Thus, before and
// after every Write call, writeBuffer.curr is empty and writebuffer.p is 0.
type writeBuffer struct {
	prev []byte
	curr []byte
	p    int
}

// extend extends b's view of an io.Writer's incoming byte stream.
func (b *writeBuffer) extend(curr []byte) {
	if len(b.curr) != 0 {
		panic("inconsistent writeBuffer state")
	}
	b.curr = curr
}

// length returns the total number of bytes available in b's buffers.
func (b *writeBuffer) length() uint64 {
	return uint64(len(b.prev)-b.p) + uint64(len(b.curr))
}

// peek returns the next m bytes in b's buffers, where m is the minimum of n
// and b.length().
//
// The bytes are returned in two slices, not a single contiguous slice, in
// order to minimize copying and memory allocation.
func (b *writeBuffer) peek(n uint64) ([]byte, []byte) {
	available := uint64(len(b.prev) - b.p)
	if n <= available {
		return b.prev[b.p : b.p+int(n)], nil
	}
	n -= available
	if n <= uint64(len(b.curr)) {
		return b.prev[b.p:], b.curr[:n]
	}
	return b.prev[b.p:], b.curr
}

// advance consumes the next n bytes.
func (b *writeBuffer) advance(n uint64) {
	available := uint64(len(b.prev) - b.p)
	if n <= available {
		b.p += int(n)
		return
	}
	n -= available
	b.curr = b.curr[n:]
	b.p = len(b.prev)
}

// advancePastLeadingZeroes consumes the next n bytes, all of which are '\x00'.
func (b *writeBuffer) advancePastLeadingZeroes() (n uint64) {
	// Consume zeroes from b.prev.
	i := b.p
	for ; (i < len(b.prev)) && (b.prev[i] == '\x00'); i++ {
	}
	if i == b.p {
		return 0
	}
	n = uint64(i - b.p)
	b.p = i

	// Consume zeroes from b.curr.
	i = 0
	for ; (i < len(b.curr)) && (b.curr[i] == '\x00'); i++ {
	}
	n += uint64(i)
	b.curr = b.curr[i:]
	return n
}

// compact moves and copies any unprocessed bytes in b.prev and b.curr to be at
// the start of b.prev.
func (b *writeBuffer) compact() {
	// Move any remaining bytes in prev to the front.
	n := copy(b.prev, b.prev[b.p:])
	b.prev = b.prev[:n]
	b.p = 0
	// Move any remaining bytes in curr to prev.
	b.prev = append(b.prev, b.curr...)
	b.curr = nil
}

// NoResourceUsed is returned by CodecWriter.Compress to indicate that no
// secondary or tertiary resource was used in the compression.
const NoResourceUsed int = -1

// CodecWriter specializes a Writer to encode a specific compression codec.
//
// Instances are not required to support concurrent use.
type CodecWriter interface {
	// Close tells the CodecWriter that no further calls will be made.
	Close() error

	// Clone duplicates this. The clone and the original can run concurrently.
	Clone() CodecWriter

	// Compress returns the codec-specific compressed form of the concatenation
	// of p and q.
	//
	// The source bytes are conceptually one continuous data stream, but is
	// presented in two slices to avoid unnecessary copying in the code that
	// calls Compress. One or both of p and q may be an empty slice. A
	// Compress(p, q, etc) is equivalent to, but often more efficient than:
	//   combined := []byte(nil)
	//   combined = append(combined, p...)
	//   combined = append(combined, q...)
	//   Compress(combined, nil, etc)
	//
	// Returning a secondaryResource or tertiaryResource within the range ((0
	// <= i) && (i < len(resourcesData))) indicates that resourcesData[i] was
	// used in the compression. A value outside of that range means that no
	// resource was used in the secondary and/or tertiary slot. The
	// NoResourceUsed constant (-1) is always outside that range.
	Compress(p []byte, q []byte, resourcesData [][]byte) (
		codec Codec, compressed []byte, secondaryResource int, tertiaryResource int, retErr error)

	// CanCut returns whether the CodecWriter supports the optional Cut method.
	CanCut() bool

	// Cut modifies encoded's contents such that encoded[:encodedLen] is valid
	// codec-compressed data, assuming that encoded starts off containing valid
	// codec-compressed data.
	//
	// If a nil error is returned, then encodedLen <= maxEncodedLen will hold.
	//
	// Decompressing that modified, shorter byte slice produces a prefix (of
	// length decodedLen) of the decompression of the original, longer byte
	// slice.
	Cut(codec Codec, encoded []byte, maxEncodedLen int) (
		encodedLen int, decodedLen int, retErr error)

	// WrapResource returns the Codec-specific encoding of the raw resource.
	// For example, if raw is the bytes of a shared LZ77-style dictionary,
	// WrapResource may prepend or append length and checksum data.
	WrapResource(raw []byte) ([]byte, error)
}

// Writer provides a relatively simple way to write a RAC file - one that is
// created starting from nothing, as opposed to incrementally modifying an
// existing RAC file.
//
// Other packages may provide a more flexible (and more complicated) way to
// write or append to RAC files, but that is out of scope of this package.
//
// Do not modify its exported fields after calling any of its methods.
//
// Writer implements the io.WriteCloser interface.
type Writer struct {
	// Writer is where the RAC-encoded data is written to.
	//
	// Nil is an invalid value.
	Writer io.Writer

	// CodecWriter is the compression codec that this Writer can compress to.
	//
	// For example, use a raczlib.CodecWriter from the sibilng "raczlib"
	// package.
	//
	// Nil is an invalid value.
	CodecWriter CodecWriter

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
	// (via SeekCurrent), then it will be written to (via the rac.Writer.Write
	// method), then its position will be reset (via SeekSet), then it will be
	// read from (via the rac.Writer.Close method).
	//
	// The rac.Writer does not call TempFile.Close even if that method exists
	// (e.g. the TempFile is an os.File). In that case, closing the temporary
	// file (and deleting it) after the rac.Writer is closed is the
	// responsibility of the rac.Writer caller, not the rac.Writer itself.
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

	// CChunkSize is the compressed size (i.e. size in CSpace) of each chunk.
	// The final chunk might be smaller than this.
	//
	// This field is ignored if DChunkSize is non-zero.
	CChunkSize uint64

	// DChunkSize is the uncompressed size (i.e. size in DSpace) of each chunk.
	// The final chunk might be smaller than this.
	//
	// If both CChunkSize and DChunkSize are non-zero, DChunkSize takes
	// precedence and CChunkSize is ignored.
	//
	// If both CChunkSize and DChunkSize are zero, a default DChunkSize value
	// will be used.
	DChunkSize uint64

	// ResourcesData is a list of resources that can be shared across otherwise
	// independently compressed chunks.
	//
	// The exact semantics for a resource depends on the codec. For example,
	// the Zlib codec interprets them as shared LZ77-style dictionaries.
	//
	// One way to generate shared dictionaries from sub-slices of a single
	// source file is the command line tool at
	// https://github.com/google/brotli/blob/master/research/dictionary_generator.cc
	ResourcesData [][]byte

	// resourcesIDs is the OptResource for each ResourcesData element. Zero
	// means that corresponding resource is not yet used (and not yet written
	// to the RAC file).
	resourcesIDs []OptResource

	// cChunkSize and dChunkSize are copies of CChunkSize and DChunkSize,
	// validated during the initialize method. For example, if both CChunkSize
	// and DChunkSize are zero, dChunkSize will be defaultDChunkSize.
	cChunkSize uint64
	dChunkSize uint64

	// err is the first error encountered. It is sticky: once a non-nil error
	// occurs, all public methods will return that error.
	err error

	// chunkWriter is the low-level chunk writer.
	chunkWriter ChunkWriter

	// uncompressed are the uncompressed bytes that have been given to this
	// (via the Write method) but not yet compressed as a chunk.
	uncompressed writeBuffer

	// closed is whether this Writer is closed.
	closed bool
}

func (w *Writer) initialize() error {
	if w.err != nil {
		return w.err
	}
	if w.chunkWriter.Writer != nil {
		// We're already initialized.
		return nil
	}
	if w.Writer == nil {
		w.err = errInvalidWriter
		return w.err
	}
	if w.CodecWriter == nil {
		w.err = errInvalidCodecWriter
		return w.err
	}
	w.resourcesIDs = make([]OptResource, len(w.ResourcesData))

	if w.DChunkSize > 0 {
		w.dChunkSize = w.DChunkSize
	} else if w.CChunkSize > 0 {
		if !w.CodecWriter.CanCut() {
			w.err = ErrCodecWriterDoesNotSupportCChunkSize
			return w.err
		}
		w.cChunkSize = w.CChunkSize
		if w.cChunkSize > maxCChunkSize {
			w.cChunkSize = maxCChunkSize
		}
	} else {
		w.dChunkSize = defaultDChunkSize
	}

	w.chunkWriter.Writer = w.Writer
	w.chunkWriter.IndexLocation = w.IndexLocation
	w.chunkWriter.TempFile = w.TempFile
	w.chunkWriter.CPageSize = w.CPageSize
	return nil
}

// useResource marks a shared resource as used, if it wasn't already marked.
//
// i is the index into both the w.ResourcesData and w.resourcesIDs slices. It
// is valid to pass an i outside the range [0, len(w.resourcesIDs)), in which
// case the call is a no-op.
func (w *Writer) useResource(i int) (OptResource, error) {
	if (i < 0) || (len(w.resourcesIDs) < i) {
		return 0, nil
	}
	if id := w.resourcesIDs[i]; id != 0 {
		return id, nil
	}
	wrapped, err := w.CodecWriter.WrapResource(w.ResourcesData[i])
	if err != nil {
		w.err = err
		return 0, err
	}
	id, err := w.chunkWriter.AddResource(wrapped)
	if err != nil {
		w.err = err
		return 0, err
	}
	w.resourcesIDs[i] = id
	return id, nil
}

// Write implements io.Writer.
func (w *Writer) Write(p []byte) (int, error) {
	if err := w.initialize(); err != nil {
		return 0, err
	}
	if n := uint64(len(p)); (n > MaxSize) || (w.uncompressed.length() > (MaxSize - n)) {
		w.err = errTooMuchInput
		return 0, w.err
	}
	w.uncompressed.extend(p)
	n, err := len(p), w.write(false)
	w.uncompressed.compact()
	if err != nil {
		return 0, err
	}
	return n, nil
}

func (w *Writer) write(eof bool) error {
	if w.dChunkSize > 0 {
		return w.writeDChunks(eof)
	}
	return w.writeCChunks(eof)
}

func (w *Writer) writeDChunks(eof bool) error {
	for {
		peek0, peek1 := w.uncompressed.peek(w.dChunkSize)
		dSize := uint64(len(peek0)) + uint64(len(peek1))
		if dSize == 0 {
			return nil
		}
		if !eof && (dSize < w.dChunkSize) {
			return nil
		}

		peek1 = stripTrailingZeroes(peek1)
		if len(peek1) == 0 {
			peek0 = stripTrailingZeroes(peek0)
		}

		codec, cBytes, index2, index3, err :=
			w.CodecWriter.Compress(peek0, peek1, w.ResourcesData)
		if err != nil {
			return err
		}
		res2, err := w.useResource(index2)
		if err != nil {
			return err
		}
		res3, err := w.useResource(index3)
		if err != nil {
			return err
		}

		if err := w.chunkWriter.AddChunk(dSize, codec, cBytes, res2, res3); err != nil {
			w.err = err
			return err
		}
		w.uncompressed.advance(dSize)
	}
}

func (w *Writer) writeCChunks(eof bool) error {
	// Each outer loop iteration tries to write exactly one chunk.
outer:
	for {
		// We don't know, a priori, how many w.uncompressed bytes are needed to
		// produce a compressed chunk of size cChunkSize. We use a
		// startingTargetDChunkSize that is a small multiple of cChunkSize, and
		// keep doubling that until we build something at least as large as
		// cChunkSize, then CodecWriter.Cut it back to be exactly cChunkSize.
		targetDChunkSize := uint64(maxTargetDChunkSize)
		if !eof {
			targetDChunkSize = startingTargetDChunkSize(w.cChunkSize)
		}

		if n := w.uncompressed.length(); n == 0 {
			return nil
		} else if !eof && (n < targetDChunkSize) {
			return nil
		}

		// The inner loop keeps doubling the targetDChunkSize until it's big
		// enough to produce at least cChunkSize bytes of compressed data.
		for {
			next := targetDChunkSize * 2
			if next > maxTargetDChunkSize {
				next = maxTargetDChunkSize
			}
			force := next <= targetDChunkSize

			if err := w.tryCChunk(targetDChunkSize, force); err == nil {
				continue outer
			} else if err != errInternalShortCSize {
				return err
			}

			if w.uncompressed.length() <= targetDChunkSize {
				// Growing the targetDChunkSize isn't going to change anything.
				return nil
			}
			targetDChunkSize = next
		}
	}
}

func (w *Writer) tryCChunk(targetDChunkSize uint64, force bool) error {
	peek0, peek1 := w.uncompressed.peek(targetDChunkSize)
	dSize := uint64(len(peek0)) + uint64(len(peek1))

	codec, cBytes, index2, index3, err :=
		w.CodecWriter.Compress(peek0, peek1, w.ResourcesData)
	if err != nil {
		return err
	}
	res2, err := w.useResource(index2)
	if err != nil {
		return err
	}
	res3, err := w.useResource(index3)
	if err != nil {
		return err
	}

	switch {
	case uint64(len(cBytes)) < w.cChunkSize:
		if !force {
			return errInternalShortCSize
		}
		fallthrough
	case uint64(len(cBytes)) == w.cChunkSize:
		w.uncompressed.advance(dSize)
		dSize += w.uncompressed.advancePastLeadingZeroes()
		if err := w.chunkWriter.AddChunk(dSize, codec, cBytes, res2, res3); err != nil {
			w.err = err
			return err
		}
		return nil
	}

	eLen, dLen, err := w.CodecWriter.Cut(codec, cBytes, int(w.cChunkSize))
	if err != nil {
		w.err = err
		return err
	}
	if dLen == 0 {
		w.err = errCChunkSizeIsTooSmall
		return w.err
	}
	dSize, cBytes = uint64(dLen), cBytes[:eLen]
	w.uncompressed.advance(dSize)
	dSize += w.uncompressed.advancePastLeadingZeroes()
	if err := w.chunkWriter.AddChunk(dSize, codec, cBytes, res2, res3); err != nil {
		w.err = err
		return err
	}
	return nil
}

// Close writes the RAC index to w.Writer and marks that w accepts no further
// method calls.
//
// Calling Close will call Close on w's CodecWriter.
//
// For a one pass encoding, no further action is taken. For a two pass encoding
// (i.e. IndexLocationAtStart), it then copies w.TempFile to w.Writer. Either
// way, if this method returns nil error, the entirety of what was written to
// w.Writer constitutes a valid RAC file.
//
// Closing the underlying w.Writer, w.TempFile or both is the responsibility of
// the rac.Writer caller, not the rac.Writer itself.
func (w *Writer) Close() error {
	if w.closed {
		return w.err
	}
	w.closed = true

	if err := w.initialize(); w.err == nil {
		w.err = err
	}
	if w.err == nil {
		w.err = w.write(true)
	}
	if w.err == nil {
		w.err = w.chunkWriter.Close()
	}
	if err := w.CodecWriter.Close(); w.err == nil {
		w.err = err
	}

	if w.err == nil {
		w.err = errAlreadyClosed
		return nil
	}
	return w.err
}
