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

import (
	"fmt"
	"math/big"
	"math/rand"
	"sort"
	"strconv"
	"strings"
	"testing"
	"unicode/utf8"
)

var fromNeg7ToPos7 = []*big.Int{
	big.NewInt(-7),
	big.NewInt(-6),
	big.NewInt(-5),
	big.NewInt(-4),
	big.NewInt(-3),
	big.NewInt(-2),
	big.NewInt(-1),
	big.NewInt(+0),
	big.NewInt(+1),
	big.NewInt(+2),
	big.NewInt(+3),
	big.NewInt(+4),
	big.NewInt(+5),
	big.NewInt(+6),
	big.NewInt(+7),
	nil,
}

var fromNeg3ToPos3 = []*big.Int{
	big.NewInt(-3),
	big.NewInt(-2),
	big.NewInt(-1),
	big.NewInt(+0),
	big.NewInt(+1),
	big.NewInt(+2),
	big.NewInt(+3),
	nil,
}

// TestMotivatingExample tests the "motivating example" given in the package
// doc comment.
func TestMotivatingExample(tt *testing.T) {
	i := IntRange{big.NewInt(0), big.NewInt(255)}
	j := IntRange{big.NewInt(0), big.NewInt(3)}
	four := IntRange{big.NewInt(4), big.NewInt(4)}
	got := four.Mul(i).Add(j)
	want := IntRange{big.NewInt(0), big.NewInt(1023)}
	if !got.Eq(want) {
		tt.Fatalf("got %v, want %v", got, want)
	}
}

func TestBigIntShifts(tt *testing.T) {
	// These alternative implementations always take the fallback code path of
	// bigIntLsh and bigIntRsh. This test checks that those fallback paths give
	// the same results as *big.Int's Lsh and Rsh methods. There are fallback
	// and non-fallback code paths in interval.go because those *big.Int
	// methods aren't applicable if j's value doesn't fit in a uint.

	alternativeBigIntLsh := func(i *big.Int, j *big.Int) *big.Int {
		// Fallback code path, copy-pasted from interval.go.
		k := big.NewInt(2)
		k.Exp(k, j, nil)
		k.Mul(i, k)
		return k
	}

	alternativeBigIntRsh := func(i *big.Int, j *big.Int) *big.Int {
		// Fallback code path, copy-pasted from interval.go.
		k := big.NewInt(2)
		k.Exp(k, j, nil)
		k.Div(i, k) // This is explicitly Div, not Quo.
		return k
	}

	xs := []*big.Int{
		big.NewInt(-9),
		big.NewInt(-8),
		big.NewInt(-7),

		big.NewInt(-2),
		big.NewInt(-1),
		big.NewInt(+0),
		big.NewInt(+1),
		big.NewInt(+2),

		big.NewInt(+7),
		big.NewInt(+8),
		big.NewInt(+9),
	}

	ys := []*big.Int{
		big.NewInt(+0),
		big.NewInt(+1),
		big.NewInt(+2),
		big.NewInt(+3),
		big.NewInt(+4),
	}

	for _, x := range xs {
		for _, y := range ys {
			{
				got := bigIntLsh(x, y)
				want := alternativeBigIntLsh(x, y)
				if got.Cmp(want) != 0 {
					tt.Errorf("%v << %v: got %v, want %v", x, y, got, want)
				}
			}

			{
				got := bigIntRsh(x, y)
				want := alternativeBigIntRsh(x, y)
				if got.Cmp(want) != 0 {
					tt.Errorf("%v >> %v: got %v, want %v", x, y, got, want)
				}
			}
		}
	}
}

func trimLeadingSpaces(s string) string {
	for ; len(s) > 0 && s[0] == ' '; s = s[1:] {
	}
	return s
}

func trimTrailingSpaces(s string) string {
	for ; len(s) > 0 && s[len(s)-1] == ' '; s = s[:len(s)-1] {
	}
	return s
}

func parseInt(s string) (x *big.Int, remaining string, err error) {
	s = trimLeadingSpaces(s)
	i := 0
	for ; i < len(s); i++ {
		if c := s[i]; c == ')' || c == ',' || c == ']' {
			break
		}
	}
	s, remaining = trimTrailingSpaces(s[:i]), s[i:]
	switch s {
	case "-∞", "+∞":
		// No-op.
	default:
		n, err := strconv.Atoi(s)
		if err != nil {
			return nil, "", fmt.Errorf("parseInt(%q): %v", s, err)
		}
		x = big.NewInt(int64(n))
	}
	return x, remaining, nil
}

// parseInterval parses a string like "[-3, +4] etcetera", returning the
// interval itself and the remaining string " etcetera".
//
// It also parses infinite (unbounded) intervals like "[0, +∞)", and the
// special syntax "[...empty..]" for an empty interval (one that contains no
// elements).
func parseInterval(s string) (x IntRange, remaining string, err error) {
	const emptySyntax = "[...empty..]"
	s = trimLeadingSpaces(s)
	if strings.HasPrefix(s, emptySyntax) {
		s = s[len(emptySyntax):]
		return IntRange{big.NewInt(+1), big.NewInt(-1)}, s, nil
	}

	if s[0] != '(' && s[0] != '[' {
		return IntRange{}, "", fmt.Errorf("expected '(' or '['")
	}
	s = s[1:]
	x0, s, err := parseInt(s)
	if err != nil {
		return IntRange{}, "", err
	}
	if s[0] != ',' {
		return IntRange{}, "", fmt.Errorf("expected ','")
	}
	s = s[1:]
	x1, s, err := parseInt(s)
	if err != nil {
		return IntRange{}, "", err
	}
	if s[0] != ')' && s[0] != ']' {
		return IntRange{}, "", fmt.Errorf("expected ')' or ']'")
	}
	s = s[1:]
	if x0 != nil && x1 != nil && x0.Cmp(x1) > 0 {
		return IntRange{}, "", fmt.Errorf("invalid empty interval")
	}
	return IntRange{x0, x1}, s, nil
}

