// Copyright 2021 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

//go:build ignore
// +build ignore

package main

// strip-png-ancillary-chunks.go copies PNG data from stdin to stdout, removing
// any ancillary chunks.
//
// Specification-compliant PNG decoders are required to honor critical chunks
// but may ignore ancillary (non-critical) chunks. Stripping out ancillary
// chunks before decoding should mean that different PNG decoders will agree on
// the decoded output regardless of which ancillary chunk types they choose to
// honor. Specifically, some PNG decoders may implement color and gamma
// correction but not all do.
//
// This program will strip out all ancillary chunks, but it should be
// straightforward to copy-paste-and-modify it to strip out only certain chunk
// types (e.g. only "tRNS" transparency chunks).
//
// --------
//
// A PNG file consists of an 8-byte magic identifier and then a series of
// chunks. Each chunk is:
//
//  - a 4-byte uint32 payload length N.
//  - a 4-byte chunk type (e.g. "gAMA" for gamma correction metadata).
//  - an N-byte payload.
//  - a 4-byte CRC-32 checksum of the previous (N + 4) bytes, including the
//    chunk type but excluding the payload length.
//
// Chunk types consist of 4 ASCII letters. The upper-case / lower-case bit of
// the first letter denote critical or ancillary chunks: "IDAT" and "PLTE" are
// critical, "gAMA" and "tEXt" are ancillary. See
// https://www.w3.org/TR/2003/REC-PNG-20031110/#5Chunk-naming-conventions
//
// --------
//
// Usage:
//  go run strip-png-ancillary-chunks.go             < in.png > out.png
//
// Adding a -random=123 flag should not change the output:
//  go run strip-png-ancillary-chunks.go -random=123 < in.png > out.png

import (
	"encoding/binary"
	"flag"
	"io"
	"math/rand"
	"os"
	"os/signal"
	"syscall"
)

var (
	randomFlag = flag.Int64("random", 0, "If non-zero, the seed for generating smaller reads")
)

// chunkTypeAncillaryBit is whether the first byte of a big-endian uint32 chunk
// type (the first of four ASCII letters) is lower-case.
const chunkTypeAncillaryBit = 0x20000000

// RandomSizedReader wraps another io.Reader such that the Reader.Read []byte
// argument has randomized lengths, up to a maximum of 9 bytes.
//
// This is slower but helps manually test that PNGAncillaryChunkStripper
// produces identical output regardless of whether Read is called many times
// (with small buffers) or few times (with large buffers). Specifically, this
// exercises having Read boundaries straddle chunk boundaries.
type RandomSizedReader struct {
	// Reader is the wrapped io.Reader.
	Reader io.Reader

	// Rand is the random number generator.
	Rand *rand.Rand
}

// Read implements io.Reader.
func (r *RandomSizedReader) Read(p []byte) (int, error) {
	if n := r.Rand.Intn(10); len(p) > n {
		p = p[:n]
	}
	return r.Reader.Read(p)
}

// PNGAncillaryChunkStripper wraps another io.Reader to strip ancillary chunks,
// if the data is in the PNG file format. If the data isn't PNG, it is passed
// through unmodified.
type PNGAncillaryChunkStripper struct {
	// Reader is the wrapped io.Reader.
	Reader io.Reader

	// stickyErr is the first error returned from the wrapped io.Reader.
	stickyErr error

	// buffer[rIndex:wIndex] holds data read from the wrapped io.Reader that
	// wasn't passed through yet.
	buffer [8]byte
	rIndex int
	wIndex int

	// pending and discard is the number of remaining bytes for (and whether to
	// discard or pass through) the current chunk-in-progress.
	pending int64
	discard bool

	// notPNG is set true if the data stream doesn't start with the 8-byte PNG
	// magic identifier. If true, the wrapped io.Reader's data (including the
	// first up-to-8 bytes) is passed through without modification.
	notPNG bool

	// seenMagic is whether we've seen the 8-byte PNG magic identifier.
	seenMagic bool
}

// Read implements io.Reader.
func (r *PNGAncillaryChunkStripper) Read(p []byte) (int, error) {
	for {
		// If the wrapped io.Reader returned a non-nil error, drain r.buffer
		// (what data we have) and return that error (if fully drained).
		if r.stickyErr != nil {
			n := copy(p, r.buffer[r.rIndex:r.wIndex])
			r.rIndex += n
			if r.rIndex < r.wIndex {
				return n, nil
			}
			return n, r.stickyErr
		}

		// Handle trivial requests, including draining our buffer.
		if len(p) == 0 {
			return 0, nil
		} else if r.rIndex < r.wIndex {
			n := copy(p, r.buffer[r.rIndex:r.wIndex])
			r.rIndex += n
			return n, nil
		}

		// From here onwards, our buffer is drained: r.rIndex == r.wIndex.

		// Handle non-PNG input.
		if r.notPNG {
			return r.Reader.Read(p)
		}

		// Continue processing any PNG chunk that's in progress, whether
		// discarding it or passing it through.
		for r.pending > 0 {
			if int64(len(p)) > r.pending {
				p = p[:r.pending]
			}
			n, err := r.Reader.Read(p)
			r.pending -= int64(n)
			r.stickyErr = err
			if r.discard {
				continue
			}
			return n, err
		}

		// We're either expecting the 8-byte PNG magic identifier or the 4-byte
		// PNG chunk length + 4-byte PNG chunk type. Either way, read 8 bytes.
		r.rIndex = 0
		r.wIndex, r.stickyErr = io.ReadFull(r.Reader, r.buffer[:8])
		if r.stickyErr != nil {
			// Undo io.ReadFull converting io.EOF to io.ErrUnexpectedEOF.
			if r.stickyErr == io.ErrUnexpectedEOF {
				r.stickyErr = io.EOF
			}
			continue
		}

		// Process those 8 bytes, either:
		//  - a PNG chunk (if we've already seen the PNG magic identifier),
		//  - the PNG magic identifier itself (if the input is a PNG) or
		//  - something else (if it's not a PNG).
		if r.seenMagic {
			// The number of pending bytes is equal to (N + 4) because of the 4
			// byte trailer, a checksum.
			r.pending = int64(binary.BigEndian.Uint32(r.buffer[:4])) + 4
			chunkType := binary.BigEndian.Uint32(r.buffer[4:])
			r.discard = (chunkType & chunkTypeAncillaryBit) != 0
			if r.discard {
				r.rIndex = r.wIndex
			}
		} else if string(r.buffer[:8]) == "\x89PNG\x0D\x0A\x1A\x0A" {
			r.seenMagic = true
		} else {
			r.notPNG = true
		}
	}
}

func main() {
	// Piping to /usr/bin/head can cause SIGPIPE (in the upstream process)
	// since head can exit early, leaving this process with pending output. The
	// command line experience can be cleaner if this program ignores SIGPIPE.
	signal.Ignore(syscall.SIGPIPE)

	flag.Parse()

	r := io.Reader(&PNGAncillaryChunkStripper{
		Reader: os.Stdin,
	})

	if *randomFlag != 0 {
		r = &RandomSizedReader{
			Reader: r,
			Rand:   rand.New(rand.NewSource(*randomFlag)),
		}
	}

	io.Copy(os.Stdout, r)
}
