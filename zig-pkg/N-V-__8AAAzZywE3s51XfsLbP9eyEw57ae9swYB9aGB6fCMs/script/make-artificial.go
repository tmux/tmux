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

// make-artificial.go makes test data in various formats.
//
// See test/data/artificial/*.make.txt for examples.
//
// Usage: go run make-artificial.go < foo.make.txt

import (
	"bytes"
	"compress/lzw"
	"compress/zlib"
	"fmt"
	"hash/crc32"
	"io"
	"os"
	"sort"
	"strconv"
	"strings"
)

type stateFunc func(line string) (stateFunc, error)

type repeat struct {
	count     uint32
	remaining string
}

var (
	state   stateFunc
	repeats []repeat
	out     []byte
	formats = map[string]stateFunc{}
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	stdin, err := io.ReadAll(os.Stdin)
	if err != nil {
		return err
	}
	s := string(stdin)
	for remaining := ""; len(s) > 0; s, remaining = remaining, "" {
		if i := strings.IndexByte(s, '\n'); i >= 0 {
			s, remaining = s[:i], s[i+1:]
		}
		s = strings.TrimSpace(s)
		if s == "" || s[0] == '#' {
			continue
		}

		if state == nil {
			const m = "make "
			if !strings.HasPrefix(s, m) {
				return fmt.Errorf(`input must start with "make foo"`)
			}
			s = s[len(m):]
			state = formats[s]
			if state == nil {
				return fmt.Errorf("unsupported format %q", s)
			}
			continue
		}

		const rep = "repeat "
		if strings.HasPrefix(s, rep) {
			args := s[len(rep):]
			count, args, ok := parseNum32(args)
			if !ok || count <= 0 || args != "[" {
				return fmt.Errorf("bad repeat command: %q", s)
			}
			repeats = append(repeats, repeat{
				count:     count,
				remaining: remaining,
			})
			continue
		}

		if s == "]" {
			if len(repeats) <= 0 {
				return fmt.Errorf(`unbalanced close-repeat command: "]"`)
			}
			i := len(repeats) - 1
			repeats[i].count--
			if repeats[i].count == 0 {
				repeats = repeats[:i]
			} else {
				remaining = repeats[i].remaining
			}
			continue
		}

		state, err = state(s)
		if err != nil {
			return err
		}
		if state == nil {
			return fmt.Errorf("bad state transition")
		}
	}

	if state == nil {
		return fmt.Errorf("no 'make' line")
	} else {
		state, err = state("")
		if err != nil {
			return err
		}
	}

	_, err = os.Stdout.Write(out)
	return err
}

// ----

func appendU16LE(b []byte, u uint16) []byte {
	return append(b, uint8(u), uint8(u>>8))
}

func appendU32BE(b []byte, u uint32) []byte {
	return append(b, uint8(u>>24), uint8(u>>16), uint8(u>>8), uint8(u))
}

func log2(u uint32) (i int32) {
	for i, pow := uint32(0), uint32(1); i < 32; i, pow = i+1, pow<<1 {
		if u == pow {
			return int32(i)
		}
		if u < pow {
			break
		}
	}
	return -1
}

func parseHex32(s string) (num uint32, remaining string, ok bool) {
	if i := strings.IndexByte(s, ' '); i >= 0 {
		s, remaining = s[:i], s[i+1:]
		for len(remaining) > 0 && remaining[0] == ' ' {
			remaining = remaining[1:]
		}
	}

	if len(s) < 2 || s[0] != '0' || s[1] != 'x' {
		return 0, "", false
	}
	s = s[2:]

	u, err := strconv.ParseUint(s, 16, 32)
	if err != nil {
		return 0, "", false
	}
	return uint32(u), remaining, true
}

func parseHex64(s string) (num uint64, remaining string, ok bool) {
	if i := strings.IndexByte(s, ' '); i >= 0 {
		s, remaining = s[:i], s[i+1:]
		for len(remaining) > 0 && remaining[0] == ' ' {
			remaining = remaining[1:]
		}
	}

	if len(s) < 2 || s[0] != '0' || s[1] != 'x' {
		return 0, "", false
	}
	s = s[2:]

	u, err := strconv.ParseUint(s, 16, 64)
	if err != nil {
		return 0, "", false
	}
	return u, remaining, true
}

func parseNum32(s string) (num uint32, remaining string, ok bool) {
	if i := strings.IndexByte(s, ' '); i >= 0 {
		s, remaining = s[:i], s[i+1:]
		for len(remaining) > 0 && remaining[0] == ' ' {
			remaining = remaining[1:]
		}
	}

	u, err := strconv.ParseUint(s, 10, 32)
	if err != nil {
		return 0, "", false
	}
	return uint32(u), remaining, true
}

func reverse(x uint32, n uint32) uint32 {
	x = uint32(reverse8[0xFF&(x>>0)])<<24 |
		uint32(reverse8[0xFF&(x>>8)])<<16 |
		uint32(reverse8[0xFF&(x>>16)])<<8 |
		uint32(reverse8[0xFF&(x>>24)])<<0
	return x >> (32 - n)
}

