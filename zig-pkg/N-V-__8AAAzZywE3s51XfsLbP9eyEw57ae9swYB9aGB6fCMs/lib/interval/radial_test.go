// Copyright 2018 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package interval

// This file provides "radial numbers", which partition the infinite set
// ℤ∪{NaN} into a finite number of boxes. The source set ℤ∪{NaN} is the set of
// integers, ℤ, augmented with a "Not a Number" element to represent the result
// of computations such as a division by zero or a shift by a negative integer.
// NaN-ness is viral: if x or y is NaN then (x op y) is also NaN, for any
// binary operator op.
//
// Specifically, there are (2*R + 4) boxes, for some non-negative integer
// radius R, and all but two of them contain exactly one element. There are
// (2*R + 1) single-element boxes for "small" integers: those whose absolute
// values are less than or equal to R. There is another single-element box for
// NaN. The two remaining boxes hold "large" integers: those that are less than
// -R and those that are greater than +R.
//
// A rough analogy for a radial number is a primitive counting system: one,
// two, three, many.
//
// This file defines two concrete "radial number" types:
//  - radialInput  has a radius R of 15.
//  - radialOutput has a radius R of 16 << 16 (which equals 1048576).
//
// If x and y are "small" radialInput values or one of the two "smallest large"
// radialInput values, i.e. x and y are in the range [-16 ..= +16], then (x op
// y) will always be a "small" radialOutput value, for the common binary
// operators: add, subtract, multiply, divide, left-shift, right-shift, and,
// or.
//
// Both of these radialInput and radialOutput types are encoded as an int32:
//  - math.MinInt32 (which equals -1 << 31) encodes a NaN.
//  - "small" numbers (within the interval [-R ..= +R]) encode themselves.
//  - any other negative int32 encodes "less than -R".
//  - any other positive int32 encodes "greater than +R".
//
// For example, radialInput(20) and radialInput(30) represent the same box:
// integers greater than +15.
//
// Binary operators take two radialInput values and produce a pair of
// radialOutput values: either a [min ..= max] interval, or [NaN ..= NaN]. For
// example, adding the "-3" box to the "greater than +15" box would produce the
// radialOutPair ["13" ..= "greater than +16 << 16"], or in more conventional
// notation, the half-open interval [13 ..= +∞].
//
// These radial number types are not exported by package interval, as the
// radius values (15 and 16 << 16) are somewhat arbitrary and not so generally
// useful. They are only used for brute force testing in interval_test.go. When
// tests in that other file use radial numbers (via calling the bruteForce
// function), the test cases are constructed so that every non-nil member of an
// interval.IntRange maps to a "small" radialInput, and asSmallRadialInput will
// panic if that doesn't hold.
//
// These types exist in order to simplify calculations. The general problem of
// calculating bounds for (x * y), given intervals for x and y, becomes
// complicated if e.g. x's possible values include both positive and negative
// numbers. In comparison, a radial number's box is either a single value, or
// if multiple values, those values all have the same sign. Computation on
// "small" radial numbers can use Go's built-in operators (+, -, etc) instead
// of the clumsier *big.Int API.
//
// The radial number methods like Add and Mul also have a simpler API than the
// corresponding *big.Int methods, since the radial number methods don't
// allocate memory or therefore need a destination argument.

import (
	"math/big"
)

const (
	radialNaN = -1 << 31

	// Note that radialInput.And and radialInput.Or require that (riRadius + 1)
	// is a power of 2.
	riRadius = 15
	roRadius = 16 << 16

	roLargeNeg = -(16<<16 + 1)
	roLargePos = +(16<<16 + 1)
)

type (
	radialInput   int32
	radialOutput  int32
	radialOutPair [2]radialOutput
)

func (x radialInput) canonicalize() radialOutput {
	o := radialOutput(x)
	if o != radialNaN {
		// No-op.
	} else if o < -riRadius {
		return -riRadius - 1
	} else if o > +riRadius {
		return +riRadius + 1
	}
	return o
}

