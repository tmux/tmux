// Copyright 2020 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ----------------

// dumbindent formats C (and C-like) programs.
//
// Without explicit paths, it rewrites the standard input to standard output.
// Otherwise, the -l (list files that would change) or -w (write files in
// place) or both flags must be given. Given a file path, it operates on that
// file; given a directory path, it operates on all *.{c,h} files in that
// directory, recursively. File paths starting with a period are ignored.
//
// Pass -spaces=N or -tabs to use N spaces or 1 tab per indent level. The
// default is 2 spaces per indent level.
//
// It is similar in concept to pretty-printers like `indent` or `clang-format`.
// It is much dumber (it will not add or remove line breaks or otherwise
// re-flow lines of code just to fit within an 80 column limit) but it can
// therefore be much faster at the basic task of automatically indenting nested
// blocks. The output isn't 'perfect', but it's usually sufficiently readable
// if the input already has sensible line breaks.
//
// To quantify "much faster", on this one C file, this program was 80 times
// faster than `clang-format`, even without a column limit:
//
//	$ wc release/c/wuffs-v0.2.c
//	 11858  35980 431885 release/c/wuffs-v0.2.c
//	$ time dumbindent                               < release/c/wuffs-v0.2.c > /dev/null
//	real    0m0.008s
//	user    0m0.005s
//	sys     0m0.005s
//	$ time clang-format-9                           < release/c/wuffs-v0.2.c > /dev/null
//	real    0m0.668s
//	user    0m0.618s
//	sys     0m0.032s
//	$ time clang-format-9 -style='{ColumnLimit: 0}' < release/c/wuffs-v0.2.c > /dev/null
//	real    0m0.641s
//	user    0m0.585s
//	sys     0m0.037s
//
// More commentary is at:
// https://godoc.org/github.com/google/wuffs/lib/dumbindent
// https://nigeltao.github.io/blog/2020/dumbindent.html
package main

import (
	"bytes"
	"errors"
	"flag"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"runtime"
	"strings"

	"github.com/google/wuffs/lib/dumbindent"
)

var (
	lFlag = flag.Bool("l", false, "list files whose formatting differs from dumbindent's")
	wFlag = flag.Bool("w", false, "write result to (source) file instead of stdout")

	spacesFlag = flag.Int("spaces", 2, "the number of spaces per indent")
	tabsFlag   = flag.Bool("tabs", false, "indent with a tab instead of spaces")
)

func usage() {
	fmt.Fprintf(os.Stderr, "usage: dumbindent [flags] [path ...]\n")
	flag.PrintDefaults()
}

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	flag.Usage = usage
	flag.Parse()

	if flag.NArg() == 0 {
		if *lFlag {
			return errors.New("cannot use -l with standard input")
		}
		if *wFlag {
			return errors.New("cannot use -w with standard input")
		}
		return do(os.Stdin, "<standard input>")
	}

	if !*lFlag && !*wFlag {
		return errors.New("must use -l or -w if paths are given")
	}

	for i := 0; i < flag.NArg(); i++ {
		arg := flag.Arg(i)
		switch dir, err := os.Stat(arg); {
		case err != nil:
			return err
		case dir.IsDir():
			return filepath.Walk(arg, walk)
		default:
			if err := do(nil, arg); err != nil {
				return err
			}
		}
	}

	return nil
}

func isCHFile(info os.FileInfo) bool {
	name := info.Name()
	return !info.IsDir() && !strings.HasPrefix(name, ".") &&
		(strings.HasSuffix(name, ".c") || strings.HasSuffix(name, ".h"))
}

func walk(filename string, info os.FileInfo, err error) error {
	if (err == nil) && isCHFile(info) {
		err = do(nil, filename)
	}
	// Don't complain if a file was deleted in the meantime (i.e. the directory
	// changed concurrently while running this program).
	if (err != nil) && !os.IsNotExist(err) {
		return err
	}
	return nil
}

func do(r io.Reader, filename string) error {
	src, err := []byte(nil), error(nil)
	if r != nil {
		src, err = io.ReadAll(r)
	} else {
		src, err = os.ReadFile(filename)
	}
	if err != nil {
		return err
	}

	dst := dumbindent.FormatBytes(nil, src, &dumbindent.Options{
		Spaces: *spacesFlag,
		Tabs:   *tabsFlag,
	})

	if r != nil {
		if _, err := os.Stdout.Write(dst); err != nil {
			return err
		}
	} else if !bytes.Equal(dst, src) {
		if *lFlag {
			fmt.Println(filename)
		}
		if *wFlag {
			if err := writeFile(filename, dst); err != nil {
				return err
			}
		}
	}

	return nil
}

const chmodSupported = runtime.GOOS != "windows"

func writeFile(filename string, b []byte) error {
	f, err := os.CreateTemp(filepath.Dir(filename), filepath.Base(filename))
	if err != nil {
		return err
	}
	if chmodSupported {
		if info, err := os.Stat(filename); err == nil {
			f.Chmod(info.Mode().Perm())
		}
	}
	_, werr := f.Write(b)
	cerr := f.Close()
	if werr != nil {
		os.Remove(f.Name())
		return werr
	}
	if cerr != nil {
		os.Remove(f.Name())
		return cerr
	}
	return os.Rename(f.Name(), filename)
}