var reverse8 = [256]byte{
	0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, // 0x00 - 0x07
	0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0, // 0x08 - 0x0F
	0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, // 0x10 - 0x17
	0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8, // 0x18 - 0x1F
	0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, // 0x20 - 0x27
	0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4, // 0x28 - 0x2F
	0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, // 0x30 - 0x37
	0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC, // 0x38 - 0x3F
	0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, // 0x40 - 0x47
	0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2, // 0x48 - 0x4F
	0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, // 0x50 - 0x57
	0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA, // 0x58 - 0x5F
	0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, // 0x60 - 0x67
	0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6, // 0x68 - 0x6F
	0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, // 0x70 - 0x77
	0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE, // 0x78 - 0x7F
	0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, // 0x80 - 0x87
	0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1, // 0x88 - 0x8F
	0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, // 0x90 - 0x97
	0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9, // 0x98 - 0x9F
	0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, // 0xA0 - 0xA7
	0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5, // 0xA8 - 0xAF
	0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, // 0xB0 - 0xB7
	0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD, // 0xB8 - 0xBF
	0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, // 0xC0 - 0xC7
	0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3, // 0xC8 - 0xCF
	0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, // 0xD0 - 0xD7
	0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB, // 0xD8 - 0xDF
	0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, // 0xE0 - 0xE7
	0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7, // 0xE8 - 0xEF
	0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, // 0xF0 - 0xF7
	0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF, // 0xF8 - 0xFF
}

// ----

func init() {
	formats["bzip2"] = stateBzip2
}

var bzip2Globals struct {
	stream bzip2BitStream
}

func stateBzip2(line string) (stateFunc, error) {
	g := &bzip2Globals
	const (
		cmdB = "bits "
	)
	switch {
	case line == "":
		g.stream.flush()
		return stateBzip2, nil

	case strings.HasPrefix(line, cmdB):
		s := strings.TrimSpace(line[len(cmdB):])
		n, s, ok := parseNum32(s)
		if !ok || s == "" {
			break
		}
		x, s, ok := parseHex64(s)
		if !ok {
			break
		}
		g.stream.writeBits64(x, n)
		return stateBzip2, nil
	}

	return nil, fmt.Errorf("bad stateBzip2 command: %q", line)
}

type bzip2BitStream struct {
	bits  uint64
	nBits uint32 // Always within [0, 7].
}

// writeBits64 writes the low n bits of b to z.
func (z *bzip2BitStream) writeBits64(b uint64, n uint32) {
	if n > 56 {
		panic("writeBits64: n is too large")
	}
	z.bits |= b << (64 - n - z.nBits)
	z.nBits += n
	for z.nBits >= 8 {
		out = append(out, uint8(z.bits>>56))
		z.bits <<= 8
		z.nBits -= 8
	}
}

func (z *bzip2BitStream) flush() {
	if z.nBits > 0 {
		out = append(out, uint8(z.bits>>56))
		z.bits = 0
		z.nBits = 0
	}
}

// ----

func init() {
	formats["deflate"] = stateDeflate
}

var deflateCodeOrder = [19]uint32{
	16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15,
}

var deflateHuffmanNames = [4]string{
	0: "Unused",
	1: "CodeLength",
	2: "Literal/Length",
	3: "Distance",
}

type deflateHuffmanTable map[uint32]string

var deflateGlobals struct {
	bncData []byte
	stream  deflateBitStream

	// Dynamic Huffman state. whichHuffman indexes the huffmans array. See also
	// deflateHuffmanNames.
	whichHuffman uint32
	huffmans     [4]deflateHuffmanTable

	// DHH (Dynamic Huffman, inside a Huffman table) state.
	prevLine   string
	etcetera   bool
	incomplete bool
}

func deflateGlobalsClearDynamicHuffmanState() {
	deflateGlobals.whichHuffman = 0
	deflateGlobals.huffmans = [4]deflateHuffmanTable{}
	deflateGlobals.prevLine = ""
	deflateGlobals.etcetera = false
	deflateGlobals.incomplete = false
}

func deflateGlobalsCountCodes() (numLCodes uint32, numDCodes uint32, numCLCodeLengths uint32, retErr error) {
	for k := range deflateGlobals.huffmans[2] {
		if numLCodes < (k + 1) {
			numLCodes = (k + 1)
		}
	}

	for k := range deflateGlobals.huffmans[3] {
		if numDCodes < (k + 1) {
			numDCodes = (k + 1)
		}
	}

	for k := range deflateGlobals.huffmans[1] {
		if (k < 0) || (18 < k) {
			return 0, 0, 0, fmt.Errorf("bad CodeLength: %d", k)
		}
	}
	for i := len(deflateCodeOrder) - 1; i >= 0; i-- {
		cl := deflateCodeOrder[i]
		if _, ok := deflateGlobals.huffmans[1][cl]; ok {
			numCLCodeLengths = uint32(i + 1)
			break
		}
	}
	if numCLCodeLengths < 4 {
		numCLCodeLengths = 4
	}

	return numLCodes, numDCodes, numCLCodeLengths, nil
}