func (x radialOutput) bigInt() *big.Int {
	if x < -roRadius || +roRadius < x {
		return nil
	}
	return big.NewInt(int64(x))
}

func asSmallRadialInput(x *big.Int, defaultIfXIsNil radialInput) radialInput {
	if x == nil {
		return defaultIfXIsNil
	}
	if !x.IsInt64() {
		panic("asSmallRadialInput passed too large a value")
	}
	i := x.Int64()
	if i < -riRadius || +riRadius < i {
		panic("asSmallRadialInput passed too large a value")
	}
	return radialInput(i)
}

func enumerate(x IntRange) (min radialInput, max radialInput) {
	if x.Empty() {
		return +1, -1
	}
	return asSmallRadialInput(x[0], -riRadius-1), asSmallRadialInput(x[1], +riRadius+1)
}

func (x radialInput) Add(y radialInput) radialOutPair {
	if x == radialNaN || y == radialNaN {
		return radialOutPair{radialNaN, radialNaN}
	}
	ox := x.canonicalize()
	oy := y.canonicalize()

	switch {
	case ox < -riRadius:
		if oy > +riRadius {
			return radialOutPair{roLargeNeg, roLargePos}
		}
		return radialOutPair{roLargeNeg, ox + oy}
	case ox > +riRadius:
		if oy < -riRadius {
			return radialOutPair{roLargeNeg, roLargePos}
		}
		return radialOutPair{ox + oy, roLargePos}
	default:
		switch {
		case oy < -riRadius:
			return radialOutPair{roLargeNeg, ox + oy}
		case oy > +riRadius:
			return radialOutPair{ox + oy, roLargePos}
		default:
			return radialOutPair{ox + oy, ox + oy}
		}
	}
}

func (x radialInput) Sub(y radialInput) radialOutPair {
	if x == radialNaN || y == radialNaN {
		return radialOutPair{radialNaN, radialNaN}
	}
	ox := x.canonicalize()
	oy := y.canonicalize()

	switch {
	case ox < -riRadius:
		switch {
		case oy < -riRadius, oy > +riRadius:
			return radialOutPair{roLargeNeg, roLargePos}
		default:
			return radialOutPair{roLargeNeg, ox - oy}
		}
	case ox > +riRadius:
		switch {
		case oy < -riRadius, oy > +riRadius:
			return radialOutPair{roLargeNeg, roLargePos}
		default:
			return radialOutPair{ox - oy, roLargePos}
		}
	default:
		switch {
		case oy < -riRadius:
			return radialOutPair{ox - oy, roLargePos}
		case oy > +riRadius:
			return radialOutPair{roLargeNeg, ox - oy}
		default:
			return radialOutPair{ox - oy, ox - oy}
		}
	}
}

func (x radialInput) Mul(y radialInput) radialOutPair {
	if x == radialNaN || y == radialNaN {
		return radialOutPair{radialNaN, radialNaN}
	}
	ox := x.canonicalize()
	oy := y.canonicalize()

	switch {
	case ox < -riRadius:
		if oy < 0 {
			return radialOutPair{ox * oy, roLargePos}
		} else if oy > 0 {
			return radialOutPair{roLargeNeg, ox * oy}
		}
		return radialOutPair{0, 0}
	case ox > +riRadius:
		if oy < 0 {
			return radialOutPair{roLargeNeg, ox * oy}
		} else if oy > 0 {
			return radialOutPair{ox * oy, roLargePos}
		}
		return radialOutPair{0, 0}
	default:
		switch {
		case oy < -riRadius:
			if ox < 0 {
				return radialOutPair{ox * oy, roLargePos}
			} else if ox > 0 {
				return radialOutPair{roLargeNeg, ox * oy}
			}
			return radialOutPair{0, 0}
		case oy > +riRadius:
			if ox < 0 {
				return radialOutPair{roLargeNeg, ox * oy}
			} else if ox > 0 {
				return radialOutPair{ox * oy, roLargePos}
			}
			return radialOutPair{0, 0}
		default:
			return radialOutPair{ox * oy, ox * oy}
		}
	}
}

