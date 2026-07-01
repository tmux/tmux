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

// Package compression provides common types for other compression packages.
package compression

import (
	"io"
)

// Reader is an io.ReadCloser with a Reset method.
type Reader interface {
	io.ReadCloser

	// Reset resets this Reader to switch to a new underlying io.Reader. This
	// permits re-using a Reader instead of allocating a new one.
	//
	// It is the same as the standard library's zlib.Resetter's method.
	Reset(r io.Reader, dictionary []byte) error
}

// Writer is an io.WriteCloser with a Reset method.
type Writer interface {
	io.WriteCloser

	// Reset resets this Writer to switch to a new underlying io.Writer. This
	// permits re-using a Writer instead of allocating a new one.
	Reset(w io.Writer, dictionary []byte, level Level) error
}

// Level configures the compression trade-off between speed and size.
//
// A lower (more negative) value means better speed (faster). A higher (more
// positive) value means better size (smaller).
type Level int32

const (
	// LevelFastest means to maximize compression speed.
	LevelFastest Level = -2 << 10

	// LevelFast means to prioritize compression speed.
	//
	// It might not be as fast as LevelFastest, if doing so would require
	// substantial additional compute resources (e.g. CPU or RAM) or give up
	// substantial compression size.
	LevelFast Level = -1 << 10

	// LevelDefault means to use a reasonable default.
	LevelDefault Level = 0

	// LevelSmall means to prioritize compression size.
	//
	// It might not be as small as LevelSmallest, if doing so would require
	// substantial additional compute resources (e.g. CPU or RAM) or give up
	// substantial compression speed.
	LevelSmall Level = +1 << 10

	// LevelSmallest means to minimize compression size.
	LevelSmallest Level = +2 << 10

	// gap is the difference between each named Level.
	gap = 1 << 10
)

// Interpolate translates from Level (a codec-independent concept) to a
// codec-specific compression level numbering scheme. For exampe, a zlib
// package might translate LevelFastest to 1 and LevelSmallest to 9 by calling
// Interpolate(1, 2, 6, 9, 9).
//
// The mapping is via piecewise linear interpolation, given by five points in
// (Level, int32) space: (LevelFastest, fastest), (LevelFast, fast), etc.
func (l Level) Interpolate(fastest int32, fast int32, default_ int32, small int32, smallest int32) int32 {
	if l == 0 {
		return default_
	} else if l <= LevelFastest {
		return fastest
	} else if l <= LevelFast {
		return interpolate(-l+LevelFast, fast, fastest)
	} else if l < 0 {
		return interpolate(-l, default_, fast)
	} else if l >= LevelSmallest {
		return smallest
	} else if l >= LevelSmall {
		return interpolate(+l-LevelSmall, small, smallest)
	} else if l > 0 {
		return interpolate(+l, default_, small)
	}
	panic("unreachable")
}

func interpolate(relative Level, ia int32, ib int32) int32 {
	x := int64(ib-ia) * int64(relative) / gap
	return ia + int32(x)
}
