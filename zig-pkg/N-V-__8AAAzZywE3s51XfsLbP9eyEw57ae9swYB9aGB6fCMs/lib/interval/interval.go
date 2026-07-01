// Copyright 2018 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ----------------

// Package interval provides interval arithmetic on big integers. Big means
// arbitrary-precision, as per the standard math/big package.
//
// For example, if x is in the interval [3 ..= 6] and y is in the interval [10
// ..= 15] then x+y is in the interval [13 ..= 21].
//
// As in Rust, the [m ..= n] syntax means all integers i such that (m <= i) and
// (i <= n). Unlike Rust, neither bound can be omitted, but they may be
// infinite. For example, if x is in the interval [3 ..= +∞] and y is in the
// interval [-4 ..= -2], then x*y is in the interval [-∞ ..= -6].
//
// As a motivating example, if a compiler knows that the integer-typed
// variables i and j are in the intervals [0 ..= 255] and [0 ..= 3], and that
// the array a has 1024 elements, then it can prove that the array-index
// expression a[4*i + j] is memory-safe without needing an at-runtime bounds
// check.
//
// This package depends only on the standard math/big package.
package interval

import (
	"math/big"
)

var (
	minusOne = big.NewInt(-1)
	one      = big.NewInt(+1)

	// sharedEmptyRange is an emtpy IntRange. Users should not modify it. It is
	// not exported, directly or indirectly (by another function returning its
	// elements).
	sharedEmptyRange = IntRange{one, minusOne}
)

var smallBitMasks = [...]*big.Int{
	big.NewInt(0x00),
	big.NewInt(0x01),
	big.NewInt(0x03),
	big.NewInt(0x07),
	big.NewInt(0x0F),
	big.NewInt(0x1F),
	big.NewInt(0x3F),
	big.NewInt(0x7F),
	big.NewInt(0xFF),
}

// bitMask returns ((1<<n) - 1), where n is the largest of n0 and n1.
func bitMask(n0 int, n1 int) *big.Int {
	n := n0
	if n < n1 {
		n = n1
	}
	if n < 0 {
		panic("unreachable")
	} else if n > (1 << 30) {
		panic("interval: input is too large")
	}

	if n < len(smallBitMasks) {
		return smallBitMasks[n]
	}
	z := big.NewInt(1)
	z = z.Lsh(z, uint(n))
	z = z.Sub(z, one)
	return z
}

func bigIntNewSet(i *big.Int) *big.Int {
	if i != nil {
		return big.NewInt(0).Set(i)
	}
	return nil
}

func bigIntNewNot(i *big.Int) *big.Int {
	if i != nil {
		return big.NewInt(0).Not(i)
	}
	return nil
}

func bigIntMul(i *big.Int, j *big.Int) *big.Int { return big.NewInt(0).Mul(i, j) }
func bigIntQuo(i *big.Int, j *big.Int) *big.Int { return big.NewInt(0).Quo(i, j) }

func bigIntLsh(i *big.Int, j *big.Int) *big.Int {
	if j.IsUint64() {
		if u := j.Uint64(); u <= 0xFFFFFFFF {
			return big.NewInt(0).Lsh(i, uint(u))
		}
	}

	// Fallback code path, copy-pasted to interval_test.go.
	k := big.NewInt(2)
	k.Exp(k, j, nil)
	k.Mul(i, k)
	return k
}

func bigIntRsh(i *big.Int, j *big.Int) *big.Int {
	if j.IsUint64() {
		if u := j.Uint64(); u <= 0xFFFFFFFF {
			return big.NewInt(0).Rsh(i, uint(u))
		}
	}

	// Fallback code path, copy-pasted to interval_test.go.
	k := big.NewInt(2)
	k.Exp(k, j, nil)
	k.Div(i, k) // This is explicitly Div, not Quo.
	return k
}

// biggerInt is either a non-nil *big.Int or ±∞.
type biggerInt struct {
	// extra being less than or greater than 0 means that the biggerInt is -∞
	// or +∞ respectively. extra being zero means that the biggerInt is i,
	// which should be a non-nil pointer.
	extra int32
	i     *big.Int
}

type biggerIntPair [2]biggerInt

// newBiggerIntPair returns a pair of biggerInt values, the first being +∞
// and the second being -∞.
func newBiggerIntPair() biggerIntPair { return biggerIntPair{{extra: +1}, {extra: -1}} }

// lowerMin sets x[0] to min(x[0], y).
func (x *biggerIntPair) lowerMin(y biggerInt) {
	if x[0].extra > 0 || y.extra < 0 ||
		(x[0].extra == 0 && y.extra == 0 && x[0].i.Cmp(y.i) > 0) {
		x[0] = y
	}
}

// raiseMax sets x[1] to max(x[1], y).
func (x *biggerIntPair) raiseMax(y biggerInt) {
	if x[1].extra < 0 || y.extra > 0 ||
		(x[1].extra == 0 && y.extra == 0 && x[1].i.Cmp(y.i) < 0) {
		x[1] = y
	}
}

func (x *biggerIntPair) toIntRange() IntRange {
	if x[0].extra > 0 || x[1].extra < 0 {
		return makeEmptyRange()
	}
	return IntRange{x[0].i, x[1].i}
}

func (x *biggerIntPair) fromIntRange(y IntRange) {
	if y[0] != nil {
		x[0] = biggerInt{i: big.NewInt(0).Set(y[0])}
	} else {
		x[0] = biggerInt{extra: -1}
	}
	if y[1] != nil {
		x[1] = biggerInt{i: big.NewInt(0).Set(y[1])}
	} else {
		x[1] = biggerInt{extra: +1}
	}
}