func (x radialInput) Lsh(y radialInput) radialOutPair {
	if x == radialNaN || y == radialNaN || y < 0 {
		return radialOutPair{radialNaN, radialNaN}
	}
	ox := x.canonicalize()
	oy := uint32(y.canonicalize())

	switch {
	case ox < -riRadius:
		return radialOutPair{roLargeNeg, ox << oy}
	case ox > +riRadius:
		return radialOutPair{ox << oy, roLargePos}
	default:
		if oy <= +riRadius {
			return radialOutPair{ox << oy, ox << oy}
		} else if ox < 0 {
			return radialOutPair{roLargeNeg, ox << oy}
		} else if ox > 0 {
			return radialOutPair{ox << oy, roLargePos}
		}
		return radialOutPair{0, 0}
	}
}

func (x radialInput) Quo(y radialInput) radialOutPair {
	if x == radialNaN || y == radialNaN || y == 0 {
		return radialOutPair{radialNaN, radialNaN}
	}
	ox := x.canonicalize()
	oy := y.canonicalize()

	switch {
	case ox < -riRadius:
		switch {
		case oy < -riRadius:
			return radialOutPair{0, roLargePos}
		case oy > +riRadius:
			return radialOutPair{roLargeNeg, 0}
		default:
			if oy < 0 {
				return radialOutPair{ox / oy, roLargePos}
			}
			return radialOutPair{roLargeNeg, ox / oy}
		}
	case ox > +riRadius:
		switch {
		case oy < -riRadius:
			return radialOutPair{roLargeNeg, 0}
		case oy > +riRadius:
			return radialOutPair{0, roLargePos}
		default:
			if oy < 0 {
				return radialOutPair{roLargeNeg, ox / oy}
			}
			return radialOutPair{ox / oy, roLargePos}
		}
	default:
		switch {
		case oy < -riRadius, oy > +riRadius:
			return radialOutPair{0, 0}
		default:
			return radialOutPair{ox / oy, ox / oy}
		}
	}
}

func (x radialInput) Rsh(y radialInput) radialOutPair {
	if x == radialNaN || y == radialNaN || y < 0 {
		return radialOutPair{radialNaN, radialNaN}
	}
	ox := x.canonicalize()
	oy := uint32(y.canonicalize())

	switch {
	case ox < -riRadius:
		if oy > +riRadius {
			return radialOutPair{roLargeNeg, -1}
		}
		return radialOutPair{roLargeNeg, ox >> oy}
	case ox > +riRadius:
		if oy > +riRadius {
			return radialOutPair{0, roLargePos}
		}
		return radialOutPair{ox >> oy, roLargePos}
	default:
		if oy <= +riRadius {
			return radialOutPair{ox >> oy, ox >> oy}
		} else if ox < 0 {
			return radialOutPair{-1, -1}
		}
		return radialOutPair{0, 0}
	}
}

func (x radialInput) And(y radialInput) radialOutPair {
	if x == radialNaN || y == radialNaN {
		return radialOutPair{radialNaN, radialNaN}
	}
	ox := x.canonicalize()
	oy := y.canonicalize()

	// r is a power of 2, so that its binary representation contains one "1"
	// digit, and that digit is not shared with any "small" value <= riRadius.
	const r = riRadius + 1

	if ox < -riRadius {
		if oy < -riRadius {
			return radialOutPair{roLargeNeg, -r}
		} else if oy > +riRadius {
			return radialOutPair{0, roLargePos}
		} else if oy < 0 {
			return radialOutPair{roLargeNeg, -r}
		} else {
			return radialOutPair{0, oy}
		}
	} else if ox > +riRadius {
		if oy < -riRadius {
			return radialOutPair{0, roLargePos}
		} else if oy > +riRadius {
			return radialOutPair{0, roLargePos}
		} else if oy < 0 {
			return radialOutPair{+r, roLargePos}
		} else {
			return radialOutPair{0, oy}
		}
	}

	if oy < -riRadius {
		if ox < 0 {
			return radialOutPair{roLargeNeg, -r}
		} else {
			return radialOutPair{0, ox}
		}
	} else if oy > +riRadius {
		if ox < 0 {
			return radialOutPair{+r, roLargePos}
		} else {
			return radialOutPair{0, ox}
		}
	}

	return radialOutPair{ox & oy, ox & oy}
}