func TestContainsEtc(tt *testing.T) {
	testCases := []struct {
		s  string
		cn uint32
		cz uint32
		cp uint32
	}{
		{"<empty-->", 0, 0, 0},
		{"<empty0->", 0, 0, 0},
		{"<empty+->", 0, 0, 0},
		{"<empty+0>", 0, 0, 0},
		{"<empty++>", 0, 0, 0},

		{"(-∞, -1]", 1, 0, 0},
		{"(-∞,  0]", 1, 1, 0},
		{"(-∞, +1]", 1, 1, 1},
		{"(-∞, +∞)", 1, 1, 1},

		{"[-1, -1]", 1, 0, 0},
		{"[-1,  0]", 1, 1, 0},
		{"[-1, +1]", 1, 1, 1},
		{"[-1, +∞)", 1, 1, 1},

		{"[ 0,  0]", 0, 1, 0},
		{"[ 0, +1]", 0, 1, 1},
		{"[ 0, +∞)", 0, 1, 1},

		{"[+1, +1]", 0, 0, 1},
		{"[+1, +∞)", 0, 0, 1},
	}

	// eqTestCases is appended to throughout the "range testCases" loop, but it
	// always contains exactly one empty IntRange.
	eqTestCases := []IntRange{
		makeEmptyRange(),
	}

	for _, tc := range testCases {
		x := IntRange{}
		switch tc.s {
		case "<empty-->":
			x = IntRange{big.NewInt(-1), big.NewInt(-2)}
		case "<empty0->":
			x = IntRange{big.NewInt(00), big.NewInt(-2)}
		case "<empty+->":
			x = IntRange{big.NewInt(+2), big.NewInt(-2)}
		case "<empty+0>":
			x = IntRange{big.NewInt(+2), big.NewInt(00)}
		case "<empty++>":
			x = IntRange{big.NewInt(+2), big.NewInt(+1)}
		default:
			err := error(nil)
			x, _, err = parseInterval(tc.s)
			if err != nil {
				tt.Errorf("%s: %v", tc.s, err)
				continue
			}
		}

		if got, want := x.ContainsNegative(), tc.cn != 0; got != want {
			tt.Errorf("%s.ContainsNegative(): got %t, want %t", tc.s, got, want)
		}
		if got, want := x.ContainsZero(), tc.cz != 0; got != want {
			tt.Errorf("%s.ContainsZero(): got %t, want %t", tc.s, got, want)
		}
		if got, want := x.ContainsPositive(), tc.cp != 0; got != want {
			tt.Errorf("%s.ContainsPositive(): got %t, want %t", tc.s, got, want)
		}

		if got, want := x.Empty(), strings.Contains(tc.s, "empty"); got != want {
			tt.Errorf("%s.Empty(): got %t, want %t", tc.s, got, want)
		} else if !got {
			eqTestCases = append(eqTestCases, x)
		} else if !x.Eq(sharedEmptyRange) {
			tt.Errorf("%v eq %v: got %t, want %t", x, sharedEmptyRange, got, want)
		}

		if got, want := x.justZero(), tc.s == "[ 0,  0]"; got != want {
			tt.Errorf("%s.justZero(): got %t, want %t", tc.s, got, want)
		}
	}

	for i, x := range eqTestCases {
		for j, y := range eqTestCases {
			got := x.Eq(y)
			want := i == j
			if got != want {
				tt.Errorf("%v eq %v: got %t, want %t", x, y, got, want)
			}
		}
	}

	for _, x := range eqTestCases {
		neg, pos, hasNeg, hasZero, hasPos := x.split3Ways()
		negEmpty, posEmpty := !hasNeg, !hasPos
		if got, want := neg.Empty(), negEmpty; got != want {
			tt.Errorf("%v: neg.Empty() == %t, negEmpty == %t", x, got, want)
		}
		if got, want := !x.ContainsNegative(), negEmpty; got != want {
			tt.Errorf("%v: !x.ContainsNegative() == %t, negEmpty == %t", x, got, want)
		}
		if got, want := x.ContainsZero(), hasZero; got != want {
			tt.Errorf("%v: x.ContainsZero() == %t, hasZero == %t", x, got, want)
		}
		if got, want := !x.ContainsPositive(), posEmpty; got != want {
			tt.Errorf("%v: !x.ContainsPositive() == %t, posEmpty == %t", x, got, want)
		}
		if got, want := pos.Empty(), posEmpty; got != want {
			tt.Errorf("%v: pos.Empty() == %t, posEmpty == %t", x, got, want)
		}
	}
}

func shareBigIntPointers(x IntRange, y IntRange) bool {
	return ((x[0] == y[0]) && (x[0] != nil)) ||
		((x[0] == y[1]) && (x[0] != nil)) ||
		((x[1] == y[0]) && (x[1] != nil)) ||
		((x[1] == y[1]) && (x[1] != nil))
}

func testBruteForceAgrees(x IntRange, y IntRange, opKey rune) error {
	brute, bruteOK := bruteForce(x, y, opKey)
	got, gotOK := intOperators[opKey](x, y)
	if shareBigIntPointers(got, x) || shareBigIntPointers(got, y) {
		return fmt.Errorf("*big.Int pointers (pointing to mutable state) were re-used")
	}
	if !got.Eq(brute) || gotOK != bruteOK {
		return fmt.Errorf("got %v, %t, brute force gave %v, %t", got, gotOK, brute, bruteOK)
	}
	return nil
}

func TestBruteForceAgreesSystematically(tt *testing.T) {
	for _, opKey := range intOperatorsKeys {
		for _, x0 := range fromNeg7ToPos7 {
			for _, x1 := range fromNeg7ToPos7 {
				x := IntRange{x0, x1}

				for _, y0 := range fromNeg7ToPos7 {
					for _, y1 := range fromNeg7ToPos7 {
						y := IntRange{y0, y1}

						if err := testBruteForceAgrees(x, y, opKey); err != nil {
							tt.Fatalf("%v %c %v: %v", x, opKey, y, err)
						}
					}
				}
			}
		}
	}
}

