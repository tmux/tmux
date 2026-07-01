// Copyright 2021 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ----------------

//go:build ignore
// +build ignore

package main

import (
	"bufio"
	"bytes"
	"flag"
	"fmt"
	"io"
	"net/http"
	"os"
	"sort"
	"strings"

	"github.com/google/wuffs/lib/armneonintrinsics"
)

var (
	// The input can be "http://etc", "https://etc" or a local file path
	// (without the "file://" protocol prefix). A local file can allow faster
	// iteration when debugging this generator.
	//
	// For example, download arm_neon.h from raw.githubusercontent.com and
	// replace the middle argument with "/the/path/to/arm_neon.h".
	inputFlag = flag.String("input",
		"https://raw.githubusercontent.com/gcc-mirror/gcc/master/gcc/config/aarch64/arm_neon.h",
		"source file")
)

const debug = false

// hashTableSize should be a power of two.
const hashTableSize = 8192

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	flag.Parse()
	if err := build(); err != nil {
		return err
	}
	sort.Slice(allFuncs, func(i, j int) bool {
		return allFuncs[i][0].name < allFuncs[j][0].name
	})

	w := &bytes.Buffer{}

	fmt.Fprintln(w, header)

	strtabBytes := []byte(nil)
	sort.Strings(strtabList)
	for _, s := range strtabList {
		strtabBytes = append(strtabBytes, s...)
	}
	strtab := string(strtabBytes)

	fmt.Fprintf(w, "const stringTable = \"\" +\n")
	for s := strtab; ; {
		if len(s) <= 64 {
			fmt.Fprintf(w, "\t%q\n", s)
			break
		}
		fmt.Fprintf(w, "\t%q +\n", s[:64])
		s = s[64:]
	}

	fmt.Fprintln(w)

	fmt.Fprintf(w, "var elementTable = [...]element{\n\t{},\n")
	etIndex := uint32(1)
	for _, f := range allFuncs {
		for j, e := range f {
			o := strings.Index(strtab, e.name)
			n := len(e.name)
			if (o < 0) || (0xFFFF < o) || (0x7F < n) {
				return fmt.Errorf(`could not index name %q`, e.name)
			}
			if j == (len(f) - 1) {
				n |= 0x80
			}
			fmt.Fprintf(w, "\t{0x%04X, 0x%02X, 0x%02X},", o, n, uint8(e.typ))
			if j == 0 {
				hash, htIndex := insert(e.name, etIndex)
				if debug {
					fmt.Fprintf(w, "  // 0x%04X 0x%04X 0x%04X %s", etIndex, hash, htIndex, e.name)
				}
			}
			fmt.Fprintln(w)
			etIndex++
		}
	}
	fmt.Fprintf(w, "}\n")

	fmt.Fprintln(w)

	fmt.Fprintf(w, "var hashTable = [%d]uint16{\n", hashTableSize)
	for i, h := range hashTable {
		if h.etIndex != 0 {
			fmt.Fprintf(w, "\t0x%04X: 0x%04X,", i, h.etIndex)
			if debug {
				fmt.Fprintf(w, "  // %s", h.name)
			}
			fmt.Fprintln(w)
		}
	}
	fmt.Fprintf(w, "}\n")

	fmt.Fprintln(w)

	fmt.Fprintf(w, "// sizeof(stringTable)  = %5d × 1 = %5d\n",
		len(strtab), len(strtab))
	fmt.Fprintf(w, "// sizeof(elementTable) = %5d × 4 = %5d\n",
		etIndex, etIndex*4)
	fmt.Fprintf(w, "// sizeof(hashTable)    = %5d × 2 = %5d\n",
		hashTableSize, hashTableSize*2)

	fmt.Fprintln(w)

	fmt.Fprintf(w, "// hashTable statistics:\n")
	fmt.Fprintf(w, "// %4d size\n", hashTableSize)
	fmt.Fprintf(w, "// %4d entries\n", htStats.nEntries)
	fmt.Fprintf(w, "// ----\n")
	for i, x := range htStats.nProbes {
		plus := " "
		if i == (len(htStats.nProbes) - 1) {
			plus = "+"
		}
		fmt.Fprintf(w, "// %4d entries have %2d%s probes\n", x, i, plus)
	}

	return os.WriteFile("data.go", w.Bytes(), 0644)
}

