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
	"fmt"
	"io"
)

// zeroesReader is an io.Reader that serves up a finite number of '\x00' bytes.
type zeroesReader int64

// Read implements io.Reader.
func (z *zeroesReader) Read(p []byte) (int, error) {
	if int64(len(p)) > int64(*z) {
		p = p[:*z]
	}
	for i := range p {
		p[i] = 0
	}
	*z -= zeroesReader(len(p))
	if *z == 0 {
		return len(p), io.EOF
	}
	return len(p), nil
}

// CodecReader specializes a Reader to decode a specific compression codec.
//
// Instances are not required to support concurrent use.
type CodecReader interface {
	// Close tells the CodecReader that no further calls will be made.
	Close() error

	// Accepts returns whether this CodecReader can decode a Codec.
	Accepts(c Codec) bool

	// Clone duplicates this. The clone and the original can run concurrently.
	Clone() CodecReader

	// MakeDecompressor returns the Codec-specific io.Reader for a chunk.
	//
	// The returned io.Reader may optionally implement the io.Closer interface,
	// in which case this Reader will call Close when has finished the chunk.
	MakeDecompressor(racFile io.ReadSeeker, c Chunk) (io.Reader, error)
}

// Reader reads a RAC file.
//
// Do not modify its exported fields after calling any of its methods.
//
// Reader implements the io.ReadSeeker and io.Closer interfaces.
type Reader struct {
	// ReadSeeker is where the RAC-encoded data is read from.
	//
	// It may optionally implement io.ReaderAt, in which case its ReadAt method
	// will be preferred and its Read and Seek methods will never be called.
	// The ReadAt method is safe to use concurrently, so that multiple
	// rac.Reader's can concurrently use the same source provided that the
	// source (this field, nominally an io.ReadSeeker) implements io.ReaderAt.
	//
	// In particular, if the source is a bytes.Reader or an os.File, multiple
	// rac.Reader's can work in parallel, which can speed up decoding.
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

	// CodecReaders are the compression codecs that this Reader can decompress.
	//
	// For example, use a raczlib.CodecReader from the sibilng "raczlib"
	// package.
	CodecReaders []CodecReader

	// Concurrency is how many worker goroutines are used to decode RAC chunks.
	// Bigger values often lead to faster throughput, up to a
	// hardware-dependent point, but also larger memory requirements.
	//
	// If positive, then the ReadSeeker must also be an io.ReaderAt.
	//
	// Non-positive values (including zero) mean a non-concurrent
	// (single-goroutine) reader.
	Concurrency int

	// err is the first error encountered. It is sticky: once a non-nil error
	// occurs, all public methods will return that error.
	err error

	// chunkReader is the low-level RAC chunk reader.
	chunkReader ChunkReader

	// These two fields combine for a 3-state state machine:
	//
	//  - "State A" (both fields are zero): no RAC chunk is loaded.
	//
	//  - "State B" (decompressor is non-zero, inImplicitZeroes is zero): a RAC
	//    chunk is loaded, but not fully exhausted: decompressing the e.g. zlib
	//    stream has not seen io.EOF yet.
	//
	//  - "State C" (decompressor is zero, inImplicitZeroes is non-zero): a RAC
	//    chunk was exhausted, and we now serve the implicit NUL bytes after a
	//    chunk's explicitly encoded data. The number of NUL bytes can be (and
	//    often is) zero.
	//
	// Calling Read may trigger state transitions (which form a cycle): "State
	// A" -> "State B" -> "State C" -> "State A" -> "State B" -> etc.
	//
	// Calling Seek may reset the state machine to "State A".
	//
	// The initial state is "State A".
	decompressor     io.Reader
	inImplicitZeroes bool

	// closed is whether this Reader is closed.
	closed bool

	// pos is the current position, in DSpace. It is the base value when Seek
	// is called with io.SeekCurrent.
	pos int64

	// posLimit is an upper limit on pos. pos can go higher than it (e.g.
	// seeking past the end of the file in DSpace), but after doing so, Read
	// will always return (0, io.EOF).
	posLimit int64

	// dRange is, in "State B" and "State C", what part (in DSpace) of the
	// current chunk has not yet been passed on (via this type's Read method).
	//
	// Within those states, dRange[0] increases over time, as parts of the
	// chunk are decompressed and passed on, but dRange[1] does not change.
	//
	// An invariant is that ((dRange[0] <= pos) && (pos <= dRange[1])).
	//
	// If the first inequality is strict (i.e. dRange[0] < pos) then we have
	// Seek'ed to a pos that is not a chunk boundary, and satisfying the Read
	// method will first require decompressing and discarding some of the chunk
	// data, until dRange[0] reaches pos.
	//
	// If the second inequality is strict (i.e. pos < dRange[1]) and we are in
	// "State C" then we have a non-zero number of implicit NUL bytes left.
	//
	// In "State A", the dRange is empty and unused, other than trivially
	// maintaining the invariant.
	dRange Range

	// zeroes serves the Zeroes Codec.
	zeroes zeroesReader

	// concReader decodes the RAC-compressed data concurrently.
	concReader concReader
}