func deflateGlobalsIsHuffmanCanonical() bool {
	g := &deflateGlobals

	// Gather the code+bitstring pairs. Deflate bitstrings cannot be longer
	// than 15 bits.
	type pair struct {
		k uint32
		v string
	}
	pairs := []pair{}
	for k, v := range g.huffmans[g.whichHuffman] {
		if len(v) > 15 {
			return false
		}
		pairs = append(pairs, pair{k, v})
	}
	if len(pairs) == 0 {
		return false
	}

	// Sort by bitstring-length and then code.
	sort.Slice(pairs, func(i, j int) bool {
		pi, pj := &pairs[i], &pairs[j]
		if len(pi.v) != len(pj.v) {
			return len(pi.v) < len(pj.v)
		}
		return pi.k < pj.k
	})

	// Set up a generator for the canonical bitstrings.
	buf := [15]byte{'0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0'}
	prevV := ""
	next := func() bool {
		if i := len(prevV); i > 0 {
			for {
				i--
				if i < 0 {
					return false
				}
				if buf[i] == '0' {
					buf[i] = '1'
					break
				}
				buf[i] = '0'
			}
		}
		return true
	}

	// Verify the bitstrings.
	for _, p := range pairs {
		if !next() {
			return false // Over-subscribed.
		} else if p.v != string(buf[:len(p.v)]) {
			return false // Non-canonical.
		}
		prevV = p.v
	}
	if next() != g.incomplete {
		return false // Under-subscribed.
	}
	return true
}

func deflateGlobalsWriteDynamicHuffmanTables() error {
	g := &deflateGlobals
	numLCodes, numDCodes, numCLCodeLengths, err := deflateGlobalsCountCodes()
	if err != nil {
		return err
	}
	if (numLCodes < 257) || (257+31 < numLCodes) {
		return fmt.Errorf("bad numLCodes: %d", numLCodes)
	}
	g.stream.writeBits(numLCodes-257, 5)
	if (numDCodes < 1) || (1+31 < numDCodes) {
		return fmt.Errorf("bad numDCodes: %d", numDCodes)
	}
	g.stream.writeBits(numDCodes-1, 5)
	if (numCLCodeLengths < 4) || (4+15 < numCLCodeLengths) {
		return fmt.Errorf("bad numCLCodeLengths: %d", numCLCodeLengths)
	}
	g.stream.writeBits(numCLCodeLengths-4, 4)

	// Write the Huffman table for CodeLength.
	{
		for i := uint32(0); i < numCLCodeLengths; i++ {
			n := len(g.huffmans[1][deflateCodeOrder[i]])
			g.stream.writeBits(uint32(n), 3)
		}
		for i := numCLCodeLengths; i < uint32(len(deflateCodeOrder)); i++ {
			n := len(g.huffmans[1][deflateCodeOrder[i]])
			if n > 0 {
				return fmt.Errorf("short numCLCodeLengths: %d", numCLCodeLengths)
			}
		}
	}

	// Write the Huffman tables for Literal/Length and Distance.
	{
		numZeroes := uint32(0)
		for i := uint32(0); i < numLCodes+numDCodes; i++ {
			codeLen := uint32(0)
			if i < numLCodes {
				codeLen = uint32(len(g.huffmans[2][i]))
			} else {
				codeLen = uint32(len(g.huffmans[3][i-numLCodes]))
			}
			if codeLen == 0 {
				numZeroes++
				continue
			}

			if err := deflateGlobalsWriteDynamicHuffmanZeroes(numZeroes); err != nil {
				return err
			}
			numZeroes = 0

			codeLenCode := g.huffmans[1][codeLen]
			if codeLenCode == "" {
				return fmt.Errorf("no code for code-length %d", codeLen)
			}
			deflateGlobalsWriteDynamicHuffmanBits(g.huffmans[1][codeLen])
		}
		if err := deflateGlobalsWriteDynamicHuffmanZeroes(numZeroes); err != nil {
			return err
		}
	}

	return nil
}

func deflateGlobalsWriteDynamicHuffmanZeroes(numZeroes uint32) error {
	g := &deflateGlobals
	if numZeroes == 0 {
		return nil
	}

	if s := g.huffmans[1][18]; s != "" {
		for numZeroes >= 11 {
			extra := numZeroes - 11
			if extra > 127 {
				extra = 127
			}
			deflateGlobalsWriteDynamicHuffmanBits(s)
			g.stream.writeBits(extra, 7)
			numZeroes -= 11 + extra
		}
	}

	if s := g.huffmans[1][17]; s != "" {
		for numZeroes >= 3 {
			extra := numZeroes - 3
			if extra > 7 {
				extra = 7
			}
			deflateGlobalsWriteDynamicHuffmanBits(s)
			g.stream.writeBits(extra, 3)
			numZeroes -= 3 + extra
		}
	}

	if s := g.huffmans[1][0]; s != "" {
		for ; numZeroes > 0; numZeroes-- {
			deflateGlobalsWriteDynamicHuffmanBits(s)
		}
	}

	if numZeroes > 0 {
		return fmt.Errorf("could not write a run of zero-valued code lengths")
	}
	return nil
}

func deflateGlobalsWriteDynamicHuffmanBits(s string) {
	g := &deflateGlobals
	for i := 0; i < len(s); i++ {
		g.stream.writeBits(uint32(s[i]&1), 1)
	}
}

func parseDeflateWhichHuffman(s string) (num uint32, remaining string, ok bool) {
	if i := strings.IndexByte(s, ' '); i >= 0 {
		s, remaining = s[:i], s[i+1:]
		for len(remaining) > 0 && remaining[0] == ' ' {
			remaining = remaining[1:]
		}
	}

	switch s {
	case "CodeLength":
		return 1, remaining, true
	case "Literal/Length":
		return 2, remaining, true
	case "Distance":
		return 3, remaining, true
	}
	return 0, "", false
}

