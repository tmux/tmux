// Copyright 2017 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package generate

import (
	"flag"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"

	"github.com/google/wuffs/lang/check"
	"github.com/google/wuffs/lang/parse"
	"github.com/google/wuffs/lang/wuffsroot"

	a "github.com/google/wuffs/lang/ast"
	t "github.com/google/wuffs/lang/token"
)

type Generator func(packageName string, tm *t.Map, files []*a.File) ([]byte, error)

func Do(flags *flag.FlagSet, args []string, g Generator) error {
	packageName := flags.String("package_name", "", "the package name of the Wuffs input code")
	if err := flags.Parse(args); err != nil {
		return err
	}
	out := []byte(nil)

	if *packageName == "base" && len(flags.Args()) == 0 {
		var err error
		out, err = g("base", nil, nil)
		if err != nil {
			return err
		}

	} else {
		pkgName := checkPackageName(*packageName)
		if pkgName == "" {
			return fmt.Errorf("prohibited package name %q", *packageName)
		}

		tm := &t.Map{}
		files, err := parseFiles(tm, flags.Args())
		if err != nil {
			return err
		}

		if _, err := check.Check(tm, files, resolveUse); err != nil {
			return err
		}

		out, err = g(pkgName, tm, files)
		if err != nil {
			return err
		}
	}

	_, err := os.Stdout.Write(out)
	return err
}

func checkPackageName(s string) string {
	allUnderscores := true
	for i := 0; i < len(s); i++ {
		c := s[i]
		if ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') || (c == '_') || ('0' <= c && c <= '9') {
			allUnderscores = allUnderscores && c == '_'
		} else {
			return ""
		}
	}
	if allUnderscores {
		return ""
	}
	s = strings.ToLower(s)
	// Deny certain package names.
	switch s {
	case "base", "base_private", "base_private_header", "base_public", "base_public_header",
		"config", "implementation", "include_guard", "initialize", "private", "version":
		return ""
	}
	if strings.HasPrefix(s, "private_") {
		return ""
	}
	return s
}

func parseFiles(tm *t.Map, filenames []string) (files []*a.File, err error) {
	if len(filenames) == 0 {
		const filename = "stdin"
		src, err := io.ReadAll(os.Stdin)
		if err != nil {
			return nil, err
		}
		tokens, _, err := t.Tokenize(tm, filename, src)
		if err != nil {
			return nil, err
		}
		f, err := parse.Parse(tm, filename, tokens, nil)
		if err != nil {
			return nil, err
		}
		return []*a.File{f}, nil
	}
	return ParseFiles(tm, filenames, nil)
}

func ParseFiles(tm *t.Map, filenames []string, opts *parse.Options) (files []*a.File, err error) {
	for _, filename := range filenames {
		src, err := os.ReadFile(filename)
		if err != nil {
			return nil, err
		}
		tokens, _, err := t.Tokenize(tm, filename, src)
		if err != nil {
			return nil, err
		}
		f, err := parse.Parse(tm, filename, tokens, opts)
		if err != nil {
			return nil, err
		}
		files = append(files, f)
	}
	return files, nil
}

func resolveUse(usePath string) ([]byte, error) {
	wuffsRoot, err := wuffsroot.Value()
	if err != nil {
		return nil, err
	}
	return os.ReadFile(filepath.Join(wuffsRoot, "gen", "wuffs", filepath.FromSlash(usePath)))
}