func TestBruteForceAgreesRandomly(tt *testing.T) {
	gen := func(rng *rand.Rand) *big.Int {
		r := rng.Intn(2*riRadius+2) - (riRadius + 1)
		if r == -(riRadius + 1) {
			return nil
		}
		return big.NewInt(int64(r))
	}

	rng := rand.New(rand.NewSource(0))
	for _, opKey := range intOperatorsKeys {
		for i := 0; i < 10000; i++ {
			x := IntRange{gen(rng), gen(rng)}
			y := IntRange{gen(rng), gen(rng)}
			if err := testBruteForceAgrees(x, y, opKey); err != nil {
				tt.Fatalf("%v %c %v: %v", x, opKey, y, err)
			}
		}
	}
}

func testOp(tt *testing.T, testCases ...string) {
	for _, tc := range testCases {
		if err := testOp1(tc); err != nil {
			tt.Errorf("%q: %v", tc, err)
		}
	}
}

func testOp1(s string) error {
	x, s, err := parseInterval(s)
	if err != nil {
		return err
	}
	s = trimLeadingSpaces(s)

	i := 0
	for ; i < len(s) && s[i] != ' '; i++ {
	}
	opKey, _ := utf8.DecodeRuneInString(s[:i])
	s = s[i:]

	y, s, err := parseInterval(s)
	if err != nil {
		return err
	}
	s = trimLeadingSpaces(s)
	if !strings.HasPrefix(s, "==") {
		return fmt.Errorf(`expected "=="`)
	}
	s = s[2:]
	s = trimLeadingSpaces(s)
	want, wantOK := IntRange{}, false
	if s == "invalid" {
		s = ""
	} else {
		wantOK = true
		want, s, err = parseInterval(s)
		if err != nil {
			return err
		}
		s = trimLeadingSpaces(s)
		if s != "" {
			return fmt.Errorf("trailing specification %q", s)
		}
	}

	if err := testBruteForceAgrees(x, y, opKey); err != nil {
		return err
	}

	got, gotOK := intOperators[opKey](x, y)
	if !got.Eq(want) || gotOK != wantOK {
		return fmt.Errorf("got %v, %t, want %v, %t", got, gotOK, want, wantOK)
	}
	return nil
}

var intOperators = map[rune]func(IntRange, IntRange) (IntRange, bool){
	'∪': IntRange.TryUnite,
	'∩': IntRange.TryIntersect,
	'+': IntRange.TryAdd,
	'-': IntRange.TrySub,
	'*': IntRange.TryMul,
	'/': IntRange.TryQuo,
	'«': IntRange.TryLsh,
	'»': IntRange.TryRsh,
	'&': IntRange.TryAnd,
	'|': IntRange.TryOr,
}

var intOperatorsKeys []rune

func init() {
	for k := range intOperators {
		intOperatorsKeys = append(intOperatorsKeys, k)
	}
	sort.Slice(intOperatorsKeys, func(i, j int) bool {
		return intOperatorsKeys[i] < intOperatorsKeys[j]
	})
}

func TestOpUnite(tt *testing.T) {
	testOp(tt,
		"[   3,    3]  ∪  [  -5,   -5]  ==  [  -5,    3]",
		"[   3,    3]  ∪  [   0,    0]  ==  [   0,    3]",
		"[   0,    0]  ∪  [  -7,    7]  ==  [  -7,    7]",
		"[   0,    2]  ∪  [   0,    5]  ==  [   0,    5]",
		"[   3,    6]  ∪  [  10,   15]  ==  [   3,   15]",
		"[   3,   +∞)  ∪  [  -4,   -2]  ==  [  -4,   +∞)",
		"[   3,   +∞)  ∪  [  10,   15]  ==  [   3,   +∞)",
		"[   3,   +∞)  ∪  (  -∞,   15]  ==  (  -∞,   +∞)",
		"[   3,    6]  ∪  (  -∞,   15]  ==  (  -∞,   15]",
		"[   3,    6]  ∪  (  -∞,   +∞)  ==  (  -∞,   +∞)",
		"(  -∞,   +∞)  ∪  (  -∞,   +∞)  ==  (  -∞,   +∞)",
		"(  -∞,   +∞)  ∪  [   1,    2]  ==  (  -∞,   +∞)",
		"(  -∞,   +∞)  ∪  [   0,    0]  ==  (  -∞,   +∞)",
		"[   3,    6]  ∪  [...empty..]  ==  [   3,    6]",
		"[...empty..]  ∪  [  10,   15]  ==  [  10,   15]",
		"[...empty..]  ∪  [...empty..]  ==  [...empty..]",
		"(  -∞,   +∞)  ∪  [...empty..]  ==  (  -∞,   +∞)",
	)
}

func TestOpIntersect(tt *testing.T) {
	testOp(tt,
		"[   3,    3]  ∩  [  -5,   -5]  ==  [...empty..]",
		"[   3,    3]  ∩  [   0,    0]  ==  [...empty..]",
		"[   0,    0]  ∩  [  -7,    7]  ==  [   0,    0]",
		"[   0,    2]  ∩  [   0,    5]  ==  [   0,    2]",
		"[   3,    6]  ∩  [  10,   15]  ==  [...empty..]",
		"[   3,   +∞)  ∩  [  -4,   -2]  ==  [...empty..]",
		"[   3,   +∞)  ∩  [  10,   15]  ==  [  10,   15]",
		"[   3,   +∞)  ∩  (  -∞,   15]  ==  [   3,   15]",
		"[   3,    6]  ∩  (  -∞,   15]  ==  [   3,    6]",
		"[   3,    6]  ∩  (  -∞,   +∞)  ==  [   3,    6]",
		"(  -∞,   +∞)  ∩  (  -∞,   +∞)  ==  (  -∞,   +∞)",
		"(  -∞,   +∞)  ∩  [   1,    2]  ==  [   1,    2]",
		"(  -∞,   +∞)  ∩  [   0,    0]  ==  [   0,    0]",
		"[   3,    6]  ∩  [...empty..]  ==  [...empty..]",
		"[...empty..]  ∩  [  10,   15]  ==  [...empty..]",
		"[...empty..]  ∩  [...empty..]  ==  [...empty..]",
		"(  -∞,   +∞)  ∩  [...empty..]  ==  [...empty..]",
	)
}