func (r *Reader) initialize() error {
	if r.err != nil {
		return r.err
	}
	if r.chunkReader.initialized {
		return nil
	}
	r.chunkReader.ReadSeeker = r.ReadSeeker
	r.chunkReader.CompressedSize = r.CompressedSize
	if r.Concurrency > 0 {
		if _, ok := r.ReadSeeker.(io.ReaderAt); !ok {
			r.err = fmt.Errorf("rac: Concurrency > 0 requires the ReadSeeker to be an io.ReaderAt")
			return r.err
		}
	}
	if err := r.chunkReader.initialize(); err != nil {
		r.err = err
		return r.err
	}
	r.posLimit = r.chunkReader.decompressedSize
	r.concReader.initialize(r)
	return nil
}

func (r *Reader) clone() *Reader {
	c := &Reader{
		ReadSeeker:     r.ReadSeeker,
		CompressedSize: r.CompressedSize,
		CodecReaders:   make([]CodecReader, len(r.CodecReaders)),
		Concurrency:    r.Concurrency,
	}
	for i := range c.CodecReaders {
		c.CodecReaders[i] = r.CodecReaders[i].Clone()
	}
	return c
}

// Read implements io.Reader.
func (r *Reader) Read(p []byte) (int, error) {
	if err := r.initialize(); err != nil {
		return 0, err
	}
	if r.concReader.ready() {
		n, err := r.concReader.Read(p)
		r.err = err
		return n, err
	}

	if r.pos >= r.posLimit {
		return 0, io.EOF
	}
	if n := r.posLimit - r.pos; int64(len(p)) > n {
		p = p[:n]
	}

	for numRead := 0; ; {
		if r.pos >= r.posLimit {
			return numRead, io.EOF
		}
		if len(p) == 0 {
			return numRead, nil
		}
		if (r.pos < r.dRange[0]) || (r.dRange[1] < r.pos) {
			r.err = errInternalInconsistentPosition
			return numRead, r.err
		}

		readFunc := (func(*Reader, []byte) (int, error))(nil)
		switch {
		default: // "State A".
			if err := r.nextChunk(); err != nil {
				return numRead, err
			}
			continue

		case r.decompressor != nil: // "State B".
			readFunc = (*Reader).readExplicitData

		case r.inImplicitZeroes: // "State C".
			readFunc = (*Reader).readImplicitZeroes
		}

		n, err := readFunc(r, p)
		numRead += n
		p = p[n:]
		if err != nil {
			return numRead, err
		}
	}
}

