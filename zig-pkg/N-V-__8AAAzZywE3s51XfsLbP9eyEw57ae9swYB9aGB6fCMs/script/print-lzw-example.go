// Copyright 2018 The Wuffs Authors.
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

// print-lzw-example.go prints a worked example discussed in std/lzw/README.md,
// based on three implementations of a simplified core of the LZW algorithm.
// Simplifications include assuming LSB-first, 8 bit literals, no clear codes,
// no full tables and that the input is given in terms of a code stream, not a
// bit stream.
//
// The implementations are designed for ease of studying, and for e.g. a
// minimal diff between the suf1 and sufQ implementations, and are not
// optimized for performance.
//
// Usage: go run print-lzw-example.go
//
// You can also play with the code (e.g. modify Q and re-run) online:
// https://play.golang.org/p/1aLC_XHZzoA

import (
	"fmt"
	"os"
)

const (
	expectedOutput = "TOBEORNOTTOBEORTOBEORNOTXOTXOTXOOTXOOOTXOOOTOBEY"

	clearCode = 0x100
	endCode   = 0x101

	// Neither clearCode or endCode can ever be a valid prefix code. We
	// arbitrarily pick clearCode to represent "no prefix".
	noPrefix = clearCode

	// Q is the maximum possible suffix length, in bytes, which equals 1 plus
	// the maximum possible "suffix length minus 1". Q == 1 means that sufQ is
	// equivalent to suf1.
	//
	// In std/lzw/README.md, Q is 3 to keep the example short and simple. In
	// practice, especially on modern CPUs that can do unaligned 32 or 64 bit
	// loads and stores, Q is 4 or 8.
	Q = 3
)

var codes = []int{
	'T', 'O', 'B', 'E', 'O', 'R', 'N', 'O', 'T',
	0x102, 0x104, 0x106, 0x10B, 0x105, 0x107, 0x109,
	'X',
	0x111, 0x113, 0x114, 0x115, 0x10E,
	'Y',
	0x101,
}

func codeString(code int) string {
	if code < clearCode {
		return fmt.Sprintf("%5c", code)
	} else if code == noPrefix {
		return "    -"
	}
	return fmt.Sprintf("0x%03X", code)
}

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	n, s, q := new(naive), new(suf1), new(sufQ)
	decode(n)
	decode(s)
	decode(q)

	key := endCode
	fmt.Printf(" Code   Emits    Key    Value   Pre1+Suf1  LM1  /Q  %%Q   PreQ+SufQ\n")
	for _, code := range codes {
		// Code
		fmt.Printf("%s", codeString(code))

		// Emits
		if code == endCode {
			fmt.Printf("   -end-\n")
			break
		}
		fmt.Printf("  %6s", n[code])

		if key != endCode {
			fmt.Printf("  0x%03X  %7s  %s %c     %3d %3d %3d  %s %s",
				key,                              // Key
				n[key],                           // Value
				codeString(int(s.prefixes[key])), // Pre1
				s.suffixes[key],                  // Suf1
				q.lengthM1s[key],                 // LM1
				q.lengthM1s[key]/Q,               // /Q
				q.lengthM1s[key]%Q,               // %Q
				codeString(int(q.prefixes[key])), // PreQ
				string(q.suffixes[key][:]),       // SufQ
			)
		}

		fmt.Println()
		key++
	}
	return nil
}

type implementation interface {
	initialize()
	emit(buffer []byte, code int, prevCode int, n int) []byte
}

func decode(impl implementation) {
	impl.initialize()

	output, prevCode, n := []byte(nil), -1, 0x101
	for _, code := range codes {
		if code == clearCode {
			panic("unimplemented clearCode")
		} else if code == endCode {
			if string(output) != expectedOutput {
				panic("unexpected output")
			}
			return
		} else if code > n {
			panic("invalid code")
		}

		// We have a literal or copy code.
		output = impl.emit(output, code, prevCode, n)

		prevCode, n = code, n+1
		if n >= 4096 {
			panic("unimplemented table-is-full")
		}
	}
	panic("missing endCode")
}

// naive is a simple implementation that, in the worst case, requires O(N*N)
// memory.
type naive [4096]string

