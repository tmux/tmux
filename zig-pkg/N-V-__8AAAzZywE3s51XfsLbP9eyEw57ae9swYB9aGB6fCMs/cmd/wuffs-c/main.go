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

// wuffs-c handles the C language specific parts of the wuffs tool.
package main

import (
	"fmt"
	"os"

	"github.com/google/wuffs/internal/cgen"
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	if len(os.Args) < 2 {
		return fmt.Errorf("no sub-command given")
	}
	args := os.Args[2:]
	switch os.Args[1] {
	case "bench":
		return doBench(args)
	case "gen":
		return cgen.Do(args)
	case "genlib":
		return doGenlib(args)
	case "genrelease":
		return doGenrelease(args)
	case "test":
		return doTest(args)
	}
	return fmt.Errorf("bad sub-command %q", os.Args[1])
}