func stateDeflate(line string) (stateFunc, error) {
	g := &deflateGlobals
	const (
		cmdB   = "bytes "
		cmdBDH = "blockDynamicHuffman "
		cmdBFH = "blockFixedHuffman "
		cmdBNC = "blockNoCompression "
	)
	bits := uint32(0)
	s := ""

	retState := stateFunc(nil)
	switch {
	case line == "":
		g.stream.flush()
		return stateDeflate, nil

	case strings.HasPrefix(line, cmdB):
		s := line[len(cmdB):]
		for s != "" {
			x, ok := uint32(0), false
			x, s, ok = parseHex32(s)
			if !ok {
				return nil, fmt.Errorf("bad stateDeflate command: %q", line)
			}
			out = append(out, uint8(x))
		}
		return stateDeflate, nil

	case strings.HasPrefix(line, cmdBNC):
		s = line[len(cmdBNC):]
		retState = stateDeflateNoCompression
		bits |= 0 << 1
	case strings.HasPrefix(line, cmdBFH):
		s = line[len(cmdBFH):]
		retState = stateDeflateFixedHuffman
		bits |= 1 << 1
	case strings.HasPrefix(line, cmdBDH):
		s = line[len(cmdBDH):]
		retState = stateDeflateDynamicHuffman
		bits |= 2 << 1
	default:
		return nil, fmt.Errorf("bad stateDeflate command: %q", line)
	}

	switch s {
	case "(final) {":
		bits |= 1
	case "(nonFinal) {":
		// No-op.
	default:
		return nil, fmt.Errorf("bad stateDeflate command: %q", line)
	}

	g.stream.writeBits(bits, 3)
	return retState, nil
}

func stateDeflateNoCompression(line string) (stateFunc, error) {
	g := &deflateGlobals
	if line == "}" {
		if len(g.bncData) > 0xFFFF {
			return nil, fmt.Errorf("bncData is too long")
		}
		n := uint32(len(g.bncData))
		g.stream.flush()
		g.stream.writeBits(n, 16)
		g.stream.writeBits(0xFFFF-n, 16)
		g.stream.writeBytes(g.bncData)
		g.bncData = g.bncData[:0]
		return stateDeflate, nil
	}

	if lit, ok := deflateParseLiteral(line); ok {
		g.bncData = append(g.bncData, lit...)
		return stateDeflateNoCompression, nil
	}

	return nil, fmt.Errorf("bad blockNoCompression command: %q", line)
}

func stateDeflateFixedHuffman(line string) (stateFunc, error) {
	g := &deflateGlobals
	if line == "}" {
		return stateDeflate, nil
	}

	if line == "endOfBlock" {
		g.stream.writeBits(0, 7)
		return stateDeflateFixedHuffman, nil
	}

	if lit, ok := deflateParseLiteral(line); ok {
		for i := 0; i < len(lit); i++ {
			g.stream.writeFixedHuffmanLCode(uint32(lit[i]))
		}
		return stateDeflateFixedHuffman, nil
	}

	if line == "len 3 distCode 31" {
		lCode, lExtra, lNExtra := deflateEncodeLength(3)
		g.stream.writeFixedHuffmanLCode(lCode)
		g.stream.writeBits(lExtra, lNExtra)
		dCode, dExtra, dNExtra := uint32(31), uint32(0), uint32(0)
		g.stream.writeBits(reverse(dCode, 5), 5)
		g.stream.writeBits(dExtra, dNExtra)
		return stateDeflateFixedHuffman, nil
	}

	if l, d, ok := deflateParseLenDist(line); ok {
		lCode, lExtra, lNExtra := deflateEncodeLength(l)
		g.stream.writeFixedHuffmanLCode(lCode)
		g.stream.writeBits(lExtra, lNExtra)
		dCode, dExtra, dNExtra := deflateEncodeDistance(d)
		g.stream.writeBits(reverse(dCode, 5), 5)
		g.stream.writeBits(dExtra, dNExtra)
		return stateDeflateFixedHuffman, nil
	}

	return nil, fmt.Errorf("bad stateDeflateFixedHuffman command: %q", line)
}

func stateDeflateDynamicHuffman(line string) (stateFunc, error) {
	g := &deflateGlobals
	const (
		cmdH = "huffman "
	)
	switch {
	case line == "}":
		deflateGlobalsClearDynamicHuffmanState()
		return stateDeflate, nil

	case strings.HasPrefix(line, cmdH):
		s := line[len(cmdH):]
		n, s, ok := parseDeflateWhichHuffman(s)
		if !ok {
			break
		}
		g.whichHuffman = n
		return stateDeflateDynamicHuffmanHuffman, nil
	}

	if lit, ok := deflateParseLiteral(line); ok {
		for i := 0; i < len(lit); i++ {
			s := g.huffmans[2][uint32(lit[i])]
			if s == "" {
				return nil, fmt.Errorf("no code for literal %q (%d)", lit[i:i+1], lit[i])
			}
			deflateGlobalsWriteDynamicHuffmanBits(s)
		}
		return stateDeflateDynamicHuffman, nil

	} else if l, d, ok := deflateParseLenDist(line); ok {
		if (l > 10) || (d > 4) {
			return nil, fmt.Errorf("TODO: support len/dist ExtraBits > 0 for dynamic Huffman blocks")
		}
		// len 3 is code 257, len 4 is code 258, etc. All with no ExtraBits.
		if s := g.huffmans[2][l+254]; s != "" {
			deflateGlobalsWriteDynamicHuffmanBits(s)
		} else {
			return nil, fmt.Errorf("no code for literal/length symbol %d", l+254)
		}
		// dist 1 is code 0, dist 2 is code 1, etc. All with no ExtraBits.
		if s := g.huffmans[3][d-1]; s != "" {
			deflateGlobalsWriteDynamicHuffmanBits(s)
		} else {
			return nil, fmt.Errorf("no code for distance symbol %d", d-1)
		}
		return stateDeflateDynamicHuffman, nil

	} else if line == "endOfBlock" {
		s := g.huffmans[2][256]
		if s == "" {
			return nil, fmt.Errorf("no code for end-of-block (256)")
		}
		deflateGlobalsWriteDynamicHuffmanBits(s)
		return stateDeflateDynamicHuffman, nil
	}

	return nil, fmt.Errorf("bad stateDeflateDynamicHuffman command: %q", line)
}