// IntRange is an integer interval. The array elements are the minimum and
// maximum values, inclusive (if non-nil) on both ends. A nil element means
// unbounded: negative or positive infinity.
//
// The zero value (zero in the Go sense, not in the integer sense) is a valid,
// infinitely sized interval, unbounded at both ends.
//
// IntRange's operator-like methods (Add, Mul, etc) return an IntRange whose
// *big.Int pointer values (if non-nil) are always distinct from its inputs'
// *big.Int pointer values. Specifically, after "z = x.Add(y)", mutating
// "*z[0]" will not affect "*x[0]", "*x[1]", "*y[0]" or "*y[1]".
//
// Those operator-like methods come in two forms: Foo and TryFoo. The TryFoo
// forms return (z IntRange, ok bool). The bool indicates success, as
// operations like dividing by zero or shifting by a negative value can fail.
// When TryFoo can never fail, there is also a Foo method that omits the
// always-true bool. For example, there are Add, TryAdd and TryLsh methods, but
// no Lsh method.
//
// A subtle point is that an interval's minimum or maximum can be infinite, but
// if an integer value i is known to be within such an interval, i's possible
// values are arbitrarily large but not infinite. Specifically, 0*i is
// unambiguously always equal to 0.
//
// It is also valid for the first element to be greater than the second
// element. This represents an empty interval. There is more than one
// representation of an empty interval.
//
// "Int" abbreviates "Integer" (the same as for big.Int), not "Interval".
// Similarly, we use "Range" instead of "Interval" to avoid unnecessary
// confusion, even though this type is indeed an integer interval.
type IntRange [2]*big.Int

// String returns a string representation of x.
func (x IntRange) String() string {
	if x.Empty() {
		return "[empty]"
	}
	buf := []byte(nil)
	if x[0] == nil {
		buf = append(buf, "[-∞ ..= "...)
	} else {
		buf = append(buf, '[')
		buf = x[0].Append(buf, 10)
		buf = append(buf, " ..= "...)
	}
	if x[1] == nil {
		buf = append(buf, "+∞]"...)
	} else {
		buf = x[1].Append(buf, 10)
		buf = append(buf, ']')
	}
	return string(buf)
}

// makeEmptyRange returns an empty IntRange: one that contains no elements.
func makeEmptyRange() IntRange {
	return IntRange{big.NewInt(+1), big.NewInt(-1)}
}

// ContainsNegative returns whether x contains at least one negative value.
func (x IntRange) ContainsNegative() bool {
	if x[0] == nil {
		return true
	}
	if x[0].Sign() >= 0 {
		return false
	}
	return x[1] == nil || x[0].Cmp(x[1]) <= 0
}

// ContainsNonNegative returns whether x contains at least one non-negative
// value.
func (x IntRange) ContainsNonNegative() bool {
	if x[1] == nil {
		return true
	}
	if x[1].Sign() < 0 {
		return false
	}
	return x[0] == nil || x[0].Cmp(x[1]) <= 0
}

// ContainsPositive returns whether x contains at least one positive value.
func (x IntRange) ContainsPositive() bool {
	if x[1] == nil {
		return true
	}
	if x[1].Sign() <= 0 {
		return false
	}
	return x[0] == nil || x[0].Cmp(x[1]) <= 0
}

// ContainsZero returns whether x contains zero.
func (x IntRange) ContainsZero() bool {
	return (x[0] == nil || x[0].Sign() <= 0) &&
		(x[1] == nil || x[1].Sign() >= 0)
}

// ContainsInt returns whether x contains i.
func (x IntRange) ContainsInt(i *big.Int) bool {
	return (x[0] == nil || x[0].Cmp(i) <= 0) &&
		(x[1] == nil || x[1].Cmp(i) >= 0)
}

// ContainsIntRange returns whether x contains every element of y.
//
// It returns true if y is empty.
func (x IntRange) ContainsIntRange(y IntRange) bool {
	if y.Empty() {
		return true
	}
	if (x[0] != nil) && (y[0] == nil || x[0].Cmp(y[0]) > 0) {
		return false
	}
	if (x[1] != nil) && (y[1] == nil || x[1].Cmp(y[1]) < 0) {
		return false
	}
	return true
}

// Eq returns whether x equals y.
func (x IntRange) Eq(y IntRange) bool {
	if xe, ye := x.Empty(), y.Empty(); xe || ye {
		return xe == ye
	}
	if x0, y0 := x[0] != nil, y[0] != nil; x0 != y0 {
		return false
	} else if x0 && x[0].Cmp(y[0]) != 0 {
		return false
	}
	if x1, y1 := x[1] != nil, y[1] != nil; x1 != y1 {
		return false
	} else if x1 && x[1].Cmp(y[1]) != 0 {
		return false
	}
	return true
}

// Empty returns whether x is empty.
func (x IntRange) Empty() bool {
	return x[0] != nil && x[1] != nil && x[0].Cmp(x[1]) > 0
}

// justZero returns whether x is the [0 ..= 0] interval, containing exactly one
// element: the integer zero.
func (x IntRange) justZero() bool {
	return x[0] != nil && x[1] != nil && x[0].Sign() == 0 && x[1].Sign() == 0
}

// split2Ways splits x into negative and non-negative sub-intervals. The
// IntRange values returned may be empty, which means that x does not contain
// any negative or non-negative elements.
func (x IntRange) split2Ways() (neg IntRange, nonNeg IntRange, hasNeg bool, hasNonNeg bool) {
	if x.Empty() {
		return sharedEmptyRange, sharedEmptyRange, false, false
	}
	if x[0] != nil && x[0].Sign() >= 0 {
		return sharedEmptyRange, x, false, true
	}
	if x[1] != nil && x[1].Sign() < 0 {
		return x, sharedEmptyRange, true, false
	}

	neg[0] = x[0]
	neg[1] = big.NewInt(-1)
	if x[1] != nil && x[1].Cmp(neg[1]) < 0 {
		neg[1] = x[1]
	}

	nonNeg[0] = big.NewInt(0)
	if x[0] != nil && x[0].Cmp(nonNeg[0]) > 0 {
		nonNeg[0] = x[0]
	}
	nonNeg[1] = x[1]

	return neg, nonNeg, true, true
}

