// Copyright 2022 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ----------------

// Package nie provides access to NIE (Naïve Image Format) files.
//
// The NIE specification is at
// https://github.com/google/wuffs/blob/main/doc/spec/nie-spec.md
package nie

import (
	"errors"
	"image"
	"image/color"
	"io"
)

func u32le(b []byte) uint32 {
	return (uint32(b[0]) << 0) |
		(uint32(b[1]) << 8) |
		(uint32(b[2]) << 16) |
		(uint32(b[3]) << 24)
}

func init() {
	image.RegisterFormat("nie", "nïE", Decode, DecodeConfig)
}

var (
	errImageIsTooLarge = errors.New("nie: image is too large")
	errInvalidHeader   = errors.New("nie: invalid header")
)

func decodeConfig(r io.Reader) (image.Config, byte, byte, error) {
	b := [16]byte{}
	_, err := io.ReadFull(r, b[:])
	if err != nil {
		return image.Config{}, 0, 0, err
	}
	if (u32le(b[:]) != 0x45afc36e) ||
		(b[4] != 0xff) ||
		(b[5] != 'b') ||
		((b[6] != 'n') && (b[6] != 'p')) ||
		((b[7] != '4') && (b[7] != '8')) {
		return image.Config{}, 0, 0, errInvalidHeader
	}

	cm := color.Model(nil)
	switch {
	case (b[6] == 'n') && (b[7] == '4'):
		cm = color.NRGBAModel
	case (b[6] == 'n') && (b[7] == '8'):
		cm = color.NRGBA64Model
	case (b[6] == 'p') && (b[7] == '4'):
		cm = color.RGBAModel
	case (b[6] == 'p') && (b[7] == '8'):
		cm = color.RGBA64Model
	default:
		return image.Config{}, 0, 0, errInvalidHeader
	}

	w := u32le(b[8:])
	h := u32le(b[12:])
	if (w > 0x7fffffff) || (h > 0x7fffffff) {
		return image.Config{}, 0, 0, errInvalidHeader
	}
	return image.Config{
		ColorModel: cm,
		Width:      int(w),
		Height:     int(h),
	}, b[6], b[7], nil
}

// DecodeConfig returns the color model and dimensions of a NIE image without
// decoding the entire image.
func DecodeConfig(r io.Reader) (image.Config, error) {
	cfg, _, _, err := decodeConfig(r)
	return cfg, err
}

// Decode reads a NIE image from r.
func Decode(r io.Reader) (image.Image, error) {
	cfg, b6, b7, err := decodeConfig(r)
	if err != nil {
		return nil, err
	}

	const u64max = 0xffffffffffffffff
	tooBig := false
	wh := uint64(cfg.Width) * uint64(cfg.Height)
	if b7 == '4' {
		tooBig = (wh > (u64max / 4)) ||
			((wh * 4) != uint64(int((wh * 4))))
	} else {
		tooBig = (wh > (u64max / 8)) ||
			((wh * 8) != uint64(int((wh * 8))))
	}
	if tooBig {
		return nil, errImageIsTooLarge
	}

	img, pix := image.Image(nil), []byte(nil)
	switch {
	case (b6 == 'n') && (b7 == '4'):
		m := image.NewNRGBA(image.Rect(0, 0, int(cfg.Width), int(cfg.Height)))
		img, pix = m, m.Pix
	case (b6 == 'n') && (b7 == '8'):
		m := image.NewNRGBA64(image.Rect(0, 0, int(cfg.Width), int(cfg.Height)))
		img, pix = m, m.Pix
	case (b6 == 'p') && (b7 == '4'):
		m := image.NewRGBA(image.Rect(0, 0, int(cfg.Width), int(cfg.Height)))
		img, pix = m, m.Pix
	case (b6 == 'p') && (b7 == '8'):
		m := image.NewRGBA64(image.Rect(0, 0, int(cfg.Width), int(cfg.Height)))
		img, pix = m, m.Pix
	}
	if _, err := io.ReadFull(r, pix); err != nil {
		return nil, err
	}

	// NIE is little-endian BGRA. Go is big-endian RGBA.
	if b7 == '4' {
		for i := 0; i < len(pix); i += 4 {
			pix[i+0], pix[i+2] = pix[i+2], pix[i+0]
		}
	} else {
		for i := 0; i < len(pix); i += 8 {
			pix[i+0], pix[i+1], pix[i+2], pix[i+3], pix[i+4], pix[i+5], pix[i+6], pix[i+7] =
				pix[i+5], pix[i+4], pix[i+3], pix[i+2], pix[i+1], pix[i+0], pix[i+7], pix[i+6]
		}
	}

	return img, err
}
