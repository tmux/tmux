// Copyright 2020 The Wuffs Authors.
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

// extract-cbor-rfc-7049-examples.go extracts the examples (Appendix A) from
// RFC 7049 "Concise Binary Object Representation (CBOR)". If the -concat flag
// is passed, a single CBOR array containing all of the examples are written to
// stdout. Otherwise, each example is written to a separate file under a
// "cbor-rfc-7049-examples" directory.
//
// Usage:
// go run extract-cbor-rfc-7049-examples.go < rfc7049-errata-corrected.txt
//
// The rfc7049-errata-corrected.txt file comes from
// https://github.com/cbor/spec-with-errata-fixed
//
// In hindsight, it would have been easier to start from
// https://github.com/cbor/test-vectors

import (
	"bytes"
	"flag"
	"fmt"
	"io"
	"os"
)

var concat = flag.Bool("concat", false, "output one monolithic CBOR file")

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	src, err := readExampleTable()
	if err != nil {
		return err
	}
	if *concat {
		// Start of CBOR indefinite-length array.
		os.Stdout.Write([]byte("\x9F"))
	} else if err := os.Mkdir("cbor-rfc-7049-examples", 0755); err != nil {
		return err
	}
	for remaining := []byte(nil); len(src) > 0; src = remaining {
		if i := bytes.IndexByte(src, '\n'); i >= 0 {
			src, remaining = src[:i], src[i+1:]
		} else {
			remaining = nil
		}
		src = bytes.TrimSpace(src)
		if (len(src) == 0) || (src[0] != '|') {
			continue
		}
		if (len(src) != 69) || (src[31] != '|') || (src[68] != '|') {
			return fmt.Errorf("main: unexpected table row: %q", string(src))
		}
		src = bytes.TrimSpace(src[33:68])
		if len(src) == 0 {
			if err := emit(); err != nil {
				return err
			}
			continue
		}
		if (len(src) > 1) && (src[0] == '0') && (src[1] == 'x') {
			src = src[2:]
		}
		for ; len(src) >= 2; src = src[2:] {
			contents = append(contents, (unhex(src[0])<<4)|unhex(src[1]))
		}
	}
	if err := emit(); err != nil {
		return err
	}
	if *concat {
		// End of CBOR indefinite-length array.
		os.Stdout.Write([]byte("\xFF"))
	}
	return nil
}

var (
	contents   = []byte(nil)
	fileNumber = 0
)

func emit() error {
	if len(contents) == 0 {
		return nil
	}
	if *concat {
		os.Stdout.Write(contents)
		contents = nil
		return nil
	}
	filename := fmt.Sprintf("cbor-rfc-7049-examples/%02d.cbor", fileNumber)
	fileNumber++
	err := os.WriteFile(filename, contents, 0644)
	contents = nil
	return err
}

func unhex(x uint8) uint8 {
	if x >= 'a' {
		return x + 10 - 'a'
	}
	return x - '0'
}

func readExampleTable() ([]byte, error) {
	appendixA := []byte("Appendix A.  Examples\n\n")
	ruler := []byte("+------------------------------+------------------------------------+\n")

	src, err := io.ReadAll(os.Stdin)
	if err != nil {
		return nil, err
	}
	if i := bytes.Index(src, appendixA); i < 0 {
		return nil, fmt.Errorf("main: couldn't find Appendix A")
	} else {
		src = src[i+len(appendixA):]
	}
	if i := bytes.Index(src, ruler); i < 0 {
		return nil, fmt.Errorf("main: couldn't find ---- ruler")
	} else {
		src = src[i+len(ruler):]
	}
	if i := bytes.Index(src, ruler); i < 0 {
		return nil, fmt.Errorf("main: couldn't find ---- ruler")
	} else {
		src = src[i+len(ruler):]
	}
	if i := bytes.Index(src, ruler); i < 0 {
		return nil, fmt.Errorf("main: couldn't find Appendix B")
	} else {
		src = src[:i]
	}
	return bytes.TrimSpace(src), nil
}