func (t *naive) initialize() {
	for i := 0; i < clearCode; i++ {
		t[i] = string([]byte{byte(i)})
	}
}

func (t *naive) emit(output []byte, code int, prevCode int, n int) []byte {
	if prevCode >= 0 {
		prevOutput := t[prevCode]
		if code < n {
			t[n] = prevOutput + t[code][:1]
		} else {
			t[n] = prevOutput + prevOutput[:1]
		}
	}
	return append(output, t[code]...)
}

// suf1 keeps separate prefix and suffix tables, 1 byte per suffix.
type suf1 struct {
	prefixes [4096]uint16
	suffixes [4096]byte
}

func (t *suf1) initialize() {
	for i := 0; i < clearCode; i++ {
		t.prefixes[i] = noPrefix
		t.suffixes[i] = byte(i)
	}
}

func (t *suf1) emit(output []byte, code int, prevCode int, n int) []byte {
	// Fill an intermediate buffer from back to front, 1 byte at a time.
	buffer := [4096]byte{}
	i := len(buffer)

	c := code
	if code == n {
		c = prevCode
		i--
		// buffer[4095] will be set later.
	}

	firstByte := byte(0)
	for {
		suffix := t.suffixes[c]
		i--
		buffer[i] = suffix
		c = int(t.prefixes[c])
		if c == noPrefix {
			firstByte = suffix
			break
		}
	}

	if code == n {
		buffer[4095] = firstByte
	}

	// The "if prevCode >= 0" guard, and initializing prevCode to -1 instead of
	// 0, is correct conceptually, but unnecessary in practice. Look for "fake
	// key-value pair" in std/lzw/README.md.
	if prevCode >= 0 {
		t.prefixes[n] = uint16(prevCode)
		t.suffixes[n] = firstByte
	}

	return append(output, buffer[i:4096]...)
}

// sufQ keeps separate prefix and suffix tables, up to Q bytes per suffix.
type sufQ struct {
	lengthM1s [4096]uint16
	prefixes  [4096]uint16
	suffixes  [4096][Q]byte
}

func (t *sufQ) initialize() {
	for i := 0; i < clearCode; i++ {
		t.lengthM1s[i] = 0
		t.prefixes[i] = noPrefix
		t.suffixes[i][0] = byte(i)
		for j := 1; j < Q; j++ {
			// Unnecessary for correct output, but makes the printed table prettier.
			t.suffixes[i][j] = '.'
		}
	}
}

func (t *sufQ) emit(output []byte, code int, prevCode int, n int) []byte {
	// Fill an intermediate buffer from back to front, Q bytes at a time.
	buffer := [4096 + Q - 1]byte{}
	i := len(buffer)

	c := code
	if code == n {
		c = prevCode
		i--
		// buffer[4095] will be set later.
	}

	i -= int(t.lengthM1s[c] % Q)

	firstByte := byte(0)
	for {
		suffix := t.suffixes[c]
		i -= Q
		copy(buffer[i:i+Q], suffix[:])
		c = int(t.prefixes[c])
		if c == noPrefix {
			firstByte = suffix[0]
			break
		}
	}

	if code == n {
		buffer[4095] = firstByte
	}

	// The "if prevCode >= 0" guard, and initializing prevCode to -1 instead of
	// 0, is correct conceptually, but unnecessary in practice. Look for "fake
	// key-value pair" in std/lzw/README.md.
	if prevCode >= 0 {
		lm1 := t.lengthM1s[prevCode] + 1
		t.lengthM1s[n] = lm1
		lm1 %= Q
		if lm1 != 0 {
			// Copy the prevCode's prefix and suffix, and then extend the suffix.
			t.prefixes[n] = t.prefixes[prevCode]
			t.suffixes[n] = t.suffixes[prevCode]
			t.suffixes[n][lm1] = firstByte
		} else {
			// Start a new suffix.
			t.prefixes[n] = uint16(prevCode)
			t.suffixes[n][0] = firstByte
			for j := 1; j < Q; j++ {
				// Unnecessary for correct output, but makes the printed table prettier.
				t.suffixes[n][j] = '.'
			}
		}
	}

	return append(output, buffer[i:4096]...)
}
