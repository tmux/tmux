// Copyright 2017 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ----------------

// Package base38 converts a 4-byte string, each byte in [.0-9a-z~], to a base
// 38 number.
package base38

const (
	// Max is the inclusive upper bound for the value returned by Encode. Its
	// value equals (38*38*38*38 - 1).
	Max = 2085135
	// MaxBits is the number of bits required to represent Max. It satisfies
	// (1<<(MaxBits-1) <= Max) && (Max < 1<<MaxBits).
	MaxBits = 21
)

// Encode encodes a 4-byte string as a uint32 in the range [0, Max].
//
// Each byte must be one of:
//   - '.'
//   - in the range ['0', '9']
//   - in the range ['a', 'z']
//   - '~'
//
// The string "...." is mapped to zero.
func Encode(s string) (u uint32, ok bool) {
	if len(s) == 4 {
		for i := 0; i < 4; i++ {
			x := uint32(table[s[i]])
			if x == 0 {
				break
			}
			u += x - 1
			if i == 3 {
				return u, true
			}
			u *= 38
		}
	}
	return 0, false
}

var table = [256]uint8{
	'.': 1,
	'0': 2,
	'1': 3,
	'2': 4,
	'3': 5,
	'4': 6,
	'5': 7,
	'6': 8,
	'7': 9,
	'8': 10,
	'9': 11,
	'a': 12,
	'b': 13,
	'c': 14,
	'd': 15,
	'e': 16,
	'f': 17,
	'g': 18,
	'h': 19,
	'i': 20,
	'j': 21,
	'k': 22,
	'l': 23,
	'm': 24,
	'n': 25,
	'o': 26,
	'p': 27,
	'q': 28,
	'r': 29,
	's': 30,
	't': 31,
	'u': 32,
	'v': 33,
	'w': 34,
	'x': 35,
	'y': 36,
	'z': 37,
	'~': 38,
}