// split3Ways splits x into negative, zero and positive sub-intervals. The
// IntRange values returned may be empty, which means that x does not contain
// any negative or positive elements.
func (x IntRange) split3Ways() (neg IntRange, pos IntRange, hasNeg bool, hasZero bool, hasPos bool) {
	if x.Empty() {
		return sharedEmptyRange, sharedEmptyRange, false, false, false
	}
	if x[0] != nil && x[0].Sign() > 0 {
		return sharedEmptyRange, x, false, false, true
	}
	if x[1] != nil && x[1].Sign() < 0 {
		return x, sharedEmptyRange, true, false, false
	}

	neg[0] = x[0]
	neg[1] = big.NewInt(-1)
	if x[1] != nil && x[1].Cmp(neg[1]) < 0 {
		neg[1] = x[1]
	}

	pos[0] = big.NewInt(+1)
	if x[0] != nil && x[0].Cmp(pos[0]) > 0 {
		pos[0] = x[0]
	}
	pos[1] = x[1]

	return neg, pos, !neg.Empty(), x.ContainsZero(), !pos.Empty()
}

// Unite returns z = x ∪ y, the union of two intervals.
func (x IntRange) Unite(y IntRange) (z IntRange) {
	if x.Empty() {
		return IntRange{
			bigIntNewSet(y[0]),
			bigIntNewSet(y[1]),
		}
	}
	if y.Empty() {
		return IntRange{
			bigIntNewSet(x[0]),
			bigIntNewSet(x[1]),
		}
	}

	if (x[0] != nil) && (y[0] != nil) {
		if x[0].Cmp(y[0]) < 0 {
			z[0] = big.NewInt(0).Set(x[0])
		} else {
			z[0] = big.NewInt(0).Set(y[0])
		}
	}

	if (x[1] != nil) && (y[1] != nil) {
		if x[1].Cmp(y[1]) > 0 {
			z[1] = big.NewInt(0).Set(x[1])
		} else {
			z[1] = big.NewInt(0).Set(y[1])
		}
	}

	return z
}

// TryUnite returns (x.Unite(y), true).
func (x IntRange) TryUnite(y IntRange) (z IntRange, ok bool) {
	return x.Unite(y), true
}

func (x *IntRange) inPlaceUnite(y IntRange) {
	if y.Empty() {
		return
	}

	if x.Empty() {
		if y[0] == nil {
			x[0] = nil
		} else {
			x[0].Set(y[0])
		}

		if y[1] == nil {
			x[1] = nil
		} else {
			x[1].Set(y[1])
		}
	}

	if x[0] != nil {
		if y[0] == nil {
			x[0] = nil
		} else if x[0].Cmp(y[0]) > 0 {
			x[0].Set(y[0])
		}
	}

	if x[1] != nil {
		if y[1] == nil {
			x[1] = nil
		} else if x[1].Cmp(y[1]) < 0 {
			x[1].Set(y[1])
		}
	}
}

// Intersect returns z = x ∩ y, the intersection of two intervals.
func (x IntRange) Intersect(y IntRange) (z IntRange) {
	if x.Empty() || y.Empty() {
		return makeEmptyRange()
	}

	if x[0] == nil {
		z[0] = bigIntNewSet(y[0])
	} else if y[0] == nil {
		z[0] = bigIntNewSet(x[0])
	} else if x[0].Cmp(y[0]) < 0 {
		z[0] = big.NewInt(0).Set(y[0])
	} else {
		z[0] = big.NewInt(0).Set(x[0])
	}

	if x[1] == nil {
		z[1] = bigIntNewSet(y[1])
	} else if y[1] == nil {
		z[1] = bigIntNewSet(x[1])
	} else if x[1].Cmp(y[1]) < 0 {
		z[1] = big.NewInt(0).Set(x[1])
	} else {
		z[1] = big.NewInt(0).Set(y[1])
	}

	return z
}

// TryIntersect returns (x.Intersect(y), true).
func (x IntRange) TryIntersect(y IntRange) (z IntRange, ok bool) {
	return x.Intersect(y), true
}

// Add returns z = x + y.
func (x IntRange) Add(y IntRange) (z IntRange) {
	if x.Empty() || y.Empty() {
		return makeEmptyRange()
	}
	if x[0] != nil && y[0] != nil {
		z[0] = big.NewInt(0).Add(x[0], y[0])
	}
	if x[1] != nil && y[1] != nil {
		z[1] = big.NewInt(0).Add(x[1], y[1])
	}
	return z
}

// TryAdd returns (x.Add(y), true).
func (x IntRange) TryAdd(y IntRange) (z IntRange, ok bool) {
	return x.Add(y), true
}

// Sub returns z = x - y.
func (x IntRange) Sub(y IntRange) (z IntRange) {
	if x.Empty() || y.Empty() {
		return makeEmptyRange()
	}
	if x[0] != nil && y[1] != nil && (x[1] != nil || y[0] != nil) {
		z[0] = big.NewInt(0).Sub(x[0], y[1])
	}
	if x[1] != nil && y[0] != nil && (x[0] != nil || y[1] != nil) {
		z[1] = big.NewInt(0).Sub(x[1], y[0])
	}
	return z
}

// TrySub returns (x.Sub(y), true).
func (x IntRange) TrySub(y IntRange) (z IntRange, ok bool) {
	return x.Sub(y), true
}