func TestOpAdd(tt *testing.T) {
	testOp(tt,
		"[   3,    3]  +  [  -5,   -5]  ==  [  -2,   -2]",
		"[   3,    3]  +  [   0,    0]  ==  [   3,    3]",
		"[   0,    0]  +  [  -7,    7]  ==  [  -7,    7]",
		"[   0,    2]  +  [   0,    5]  ==  [   0,    7]",
		"[   3,    6]  +  [  10,   15]  ==  [  13,   21]",
		"[   3,   +∞)  +  [  -4,   -2]  ==  [  -1,   +∞)",
		"[   3,   +∞)  +  [  10,   15]  ==  [  13,   +∞)",
		"[   3,   +∞)  +  (  -∞,   15]  ==  (  -∞,   +∞)",
		"[   3,    6]  +  (  -∞,   15]  ==  (  -∞,   21]",
		"[   3,    6]  +  (  -∞,   +∞)  ==  (  -∞,   +∞)",
		"(  -∞,   +∞)  +  (  -∞,   +∞)  ==  (  -∞,   +∞)",
		"(  -∞,   +∞)  +  [   1,    2]  ==  (  -∞,   +∞)",
		"(  -∞,   +∞)  +  [   0,    0]  ==  (  -∞,   +∞)",
		"[   3,    6]  +  [...empty..]  ==  [...empty..]",
		"[...empty..]  +  [  10,   15]  ==  [...empty..]",
		"[...empty..]  +  [...empty..]  ==  [...empty..]",
		"(  -∞,   +∞)  +  [...empty..]  ==  [...empty..]",
	)
}

func TestOpSub(tt *testing.T) {
	testOp(tt,
		"[   3,    3]  -  [  -5,   -5]  ==  [   8,    8]",
		"[   3,    3]  -  [   0,    0]  ==  [   3,    3]",
		"[   0,    0]  -  [  -7,    7]  ==  [  -7,    7]",
		"[   0,    2]  -  [   0,    5]  ==  [  -5,    2]",
		"[   3,    6]  -  [  10,   15]  ==  [ -12,   -4]",
		"[   3,   +∞)  -  [  -4,   -2]  ==  [   5,   +∞)",
		"[   3,   +∞)  -  [  10,   15]  ==  [ -12,   +∞)",
		"[   3,   +∞)  -  (  -∞,   15]  ==  (  -∞,   +∞)",
		"[   3,    6]  -  (  -∞,   15]  ==  [ -12,   +∞)",
		"[   3,    6]  -  (  -∞,   +∞)  ==  (  -∞,   +∞)",
		"(  -∞,   +∞)  -  (  -∞,   +∞)  ==  (  -∞,   +∞)",
		"(  -∞,   +∞)  -  [   1,    2]  ==  (  -∞,   +∞)",
		"(  -∞,   +∞)  -  [   0,    0]  ==  (  -∞,   +∞)",
		"[   3,    6]  -  [...empty..]  ==  [...empty..]",
		"[...empty..]  -  [  10,   15]  ==  [...empty..]",
		"[...empty..]  -  [...empty..]  ==  [...empty..]",
		"(  -∞,   +∞)  -  [...empty..]  ==  [...empty..]",
	)
}

func TestOpMul(tt *testing.T) {
	testOp(tt,
		"[   3,    3]  *  [  -5,   -5]  ==  [ -15,  -15]",
		"[   3,    3]  *  [   0,    0]  ==  [   0,    0]",
		"[   0,    0]  *  [  -7,    7]  ==  [   0,    0]",
		"[   0,    2]  *  [   0,    5]  ==  [   0,   10]",
		"[   3,    6]  *  [  10,   15]  ==  [  30,   90]",
		"[   3,   +∞)  *  [  -4,   -2]  ==  (  -∞,   -6]",
		"[   3,   +∞)  *  [  10,   15]  ==  [  30,   +∞)",
		"[   3,   +∞)  *  (  -∞,   15]  ==  (  -∞,   +∞)",
		"[   3,    6]  *  (  -∞,   15]  ==  (  -∞,   90]",
		"[   3,    6]  *  (  -∞,   +∞)  ==  (  -∞,   +∞)",
		"(  -∞,   +∞)  *  (  -∞,   +∞)  ==  (  -∞,   +∞)",
		"(  -∞,   +∞)  *  [   1,    2]  ==  (  -∞,   +∞)",
		"(  -∞,   +∞)  *  [   0,    0]  ==  [   0,    0]",
		"[   3,    6]  *  [...empty..]  ==  [...empty..]",
		"[...empty..]  *  [  10,   15]  ==  [...empty..]",
		"[...empty..]  *  [...empty..]  ==  [...empty..]",
		"(  -∞,   +∞)  *  [...empty..]  ==  [...empty..]",

		"[  -3,   -1]  *  [ -11,  -10]  ==  [  10,   33]",
		"[  -3,    0]  *  [ -11,  -10]  ==  [   0,   33]",
		"[  -3,    1]  *  [ -11,  -10]  ==  [ -11,   33]",
		"[  -3,    4]  *  [ -11,  -10]  ==  [ -44,   33]",
		"[  -1,    4]  *  [ -11,  -10]  ==  [ -44,   11]",
		"[   0,    4]  *  [ -11,  -10]  ==  [ -44,    0]",
		"[   1,    4]  *  [ -11,  -10]  ==  [ -44,  -10]",

		"[  -3,   -1]  *  [  -6,    2]  ==  [  -6,   18]",
		"[  -3,    0]  *  [  -6,    2]  ==  [  -6,   18]",
		"[  -3,    1]  *  [  -6,    2]  ==  [  -6,   18]",
		"[  -3,    4]  *  [  -6,    2]  ==  [ -24,   18]",
		"[  -1,    4]  *  [  -6,    2]  ==  [ -24,    8]",
		"[   0,    4]  *  [  -6,    2]  ==  [ -24,    8]",
		"[   1,    4]  *  [  -6,    2]  ==  [ -24,    8]",

		"[  -3,   -1]  *  [   0,    3]  ==  [  -9,    0]",
		"[  -3,    0]  *  [   0,    3]  ==  [  -9,    0]",
		"[  -3,    1]  *  [   0,    3]  ==  [  -9,    3]",
		"[  -3,    4]  *  [   0,    3]  ==  [  -9,   12]",
		"[  -1,    4]  *  [   0,    3]  ==  [  -3,   12]",
		"[   0,    4]  *  [   0,    3]  ==  [   0,   12]",
		"[   1,    4]  *  [   0,    3]  ==  [   0,   12]",

		"[  -3,   -1]  *  [   2,    3]  ==  [  -9,   -2]",
		"[  -3,    0]  *  [   2,    3]  ==  [  -9,    0]",
		"[  -3,    1]  *  [   2,    3]  ==  [  -9,    3]",
		"[  -3,    4]  *  [   2,    3]  ==  [  -9,   12]",
		"[  -1,    4]  *  [   2,    3]  ==  [  -3,   12]",
		"[   0,    4]  *  [   2,    3]  ==  [   0,   12]",
		"[   1,    4]  *  [   2,    3]  ==  [   2,   12]",

		"[  -9,   +∞)  *  [   2,   +∞)  ==  (  -∞,   +∞)",
		"[  -1,   +∞)  *  [   2,   +∞)  ==  (  -∞,   +∞)",
		"[   0,   +∞)  *  [   2,   +∞)  ==  [   0,   +∞)",
		"[   1,   +∞)  *  [   2,   +∞)  ==  [   2,   +∞)",
		"[   7,   +∞)  *  [   2,   +∞)  ==  [  14,   +∞)",
		"[  -1,    1]  *  (  -∞,   +∞)  ==  (  -∞,   +∞)",
		"[   0,    0]  *  (  -∞,   +∞)  ==  [   0,    0]",
		"[   1,    1]  *  (  -∞,   +∞)  ==  (  -∞,   +∞)",
	)
}

