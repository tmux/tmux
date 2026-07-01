// Copyright 2017 The Wuffs Authors.
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

// checksum.go prints a checksum of stdin's bytes, or of the opening digits of
// π. Checksum algorithms include "adler32", "crc32/ieee" and "xxhash32".
//
// Usage: go run checksum.go -algorithm=crc32/ieee < foo.bar

import (
	"crypto/sha256"
	"flag"
	"fmt"
	"hash"
	"hash/adler32"
	"hash/crc32"
	"hash/crc64"
	"io"
	"os"
	"strings"

	"github.com/pierrec/xxHash/xxHash32"
	"github.com/pierrec/xxHash/xxHash64"
)

var (
	algorithm = flag.String("algorithm", "adler32", "checksum algorithm")
	count     = flag.Int("count", 300, "number of cases (max 300), needs -pi flag")
	pi        = flag.Bool("pi", false, "checksum the digits of pi instead of stdin")
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	flag.Parse()

	if *pi {
		const digits = "3." +
			"141592653589793238462643383279502884197169399375105820974944592307816406" +
			"286208998628034825342117067982148086513282306647093844609550582231725359" +
			"408128481117450284102701938521105559644622948954930381964428810975665933" +
			"446128475648233786783165271201909145648566923460348610454326648213393607" +
			"260249141"
		if len(digits) != 299 {
			panic("bad len(digits)")
		} else if (*count < 0) || (len(digits) < (*count - 1)) {
			return fmt.Errorf("invalid count: %d", *count)
		}
		newLine := false
		for i := 0; i < *count; i++ {
			if err := do(strings.NewReader(digits[:i])); err != nil {
				return err
			}
			fmt.Print(",")
			newLine = i&7 == 7
			if newLine {
				fmt.Println()
			}
		}
		if !newLine {
			fmt.Println()
		}

	} else {
		if err := do(os.Stdin); err != nil {
			return err
		}
		fmt.Println()
	}
	return nil
}

func do(r io.Reader) error {
	h := io.Writer(nil)
	switch *algorithm {
	case "adler32":
		h = adler32.New()
	case "crc32/ieee":
		h = crc32.NewIEEE()
	case "crc64/ecma":
		h = crc64.New(crc64.MakeTable(crc64.ECMA))
	case "sha256":
		h = sha256.New()
	case "xxhash32":
		h = xxHash32.New(0)
	case "xxhash64":
		h = xxHash64.New(0)
	default:
		return fmt.Errorf("unknown algorithm %q", *algorithm)
	}

	if _, err := io.Copy(h, r); err != nil {
		return err
	}

	switch h := h.(type) {
	case hash.Hash32:
		fmt.Printf("0x%08X", h.Sum32())
	case hash.Hash64:
		fmt.Printf("0x%016X", h.Sum64())
	case hash.Hash:
		b := h.Sum(nil)
		for i, x := range b {
			if i == 0 {
				fmt.Printf("0x")
			} else if (i & 7) == 0 {
				fmt.Printf("_")
			}
			fmt.Printf("%02X", x)
		}
	default:
		return fmt.Errorf("algorithm %q is not a Hash32 or Hash64", *algorithm)
	}
	return nil
}