// Mul returns z = x * y.
func (x IntRange) Mul(y IntRange) (z IntRange) {
	return x.mulLsh(y, false)
}

// TryMul returns (x.Mul(y), true).
func (x IntRange) TryMul(y IntRange) (z IntRange, ok bool) {
	return x.Mul(y), true
}

// TryLsh returns z = x << y.
//
// ok is false (and z will be IntRange{nil, nil}) if x is non-empty and y
// contains at least one negative value, as it's invalid to shift by a negative
// number. Otherwise, ok is true.
func (x IntRange) TryLsh(y IntRange) (z IntRange, ok bool) {
	if !x.Empty() && y.ContainsNegative() {
		return IntRange{}, false
	}
	return x.mulLsh(y, true), true
}

func (x IntRange) mulLsh(y IntRange, shift bool) (z IntRange) {
	if x.Empty() || y.Empty() {
		return makeEmptyRange()
	}
	if x.justZero() || (!shift && y.justZero()) {
		return IntRange{big.NewInt(0), big.NewInt(0)}
	}

	combine := bigIntMul
	if shift {
		combine = bigIntLsh
	}

	ret := newBiggerIntPair()

	// Split x and y into negative, zero and positive parts.
	negX, posX, hasNegX, hasZeroX, hasPosX := x.split3Ways()
	negY, posY, hasNegY, hasZeroY, hasPosY := y.split3Ways()

	if hasZeroY && shift {
		ret.fromIntRange(x)
	} else if (hasZeroY && !shift) || hasZeroX {
		ret[0] = biggerInt{i: big.NewInt(0)}
		ret[1] = biggerInt{i: big.NewInt(0)}
	}

	if hasNegX {
		if hasNegY {
			// x is negative and y is negative, so x op y is positive.
			//
			// If op is << instead of * then we have previously checked that y
			// is non-negative, so this should be unreachable.
			ret.lowerMin(biggerInt{i: combine(negX[1], negY[1])})
			if negX[0] == nil || negY[0] == nil {
				ret.raiseMax(biggerInt{extra: +1})
			} else {
				ret.raiseMax(biggerInt{i: combine(negX[0], negY[0])})
			}
		}

		if hasPosY {
			// x is negative and y is positive, so x op y is negative.
			if negX[0] == nil || posY[1] == nil {
				ret.lowerMin(biggerInt{extra: -1})
			} else {
				ret.lowerMin(biggerInt{i: combine(negX[0], posY[1])})
			}
			ret.raiseMax(biggerInt{i: combine(negX[1], posY[0])})
		}
	}

	if hasPosX {
		if hasNegY {
			// x is positive and y is negative, so x op y is negative.
			//
			// If op is << instead of * then we have previously checked that y
			// is non-negative, so this should be unreachable.
			if posX[1] == nil || negY[0] == nil {
				ret.lowerMin(biggerInt{extra: -1})
			} else {
				ret.lowerMin(biggerInt{i: combine(posX[1], negY[0])})
			}
			ret.raiseMax(biggerInt{i: combine(posX[0], negY[1])})
		}

		if hasPosY {
			// x is positive and y is positive, so x op y is positive.
			ret.lowerMin(biggerInt{i: combine(posX[0], posY[0])})
			if posX[1] == nil || posY[1] == nil {
				ret.raiseMax(biggerInt{extra: +1})
			} else {
				ret.raiseMax(biggerInt{i: combine(posX[1], posY[1])})
			}
		}
	}

	return ret.toIntRange()
}

// TryQuo returns z = x / y. Like the big.Int.Quo method (and unlike the
// big.Int.Div method), it truncates towards zero.
//
// ok is false (and z will be IntRange{nil, nil}) if x is non-empty and y
// contains zero, as it's invalid to divide by zero. Otherwise, ok is true.
func (x IntRange) TryQuo(y IntRange) (z IntRange, ok bool) {
	if x.Empty() || y.Empty() {
		return makeEmptyRange(), true
	}
	if y.ContainsZero() {
		return IntRange{}, false
	}
	if x.justZero() {
		return IntRange{big.NewInt(0), big.NewInt(0)}, true
	}

	ret := newBiggerIntPair()

	// Split x and y into negative, zero and positive parts.
	negX, posX, hasNegX, hasZeroX, hasPosX := x.split3Ways()
	negY, posY, hasNegY, _, hasPosY := y.split3Ways()

	if hasZeroX {
		ret[0] = biggerInt{i: big.NewInt(0)}
		ret[1] = biggerInt{i: big.NewInt(0)}
	}

	if hasNegX {
		if hasNegY {
			// x is negative and y is negative, so x / y is non-negative.
			if negX[0] == nil {
				ret.raiseMax(biggerInt{extra: +1})
			} else {
				ret.raiseMax(biggerInt{i: bigIntQuo(negX[0], negY[1])})
			}
			if negY[0] == nil {
				ret.lowerMin(biggerInt{i: big.NewInt(0)})
			} else {
				ret.lowerMin(biggerInt{i: bigIntQuo(negX[1], negY[0])})
			}
		}

		if hasPosY {
			// x is negative and y is positive, so x / y is non-positive.
			if negX[0] == nil {
				ret.lowerMin(biggerInt{extra: -1})
			} else {
				ret.lowerMin(biggerInt{i: bigIntQuo(negX[0], posY[0])})
			}
			if posY[1] == nil {
				ret.raiseMax(biggerInt{i: big.NewInt(0)})
			} else {
				ret.raiseMax(biggerInt{i: bigIntQuo(negX[1], posY[1])})
			}
		}
	}

	if hasPosX {
		if hasNegY {
			// x is positive and y is negative, so x / y is non-positive.
			if posX[1] == nil {
				ret.lowerMin(biggerInt{extra: -1})
			} else {
				ret.lowerMin(biggerInt{i: bigIntQuo(posX[1], negY[1])})
			}
			if negY[0] == nil {
				ret.raiseMax(biggerInt{i: big.NewInt(0)})
			} else {
				ret.raiseMax(biggerInt{i: bigIntQuo(posX[0], negY[0])})
			}
		}

		if hasPosY {
			// x is positive and y is positive, so x / y is non-negative.
			if posX[1] == nil {
				ret.raiseMax(biggerInt{extra: +1})
			} else {
				ret.raiseMax(biggerInt{i: bigIntQuo(posX[1], posY[0])})
			}
			if posY[1] == nil {
				ret.lowerMin(biggerInt{i: big.NewInt(0)})
			} else {
				ret.lowerMin(biggerInt{i: bigIntQuo(posX[0], posY[1])})
			}
		}
	}

	return ret.toIntRange(), true
}

