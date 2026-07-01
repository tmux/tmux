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

// draw-with-mask.go draws one image using a second image as an alpha mask
// (scaled to the size fo the first image). The resultant image is PNG-encoded
// to stdout.
//
// The result isn't necessarily beautiful, but it is an easy way to generate
// test images containing an alpha channel.
//
// Usage: go run draw-with-mask.go color.png alpha.png > foo.png

import (
	"bufio"
	"errors"
	"image"
	"image/png"
	"os"

	"golang.org/x/image/draw"

	_ "image/gif"
	_ "image/jpeg"
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	if len(os.Args) != 3 {
		return errors.New(`bad arguments: usage: "go draw-with-mask.go color.png alpha.png"`)
	}
	color, err := decode(os.Args[1])
	if err != nil {
		return err
	}
	alpha, err := decode(os.Args[2])
	if err != nil {
		return err
	}
	b := color.Bounds()
	mask := makeMask(b, alpha)

	dst := image.NewRGBA(b)
	draw.DrawMask(dst, b, color, b.Min, mask, mask.Bounds().Min, draw.Src)

	w := bufio.NewWriter(os.Stdout)
	defer w.Flush()
	return png.Encode(w, dst)
}

func decode(filename string) (image.Image, error) {
	f, err := os.Open(filename)
	if err != nil {
		return nil, err
	}
	defer f.Close()
	m, _, err := image.Decode(f)
	return m, err
}

func makeMask(bounds image.Rectangle, src image.Image) *image.Alpha {
	dst := image.NewGray(bounds)
	draw.CatmullRom.Scale(dst, dst.Bounds(), src, src.Bounds(), draw.Src, nil)
	return &image.Alpha{
		Pix:    dst.Pix,
		Stride: dst.Stride,
		Rect:   dst.Rect,
	}
}
