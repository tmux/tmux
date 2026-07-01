// Copyright 2017 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package base38

import (
	"testing"
)

func TestMax(tt *testing.T) {
	if Max == 38*38*38*38-1 {
		return
	}
	tt.Fatal("Max does not satisfy its definition")
}

func TestMaxBits(tt *testing.T) {
	if (1<<(MaxBits-1) <= Max) && (Max < 1<<MaxBits) {
		return
	}
	tt.Fatal("MaxBits does not satisfy its definition")
}

func mk(a, b, c, d uint32) uint32 {
	return a*38*38*38 + b*38*38 + c*38 + d
}

func TestValid(tt *testing.T) {
	testCases := []struct {
		s    string
		want uint32
	}{
		{"....", mk(0, 0, 0, 0)},
		{"0...", mk(1, 0, 0, 0)},
		{".0..", mk(0, 1, 0, 0)},
		{"..0.", mk(0, 0, 1, 0)},
		{"...0", mk(0, 0, 0, 1)},
		{"aa12", mk(11, 11, 2, 3)},
		{"6543", mk(7, 6, 5, 4)},
		{"789a", mk(8, 9, 10, 11)},
		{"bcde", mk(12, 13, 14, 15)},
		{"fghi", mk(16, 17, 18, 19)},
		{"jklm", mk(20, 21, 22, 23)},
		{".m0m", mk(0, 23, 1, 23)},
		{"nopq", mk(24, 25, 26, 27)},
		{"rstu", mk(28, 29, 30, 31)},
		{"vwxy", mk(32, 33, 34, 35)},
		{"zzzz", mk(36, 36, 36, 36)},
		{"z~z9", mk(36, 37, 36, 10)},
		{"~z~9", mk(37, 36, 37, 10)},
		{"~~~~", mk(37, 37, 37, 37)},
	}

	maxSeen := false
	for _, tc := range testCases {
		got, gotOK := Encode(tc.s)
		if !gotOK {
			tt.Errorf("%q: ok: got %t, want %t", tc.s, gotOK, true)
			continue
		}
		if got != tc.want {
			tt.Errorf("%q: got %d, want %d", tc.s, got, tc.want)
			continue
		}
		if got > Max || got > 1<<MaxBits-1 {
			tt.Errorf("%q: got %d, want <= %d and <= %d", tc.s, got, Max, 1<<MaxBits-1)
			continue
		}
		maxSeen = maxSeen || (got == Max)
	}
	if !maxSeen {
		tt.Error("Max was not seen")
	}
}

func TestInvalid(tt *testing.T) {
	testCases := []string{
		"",
		".",
		"..",
		"...",
		"    ",
		".....",
		"......",
		"Abcd",
		"a\x00cd",
		"ab+d",
		"abc\x80",
	}

	for _, tc := range testCases {
		if _, gotOK := Encode(tc); gotOK {
			tt.Errorf("%q: ok: got %t, want %t", tc, gotOK, false)
			continue
		}
	}
}
