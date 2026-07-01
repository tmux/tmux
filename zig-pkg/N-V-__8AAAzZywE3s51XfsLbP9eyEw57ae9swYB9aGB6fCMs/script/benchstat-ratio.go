// Copyright 2021 The Wuffs Authors.
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

// benchstat-ratio.go prints the relative ratios of similarly named benchstat
// summary lines. The benchstat tool (written in Go) is not specific to Wuffs.
// It is documented at https://godoc.org/golang.org/x/perf/cmd/benchstat
//
// Out of the box, benchstat (when passed two filename arguments) compares
// *identically* named lines (i.e. before and after a code change). This
// program computes ratios for *different-but-similar* named lines (i.e.
// multiple implementations or compilers for the same algorithm or test data),
// grouped by the name fragment after the first '_' and before the first '/'.
//
// By default, the baseline case (normalized to a ratio of 1.00x) is the one
// whose name starts with "mimic_" and contains "/gcc".
//
// For example, given benchstat output like:
//
// wuffs_crc32_ieee_10k/clang9   8.32GB/s ± 4%
// wuffs_crc32_ieee_100k/clang9  10.8GB/s ± 1%
// mimic_crc32_ieee_10k/clang9    771MB/s ± 0%
// mimic_crc32_ieee_100k/clang9   772MB/s ± 0%
// wuffs_crc32_ieee_10k/gcc10    8.68GB/s ± 0%
// wuffs_crc32_ieee_100k/gcc10   10.9GB/s ± 0%
// mimic_crc32_ieee_10k/gcc10     775MB/s ± 0%
// mimic_crc32_ieee_100k/gcc10    773MB/s ± 0%
//
// Piping that through this program prints:
//
// wuffs_crc32_ieee_10k/clang9   8.32GB/s ± 4%  10.74x
// wuffs_crc32_ieee_100k/clang9  10.8GB/s ± 1%  13.97x
// mimic_crc32_ieee_10k/clang9    771MB/s ± 0%   0.99x
// mimic_crc32_ieee_100k/clang9   772MB/s ± 0%   1.00x
// wuffs_crc32_ieee_10k/gcc10    8.68GB/s ± 0%  11.20x
// wuffs_crc32_ieee_100k/gcc10   10.9GB/s ± 0%  14.10x
// mimic_crc32_ieee_10k/gcc10     775MB/s ± 0%   1.00x
// mimic_crc32_ieee_100k/gcc10    773MB/s ± 0%   1.00x
//
// There are two groups here: "crc32_ieee_10k" and "crc32_ieee_100k".
//
// Usage: benchstat etc | go run benchstat-ratio.go

import (
	"bufio"
	"flag"
	"fmt"
	"os"
	"strconv"
	"strings"
)

var (
	baseline = flag.String("baseline", "mimic", "benchmark name prefix of the 1.00x case")
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	flag.Parse()
	baselineUnderscore = *baseline + "_"

	r := bufio.NewScanner(os.Stdin)
	for r.Scan() {
		line := strings.TrimSpace(string(r.Bytes()))
		pmIndex := strings.Index(line, plusMinus)
		if pmIndex < 0 {
			// Collapse multiple blank lines to a single one.
			if (len(entries) > 0) && (entries[len(entries)-1].key != "") {
				entries = append(entries, entry{})
			}
			continue
		}

		// Find the "/gcc10" in "mimic_etc/gcc10".
		if (gccSuffix == "") && strings.HasPrefix(line, baselineUnderscore) {
			if i := strings.Index(line, slashGCC); i >= 0 {
				gccSuffix = line[i:]
				if j := strings.IndexByte(gccSuffix, ' '); j >= 0 {
					gccSuffix = gccSuffix[:j]
				}
			}
		}

		key, val := parse(line[:pmIndex])
		if key == "" {
			return fmt.Errorf("could not parse %q", line)
		} else if _, ok := values[key]; ok {
			return fmt.Errorf("duplicate key for %q", line)
		}
		entries = append(entries, entry{key, line})
		values[key] = val
	}
	if err := r.Err(); err != nil {
		return err
	}

	for _, e := range entries {
		if e.key == "" {
			fmt.Println()
			continue
		}
		eVal, eOK := values[e.key]
		bVal, bOK := values[baselineKeyFor(e.key)]
		if eOK && bOK {
			switch e.key[0] {
			case '1':
				fmt.Printf("%s %6.2fx\n", e.line, bVal/eVal)
				continue
			case '2':
				fmt.Printf("%s %6.2fx\n", e.line, eVal/bVal)
				continue
			}
		}
		fmt.Printf("%s\n", e.line)
	}

	return nil
}

func parse(line string) (key string, val float64) {
	i := strings.IndexByte(line, ' ')
	if i < 0 {
		return "", 0
	}
	k, v := line[:i], strings.TrimSpace(line[i+1:])

	prefix, multiplier := "", 1e0
	switch {
	case strings.HasSuffix(v, suffix_GBs):
		prefix, multiplier = prefix2, 1e9
		v = v[:len(v)-len(suffix_GBs)]
	case strings.HasSuffix(v, suffix_MBs):
		prefix, multiplier = prefix2, 1e6
		v = v[:len(v)-len(suffix_MBs)]
	case strings.HasSuffix(v, suffix_KBs):
		prefix, multiplier = prefix2, 1e3
		v = v[:len(v)-len(suffix_KBs)]
	case strings.HasSuffix(v, suffix__Bs):
		prefix, multiplier = prefix2, 1e0
		v = v[:len(v)-len(suffix__Bs)]

	case strings.HasSuffix(v, suffix_ns):
		prefix, multiplier = prefix1, 1e0
		v = v[:len(v)-len(suffix_ns)]
	case strings.HasSuffix(v, suffix_µs):
		prefix, multiplier = prefix1, 1e3
		v = v[:len(v)-len(suffix_µs)]
	case strings.HasSuffix(v, suffix_ms):
		prefix, multiplier = prefix1, 1e6
		v = v[:len(v)-len(suffix_ms)]
	case strings.HasSuffix(v, suffix__s):
		prefix, multiplier = prefix1, 1e9
		v = v[:len(v)-len(suffix__s)]
	default:
		return "", 0
	}

	x, err := strconv.ParseFloat(v, 64)
	if err != nil {
		return "", 0
	}
	return prefix + string(k), x * multiplier
}

func baselineKeyFor(key string) (bKey string) {
	if key == "" {
		return ""
	}
	i := strings.IndexByte(key, '_')
	if i < 0 {
		return ""
	}
	j := strings.IndexByte(key[i:], '/')
	if j < 0 {
		j = len(key) - i
	}
	return key[:1] + *baseline + key[i:i+j] + gccSuffix
}

type entry struct {
	key  string
	line string
}

var (
	entries []entry
	values  = map[string]float64{}

	baselineUnderscore string
	gccSuffix          string
)

const (
	plusMinus = " ± "
	slashGCC  = "/gcc"

	prefix1 = "1" // val is nanoseconds.
	prefix2 = "2" // val is bytes per second.

	suffix_GBs = "GB/s"
	suffix_MBs = "MB/s"
	suffix_KBs = "KB/s"
	suffix__Bs = "B/s"

	suffix_ns = "ns"
	suffix_µs = "µs"
	suffix_ms = "ms"
	suffix__s = "s"
)