func stateDeflateDynamicHuffmanHuffman(line string) (stateFunc, error) {
	g := &deflateGlobals
outer:
	switch {
	case line == "}":
		if !deflateGlobalsIsHuffmanCanonical() {
			return nil, fmt.Errorf(`"huffman %s" is non-canonical`, deflateHuffmanNames[g.whichHuffman])
		}
		g.whichHuffman = 0
		g.prevLine = ""
		g.etcetera = false
		g.incomplete = false

		// If we have all three Huffman tables, write them.
		for i := 1; ; i++ {
			if i == 4 {
				if err := deflateGlobalsWriteDynamicHuffmanTables(); err != nil {
					return nil, err
				}
				break
			}
			if g.huffmans[i] == nil {
				break
			}
		}

		return stateDeflateDynamicHuffman, nil

	case line == "etcetera":
		g.etcetera = true
		return stateDeflateDynamicHuffmanHuffman, nil

	case line == "incomplete":
		g.incomplete = true
		return stateDeflateDynamicHuffmanHuffman, nil

	default:
		s := line
		n, s, ok := parseNum32(s)
		if !ok || s == "" {
			break
		}
		for i := 0; i < len(s); i++ {
			if c := s[i]; c != '0' && c != '1' {
				break outer
			}
		}
		if (len(s) < 1) || (15 < len(s)) {
			return nil, fmt.Errorf("%q code length, %d, is out of range", s, len(s))
		}

		if g.etcetera {
			g.etcetera = false
			n0, s0, ok := parseNum32(g.prevLine)
			if !ok {
				return nil, fmt.Errorf("bad etcetera command")
			}
			if err := stateDeflateDHHEtcetera(n0, s0, n, s); err != nil {
				return nil, err
			}
		}

		if g.huffmans[g.whichHuffman] == nil {
			g.huffmans[g.whichHuffman] = deflateHuffmanTable{}
		}
		g.huffmans[g.whichHuffman][n] = s
		g.prevLine = line
		return stateDeflateDynamicHuffmanHuffman, nil
	}

	return nil, fmt.Errorf("bad stateDeflateDynamicHuffmanHuffman command: %q", line)
}

// stateDeflateDHHEtcetera expands the "etcetera" line in:
//
// 0  0000
// 1  0001
// etcetera
// 7  0111
//
// to produce the implicit lines:
//
// 0  0000
// 1  0001
// 2  0010
// 3  0011
// 4  0100
// 5  0101
// 6  0110
// 7  0111
func stateDeflateDHHEtcetera(n0 uint32, s0 string, n1 uint32, s1 string) error {
	b := []byte(s0)
	if !incrementBitstring(b) {
		return fmt.Errorf("etcetera: could not increment bitstring")
	}
	for n := n0 + 1; n < n1; n++ {
		line := fmt.Sprintf("%d %s", n, b)
		if _, err := stateDeflateDynamicHuffmanHuffman(line); err != nil {
			return err
		}
		if !incrementBitstring(b) {
			return fmt.Errorf("etcetera: could not increment bitstring")
		}
	}
	if string(b) != s1 {
		return fmt.Errorf("etcetera: final bitstring: got %q, want %q", b, s1)
	}
	return nil
}

func incrementBitstring(b []byte) (ok bool) {
	for i := len(b) - 1; i >= 0; i-- {
		switch b[i] {
		case '0':
			b[i] = '1'
			return true
		case '1':
			b[i] = '0'
		default:
			return false
		}
	}
	return false
}

type deflateBitStream struct {
	bits  uint32
	nBits uint32 // Always within [0, 7].
}

// writeBits writes the low n bits of b to z.
func (z *deflateBitStream) writeBits(b uint32, n uint32) {
	if n > 24 {
		panic("writeBits: n is too large")
	}
	z.bits |= b << z.nBits
	z.nBits += n
	for z.nBits >= 8 {
		out = append(out, uint8(z.bits))
		z.bits >>= 8
		z.nBits -= 8
	}
}

func (z *deflateBitStream) writeBytes(b []byte) {
	z.flush()
	out = append(out, b...)
}

func (z *deflateBitStream) writeFixedHuffmanLCode(lCode uint32) {
	switch {
	case lCode < 144: // 0b._0011_0000 through 0b._1011_1111
		lCode += 0x030
		z.writeBits(reverse(lCode, 8), 8)
	case lCode < 256: // 0b1_1001_0000 through 0b1_1111_1111
		lCode += 0x190 - 144
		z.writeBits(reverse(lCode, 9), 9)
	case lCode < 280: // 0b._.000_0000 through 0b._.001_0111
		lCode -= 256 - 0x000
		z.writeBits(reverse(lCode, 7), 7)
	default: //          0b._1100_0000 through 0b._1100_0111
		lCode -= 280 - 0x0C0
		z.writeBits(reverse(lCode, 8), 8)
	}
}

