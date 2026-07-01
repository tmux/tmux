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

// commonflags holds flag defaults and usage messages that are common to the
// Wuffs command line tools.
//
// It also holds functions to parse and validate these flag values.
package commonflags

import (
	"fmt"
	"path"
	"strings"
)

const (
	CcompilersDefault = "clang,gcc"
	CcompilersUsage   = `comma-separated list of C compilers`

	FocusDefault = ""
	FocusUsage   = `comma-separated list of tests or benchmarks (name prefixes) to focus on, e.g. "wuffs_gif_decode"`

	GenlinenumDefault = false
	GenlinenumUsage   = `whether to generate filename:line_number comments`

	IterscaleDefault = 100
	IterscaleMin     = 0
	IterscaleMax     = 1000000
	IterscaleUsage   = `a scaling factor for the number of iterations per benchmark`

	MimicDefault = false
	MimicUsage   = `whether to compare Wuffs' output with other libraries' output`

	RepsDefault = 5
	RepsMin     = 0
	RepsMax     = 1000000
	RepsUsage   = `the number of repetitions per benchmark`

	VersionDefault = "0.0.0"
	VersionUsage   = `version string, e.g. "1.2.3-beta.4"`
)

// TODO: do IsAlphaNumericIsh and IsValidUsePath belong in a separate package,
// such as lang/validate? Perhaps together with token.Unescape?

// IsAlphaNumericIsh returns whether s contains only ASCII alpha-numerics and a
// limited set of punctuation such as commas and slashes, but not containing
// e.g. spaces, semi-colons, colons or backslashes.
//
// The intent is that if s is alpha-numeric-ish, then it should not need
// escaping when passed to other programs as command line arguments.
func IsAlphaNumericIsh(s string) bool {
	for i := 0; i < len(s); i++ {
		c := s[i]
		if c == ',' || c == '-' || c == '.' || c == '/' || ('0' <= c && c <= '9') || ('A' <= c && c <= 'Z') ||
			c == '_' || ('a' <= c && c <= 'z') {
			continue
		}
		return false
	}
	return true
}

func IsValidUsePath(s string) bool {
	return s == path.Clean(s) && s != "" && s[0] != '.' && s[0] != '/'
}

type Version struct {
	Major     uint32
	Minor     uint16
	Patch     uint16
	Extension string
}

func ParseVersion(s string) (v Version, ok bool) {
	if !IsAlphaNumericIsh(s) {
		return Version{}, false
	}
	if i := strings.IndexByte(s, '-'); i >= 0 {
		s, v.Extension = s[:i], s[i+1:]
	}
	if _, err := fmt.Sscanf(s, "%d.%d.%d", &v.Major, &v.Minor, &v.Patch); err != nil {
		return Version{}, false
	}
	return v, true
}

func (v Version) String() string {
	s := fmt.Sprintf("%d.%d.%d", v.Major, v.Minor, v.Patch)
	if v.Extension != "" {
		s += "-" + v.Extension
	}
	return s
}

func (v Version) Uint64() uint64 {
	return (uint64(v.Major) << 32) | (uint64(v.Minor) << 16) | (uint64(v.Patch) << 0)
}