// TryRsh returns z = x >> y.
//
// ok is false (and z will be IntRange{nil, nil}) if x is non-empty and y
// contains at least one negative value, as it's invalid to shift by a negative
// number. Otherwise, ok is true.
func (x IntRange) TryRsh(y IntRange) (z IntRange, ok bool) {
	if x.Empty() || y.Empty() {
		return makeEmptyRange(), true
	}
	if y.ContainsNegative() {
		return IntRange{}, false
	}
	if x.justZero() {
		return IntRange{big.NewInt(0), big.NewInt(0)}, true
	}

	ret := newBiggerIntPair()

	// Split x and y into negative and zero-or-positive parts.
	negX, posX, hasNegX, hasZeroX, hasPosX := x.split3Ways()

	if hasZeroX {
		ret[0] = biggerInt{i: big.NewInt(0)}
		ret[1] = biggerInt{i: big.NewInt(0)}
	}

	if hasNegX {
		// x is negative and y is positive, so x >> y is non-positive.
		if negX[0] == nil {
			ret.lowerMin(biggerInt{extra: -1})
		} else {
			ret.lowerMin(biggerInt{i: bigIntRsh(negX[0], y[0])})
		}
		if y[1] == nil {
			ret.raiseMax(biggerInt{i: big.NewInt(-1)})
		} else {
			ret.raiseMax(biggerInt{i: bigIntRsh(negX[1], y[1])})
		}
	}

	if hasPosX {
		// x is positive and y is positive, so x >> y is non-negative.
		if y[1] == nil {
			ret.lowerMin(biggerInt{i: big.NewInt(0)})
		} else {
			ret.lowerMin(biggerInt{i: bigIntRsh(posX[0], y[1])})
		}
		if posX[1] == nil {
			ret.raiseMax(biggerInt{extra: +1})
		} else {
			ret.raiseMax(biggerInt{i: bigIntRsh(posX[1], y[0])})
		}
	}

	return ret.toIntRange(), true
}

// And returns z = x & y.
func (x IntRange) And(y IntRange) (z IntRange) {
	if x.Empty() || y.Empty() {
		return makeEmptyRange()
	}
	if !x.ContainsNegative() && !y.ContainsNegative() {
		return andBothNonNeg(x, y)
	}

	negX, nonX, hasNegX, hasNonX := x.split2Ways()
	negY, nonY, hasNegY, hasNonY := y.split2Ways()

	z = makeEmptyRange()
	if hasNegX {
		if hasNegY {
			w := orBothNonNeg(IntRange{
				bigIntNewNot(negX[1]),
				bigIntNewNot(negX[0]),
			}, IntRange{
				bigIntNewNot(negY[1]),
				bigIntNewNot(negY[0]),
			})
			z.inPlaceUnite(IntRange{
				bigIntNewNot(w[1]),
				bigIntNewNot(w[0]),
			})
		}
		if hasNonY {
			z.inPlaceUnite(andOneNegOneNonNeg(negX, nonY))
		}
	}
	if hasNonX {
		if hasNegY {
			z.inPlaceUnite(andOneNegOneNonNeg(negY, nonX))
		}
		if hasNonY {
			z.inPlaceUnite(andBothNonNeg(nonX, nonY))
		}
	}
	return z
}

// TryAnd returns (x.And(y), true).
func (x IntRange) TryAnd(y IntRange) (z IntRange, ok bool) {
	return x.And(y), true
}

func andBothNonNeg(x IntRange, y IntRange) (z IntRange) {
	if x.Empty() || x.ContainsNegative() || y.Empty() || y.ContainsNegative() {
		panic("pre-condition failure")
	}

	zMax := (*big.Int)(nil)
	if x[1] != nil {
		if y[1] != nil {
			zMax = x.andMax(y)
		} else {
			return IntRange{big.NewInt(0), big.NewInt(0).Set(x[1])}
		}
	} else {
		if y[1] != nil {
			return IntRange{big.NewInt(0), big.NewInt(0).Set(y[1])}
		} else {
			return IntRange{big.NewInt(0), nil}
		}
	}

	// andMin(x, y) is ~orMax(~x, ~y).
	notX := IntRange{
		big.NewInt(0).Not(x[1]),
		big.NewInt(0).Not(x[0]),
	}
	notY := IntRange{
		big.NewInt(0).Not(y[1]),
		big.NewInt(0).Not(y[0]),
	}
	zMin := notX.orMax(notY)
	zMin.Not(zMin)

	return IntRange{zMin, zMax}
}