func (z *deflateBitStream) flush() {
	if z.nBits > 0 {
		out = append(out, uint8(z.bits))
		z.bits = 0
		z.nBits = 0
	}
}

func deflateEncodeLength(l uint32) (code uint32, extra uint32, nExtra uint32) {
	switch {
	case l < 3:
		// No-op.
	case l < 11:
		l -= 3
		return (l >> 0) + 257, l & 0x0000, 0
	case l < 19:
		l -= 11
		return (l >> 1) + 265, l & 0x0001, 1
	case l < 35:
		l -= 19
		return (l >> 2) + 269, l & 0x0003, 2
	case l < 67:
		l -= 35
		return (l >> 3) + 273, l & 0x0007, 3
	case l < 131:
		l -= 67
		return (l >> 4) + 277, l & 0x000F, 4
	case l < 258:
		l -= 131
		return (l >> 5) + 281, l & 0x001F, 5
	case l == 258:
		return 285, 0, 0
	}
	panic(fmt.Sprintf("deflateEncodeLength: l=%d", l))
}

func deflateEncodeDistance(d uint32) (code uint32, extra uint32, nExtra uint32) {
	switch {
	case d < 1:
		// No-op.
	case d < 5:
		d -= 1
		return (d >> 0) + 0, d & 0x0000, 0
	case d < 9:
		d -= 5
		return (d >> 1) + 4, d & 0x0001, 1
	case d < 17:
		d -= 9
		return (d >> 2) + 6, d & 0x0003, 2
	case d < 33:
		d -= 17
		return (d >> 3) + 8, d & 0x0007, 3
	case d < 65:
		d -= 33
		return (d >> 4) + 10, d & 0x000F, 4
	case d < 129:
		d -= 65
		return (d >> 5) + 12, d & 0x001F, 5
	case d < 257:
		d -= 129
		return (d >> 6) + 14, d & 0x003F, 6
	case d < 513:
		d -= 257
		return (d >> 7) + 16, d & 0x007F, 7
	case d < 1025:
		d -= 513
		return (d >> 8) + 18, d & 0x00FF, 8
	case d < 2049:
		d -= 1025
		return (d >> 9) + 20, d & 0x01FF, 9
	case d < 4097:
		d -= 2049
		return (d >> 10) + 22, d & 0x03FF, 10
	case d < 8193:
		d -= 4097
		return (d >> 11) + 24, d & 0x07FF, 11
	case d < 16385:
		d -= 8193
		return (d >> 12) + 26, d & 0x0FFF, 12
	case d < 32769:
		d -= 16385
		return (d >> 13) + 28, d & 0x1FFF, 13
	}
	panic(fmt.Sprintf("deflateEncodeDistance: d=%d", d))
}

func deflateParseLiteral(s string) (lit string, ok bool) {
	// TODO: support "\xAB" escape codes in the script?
	const (
		prefix = `literal "`
		suffix = `"`
	)
	if strings.HasPrefix(s, prefix) {
		s = s[len(prefix):]
		if strings.HasSuffix(s, suffix) {
			return s[:len(s)-len(suffix)], true
		}
	}
	return "", false
}

func deflateParseLenDist(line string) (l uint32, d uint32, ok bool) {
	const (
		lStr = "len "
		dStr = "dist "
	)
	s := line
	if strings.HasPrefix(s, lStr) {
		s = s[len(lStr):]
		if l, s, ok := parseNum32(s); ok && 3 <= l && l <= 258 {
			if strings.HasPrefix(s, dStr) {
				s = s[len(dStr):]
				if d, s, ok := parseNum32(s); ok && 1 <= d && d <= 32768 && s == "" {
					return l, d, true
				}
			}
		}
	}
	return 0, 0, false
}

// ----

func init() {
	formats["gif"] = stateGif
}

var gifGlobals struct {
	imageWidth                uint32
	imageHeight               uint32
	imageBackgroundColorIndex uint32

	frameInterlaced bool
	frameLeft       uint32
	frameTop        uint32
	frameWidth      uint32
	frameHeight     uint32

	globalPalette [][4]uint8
	localPalette  [][4]uint8
}

