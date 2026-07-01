// Copyright 2020 The Wuffs Authors.
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

// convert-png-to-wbmp.go decodes PNG from stdin and encodes WBMP Type 0 to
// stdout.
//
// WBMP (Wireless Bitmap) is described in Section 6 and Appendix A of
// http://www.wapforum.org/what/technical/SPEC-WAESpec-19990524.pdf
//
// Usage: go run convert-png-to-wbmp.go < foo.png > foo.wbmp

import (
	"bufio"
	"errors"
	"image"
	"image/draw"
	"image/png"
	"os"
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	src, err := png.Decode(os.Stdin)
	if err != nil {
		return err
	}
	b := src.Bounds()
	dst := image.NewGray(b)
	draw.Draw(dst, b, src, b.Min, draw.Src)

	w := bufio.NewWriter(os.Stdout)
	defer w.Flush()

	// TypeField, FixHeaderField.
	w.WriteString("\x00\x00")

	// Width, height.
	if err := writeMultiByteInteger(w, b.Dx()); err != nil {
		return err
	}
	if err := writeMultiByteInteger(w, b.Dy()); err != nil {
		return err
	}

	// Rows.
	for y := b.Min.Y; y < b.Max.Y; y++ {
		i := dst.PixOffset(b.Min.X, y)
		j := dst.PixOffset(b.Max.X, y)
		if err := writeRow(w, dst.Pix[i:j]); err != nil {
			return err
		}
	}
	return nil
}

func writeMultiByteInteger(w *bufio.Writer, x int) error {
	if x < 0 {
		return errors.New("cannot encode negative integer")
	}
	n := 1
	for z := x; z > 0x7F; n, z = n+1, z>>7 {
	}
	for i := n - 1; i >= 0; i-- {
		c := (x >> uint(7*i)) & 0x7F
		if i > 0 {
			c |= 0x80
		}
		if err := w.WriteByte(byte(c)); err != nil {
			return err
		}
	}
	return nil
}

func writeRow(w *bufio.Writer, row []byte) error {
	// Pack as many complete groups of 8 pixels as we can.
	n := len(row) / 8
	for i := 0; i < n; i++ {
		row8 := row[i*8 : i*8+8]
		c := ((row8[0] & 0x80) >> 0) |
			((row8[1] & 0x80) >> 1) |
			((row8[2] & 0x80) >> 2) |
			((row8[3] & 0x80) >> 3) |
			((row8[4] & 0x80) >> 4) |
			((row8[5] & 0x80) >> 5) |
			((row8[6] & 0x80) >> 6) |
			((row8[7] & 0x80) >> 7)
		if err := w.WriteByte(c); err != nil {
			return err
		}
	}
	row = row[8*n:]

	// Pack up to 7 remaining pixels.
	if len(row) > 0 {
		c := byte(0)
		for i, x := range row {
			c |= (x & 0x80) >> uint(i)
		}
		if err := w.WriteByte(c); err != nil {
			return err
		}
	}
	return nil
}