func andOneNegOneNonNeg(neg IntRange, non IntRange) (z IntRange) {
	if neg.Empty() || neg.ContainsNonNegative() || non.Empty() || non.ContainsNegative() {
		panic("pre-condition failure")
	}

	if neg[0] == nil {
		return IntRange{big.NewInt(0), bigIntNewSet(non[1])}
	} else if non[1] == nil {
		mask := bitMask(neg[0].BitLen(), non[0].BitLen())
		biasedNeg := IntRange{
			big.NewInt(0).And(mask, neg[0]),
			big.NewInt(0).And(mask, neg[1]),
		}
		w := andBothNonNeg(biasedNeg, IntRange{non[0], mask})
		return IntRange{w[0], nil}
	} else {
		mask := bitMask(neg[0].BitLen(), non[1].BitLen())
		biasedNeg := IntRange{
			big.NewInt(0).And(mask, neg[0]),
			big.NewInt(0).And(mask, neg[1]),
		}
		return andBothNonNeg(biasedNeg, non)
	}
}

// Or returns z = x | y.
func (x IntRange) Or(y IntRange) (z IntRange) {
	if x.Empty() || y.Empty() {
		return makeEmptyRange()
	}
	if !x.ContainsNegative() && !y.ContainsNegative() {
		return orBothNonNeg(x, y)
	}

	negX, nonX, hasNegX, hasNonX := x.split2Ways()
	negY, nonY, hasNegY, hasNonY := y.split2Ways()

	z = makeEmptyRange()
	if hasNegX {
		if hasNegY {
			w := andBothNonNeg(IntRange{
				bigIntNewNot(negX[1]),
				bigIntNewNot(negX[0]),
			}, IntRange{
				bigIntNewNot(negY[1]),
				bigIntNewNot(negY[0]),
			})
			z.inPlaceUnite(IntRange{
				bigIntNewNot(w[1]),
				bigIntNewNot(w[0]),
			})
		}
		if hasNonY {
			z.inPlaceUnite(orOneNegOneNonNeg(negX, nonY))
		}
	}
	if hasNonX {
		if hasNegY {
			z.inPlaceUnite(orOneNegOneNonNeg(negY, nonX))
		}
		if hasNonY {
			z.inPlaceUnite(orBothNonNeg(nonX, nonY))
		}
	}
	return z
}

// TryOr returns (x.Or(y), true).
func (x IntRange) TryOr(y IntRange) (z IntRange, ok bool) {
	return x.Or(y), true
}

func orBothNonNeg(x IntRange, y IntRange) (z IntRange) {
	if x.Empty() || x.ContainsNegative() || y.Empty() || y.ContainsNegative() {
		panic("pre-condition failure")
	}

	zMax := (*big.Int)(nil)
	if x[1] != nil && y[1] != nil {
		zMax = x.orMax(y)
	} else {
		// Keep zMax as nil, which means that (x | y) can be arbitrarily large.
		//
		// If the integers xx and yy are in the intervals x and y, then (xx |
		// yy) is at least yy, since a bit-wise or can only turn bits on, and
		// yy is at least y's lower bound, y[0].
		//
		// Therefore, (z[0] >= x[0]) && (z[0] >= y[0]) is a necessary bound.
		// The smaller of those is also a sufficient bound if that smaller
		// value is contained in the other interval. For example, if both xx
		// and yy can be x[0], then (x[0] | x[0]) is simply x[0].
		if x.ContainsInt(y[0]) {
			return IntRange{big.NewInt(0).Set(y[0]), nil}
		}
		if y.ContainsInt(x[0]) {
			return IntRange{big.NewInt(0).Set(x[0]), nil}
		}
		if x[1] == nil && y[1] == nil {
			panic("unreachable")
		}

		// The two intervals are non-empty but don't overlap. Furthermore,
		// exactly one of the two intervals have an infinite upper bound.
		// Without loss of generality, assume that that interval is y.
		//
		// Therefore, (x[0] <= x[1]) && (x[1] < y[0]) && (y[1] == nil).
		if x[1] == nil {
			x, y = y, x
		}
		if x[1].Cmp(y[0]) >= 0 {
			panic("unreachable")
		}

		// We've already calculated zMax: it is infinite. To calculate zMin,
		// also known as z[0], replace the infinite upper bound y[1] with a
		// finite value equal to right-filling all of y[0]'s bits.
		y[1] = big.NewInt(0).Set(y[0])
		bitFillRight(y[1])
	}

	// orMin(x, y) is ~andMax(~x, ~y).
	notX := IntRange{
		big.NewInt(0).Not(x[1]),
		big.NewInt(0).Not(x[0]),
	}
	notY := IntRange{
		big.NewInt(0).Not(y[1]),
		big.NewInt(0).Not(y[0]),
	}
	zMin := notX.andMax(notY)
	zMin.Not(zMin)

	return IntRange{zMin, zMax}
}

func orOneNegOneNonNeg(neg IntRange, non IntRange) (z IntRange) {
	w := andOneNegOneNonNeg(IntRange{
		bigIntNewNot(non[1]),
		bigIntNewNot(non[0]),
	}, IntRange{
		bigIntNewNot(neg[1]),
		bigIntNewNot(neg[0]),
	})
	return IntRange{
		bigIntNewNot(w[1]),
		bigIntNewNot(w[0]),
	}
}

