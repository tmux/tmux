// Copyright 2024 The Wuffs Authors.
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

// Usage: go run make.go

import (
	"fmt"
	"hash/crc32"
	"os"
	"os/exec"
	"strings"
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	recipes := []struct {
		filterID   string
		filterName string
		generator  func() []byte
	}{
		// No test case generated for 04/x86, as the x86 CPU bytecode format is
		// complicated (variable length instructions). Test coverage is instead
		// provided by xz-tests-files's good-1-x86-lzma2.xz file.
		//
		// Similarly, no test case here for 0b/riscv. It's covered by the
		// good-1-riscv-lzma2-*.xz files.
		{"05", "powerpc", genPowerpc},
		{"06", "ia64", genIa64},
		{"07", "arm", genArm},
		{"07", "arm=start=1000", genArm},
		{"08", "armthumb", genArmthumb},
		{"09", "sparc", genSparc},
		{"0a", "arm64", genArm64},
	}

	for _, recipe := range recipes {
		contents := recipe.generator()
		filename := fmt.Sprintf("xz-filter-%s-%08x-%s.dat",
			recipe.filterID,
			crc32.ChecksumIEEE(contents),
			strings.Replace(recipe.filterName, "=", "_", -1))
		if err := os.WriteFile(filename, contents, 0644); err != nil {
			return err
		}
		fmt.Printf("Wrote %s\n", filename)

		cmd := exec.Command("xz", "--keep", "--force", "--"+recipe.filterName, "--lzma2", filename)
		cmd.Stderr = os.Stderr
		if err := cmd.Run(); err != nil {
			return err
		}
		fmt.Printf("Wrote %s.xz\n", filename)
	}
	return nil
}

// pi's digits are a source of random-looking numbers. len(pi) is 128.
const pi = "3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117067982148086513282306647093844"

// The artificial data generated below is designed to tickle XZ's BCJ (Branch,
// Call, Jump) filters. See xz-embedded's linux/lib/xz/xz_dec_bcj.c for C code
// implementations.

func genPowerpc() []byte {
	b := make256Bytes()
	for i := 0; (i + 4) <= len(b); i += 4 {
		x := peekU32BE(b[i:])
		switch pi[i>>2] & 1 {
		case 0:
			x = (x & 0x03FF_FFFC) | 0x4800_0001
		default:
			continue
		}
		pokeU32BE(b[i:], x)
	}
	return b
}

func genIa64() []byte {
	var bitMasks = [8]uint64{0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F}
	interveneIndex := 0

	b := make256Bytes()
	for i := 0; (i + 16) <= len(b); i += 16 {
		mask := 0
		switch pi[i>>2] & 3 {
		case 0:
			continue
		case 1:
			b[i], mask = 0x10, 4
		case 2:
			b[i], mask = 0x12, 6
		case 3:
			b[i], mask = 0x16, 7
		}

	loopUntilIntervention:
		for true {
			for slot, bitPos := 0, 5; slot < 3; slot, bitPos = slot+1, bitPos+41 {
				if ((mask >> slot) & 1) == 0 {
					continue
				}
				bytePos := bitPos >> 3
				bitRes := bitPos & 7
				instr := uint64(0)
				for j := 0; j < 6; j++ {
					instr |= uint64(b[i+j+bytePos]) << (8 * j)
				}

				intervene := (pi[interveneIndex%len(pi)] & 3) == 0
				interveneIndex++
				if !intervene {
					continue
				}
				norm := instr >> bitRes
				norm = (norm &^ 0x01E0_0000_0000) | 0x00A0_0000_0000
				norm = (norm &^ 0x0E00) | 0x0000
				newInstr := (instr & bitMasks[bitRes]) | (norm << bitRes)
				for j := 0; j < 6; j++ {
					b[i+j+bytePos] = uint8(newInstr >> (8 * j))
				}

				instr = uint64(0)
				for j := 0; j < 6; j++ {
					instr |= uint64(b[i+j+bytePos]) << (8 * j)
				}
				norm = instr >> bitRes
				if ((norm>>37)&0x0F) == 0x05 && ((norm>>9)&0x07) == 0x00 {
					break loopUntilIntervention
				}
				panic("intervention failed")
			}
		}
	}
	return b
}

func genArm() []byte {
	b := make256Bytes()
	for i := range b {
		if ((i & 3) == 3) &&
			((pi[i>>2] & 1) != 0) {
			b[i] = 0xEB
		}
	}
	return b
}

func genArmthumb() []byte {
	b := make256Bytes()
	for i := 0; (i + 4) <= len(b); {
		x := peekU32LE(b[i:])
		switch pi[i>>2] & 1 {
		case 0:
			i += 2
		case 1:
			x = (x & 0x07FF_07FF) | 0xF800_F000
			pokeU32LE(b[i:], x)
			i += 4
		}
	}
	return b
}

func genSparc() []byte {
	b := make256Bytes()
	for i := 0; (i + 4) <= len(b); i += 4 {
		x := peekU32BE(b[i:])
		switch pi[i>>2] & 3 {
		case 0:
			x = (x & 0x003F_FFFF) | 0x4000_0000
		case 1:
			x = (x & 0x003F_FFFF) | 0x7FC0_0000
		default:
			continue
		}
		pokeU32BE(b[i:], x)
	}
	return b
}

func genArm64() []byte {
	b := make256Bytes()
	for i := 0; (i + 4) <= len(b); i += 4 {
		x := peekU32LE(b[i:])
		switch pi[i>>2] & 3 {
		case 0:
			x = (x & 0x03FF_FFFF) | 0x9400_0000
		case 1:
			x = (x & 0x60FF_FFFF) | 0x90C0_0000
		default:
			continue
		}
		pokeU32LE(b[i:], x)
	}
	return b
}

func make256Bytes() []byte {
	b := make([]byte, 256)
	for i := range b {
		b[i] = byte(i)
	}
	return b
}

func peekU32BE(b []byte) uint32 {
	return (uint32(b[0]) << 0x18) |
		(uint32(b[1]) << 0x10) |
		(uint32(b[2]) << 0x08) |
		(uint32(b[3]) << 0x00)
}

func peekU32LE(b []byte) uint32 {
	return (uint32(b[0]) << 0x00) |
		(uint32(b[1]) << 0x08) |
		(uint32(b[2]) << 0x10) |
		(uint32(b[3]) << 0x18)
}

func pokeU32BE(b []byte, x uint32) {
	b[0] = uint8(x >> 0x18)
	b[1] = uint8(x >> 0x10)
	b[2] = uint8(x >> 0x08)
	b[3] = uint8(x >> 0x00)
}

func pokeU32LE(b []byte, x uint32) {
	b[0] = uint8(x >> 0x00)
	b[1] = uint8(x >> 0x08)
	b[2] = uint8(x >> 0x10)
	b[3] = uint8(x >> 0x18)
}