func stateGif(line string) (stateFunc, error) {
	const (
		cmdB  = "bytes "
		cmdGC = "graphicControl "
		cmdL  = "lzw "
		cmdLC = "loopCount "
	)
outer:
	switch {
	case line == "":
		return stateGif, nil

	case line == "frame {":
		gifGlobals.frameInterlaced = false
		gifGlobals.frameLeft = 0
		gifGlobals.frameTop = 0
		gifGlobals.frameWidth = 0
		gifGlobals.frameHeight = 0
		gifGlobals.localPalette = nil
		out = append(out, 0x2C)
		return stateGifFrame, nil

	case line == "header":
		out = append(out, "GIF89a"...)
		return stateGif, nil

	case line == "image {":
		return stateGifImage, nil

	case line == "trailer":
		out = append(out, 0x3B)
		return stateGif, nil

	case strings.HasPrefix(line, cmdB):
		s := line[len(cmdB):]
		for s != "" {
			x, ok := uint32(0), false
			x, s, ok = parseHex32(s)
			if !ok {
				break outer
			}
			out = append(out, uint8(x))
		}
		return stateGif, nil

	case strings.HasPrefix(line, cmdGC):
		s := line[len(cmdGC):]

		flags := uint8(0)
		duration := uint32(0)
		transparentIndex := uint8(0)
		for s != "" {
			term := ""
			if i := strings.IndexByte(s, ' '); i >= 0 {
				term, s = s[:i], s[i+1:]
			} else {
				term, s = s, ""
			}

			const (
				ms    = "ms"
				trans = "transparentIndex="
			)
			switch {
			case term == "animationDisposalNone":
				flags |= 0x00
			case term == "animationDisposalRestoreBackground":
				flags |= 0x08
			case term == "animationDisposalRestorePrevious":
				flags |= 0x0C
			case strings.HasPrefix(term, trans):
				num, err := strconv.ParseUint(term[len(trans):], 0, 8)
				if err != nil {
					break outer
				}
				flags |= 0x01
				transparentIndex = uint8(num)
			case strings.HasSuffix(term, ms):
				num, remaining, ok := parseNum32(term[:len(term)-len(ms)])
				if !ok || remaining != "" {
					break outer
				}
				duration = num / 10 // GIF's unit of time is 10ms.
			default:
				break outer
			}
		}

		out = append(out,
			0x21, 0xF9, 0x04,
			flags,
			uint8(duration>>0),
			uint8(duration>>8),
			transparentIndex,
			0x00,
		)
		return stateGif, nil

	case strings.HasPrefix(line, cmdL):
		s := line[len(cmdL):]
		litWidth, s, ok := parseNum32(s)
		if !ok || litWidth < 2 || 8 < litWidth {
			break
		}
		out = append(out, uint8(litWidth))

		uncompressed := []byte(nil)
		for s != "" {
			x := uint32(0)
			x, s, ok = parseHex32(s)
			if !ok {
				break outer
			}
			uncompressed = append(uncompressed, uint8(x))
		}

		buf := bytes.NewBuffer(nil)
		w := lzw.NewWriter(buf, lzw.LSB, int(litWidth))
		if _, err := w.Write(uncompressed); err != nil {
			return nil, err
		}
		if err := w.Close(); err != nil {
			return nil, err
		}
		compressed := buf.Bytes()

		for len(compressed) > 0 {
			if len(compressed) <= 0xFF {
				out = append(out, uint8(len(compressed)))
				out = append(out, compressed...)
				compressed = nil
			} else {
				out = append(out, 0xFF)
				out = append(out, compressed[:0xFF]...)
				compressed = compressed[0xFF:]
			}
		}
		out = append(out, 0x00)
		return stateGif, nil

	case strings.HasPrefix(line, cmdLC):
		s := line[len(cmdLC):]
		loopCount, _, ok := parseNum32(s)
		if !ok || 0xFFFF < loopCount {
			break
		}
		out = append(out,
			0x21, // Extension Introducer.
			0xFF, // Application Extension Label.
			0x0B, // Block Size.
		)
		out = append(out, "NETSCAPE2.0"...)
		out = append(out,
			0x03, // Block Size.
			0x01, // Magic Number.
			byte(loopCount),
			byte(loopCount>>8),
			0x00, // Block Terminator.
		)
		return stateGif, nil
	}

	return nil, fmt.Errorf("bad stateGif command: %q", line)
}

func stateGifImage(line string) (stateFunc, error) {
	g := &gifGlobals
	if line == "}" {
		out = appendU16LE(out, uint16(g.imageWidth))
		out = appendU16LE(out, uint16(g.imageHeight))
		if g.globalPalette == nil {
			out = append(out, 0x00)
		} else if n := log2(uint32(len(g.globalPalette))); n < 2 || 8 < n {
			return nil, fmt.Errorf("bad len(g.globalPalette): %d", len(g.globalPalette))
		} else {
			out = append(out, 0x80|uint8(n-1))
		}
		out = append(out, uint8(g.imageBackgroundColorIndex))
		out = append(out, 0x00)
		for _, x := range g.globalPalette {
			out = append(out, x[0], x[1], x[2])
		}
		return stateGif, nil
	}

	const (
		cmdBCI = "backgroundColorIndex "
		cmdIWH = "imageWidthHeight "
		cmdP   = "palette {"
	)
	switch {
	case strings.HasPrefix(line, cmdBCI):
		s := line[len(cmdBCI):]
		if i, _, ok := parseNum32(s); ok {
			g.imageBackgroundColorIndex = i
		}
		return stateGifImage, nil

	case strings.HasPrefix(line, cmdIWH):
		s := line[len(cmdIWH):]
		if w, s, ok := parseNum32(s); ok {
			if h, _, ok := parseNum32(s); ok {
				g.imageWidth = w
				g.imageHeight = h
				return stateGifImage, nil
			}
		}

	case strings.HasPrefix(line, cmdP):
		return stateGifImagePalette, nil
	}

	return nil, fmt.Errorf("bad stateGifImage command: %q", line)
}