func (x radialInput) Or(y radialInput) radialOutPair {
	if x == radialNaN || y == radialNaN {
		return radialOutPair{radialNaN, radialNaN}
	}
	ox := x.canonicalize()
	oy := y.canonicalize()

	// r is a power of 2, so that its binary representation contains one "1"
	// digit, and that digit is not shared with any "small" value <= riRadius.
	const r = riRadius + 1

	if ox < -riRadius {
		if oy < -riRadius {
			return radialOutPair{roLargeNeg, -1}
		} else if oy > +riRadius {
			return radialOutPair{roLargeNeg, -1}
		} else if oy < 0 {
			return radialOutPair{oy, -1}
		} else {
			return radialOutPair{roLargeNeg, oy | -r}
		}
	} else if ox > +riRadius {
		if oy < -riRadius {
			return radialOutPair{roLargeNeg, -1}
		} else if oy > +riRadius {
			return radialOutPair{+r, roLargePos}
		} else if oy < 0 {
			return radialOutPair{oy, -1}
		} else {
			return radialOutPair{oy | +r, roLargePos}
		}
	}

	if oy < -riRadius {
		if ox < 0 {
			return radialOutPair{ox, -1}
		} else {
			return radialOutPair{roLargeNeg, ox | -r}
		}
	} else if oy > +riRadius {
		if ox < 0 {
			return radialOutPair{ox, -1}
		} else {
			return radialOutPair{ox | +r, roLargePos}
		}
	}

	return radialOutPair{ox | oy, ox | oy}
}

var riOperators = map[rune]func(radialInput, radialInput) radialOutPair{
	'+': radialInput.Add,
	'-': radialInput.Sub,
	'*': radialInput.Mul,
	'/': radialInput.Quo,
	'«': radialInput.Lsh,
	'»': radialInput.Rsh,
	'&': radialInput.And,
	'|': radialInput.Or,
}

func bruteForce(x IntRange, y IntRange, opKey rune) (z IntRange, ok bool) {
	op := riOperators[opKey]
	iMin, iMax := enumerate(x)
	jMin, jMax := enumerate(y)
	result := radialOutPair{}
	first := true
	merge := func(k radialOutPair) {
		if first {
			result = k
			first = false
			return
		}
		if result[0] > k[0] {
			result[0] = k[0]
		}
		if result[1] < k[1] {
			result[1] = k[1]
		}
	}

	switch opKey {
	case '∪':
		if iMinC, iMaxC := iMin.canonicalize(), iMax.canonicalize(); iMinC <= iMaxC {
			merge(radialOutPair{iMinC, iMaxC})
		}
		if jMinC, jMaxC := jMin.canonicalize(), jMax.canonicalize(); jMinC <= jMaxC {
			merge(radialOutPair{jMinC, jMaxC})
		}
		if result[0] < -riRadius {
			result[0] = -roRadius - 1
		}
		if result[1] > +riRadius {
			result[1] = +roRadius + 1
		}

	case '∩':
		iMinC, iMaxC := iMin.canonicalize(), iMax.canonicalize()
		for j := jMin; j <= jMax; j++ {
			jC := j.canonicalize()
			if (iMinC <= jC) && (jC <= iMaxC) {
				merge(radialOutPair{jC, jC})
			}
		}
		if result[0] < -riRadius {
			result[0] = -roRadius - 1
		}
		if result[1] > +riRadius {
			result[1] = +roRadius + 1
		}

	default:
		for i := iMin; i <= iMax; i++ {
			for j := jMin; j <= jMax; j++ {
				k := op(i, j)
				if k[0] == radialNaN || k[1] == radialNaN {
					return IntRange{}, false
				}
				merge(k)
			}
		}
	}

	if first {
		return makeEmptyRange(), true
	}
	return IntRange{result[0].bigInt(), result[1].bigInt()}, true
}