func TestOpQuo(tt *testing.T) {
	testOp(tt,
		"[   3,    3]  /  [  -5,   -5]  ==  [   0,    0]",
		"[   3,    3]  /  [   0,    0]  ==  invalid",
		"[   0,    0]  /  [  -7,    7]  ==  invalid",
		"[   0,    2]  /  [   0,    5]  ==  invalid",
		"[   3,    6]  /  [  10,   15]  ==  [   0,    0]",
		"[   3,   +∞)  /  [  -4,   -2]  ==  (  -∞,    0]",
		"[   3,   +∞)  /  [  10,   15]  ==  [   0,   +∞)",
		"[   3,   +∞)  /  (  -∞,   15]  ==  invalid",
		"[   3,    6]  /  (  -∞,   15]  ==  invalid",
		"[   3,    6]  /  (  -∞,   +∞)  ==  invalid",
		"(  -∞,   +∞)  /  (  -∞,   +∞)  ==  invalid",
		"(  -∞,   +∞)  /  [   1,    2]  ==  (  -∞,   +∞)",
		"(  -∞,   +∞)  /  [   0,    0]  ==  invalid",
		"[   3,    6]  /  [...empty..]  ==  [...empty..]",
		"[...empty..]  /  [  10,   15]  ==  [...empty..]",
		"[...empty..]  /  [...empty..]  ==  [...empty..]",
		"(  -∞,   +∞)  /  [...empty..]  ==  [...empty..]",

		"[   1,    4]  /  [ -11,  -10]  ==  [   0,    0]",

		"[   1,    4]  /  [  -6,    2]  ==  invalid",

		"[  -3,   -1]  /  [   1,    3]  ==  [  -3,    0]",
		"[  -3,    0]  /  [   1,    3]  ==  [  -3,    0]",
		"[  -3,    1]  /  [   1,    3]  ==  [  -3,    1]",
		"[  -3,    4]  /  [   1,    3]  ==  [  -3,    4]",
		"[  -1,    4]  /  [   1,    3]  ==  [  -1,    4]",
		"[   0,    4]  /  [   1,    3]  ==  [   0,    4]",
		"[   1,    4]  /  [   1,    3]  ==  [   0,    4]",

		"[  -3,   -1]  /  [   2,    3]  ==  [  -1,    0]",
		"[  -3,    0]  /  [   2,    3]  ==  [  -1,    0]",
		"[  -3,    1]  /  [   2,    3]  ==  [  -1,    0]",
		"[  -3,    4]  /  [   2,    3]  ==  [  -1,    2]",
		"[  -1,    4]  /  [   2,    3]  ==  [   0,    2]",
		"[   0,    4]  /  [   2,    3]  ==  [   0,    2]",
		"[   1,    4]  /  [   2,    3]  ==  [   0,    2]",

		"[  -9,   +∞)  /  [   2,   +∞)  ==  [  -4,   +∞)",
		"[  -1,   +∞)  /  [   2,   +∞)  ==  [   0,   +∞)",
		"[   0,   +∞)  /  [   2,   +∞)  ==  [   0,   +∞)",
		"[   1,   +∞)  /  [   2,   +∞)  ==  [   0,   +∞)",
		"[   7,   +∞)  /  [   2,   +∞)  ==  [   0,   +∞)",
		"[  -1,    1]  /  (  -∞,   +∞)  ==  invalid",
		"[   0,    0]  /  (  -∞,   +∞)  ==  invalid",
		"[   1,    1]  /  (  -∞,   +∞)  ==  invalid",
	)
}