// readExplicitData serves the compressed data in a chunk.
func (r *Reader) readExplicitData(p []byte) (int, error) {
	// If the chunk started before r.pos, discard the opening bytes of the
	// chunk's decompressed data.
	for r.pos > r.dRange[0] {
		discardBuffer := p
		discardBufferLen := r.pos - r.dRange[0]
		if int64(len(discardBuffer)) > discardBufferLen {
			discardBuffer = discardBuffer[:discardBufferLen]
		}

		n, err := r.decompressor.Read(discardBuffer)
		r.dRange[0] += int64(n)
		if err == io.EOF {
			return 0, r.transitionFromStateBToStateC()
		}
		if err != nil {
			r.err = err
			return 0, r.err
		}
	}

	// Delegate to the decompressor.
	n, err := r.decompressor.Read(p)
	if size := r.dRange.Size(); int64(n) > size {
		n = int(size)
		err = errInvalidChunkTooLarge
	}
	r.pos += int64(n)
	r.dRange[0] += int64(n)
	if err == io.EOF {
		return n, r.transitionFromStateBToStateC()
	} else if err == io.ErrUnexpectedEOF {
		err = errInvalidChunkTruncated
	}
	if err != nil {
		r.err = err
	}
	return n, err
}

func (r *Reader) transitionFromStateBToStateC() error {
	if c, ok := r.decompressor.(io.Closer); ok {
		if err := c.Close(); err != nil {
			if err == io.EOF {
				err = io.ErrUnexpectedEOF
			}
			r.err = err
			return r.err
		}
	}
	r.decompressor = nil
	r.inImplicitZeroes = true
	return nil
}

// readImplicitZeroes serves the implicit NUL bytes after a chunk's explicit
// data. As
// https://github.com/google/wuffs/blob/main/doc/spec/rac-spec.md#decompressing-a-leaf-node
// says, "The Codec may produce fewer bytes than the DRange size. In that case,
// the remaining bytes (in DSpace) are set to NUL (memset to zero)."
func (r *Reader) readImplicitZeroes(p []byte) (int, error) {
	// If the chunk's explicit data finished before r.pos, discard some of the
	// implicit NULs.
	if r.dRange[0] < r.pos {
		r.dRange[0] = r.pos
	}

	// The next r.dRange.Size() bytes are all implicitly zero.
	n := r.dRange.Size()
	if int64(len(p)) > n {
		p = p[:n]
	}
	for i := range p {
		p[i] = 0
	}

	// Update the cursors, check for exhaustion and return.
	r.pos += int64(len(p))
	r.dRange[0] += int64(len(p))
	if r.dRange.Empty() {
		// Transition from "State C" to "State A".
		r.inImplicitZeroes = false
	}
	return len(p), nil
}

// nextChunk loads the next independently compressed chunk. It transitions from
// "State A" to "State B".
//
// It may return io.EOF, in which case the Reader stays in "State A", and the
// r.err "sticky error" field stays nil.
func (r *Reader) nextChunk() error {
	chunk, err := r.chunkReader.NextChunk()
	if err != nil {
		if err == io.EOF {
			return io.EOF
		}
		r.err = err
		return r.err
	}
	if chunk.DRange.Empty() {
		r.err = errInvalidChunk
		return r.err
	}

	if (chunk.Codec == CodecZeroes) || (chunk.Codec == codecLongZeroes) {
		r.dRange = chunk.DRange
		r.zeroes = zeroesReader(r.dRange.Size())
		r.decompressor = &r.zeroes
		return nil
	}

	codecReader := CodecReader(nil)
	for _, cr := range r.CodecReaders {
		if cr.Accepts(chunk.Codec) {
			codecReader = cr
			break
		}
	}
	if codecReader == nil {
		name0, name1, name2 := "", chunk.Codec.name(), ""
		if name1 != "" {
			name0, name2 = " (", ")"
		}
		r.err = fmt.Errorf("rac: no matching CodecReader for Codec 0x%X%s%s%s",
			chunk.Codec, name0, name1, name2)
		return r.err
	}

	decompressor, err := codecReader.MakeDecompressor(r.chunkReader.readSeeker, chunk)
	if err != nil {
		if err == io.EOF {
			err = io.ErrUnexpectedEOF
		}
		r.err = err
		return r.err
	}
	r.decompressor = decompressor
	r.dRange = chunk.DRange
	return nil
}