// The andMax and orMax algorithms are tricky.
//
// First, some notation. Let x and y be intervals, and in math notation, denote
// those intervals' bounds as [xMin, xMax] and [yMin, yMax]. In Go terms, x is
// an IntRange, xMin is x[0], xMax is x[1], and likewise for y. In the
// algorithm discussion below, we'll use square brackets only for denoting
// intervals, not array indices, and so we'll say xMin instead of x[0].
//
// xMin, xMax, yMin and yMax are all assumed to be finite (i.e. a non-nil
// *big.Int) and non-negative. The caller is responsible for enforcing this.
//
// For a given range r, define maximal(r) to be the integers in r for which you
// cannot flip any bits from 0 to 1 and have the result still be in r. Clearly
// rMax is in maximal(r), but there are other elements as well -- for each bit
// that is set in rMax, if you unset that bit, and set all bits to its right,
// the result is also in maximal(r) as long as it is >= rMin (which is true iff
// the bit is in bitFillRight(rMax & ~rMin).
//
// Clearly x.andMax(y) == maximal(x).andMax(maximal(y)) and x.orMax(y) ==
// maximal(x).orMax(maximal(y)) -- that is, we only need to consider the
// maximal elements in each range.
//
// For orMax, the max is achieved by starting with xMax | yMax, and then
// realizing that we can get a larger result by choosing the leftmost bit in
// xMax & yMax (which we effectively have twice), flipping it to zero in either
// of the inputs, and replacing all bits to its right with 1s. However, that
// might end up with the input being below the minimum in its range, so instead
// of considering all bits in xMax & yMax, we have to restrict to those that
// are also set in either bitFillRight(xMax & ~xMin) or bitFillRight(yMax &
// ~yMin).
//
// For andMax, we can again only consider the maximal elements. Here, we have
// either yMax and the best maximal element from x, or xMax and the best
// maximal element from y. For symmetry assume it's the former (though we must
// actually check both).
//
// We take yMax, and then the maximal element from x that is chosen by flipping
// the leftmost bit in xMax that will result in a number that is >= xMin and is
// not also set in yMax. That is, the leftmost bit in bitFillRight(xMax &
// ~xMin) & xMax & ~yMax.

// andMax returns an exact solution for the maximum possible (xx & yy), for all
// possible xx in x and yy in y.
//
// Algorithm:
//
//	// If the two intervals overlap, the result is the minimum of the two
//	// intervals' maxima.
//	//
//	// This overlaps code path is just an optimization.
//	if overlaps(x, y) {
//	  return min(xMax, yMax)
//	}
//	xFlip   = bitFillRight(bitFillRight(xMax & ~xMin) & xMax & ~yMax)
//	xResult = yMax & ((xMax & ~xFlip) | (xFlip >> 1))
//	yFlip   = bitFillRight(bitFillRight(yMax & ~yMin) & yMax & ~xMax)
//	yResult = xMax & ((yMax & ~yFlip) | (yFlip >> 1))
//	return max(xResult, yResult)
//
// If xMin and yMin are both zero, the overlaps branch is taken.
//
// TODO: can this algorithm be simplified??
func (x IntRange) andMax(y IntRange) *big.Int {
	// Check for overlap.
	if (y[1].Cmp(x[0]) >= 0) && (x[1].Cmp(y[0]) >= 0) {
		min := x[1]
		if x[1].Cmp(y[1]) > 0 {
			min = y[1]
		}
		return big.NewInt(0).Set(min)
	}

	// Otherwise, x and y don't overlap. Four examples:
	//  - Example #0: x is [1, 3] and y is [ 4,  9], andMax is 3.
	//  - Example #1: x is [3, 4] and y is [ 5,  6], andMax is 4.
	//  - Example #2: x is [4, 5] and y is [ 6,  7], andMax is 5.
	//  - Example #3: x is [7, 7] and y is [12, 14], andMax is 6.

	i := big.NewInt(0)
	j := big.NewInt(0)
	k := big.NewInt(0)

	// Calculate xFlip and xResult.
	{
		// j = bitFillRight(xMax & ~xMin)
		//
		// For example #0, j = bfr(3 & ~1) = bfr(2) = 3.
		// For example #1, j = bfr(4 & ~3) = bfr(4) = 7.
		// For example #2, j = bfr(5 & ~4) = bfr(1) = 1.
		// For example #3, j = bfr(7 & ~7) = bfr(0) = 0.
		j.AndNot(x[1], x[0])
		bitFillRight(j)

		// j = xFlip = bitFillRight(j & xMax & ~yMax)
		//
		// For example #0, j = bfr(3 & 3 & ~ 9) = bfr(2) = 3.
		// For example #1, j = bfr(7 & 4 & ~ 6) = bfr(0) = 0.
		// For example #2, j = bfr(1 & 5 & ~ 7) = bfr(0) = 0.
		// For example #3, j = bfr(0 & 7 & ~15) = bfr(0) = 0.
		j.And(j, x[1])
		j.AndNot(j, y[1])
		bitFillRight(j)

		// i = xMax & ~xFlip
		//
		// For example #0, i = 3 & ~3 = 0.
		// For example #1, i = 4 & ~0 = 4.
		// For example #2, i = 5 & ~0 = 5.
		// For example #3, i = 7 & ~0 = 7.
		i.AndNot(x[1], j)

		// j = xResult = yMax & (i | (xFlip >> 1))
		//
		// For example #0, j =  9 & (0 | (3 >> 1)) = 1.
		// For example #1, j =  6 & (4 | (0 >> 1)) = 4.
		// For example #2, j =  7 & (5 | (0 >> 1)) = 5.
		// For example #3, j = 14 & (7 | (0 >> 1)) = 6.
		j.Rsh(j, 1)
		j.Or(j, i)
		j.And(j, y[1])
	}

	// Calculate yFlip and yResult.
	{
		// k = bitFillRight(yMax & ~yMin)
		//
		// For example #0, k = bfr( 9 & ~ 4) = bfr(9) = 15.
		// For example #1, k = bfr( 6 & ~ 5) = bfr(2) =  3.
		// For example #2, k = bfr( 7 & ~ 6) = bfr(1) =  1.
		// For example #3, k = bfr(14 & ~12) = bfr(2) =  3.
		k.AndNot(y[1], y[0])
		bitFillRight(k)

		// k = yFlip = bitFillRight(k & yMax & ~xMax)
		//
		// For example #0, k = bfr(15 &  9 & ~3) = bfr(8) = 15.
		// For example #1, k = bfr( 3 &  6 & ~4) = bfr(2) =  3.
		// For example #2, k = bfr( 1 & 14 & ~5) = bfr(0) =  0.
		// For example #3, k = bfr( 3 &  7 & ~7) = bfr(0) =  0.
		k.And(k, y[1])
		k.AndNot(k, x[1])
		bitFillRight(k)

		// i = yMax & ~yFlip
		//
		// For example #0, i =  9 & ~15 =  0.
		// For example #1, i =  6 & ~ 3 =  4.
		// For example #2, i =  7 & ~ 0 =  7.
		// For example #3, i = 14 & ~ 0 = 14.
		i.AndNot(y[1], k)

		// k = yResult = xMax & (i | (yFlip >> 1))
		//
		// For example #0, k = 3 & ( 0 | (15 >> 1)) = 3.
		// For example #1, k = 4 & ( 4 | ( 3 >> 1)) = 4.
		// For example #2, k = 5 & ( 7 | ( 0 >> 1)) = 5.
		// For example #3, k = 7 & (14 | ( 0 >> 1)) = 6.
		k.Rsh(k, 1)
		k.Or(k, i)
		k.And(k, x[1])
	}

	// return max(xResult, yResult)
	//
	// For example #0, return max(1, 3).
	// For example #1, return max(4, 4).
	// For example #2, return max(5, 5).
	// For example #3, return max(6, 6).
	if j.Cmp(k) < 0 {
		return k
	}
	return j
}