func build() error {
	in := io.ReadCloser(nil)
	if strings.HasPrefix(*inputFlag, "http://") ||
		strings.HasPrefix(*inputFlag, "https://") {
		r, err := http.Get(*inputFlag)
		if err != nil {
			return err
		}
		in = r.Body
	} else {
		r, err := os.Open(*inputFlag)
		if err != nil {
			return err
		}
		in = r
	}
	defer in.Close()

	state := 0
	currFunc := []element(nil)

	s := bufio.NewScanner(in)
	for s.Scan() {
		line := s.Bytes()
		if len(line) == 0 {
			continue
		}
		b := line

		switch state {
		case 0:
			if !bytes.HasPrefix(b, bUUExtension) {
				continue
			} else if i := bytes.IndexByte(b, '\\'); i >= 0 {
				continue
			} else if i := bytes.Index(b, bUUInline); i < 0 {
				continue
			} else {
				b = bytes.TrimSpace(b[i+len(bUUInline):])
			}
			returnTyp := armneonintrinsics.ParseType(string(b))
			if returnTyp == armneonintrinsics.TypeInvalid {
				return fmt.Errorf(`could not parse %q`, line)
			}
			currFunc = funk{{"", returnTyp}}
			state = 1

		case 1:
			if !bytes.HasPrefix(b, bUUAttribute) {
				return fmt.Errorf(`expected "__attribute", got %q`, line)
			}
			state = 2

		case 2:
			i := bytes.IndexByte(b, '(')
			if i < 0 {
				return fmt.Errorf(`could not parse %q`, line)
			}
			funcName := string(bytes.TrimSpace(b[:i]))
			b = b[i+1:]
			addToStrtab(funcName)
			currFunc[0].name = funcName
			state = 3
			fallthrough

		case 3:
			b = bytes.TrimSpace(b)
			for len(b) > 0 {
				if bytes.HasPrefix(b, bUUConst) {
					b = b[2:]
				}
				i := bytesIndexAny(b, bUnderUnder, bData, bImm6, bKey, bVal)
				if i < 0 {
					return fmt.Errorf(`could not parse %q`, line)
				}
				argTyp := parseTyp(string(bytes.TrimSpace(b[:i])))
				if argTyp == armneonintrinsics.TypeInvalid {
					return fmt.Errorf(`could not parse %q`, line)
				}
				b = b[i:]

				argName := readIdent(b)
				if argName == "" {
					return fmt.Errorf(`could not parse %q`, line)
				}
				b = b[len(argName):]
				addToStrtab(argName)
				currFunc = append(currFunc, element{argName, argTyp})

				if len(b) > 0 {
					if b[0] == ',' {
						b = bytes.TrimSpace(b[1:])
						continue
					} else if b[0] == ')' {
						allFuncs = append(allFuncs, currFunc)
						currFunc = nil
						state = 0
						break
					}
				}
				return fmt.Errorf(`could not parse %q`, line)

			}
		}
	}
	return s.Err()
}

var (
	bUnderUnder = []byte("__")

	bUUAttribute = []byte("__attribute")
	bUUConst     = []byte("__const")
	bUUExtension = []byte("__extension")
	bUUInline    = []byte("__inline")

	// Most arm_neon.h variable names start with "__". Some are idiosyncratic.
	bData = []byte("data")
	bImm6 = []byte("imm6")
	bKey  = []byte("key")
	bVal  = []byte("val")
)

var (
	allFuncs = []funk(nil)

	strtabList = []string(nil)
	strtabMap  = map[string]bool{}

	hashTable = [hashTableSize]htElement{}

	htStats struct {
		nEntries uint32
		nProbes  [16]uint32
	}
)

func addToStrtab(s string) {
	if !strtabMap[s] {
		strtabMap[s] = true
		strtabList = append(strtabList, s)
	}
}

type funk []element

func (f funk) dup() funk {
	return append(funk(nil), f...)
}

type element struct {
	name string
	typ  armneonintrinsics.Type
}

type htElement struct {
	etIndex uint32
	name    string
}

func bytesIndexAny(b []byte, needles ...[]byte) int {
	for _, needle := range needles {
		if i := bytes.Index(b, needle); i >= 0 {
			return i
		}
	}
	return -1
}

func insert(name string, etIndex uint32) (hash uint32, htIndex uint32) {
	hash = jenkins(name) % hashTableSize
	htIndex = hash
	nProbes := 0
	for hashTable[htIndex].etIndex != 0 {
		htIndex = (htIndex + 1) % hashTableSize
		if htIndex == hash {
			panic("hashTableSize is too small")
		}
		nProbes++
	}
	hashTable[htIndex] = htElement{etIndex, name}
	if nProbes >= len(htStats.nProbes) {
		nProbes = len(htStats.nProbes) - 1
	}
	htStats.nProbes[nProbes]++
	htStats.nEntries++
	return hash, htIndex
}

// jenkins implements https://en.wikipedia.org/wiki/Jenkins_hash_function
func jenkins(s string) (hash uint32) {
	for i := 0; i < len(s); i++ {
		hash += uint32(s[i])
		hash += hash << 10
		hash ^= hash >> 6
	}
	hash += hash << 3
	hash ^= hash >> 11
	hash += hash << 15
	return hash
}

func parseTyp(s string) armneonintrinsics.Type {
	// arm_neon.h has a few of these idiosyncratic "int const"s.
	if s == "int const" {
		return armneonintrinsics.ParseType("const int")
	}
	return armneonintrinsics.ParseType(s)
}

func readIdent(b []byte) string {
	for i, c := range b {
		switch {
		default:
			return string(b[:i])
		case c == '_':
		case ('0' <= c) && (c <= '9'):
		case ('A' <= c) && (c <= 'A'):
		case ('a' <= c) && (c <= 'z'):
		}
	}
	return string(b)
}

const header = `// Code generated by running "go generate". DO NOT EDIT.

// Copyright 2021 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package armneonintrinsics
`