// Seek implements io.Seeker.
func (r *Reader) Seek(offset int64, whence int) (int64, error) {
	if err := r.initialize(); err != nil {
		return 0, err
	}
	const maxInt64 = (1 << 63) - 1
	return r.seek(offset, whence, maxInt64)
}

// SeekRange restricts r to the half-open range [low, high).
//
// It is more efficient than but conceptually equivalent to calling Seek(low,
// io.SeekStart) and wrapping r in an io.LimitedReader whose N is (high - low).
//
// Multiple SeekRange calls apply the most recent high limit, not the minimum
// of the high limits.
//
// Any Seek call, such as Seek(0, io.SeekCurrent), will remove the high limit.
//
// It returns an error if low > high.
func (r *Reader) SeekRange(low int64, high int64) error {
	if err := r.initialize(); err != nil {
		return err
	}
	if low > high {
		r.err = errSeekToNegativeRange
		return r.err
	}
	_, err := r.seek(low, io.SeekStart, high)
	return err
}

func (r *Reader) seek(offset int64, whence int, limit int64) (int64, error) {
	if r.concReader.ready() {
		n, err := r.concReader.seek(offset, whence, limit)
		r.err = err
		return n, err
	}

	pos := r.pos
	switch whence {
	case io.SeekStart:
		pos = offset
	case io.SeekCurrent:
		pos += offset
	case io.SeekEnd:
		pos = r.chunkReader.decompressedSize + offset
	default:
		return 0, errSeekToInvalidWhence
	}

	if r.pos != pos {
		if pos < 0 {
			r.err = errSeekToNegativePosition
			return 0, r.err
		}
		if err := r.chunkReader.SeekToChunkContaining(pos); err != nil {
			r.err = err
			return 0, r.err
		}
		r.pos = pos

		// Maintain the dRange/pos invariant.
		r.dRange[0] = pos
		r.dRange[1] = pos

		// Reset to "State A".
		r.decompressor = nil
		r.inImplicitZeroes = false
	}

	if limit > r.chunkReader.decompressedSize {
		limit = r.chunkReader.decompressedSize
	}
	r.posLimit = limit

	return r.pos, nil
}

// Close implements io.Closer.
//
// Calling Close will call Close on all of r's CodecReaders.
//
// r.ReadSeeker will not be accessed after Close returns. If r.Concurrency is
// non-zero, this may involve waiting for various goroutines to shut down,
// which may take some time. If the caller does not care about waiting until it
// is safe to close or otherwise release the r.ReadSeeker's resources, call
// CloseWithoutWaiting instead.
//
// It is not safe to call Close from a separate goroutine while another method
// call like Read or Seek is in progress.
func (r *Reader) Close() error {
	return r.close(false)
}

// CloseWithoutWaiting is like Close but does not wait until it is safe to
// close or otherwise release the r.ReadSeeker's resources.
func (r *Reader) CloseWithoutWaiting() error {
	return r.close(true)
}

func (r *Reader) close(withoutWaiting bool) error {
	if r.closed {
		return r.err
	}
	r.closed = true

	if err := r.initialize(); r.err == nil {
		r.err = err
	}

	for _, c := range r.CodecReaders {
		if err := c.Close(); r.err == nil {
			r.err = err
		}
	}

	if withoutWaiting {
		if err := r.concReader.CloseWithoutWaiting(); r.err == nil {
			r.err = err
		}
	} else {
		if err := r.concReader.Close(); r.err == nil {
			r.err = err
		}
	}

	if r.err == nil {
		r.err = errAlreadyClosed
		return nil
	}
	return r.err
}