// orMax returns an exact solution for the maximum possible (xx | yy), for all
// possible xx in x and yy in y.
//
// Algorithm:
//
//	droppable = bitFillRight((xMax & ~xMin) | (yMax & ~yMin))
//	available = xMax & yMax & droppable
//	return xMax | yMax | (bitFillRight(available) >> 1)
//
// If xMin and yMin are both zero, this simplifies to:
//
//	available = xMax & yMax
//	return xMax | yMax | (bitFillRight(available) >> 1)
func (x IntRange) orMax(y IntRange) *big.Int {
	if x[0].Sign() == 0 && y[0].Sign() == 0 {
		i := big.NewInt(0)
		i.And(x[1], y[1])
		bitFillRight(i)
		i.Rsh(i, 1)
		i.Or(i, x[1])
		i.Or(i, y[1])
		return i
	}

	// Four examples:
	//  - Example #0: x is [1, 3] and y is [ 4,  9], orMax is 11.
	//  - Example #1: x is [3, 4] and y is [ 5,  6], orMax is  7.
	//  - Example #2: x is [4, 5] and y is [ 6,  7], orMax is  7.
	//  - Example #3: x is [7, 7] and y is [12, 14], orMax is 15.

	i := big.NewInt(0)
	j := big.NewInt(0)

	// j = droppable = bitFillRight((xMax & ~xMin) | (yMax & ~yMin))
	//
	// For example #0, j = bfr((3 & ~1) | ( 9 & ~ 4)) = bfr(2 | 9) = 15.
	// For example #1, j = bfr((4 & ~3) | ( 6 & ~ 5)) = bfr(4 | 2) =  7.
	// For example #2, j = bfr((5 & ~4) | ( 7 & ~ 6)) = bfr(1 | 1) =  1.
	// For example #3, j = bfr((7 & ~7) | (14 & ~12)) = bfr(0 | 2) =  3.
	i.AndNot(x[1], x[0])
	j.AndNot(y[1], y[0])
	j.Or(j, i)
	bitFillRight(j)

	// j = available = xMax & yMax & j
	//
	// For example #0, j = 3 &  9 & 15 = 1.
	// For example #1, j = 4 &  6 &  7 = 4.
	// For example #2, j = 5 &  7 &  1 = 1.
	// For example #3, j = 7 & 14 &  3 = 2.
	j.And(j, x[1])
	j.And(j, y[1])

	// j = bitFillRight(j) >> 1
	//
	// For example #0, j = bfr(1) >> 1 = 0.
	// For example #1, j = bfr(4) >> 1 = 3.
	// For example #2, j = bfr(1) >> 1 = 0.
	// For example #3, j = bfr(2) >> 1 = 1.
	bitFillRight(j)
	j.Rsh(j, 1)

	// return xMax | yMax | j
	//
	// For example #0, return 3 |  9 | 0 = 11.
	// For example #1, return 4 |  6 | 3 =  7.
	// For example #2, return 5 |  7 | 0 =  7.
	// For example #3, return 7 | 14 | 1 = 15.
	j.Or(j, x[1])
	j.Or(j, y[1])
	return j
}

// bitFillRight modifies i to round up to the next power of 2 minus 1:
//   - If i is +0 then bitFillRight(i) sets i to  0.
//   - If i is +1 then bitFillRight(i) sets i to  1.
//   - If i is +2 then bitFillRight(i) sets i to  3.
//   - If i is +3 then bitFillRight(i) sets i to  3.
//   - If i is +4 then bitFillRight(i) sets i to  7.
//   - If i is +5 then bitFillRight(i) sets i to  7.
//   - If i is +6 then bitFillRight(i) sets i to  7.
//   - If i is +7 then bitFillRight(i) sets i to  7.
//   - If i is +8 then bitFillRight(i) sets i to 15.
//   - If i is +9 then bitFillRight(i) sets i to 15.
//   - Etc.
func bitFillRight(i *big.Int) {
	if s := i.Sign(); s < 0 {
		panic("pre-condition failure")
	} else if s == 0 {
		return
	}
	n := i.BitLen()
	if n > 0xFFFF {
		panic("interval: input is too large")
	}
	i.SetInt64(1)
	i.Lsh(i, uint(n))
	i.Sub(i, one)
}