func TestOpLsh(tt *testing.T) {
	testOp(tt,
		"[   3,    3]  «  [  -5,   -5]  ==  invalid",
		"[   3,    3]  «  [   0,    0]  ==  [   3,    3]",
		"[   0,    0]  «  [  -7,    7]  ==  invalid",
		"[   0,    2]  «  [   0,    5]  ==  [   0,   64]",
		"[   3,    6]  «  [  10,   15]  ==  [3072, 196608]",
		"[   3,   +∞)  «  [  -4,   -2]  ==  invalid",
		"[   3,   +∞)  «  [  10,   15]  ==  [3072,   +∞)",
		"[   3,   +∞)  «  (  -∞,   15]  ==  invalid",
		"[   3,    6]  «  (  -∞,   15]  ==  invalid",
		"[   3,    6]  «  (  -∞,   +∞)  ==  invalid",
		"(  -∞,   +∞)  «  (  -∞,   +∞)  ==  invalid",
		"(  -∞,   +∞)  «  [   1,    2]  ==  (  -∞,   +∞)",
		"(  -∞,   +∞)  «  [   0,    0]  ==  (  -∞,   +∞)",
		"[   3,    6]  «  [...empty..]  ==  [...empty..]",
		"[...empty..]  «  [  10,   15]  ==  [...empty..]",
		"[...empty..]  «  [...empty..]  ==  [...empty..]",
		"(  -∞,   +∞)  «  [...empty..]  ==  [...empty..]",

		"[   1,    4]  «  [ -11,  -10]  ==  invalid",

		"[   1,    4]  «  [  -6,    2]  ==  invalid",

		"[  -3,   -1]  «  [   0,    3]  ==  [ -24,   -1]",
		"[  -3,    0]  «  [   0,    3]  ==  [ -24,    0]",
		"[  -3,    1]  «  [   0,    3]  ==  [ -24,    8]",
		"[  -3,    4]  «  [   0,    3]  ==  [ -24,   32]",
		"[  -1,    4]  «  [   0,    3]  ==  [  -8,   32]",
		"[   0,    4]  «  [   0,    3]  ==  [   0,   32]",
		"[   1,    4]  «  [   0,    3]  ==  [   1,   32]",

		"[  -3,   -1]  «  [   2,    3]  ==  [ -24,   -4]",
		"[  -3,    0]  «  [   2,    3]  ==  [ -24,    0]",
		"[  -3,    1]  «  [   2,    3]  ==  [ -24,    8]",
		"[  -3,    4]  «  [   2,    3]  ==  [ -24,   32]",
		"[  -1,    4]  «  [   2,    3]  ==  [  -8,   32]",
		"[   0,    4]  «  [   2,    3]  ==  [   0,   32]",
		"[   1,    4]  «  [   2,    3]  ==  [   4,   32]",

		"[  -9,   +∞)  «  [   2,   +∞)  ==  (  -∞,   +∞)",
		"[  -1,   +∞)  «  [   2,   +∞)  ==  (  -∞,   +∞)",
		"[   0,   +∞)  «  [   2,   +∞)  ==  [   0,   +∞)",
		"[   1,   +∞)  «  [   2,   +∞)  ==  [   4,   +∞)",
		"[   7,   +∞)  «  [   2,   +∞)  ==  [  28,   +∞)",
		"[  -1,    1]  «  (  -∞,   +∞)  ==  invalid",
		"[   0,    0]  «  (  -∞,   +∞)  ==  invalid",
		"[   1,    1]  «  (  -∞,   +∞)  ==  invalid",
	)
}

func TestOpRsh(tt *testing.T) {
	testOp(tt,
		"[   3,    3]  »  [  -5,   -5]  ==  invalid",
		"[   3,    3]  »  [   0,    0]  ==  [   3,    3]",
		"[   0,    0]  »  [  -7,    7]  ==  invalid",
		"[   0,    2]  »  [   0,    5]  ==  [   0,    2]",
		"[   3,    6]  »  [  10,   15]  ==  [   0,    0]",
		"[   3,   +∞)  »  [  -4,   -2]  ==  invalid",
		"[   3,   +∞)  »  [  10,   15]  ==  [   0,   +∞)",
		"[   3,   +∞)  »  (  -∞,   15]  ==  invalid",
		"[   3,    6]  »  (  -∞,   15]  ==  invalid",
		"[   3,    6]  »  (  -∞,   +∞)  ==  invalid",
		"(  -∞,   +∞)  »  (  -∞,   +∞)  ==  invalid",
		"(  -∞,   +∞)  »  [   1,    2]  ==  (  -∞,   +∞)",
		"(  -∞,   +∞)  »  [   0,    0]  ==  (  -∞,   +∞)",
		"[   3,    6]  »  [...empty..]  ==  [...empty..]",
		"[...empty..]  »  [  10,   15]  ==  [...empty..]",
		"[...empty..]  »  [...empty..]  ==  [...empty..]",
		"(  -∞,   +∞)  »  [...empty..]  ==  [...empty..]",

		"[   1,    4]  »  [ -11,  -10]  ==  invalid",

		"[   1,    4]  »  [  -6,    2]  ==  invalid",

		"[  -3,   -1]  »  [   0,    3]  ==  [  -3,   -1]",
		"[  -3,    0]  »  [   0,    3]  ==  [  -3,    0]",
		"[  -3,    1]  »  [   0,    3]  ==  [  -3,    1]",
		"[  -3,    4]  »  [   0,    3]  ==  [  -3,    4]",
		"[  -1,    4]  »  [   0,    3]  ==  [  -1,    4]",
		"[   0,    4]  »  [   0,    3]  ==  [   0,    4]",
		"[   1,    4]  »  [   0,    3]  ==  [   0,    4]",

		"[  -3,   -1]  »  [   1,    3]  ==  [  -2,   -1]",
		"[  -3,    0]  »  [   1,    3]  ==  [  -2,    0]",
		"[  -3,    1]  »  [   1,    3]  ==  [  -2,    0]",
		"[  -3,    4]  »  [   1,    3]  ==  [  -2,    2]",
		"[  -1,    4]  »  [   1,    3]  ==  [  -1,    2]",
		"[   0,    4]  »  [   1,    3]  ==  [   0,    2]",
		"[   1,    4]  »  [   1,    3]  ==  [   0,    2]",

		"[  -9,   +∞)  »  [   2,   +∞)  ==  [  -3,   +∞)",
		"[  -1,   +∞)  »  [   2,   +∞)  ==  [  -1,   +∞)",
		"[   0,   +∞)  »  [   2,   +∞)  ==  [   0,   +∞)",
		"[   1,   +∞)  »  [   2,   +∞)  ==  [   0,   +∞)",
		"[   7,   +∞)  »  [   2,   +∞)  ==  [   0,   +∞)",
		"[  -1,    1]  »  (  -∞,   +∞)  ==  invalid",
		"[   0,    0]  »  (  -∞,   +∞)  ==  invalid",
		"[   1,    1]  »  (  -∞,   +∞)  ==  invalid",
	)
}

