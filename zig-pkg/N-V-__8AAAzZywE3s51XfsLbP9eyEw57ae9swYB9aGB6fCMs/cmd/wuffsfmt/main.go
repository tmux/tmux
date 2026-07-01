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

// wuffsfmt formats Wuffs programs.
//
// Without explicit paths, it rewrites the standard input to standard output.
// Otherwise, the -l (list files that would change) or -w (write files in
// place) or both flags must be given. Given a file path, it operates on that
// file; given a directory path, it operates on all *.wuffs files in that
// directory, recursively. File paths starting with a period are ignored.
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

	"github.com/google/wuffs/lang/parse"
	"github.com/google/wuffs/lang/render"

	t "github.com/google/wuffs/lang/token"
)

var (
	lFlag = flag.Bool("l", false, "list files whose formatting differs from wuffsfmt's")
	wFlag = flag.Bool("w", false, "write result to (source) file instead of stdout")
)

func usage() {
	fmt.Fprintf(os.Stderr, "usage: wuffsfmt [flags] [path ...]\n")
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

func isWuffsFile(info os.FileInfo) bool {
	name := info.Name()
	return !info.IsDir() && !strings.HasPrefix(name, ".") && strings.HasSuffix(name, ".wuffs")
}

func walk(filename string, info os.FileInfo, err error) error {
	if (err == nil) && isWuffsFile(info) {
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

	tm := &t.Map{}
	tokens, comments, err := t.Tokenize(tm, filename, src)
	if err != nil {
		return err
	}
	// We don't need the AST node to pretty-print, but it's worth rejecting
	// syntax errors early. This is just a parse, not a full type check.
	if _, err := parse.Parse(tm, filename, tokens, &parse.Options{
		AllowDoubleUnderscoreNames: true,
	}); err != nil {
		return err
	}
	buf := &bytes.Buffer{}
	if err := render.Render(buf, tm, tokens, comments); err != nil {
		return err
	}
	dst := buf.Bytes()

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