func stateGifFrame(line string) (stateFunc, error) {
	g := &gifGlobals
	if line == "}" {
		out = appendU16LE(out, uint16(g.frameLeft))
		out = appendU16LE(out, uint16(g.frameTop))
		out = appendU16LE(out, uint16(g.frameWidth))
		out = appendU16LE(out, uint16(g.frameHeight))
		flags := byte(0x00)
		if g.frameInterlaced {
			flags |= 0x40
		}
		if g.localPalette == nil {
			out = append(out, flags)
		} else if n := log2(uint32(len(g.localPalette))); n < 2 || 8 < n {
			return nil, fmt.Errorf("bad len(g.localPalette): %d", len(g.localPalette))
		} else {
			out = append(out, flags|0x80|uint8(n-1))
		}
		for _, x := range g.localPalette {
			out = append(out, x[0], x[1], x[2])
		}
		return stateGif, nil
	}

	const (
		cmdFLTWH = "frameLeftTopWidthHeight "
		cmdP     = "palette {"
	)
	switch {
	case line == "interlaced":
		g.frameInterlaced = true
		return stateGifFrame, nil

	case strings.HasPrefix(line, cmdFLTWH):
		s := line[len(cmdFLTWH):]
		if l, s, ok := parseNum32(s); ok {
			if t, s, ok := parseNum32(s); ok {
				if w, s, ok := parseNum32(s); ok {
					if h, _, ok := parseNum32(s); ok {
						g.frameLeft = l
						g.frameTop = t
						g.frameWidth = w
						g.frameHeight = h
						return stateGifFrame, nil
					}
				}
			}
		}

	case strings.HasPrefix(line, cmdP):
		return stateGifFramePalette, nil
	}

	return nil, fmt.Errorf("bad stateGifFrame command: %q", line)
}

func stateGifImagePalette(line string) (stateFunc, error) {
	g := &gifGlobals
	if line == "}" {
		return stateGifImage, nil
	}

	s := line
	if rgb0, s, ok := parseHex32(s); ok {
		if rgb1, s, ok := parseHex32(s); ok {
			if rgb2, _, ok := parseHex32(s); ok {
				g.globalPalette = append(g.globalPalette,
					[4]uint8{uint8(rgb0), uint8(rgb1), uint8(rgb2), 0xFF})
				return stateGifImagePalette, nil
			}
		}
	}

	return nil, fmt.Errorf("bad stateGifImagePalette command: %q", line)
}

func stateGifFramePalette(line string) (stateFunc, error) {
	g := &gifGlobals
	if line == "}" {
		return stateGifFrame, nil
	}

	s := line
	if rgb0, s, ok := parseHex32(s); ok {
		if rgb1, s, ok := parseHex32(s); ok {
			if rgb2, _, ok := parseHex32(s); ok {
				g.localPalette = append(g.localPalette,
					[4]uint8{uint8(rgb0), uint8(rgb1), uint8(rgb2), 0xFF})
				return stateGifFramePalette, nil
			}
		}
	}

	return nil, fmt.Errorf("bad stateGifFramePalette command: %q", line)
}

// ----

func init() {
	formats["png"] = statePng
}

var pngGlobals struct {
	scratch [4]byte

	animSeqNum uint32
	chunkData  bytes.Buffer
	chunkType  string
	zlibWriter *zlib.Writer
}

func statePng(line string) (stateFunc, error) {
	g := &pngGlobals
	switch {
	case line == "":
		return statePng, nil

	case line == "magic":
		out = append(out, "\x89PNG\x0D\x0A\x1A\x0A"...)
		return statePng, nil

	case (len(line) == 6) && (line[4] == ' ') && (line[5] == '{'):
		g.chunkData.Reset()
		g.chunkType = line[:4]
		return statePngChunk, nil
	}

	return nil, fmt.Errorf("bad statePng command: %q", line)
}

func statePngChunk(line string) (stateFunc, error) {
	g := &pngGlobals
	if line == "}" {
		if n := g.chunkData.Len(); n > 0xFFFF_FFFF {
			return nil, fmt.Errorf("chunkData is too long")
		}
		out = appendU32BE(out, uint32(g.chunkData.Len()))
		n := len(out)
		out = append(out, g.chunkType...)
		out = append(out, g.chunkData.Bytes()...)
		out = appendU32BE(out, crc32.ChecksumIEEE(out[n:]))
		return statePng, nil
	}

	switch {
	case line == "animSeqNum++":
		g.chunkData.Write(appendU32BE(g.scratch[:0], g.animSeqNum))
		g.animSeqNum++
		return statePngChunk, nil

	case line == "raw {":
		return statePngRaw, nil

	case line == "zlib {":
		g.zlibWriter = zlib.NewWriter(&g.chunkData)
		return statePngZlib, nil
	}

	return nil, fmt.Errorf("bad statePngChunk command: %q", line)
}

func statePngRaw(line string) (stateFunc, error) {
	g := &pngGlobals
	if line == "}" {
		return statePngChunk, nil
	}

	for s := line; s != ""; {
		if x, remaining, ok := parseHex32(s); ok {
			g.chunkData.WriteByte(byte(x))
			s = remaining
		} else {
			return nil, fmt.Errorf("bad statePngRaw command: %q", line)
		}
	}
	return statePngRaw, nil
}

func statePngZlib(line string) (stateFunc, error) {
	g := &pngGlobals
	if line == "}" {
		if err := g.zlibWriter.Close(); err != nil {
			return nil, fmt.Errorf("statePngZlib: %w", err)
		}
		return statePngChunk, nil
	}

	for s := line; s != ""; {
		if x, remaining, ok := parseHex32(s); ok {
			g.scratch[0] = byte(x)
			g.zlibWriter.Write(g.scratch[:1])
			s = remaining
		} else {
			return nil, fmt.Errorf("bad statePngZlib command: %q", line)
		}
	}
	return statePngZlib, nil
}
