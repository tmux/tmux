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

// Package raczlib provides access to RAC (Random Access Compression) files
// with the Zlib compression codec.
//
// The RAC specification is at
// https://github.com/google/wuffs/blob/main/doc/spec/rac-spec.md
package raczlib

import (
	"bytes"
	"compress/zlib"
	"errors"
	"io"

	"github.com/google/wuffs/lib/compression"
	"github.com/google/wuffs/lib/internal/racdict"
	"github.com/google/wuffs/lib/rac"
	"github.com/google/wuffs/lib/zlibcut"
)

var (
	errInvalidCodec      = errors.New("raczlib: invalid codec")
	errInvalidDictionary = errors.New("raczlib: invalid dictionary")
)

func u32BE(b []byte) uint32 {
	_ = b[3] // Early bounds check to guarantee safety of reads below.
	return (uint32(b[0]) << 24) | (uint32(b[1]) << 16) | (uint32(b[2]) << 8) | (uint32(b[3]))
}

// refine returns the last 32 KiB of b, or if b is shorter than that, it
// returns just b.
//
// The Zlib format only supports up to 32 KiB of history or shared dictionary.
func refine(b []byte) []byte {
	const n = 32 * 1024
	if len(b) > n {
		return b[len(b)-n:]
	}
	return b
}

// CodecReader specializes a rac.Reader to decode Zlib-compressed chunks.
type CodecReader struct {
	// cachedReader lets us re-use the memory allocated for a zlib reader, when
	// decompressing multiple chunks.
	cachedReader compression.Reader

	// lim provides a limited view of a RAC file.
	lim io.LimitedReader

	// dictLoader loads shared dictionaries.
	dictLoader racdict.Loader
}

// Close implements rac.CodecReader.
func (r *CodecReader) Close() error {
	return nil
}

// Accepts implements rac.CodecReader.
func (r *CodecReader) Accepts(c rac.Codec) bool {
	return c == rac.CodecZlib
}

// Clone implements rac.CodecReader.
func (r *CodecReader) Clone() rac.CodecReader {
	return &CodecReader{}
}

// MakeDecompressor implements rac.CodecReader.
func (r *CodecReader) MakeDecompressor(racFile io.ReadSeeker, chunk rac.Chunk) (io.Reader, error) {
	dict, err := r.dictLoader.Load(racFile, chunk)
	if err != nil {
		return nil, err
	}
	if _, err := racFile.Seek(chunk.CPrimary[0], io.SeekStart); err != nil {
		return nil, err
	}
	r.lim.R = racFile
	r.lim.N = chunk.CPrimary.Size()
	return r.makeDecompressor(&r.lim, dict)
}

// CodecWriter specializes a rac.Writer to encode Zlib-compressed chunks.
type CodecWriter struct {
	// These fields help reduce memory allocation by re-using previous byte
	// buffers or the zlib.Writer.
	compressed   bytes.Buffer
	cachedWriter *zlib.Writer

	// dictSaver saves shared dictionaries.
	dictSaver racdict.Saver
}

// Close implements rac.CodecWriter.
func (w *CodecWriter) Close() error {
	return nil
}

// Clone implements rac.CodecWriter.
func (w *CodecWriter) Clone() rac.CodecWriter {
	return &CodecWriter{}
}

// Compress implements rac.CodecWriter.
func (w *CodecWriter) Compress(p []byte, q []byte, resourcesData [][]byte) (
	codec rac.Codec, compressed []byte, secondaryResource int, tertiaryResource int, retErr error) {
	return w.dictSaver.Compress(
		p, q, resourcesData,
		rac.CodecZlib, w.compress, refine,
	)
}

func (w *CodecWriter) compress(p []byte, q []byte, dict []byte) ([]byte, error) {
	w.compressed.Reset()
	zw, err := (*zlib.Writer)(nil), error(nil)
	if len(dict) != 0 {
		// A zlib.Writer doesn't have a Reset(etc, dict) method, so we have to
		// allocate a new zlib.Writer.
		zw, err = zlib.NewWriterLevelDict(&w.compressed, zlib.BestCompression, dict)
		if err != nil {
			return nil, err
		}
	} else if w.cachedWriter == nil {
		zw, err = zlib.NewWriterLevelDict(&w.compressed, zlib.BestCompression, nil)
		if err != nil {
			return nil, err
		}
		w.cachedWriter = zw
	} else {
		zw = w.cachedWriter
		zw.Reset(&w.compressed)
	}

	if len(p) > 0 {
		if _, err := zw.Write(p); err != nil {
			zw.Close()
			return nil, err
		}
	}
	if len(q) > 0 {
		if _, err := zw.Write(q); err != nil {
			zw.Close()
			return nil, err
		}
	}

	if err := zw.Close(); err != nil {
		return nil, err
	}
	return w.compressed.Bytes(), nil
}

// CanCut implements rac.CodecWriter.
func (w *CodecWriter) CanCut() bool {
	return true
}

// Cut implements rac.CodecWriter.
func (w *CodecWriter) Cut(codec rac.Codec, encoded []byte, maxEncodedLen int) (encodedLen int, decodedLen int, retErr error) {
	if codec != rac.CodecZlib {
		return 0, 0, errInvalidCodec
	}
	return zlibcut.Cut(nil, encoded, maxEncodedLen)
}

// WrapResource implements rac.CodecWriter.
func (w *CodecWriter) WrapResource(raw []byte) ([]byte, error) {
	return w.dictSaver.WrapResource(raw, refine)
}