func TestOpAnd(tt *testing.T) {
	testOp(tt,
		"[   3,    3]  &  [  -5,   -5]  ==  [   3,    3]",
		"[   3,    3]  &  [   0,    0]  ==  [   0,    0]",
		"[   0,    0]  &  [  -7,    7]  ==  [   0,    0]",
		"[   0,    2]  &  [   0,    5]  ==  [   0,    2]",
		"[   3,    6]  &  [  10,   15]  ==  [   0,    6]",
		"[   3,   +∞)  &  [  -4,   -2]  ==  [   0,   +∞)",
		"[   3,   +∞)  &  [  10,   15]  ==  [   0,   15]",
		"[   3,   +∞)  &  (  -∞,   15]  ==  [   0,   +∞)",
		"[   3,    6]  &  (  -∞,   15]  ==  [   0,    6]",
		"[   3,    6]  &  (  -∞,   +∞)  ==  [   0,    6]",
		"(  -∞,   +∞)  &  (  -∞,   +∞)  ==  (  -∞,   +∞)",
		"(  -∞,   +∞)  &  [   1,    2]  ==  [   0,    2]",
		"(  -∞,   +∞)  &  [   0,    0]  ==  [   0,    0]",
		"[   3,    6]  &  [...empty..]  ==  [...empty..]",
		"[...empty..]  &  [  10,   15]  ==  [...empty..]",
		"[...empty..]  &  [...empty..]  ==  [...empty..]",
		"(  -∞,   +∞)  &  [...empty..]  ==  [...empty..]",

		"[   1,    4]  &  [ -11,  -10]  ==  [   0,    4]",

		"[   1,    4]  &  [  -6,    2]  ==  [   0,    4]",

		"[  -3,   -1]  &  [   0,    3]  ==  [   0,    3]",
		"[  -1,    4]  &  [   0,    3]  ==  [   0,    3]",
		"[   0,    4]  &  [   0,    3]  ==  [   0,    3]",
		"[   1,    4]  &  [   0,    3]  ==  [   0,    3]",

		"[  -3,   -1]  &  [   2,    3]  ==  [   0,    3]",
		"[  -1,    4]  &  [   2,    3]  ==  [   0,    3]",
		"[   0,    4]  &  [   2,    3]  ==  [   0,    3]",
		"[   1,    4]  &  [   2,    3]  ==  [   0,    3]",

		"[  -9,   +∞)  &  [   2,   +∞)  ==  [   0,   +∞)",
		"[  -1,   +∞)  &  [   2,   +∞)  ==  [   0,   +∞)",
		"[   0,   +∞)  &  [   2,   +∞)  ==  [   0,   +∞)",
		"[   1,   +∞)  &  [   2,   +∞)  ==  [   0,   +∞)",
		"[   7,   +∞)  &  [   2,   +∞)  ==  [   0,   +∞)",
		"[  -1,    1]  &  (  -∞,   +∞)  ==  (  -∞,   +∞)",
		"[   0,    0]  &  (  -∞,   +∞)  ==  [   0,    0]",
		"[   1,    1]  &  (  -∞,   +∞)  ==  [   0,    1]",

		"[   1,    3]  &  [   4,    9]  ==  [   0,    3]",
		"[   3,    4]  &  [   5,    6]  ==  [   1,    4]",
		"[   4,    5]  &  [   6,    7]  ==  [   4,    5]",
		"[   7,    7]  &  [  12,   14]  ==  [   4,    6]",

		"[   5,    6]  &  [   6,    7]  ==  [   4,    6]",
		"[   5,    6]  &  [   6,    8]  ==  [   0,    6]",
		"[   5,    9]  &  [   6,    8]  ==  [   0,    8]",
		"[   5,    9]  &  [   6,    9]  ==  [   0,    9]",

		"[   5,    6]  &  [   3,   +∞)  ==  [   0,    6]",
		"[   5,    9]  &  [   3,   +∞)  ==  [   0,    9]",
		"[   5,    6]  &  [   7,   +∞)  ==  [   0,    6]",
		"[   5,    9]  &  [   7,   +∞)  ==  [   0,    9]",
		"[   5,    6]  &  [   8,   +∞)  ==  [   0,    6]",
		"[   5,    9]  &  [   8,   +∞)  ==  [   0,    9]",
		"[   5,    6]  &  [   9,   +∞)  ==  [   0,    6]",
		"[   5,    9]  &  [   9,   +∞)  ==  [   0,    9]",
		"[   5,    6]  &  [  12,   +∞)  ==  [   0,    6]",
		"[   5,    9]  &  [  12,   +∞)  ==  [   0,    9]",

		"(  -∞,   -3]  &  [  -3,   -2]  ==  (  -∞,   -3]",
		"[   2,   +∞)  &  [   1,    2]  ==  [   0,    2]",
	)
}

