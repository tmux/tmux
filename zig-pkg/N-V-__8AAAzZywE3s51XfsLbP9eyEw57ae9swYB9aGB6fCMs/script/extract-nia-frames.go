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

// extract-nia-frames.go extracts the individual frames from a NIA animation,
// writing them out as NIE images.
//
// Usage: go run extract-nia-frames.go foo.nia bar/baz.nia
//
// This will write new files:
//  - foo.000000.nie
//  - foo.000001.nie
//  - foo.000002.nie
//  - foo.000003.nie
//  - etc
//  - bar/baz.000000.nie
//  - bar/baz.000001.nie
//  - etc

import (
	"bufio"
	"errors"
	"flag"
	"fmt"
	"io"
	"os"
	"path/filepath"
)

var (
	dryRun = flag.Bool("dry-run", false, "just print metadata; don't write NIE files")
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	flag.Parse()
	for _, a := range flag.Args() {
		if err := extract(a); err != nil {
			return err
		}
	}
	return nil
}

func extract(srcFilename string) error {
	fmt.Printf("Extract     %s\n", srcFilename)
	f, err := os.Open(srcFilename)
	if err != nil {
		return err
	}
	defer f.Close()
	r := bufio.NewReader(f)

	dstFilenamePrefix := srcFilename[:len(srcFilename)-len(filepath.Ext(srcFilename))]

	buf := [16]byte{}
	if _, err := io.ReadFull(r, buf[:16]); err != nil {
		return err
	}
	depth8 := buf[7] == '8'
	width := u32le(buf[8:])
	height := u32le(buf[12:])
	padded := !depth8 && ((width & 1) != 0) && ((height & 1) != 0)
	if (u32le(buf[:]) != 0x41AF_C36E) || // 0x41AF_C36E is 'nïA'le.
		(buf[4] != 0xFF) ||
		(buf[5] != 'b') ||
		((buf[6] != 'n') && (buf[6] != 'p')) ||
		((buf[7] != '4') && (buf[7] != '8')) ||
		(width >= 0x8000_0000) ||
		(height >= 0x8000_0000) {
		return errors.New("bad NIA header")
	}
	fmt.Printf("    Config    v1-b%c%c\n", buf[6], buf[7])
	fmt.Printf("    Width     %d\n", width)
	fmt.Printf("    Height    %d\n", height)

	nieSize := calculateNIESize(depth8, width, height)
	if nieSize < 0 {
		return errors.New("unsupported NIA (image dimensions are too large)")
	}

	prevCDD := uint64(0)
	for frameIndex := uint64(0); ; frameIndex++ {
		if _, err := io.ReadFull(r, buf[:8]); err != nil {
			return err
		}
		cdd := u64le(buf[:])
		if (cdd >> 32) == 0x8000_0000 {
			fmt.Printf("    LoopCount %d\n", uint32(cdd))
			break
		} else if ((cdd >> 32) > 0x8000_0000) || (cdd < prevCDD) {
			return errors.New("bad CDD (Cumulative Display Duration)")
		}
		fmt.Printf("    CDD       %16d flicks = %8g seconds; delta = %8g seconds\n",
			cdd,
			float64(cdd)/705_600000,
			float64(cdd-prevCDD)/705_600000,
		)
		prevCDD = cdd

		dstFilename := fmt.Sprintf("%s.%06d.nie", dstFilenamePrefix, frameIndex)
		if err := extract1(dstFilename, r, int64(nieSize)); err != nil {
			return err
		}

		if padded {
			if _, err := io.ReadFull(r, buf[:4]); err != nil {
				return err
			} else if u32le(buf[:]) != 0 {
				return errors.New("bad padding")
			}
		}
	}

	return nil
}

func calculateNIESize(depth8 bool, width uint32, height uint32) int64 {
	const maxInt64 = (1 << 63) - 1
	const maxUint64 = (1 << 64) - 1

	n := uint64(width) * uint64(height) * 4
	if depth8 {
		if n > (maxUint64 / 2) {
			return -1
		}
		n *= 2
	}
	if n > (maxInt64 - 16) {
		return -1
	}
	return int64(n + 16)
}

func extract1(dstFilename string, r *bufio.Reader, nieSize int64) error {
	if *dryRun {
		fmt.Printf("    DryRun    %s\n", dstFilename)
		_, err := io.CopyN(io.Discard, r, nieSize)
		return err
	}

	fmt.Printf("    Write     %s\n", dstFilename)
	f, err := os.Create(dstFilename)
	if err != nil {
		return err
	}
	_, err1 := io.CopyN(f, r, nieSize)
	err2 := f.Close()
	if err1 != nil {
		return err1
	}
	return err2
}

func u32le(b []byte) uint32 {
	return (uint32(b[0]) << 0) |
		(uint32(b[1]) << 8) |
		(uint32(b[2]) << 16) |
		(uint32(b[3]) << 24)
}

func u64le(b []byte) uint64 {
	return (uint64(b[0]) << 0) |
		(uint64(b[1]) << 8) |
		(uint64(b[2]) << 16) |
		(uint64(b[3]) << 24) |
		(uint64(b[4]) << 32) |
		(uint64(b[5]) << 40) |
		(uint64(b[6]) << 48) |
		(uint64(b[7]) << 56)
}
