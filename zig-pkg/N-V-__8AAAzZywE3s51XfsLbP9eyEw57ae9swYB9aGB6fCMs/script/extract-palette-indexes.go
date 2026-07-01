// Copyright 2017 The Wuffs Authors.
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

// extract-palette-indexes.go extracts the 1-byte-per-pixel indexes and, for
// paletted images, the 4-byte-per-entry BGRA (premultiplied alpha) palette of
// a paletted or gray GIF and PNG image. It checks that the GIF and PNG encode
// equivalent images.
//
// Usage: go run extract-palette-indexes.go foo.gif foo.png

import (
	"bytes"
	"fmt"
	"image"
	"image/gif"
	"image/png"
	"os"
	"path/filepath"
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	baseFilename, bPalette, bIndexes := "", []byte(nil), []byte(nil)
	for i, a := range os.Args[1:] {
		b := a[:len(a)-len(filepath.Ext(a))]
		palette, indexes, err := decode(a)
		if err != nil {
			return err
		}
		if i == 0 {
			baseFilename, bPalette, bIndexes = b, palette, indexes
		} else if baseFilename != b {
			return fmt.Errorf("arguments do not have a common base path")
		} else if !bytes.Equal(bPalette, palette) {
			return fmt.Errorf("palettes are not equivalent")
		} else if !bytes.Equal(bIndexes, indexes) {
			return fmt.Errorf("indexes are not equivalent")
		}
	}
	if bPalette != nil {
		if err := os.WriteFile(baseFilename+".palette", bPalette, 0644); err != nil {
			return err
		}
	}
	if bIndexes != nil {
		if err := os.WriteFile(baseFilename+".indexes", bIndexes, 0644); err != nil {
			return err
		}
	}
	return nil
}

func decode(filename string) (palette []byte, indexes []byte, err error) {
	f, err := os.Open(filename)
	if err != nil {
		return nil, nil, err
	}
	defer f.Close()

	m, err := image.Image(nil), error(nil)
	switch ext := filepath.Ext(filename); ext {
	case ".gif":
		m, err = gif.Decode(f)
	case ".png":
		m, err = png.Decode(f)
	default:
		return nil, nil, fmt.Errorf("unsupported filename extension %q", ext)
	}
	if err != nil {
		return nil, nil, err
	}
	switch m := m.(type) {
	case *image.Gray:
		return nil, m.Pix, nil
	case *image.Paletted:
		palette = make([]byte, 4*256)
		allGray := true
		for i, c := range m.Palette {
			r, g, b, a := c.RGBA()
			palette[4*i+0] = uint8(b >> 8)
			palette[4*i+1] = uint8(g >> 8)
			palette[4*i+2] = uint8(r >> 8)
			palette[4*i+3] = uint8(a >> 8)
			if allGray {
				y := uint32(i)
				allGray = r>>8 == y && g>>8 == y && b>>8 == y
			}
		}
		if allGray {
			return nil, m.Pix, nil
		}
		return palette, m.Pix, nil
	}
	return nil, nil, fmt.Errorf("decoded image type: got %T, want *image.Paletted", m)
}