func TestOpOr(tt *testing.T) {
	testOp(tt,
		"[   3,    3]  |  [  -5,   -5]  ==  [  -5,   -5]",
		"[   3,    3]  |  [   0,    0]  ==  [   3,    3]",
		"[   0,    0]  |  [  -7,    7]  ==  [  -7,    7]",
		"[   0,    2]  |  [   0,    5]  ==  [   0,    7]",
		"[   3,    6]  |  [  10,   15]  ==  [  11,   15]",
		"[   3,   +∞)  |  [  -4,   -2]  ==  [  -4,   -1]",
		"[   3,   +∞)  |  [  10,   15]  ==  [  10,   +∞)",
		"[   3,   +∞)  |  (  -∞,   15]  ==  (  -∞,   +∞)",
		"[   3,    6]  |  (  -∞,   15]  ==  (  -∞,   15]",
		"[   3,    6]  |  (  -∞,   +∞)  ==  (  -∞,   +∞)",
		"(  -∞,   +∞)  |  (  -∞,   +∞)  ==  (  -∞,   +∞)",
		"(  -∞,   +∞)  |  [   1,    2]  ==  (  -∞,   +∞)",
		"(  -∞,   +∞)  |  [   0,    0]  ==  (  -∞,   +∞)",
		"[   3,    6]  |  [...empty..]  ==  [...empty..]",
		"[...empty..]  |  [  10,   15]  ==  [...empty..]",
		"[...empty..]  |  [...empty..]  ==  [...empty..]",
		"(  -∞,   +∞)  |  [...empty..]  ==  [...empty..]",

		"[   1,    4]  |  [ -11,  -10]  ==  [ -11,   -9]",

		"[   1,    4]  |  [  -6,    2]  ==  [  -6,    6]",

		"[  -3,   -1]  |  [   0,    3]  ==  [  -3,   -1]",
		"[  -1,    4]  |  [   0,    3]  ==  [  -1,    7]",
		"[   0,    4]  |  [   0,    3]  ==  [   0,    7]",
		"[   1,    4]  |  [   0,    3]  ==  [   1,    7]",

		"[  -3,   -1]  |  [   2,    3]  ==  [  -2,   -1]",
		"[  -1,    4]  |  [   2,    3]  ==  [  -1,    7]",
		"[   0,    4]  |  [   2,    3]  ==  [   2,    7]",
		"[   1,    4]  |  [   2,    3]  ==  [   2,    7]",

		"[  -9,   +∞)  |  [   2,   +∞)  ==  [  -9,   +∞)",
		"[  -1,   +∞)  |  [   2,   +∞)  ==  [  -1,   +∞)",
		"[   0,   +∞)  |  [   2,   +∞)  ==  [   2,   +∞)",
		"[   1,   +∞)  |  [   2,   +∞)  ==  [   2,   +∞)",
		"[   7,   +∞)  |  [   2,   +∞)  ==  [   7,   +∞)",
		"[  -1,    1]  |  (  -∞,   +∞)  ==  (  -∞,   +∞)",
		"[   0,    0]  |  (  -∞,   +∞)  ==  (  -∞,   +∞)",
		"[   1,    1]  |  (  -∞,   +∞)  ==  (  -∞,   +∞)",

		"[   1,    3]  |  [   4,    9]  ==  [   5,   11]",
		"[   3,    4]  |  [   5,    6]  ==  [   5,    7]",
		"[   4,    5]  |  [   6,    7]  ==  [   6,    7]",
		"[   7,    7]  |  [  12,   14]  ==  [  15,   15]",

		"[   5,    6]  |  [   6,    7]  ==  [   6,    7]",
		"[   5,    6]  |  [   6,    8]  ==  [   6,   14]",
		"[   5,    9]  |  [   6,    8]  ==  [   6,   15]",
		"[   5,    9]  |  [   6,    9]  ==  [   6,   15]",

		"[   5,    6]  |  [   3,   +∞)  ==  [   5,   +∞)",
		"[   5,    9]  |  [   3,   +∞)  ==  [   5,   +∞)",
		"[   5,    6]  |  [   7,   +∞)  ==  [   7,   +∞)",
		"[   5,    9]  |  [   7,   +∞)  ==  [   7,   +∞)",
		"[   5,    6]  |  [   8,   +∞)  ==  [  13,   +∞)",
		"[   5,    9]  |  [   8,   +∞)  ==  [   8,   +∞)",
		"[   5,    6]  |  [   9,   +∞)  ==  [  13,   +∞)",
		"[   5,    9]  |  [   9,   +∞)  ==  [   9,   +∞)",
		"[   5,    6]  |  [  12,   +∞)  ==  [  13,   +∞)",
		"[   5,    9]  |  [  12,   +∞)  ==  [  12,   +∞)",

		"(  -∞,   -3]  |  [  -3,   -2]  ==  [  -3,   -1]",
		"[   2,   +∞)  |  [   1,    2]  ==  [   2,   +∞)",
	)
}

func TestOpAndWithMinusOne(tt *testing.T) {
	minusOne := IntRange{big.NewInt(-1), big.NewInt(-1)}
	for _, x0 := range fromNeg3ToPos3 {
		for _, x1 := range fromNeg3ToPos3 {
			x := IntRange{x0, x1}
			want := x

			if got := x.And(minusOne); !got.Eq(want) {
				tt.Fatalf("%v & -1: got %v, want %v", x, got, want)
			}
			if got := minusOne.And(x); !got.Eq(want) {
				tt.Fatalf("-1 & %v: got %v, want %v", x, got, want)
			}
		}
	}
}

func TestOpAndWithZero(tt *testing.T) {
	zero := IntRange{big.NewInt(+0), big.NewInt(+0)}
	for _, x0 := range fromNeg3ToPos3 {
		for _, x1 := range fromNeg3ToPos3 {
			x := IntRange{x0, x1}
			want := zero
			if x.Empty() {
				want = sharedEmptyRange
			}

			if got := x.And(zero); !got.Eq(want) {
				tt.Fatalf("%v & +0: got %v, want %v", x, got, want)
			}
			if got := zero.And(x); !got.Eq(want) {
				tt.Fatalf("+0 & %v: got %v, want %v", x, got, want)
			}
		}
	}
}

func TestOpOrWithMinusOne(tt *testing.T) {
	minusOne := IntRange{big.NewInt(-1), big.NewInt(-1)}
	for _, x0 := range fromNeg3ToPos3 {
		for _, x1 := range fromNeg3ToPos3 {
			x := IntRange{x0, x1}
			want := minusOne
			if x.Empty() {
				want = sharedEmptyRange
			}

			if got := x.Or(minusOne); !got.Eq(want) {
				tt.Fatalf("%v | -1: got %v, want %v", x, got, want)
			}
			if got := minusOne.Or(x); !got.Eq(want) {
				tt.Fatalf("-1 | %v: got %v, want %v", x, got, want)
			}
		}
	}
}

func TestOpOrWithZero(tt *testing.T) {
	zero := IntRange{big.NewInt(+0), big.NewInt(+0)}
	for _, x0 := range fromNeg3ToPos3 {
		for _, x1 := range fromNeg3ToPos3 {
			x := IntRange{x0, x1}
			want := x

			if got := x.Or(zero); !got.Eq(want) {
				tt.Fatalf("%v | +0: got %v, want %v", x, got, want)
			}
			if got := zero.Or(x); !got.Eq(want) {
				tt.Fatalf("+0 | %v: got %v, want %v", x, got, want)
			}
		}
	}
}
