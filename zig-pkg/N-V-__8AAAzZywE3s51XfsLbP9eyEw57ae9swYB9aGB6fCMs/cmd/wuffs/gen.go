// Copyright 2017 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package main

import (
	"bytes"
	"flag"
	"fmt"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"strings"

	"github.com/google/wuffs/lang/generate"
	"github.com/google/wuffs/lang/parse"

	cf "github.com/google/wuffs/cmd/commonflags"

	a "github.com/google/wuffs/lang/ast"
	t "github.com/google/wuffs/lang/token"
)

func doGen(wuffsRoot string, args []string) error    { return doGenGenlib(wuffsRoot, args, false) }
func doGenlib(wuffsRoot string, args []string) error { return doGenGenlib(wuffsRoot, args, true) }

func doGenGenlib(wuffsRoot string, args []string, genlib bool) error {
	flagSetName := `"wuffs gen <flags> std/pkg1 std/pkg2 etc"`
	if genlib {
		flagSetName = `"wuffs genlib <flags> std/pkg1 std/pkg2 etc"`
	}

	flags := flag.NewFlagSet(flagSetName, flag.ExitOnError)
	genlinenumFlag := flags.Bool("genlinenum", cf.GenlinenumDefault, cf.GenlinenumUsage)
	langsFlag := flags.String("langs", langsDefault, langsUsage)
	skipgendepsFlag := flags.Bool("skipgendeps", skipgendepsDefault, skipgendepsUsage)

	ccompilersFlag := (*string)(nil)
	skipgenFlag := (*bool)(nil)
	versionFlag := (*string)(nil)
	if genlib {
		ccompilersFlag = flags.String("ccompilers", cf.CcompilersDefault, cf.CcompilersUsage)
		skipgenFlag = flags.Bool("skipgen", skipgenDefault, skipgenUsage)
	} else {
		versionFlag = flags.String("version", cf.VersionDefault, cf.VersionUsage)
	}

	if err := flags.Parse(args); err != nil {
		return err
	}
	if genlib {
		if !cf.IsAlphaNumericIsh(*ccompilersFlag) {
			return fmt.Errorf("bad -ccompilers flag value %q", *ccompilersFlag)
		}
	}
	langs, err := parseLangs(*langsFlag)
	if err != nil {
		return err
	}
	v := cf.Version{}
	if !genlib {
		ok := false
		v, ok = cf.ParseVersion(*versionFlag)
		if !ok {
			return fmt.Errorf("bad -version flag value %q", *versionFlag)
		}
	}
	args = flags.Args()
	if len(args) == 0 {
		args = []string{"base", "std/..."}
	}

	h := genHelper{
		wuffsRoot:   wuffsRoot,
		langs:       langs,
		genlinenum:  *genlinenumFlag,
		skipgen:     genlib && *skipgenFlag,
		skipgendeps: *skipgendepsFlag,
	}
	if genlib {
		h.ccompilers = *ccompilersFlag
	}

	for _, arg := range args {
		recursive := strings.HasSuffix(arg, "/...")
		if recursive {
			arg = arg[:len(arg)-4]
		}
		if arg == "" {
			continue
		}

		if err := h.gen(arg, recursive); err != nil {
			return err
		}
	}

	if genlib {
		return h.genlibAffected()
	}
	return genrelease(wuffsRoot, langs, v)
}

type genHelper struct {
	wuffsRoot   string
	langs       []string
	ccompilers  string
	genlinenum  bool
	skipgen     bool
	skipgendeps bool

	affected []string
	seen     map[string]struct{}
	tm       t.Map
}

func (h *genHelper) gen(dirname string, recursive bool) error {
	for len(dirname) > 0 && dirname[len(dirname)-1] == '/' {
		dirname = dirname[:len(dirname)-1]
	}

	if h.seen == nil {
		h.seen = map[string]struct{}{}
	} else if _, ok := h.seen[dirname]; ok {
		return nil
	}
	h.seen[dirname] = struct{}{}

	if dirname == "base" {
		if err := h.genDir(dirname, nil); err != nil {
			return err
		}
		h.affected = append(h.affected, dirname)
		return nil
	}

	if !cf.IsValidUsePath(dirname) {
		return fmt.Errorf("invalid package path %q", dirname)
	}

	qualFilenames, dirnames, err := listDir(
		filepath.Join(h.wuffsRoot, filepath.FromSlash(dirname)), ".wuffs", recursive)
	if err != nil {
		return err
	}
	if len(qualFilenames) > 0 {
		if err := h.genDir(dirname, qualFilenames); err != nil {
			return err
		}
		h.affected = append(h.affected, dirname)
	}
	if len(dirnames) > 0 {
		for _, d := range dirnames {
			if err := h.gen(dirname+"/"+d, recursive); err != nil {
				return err
			}
		}
	}
	return nil
}

func (h *genHelper) genDir(dirname string, qualFilenames []string) error {
	// TODO: skip the generation if the output file already exists and its
	// mtime is newer than all inputs and the wuffs-gen-foo command.

	packageName := path.Base(dirname)
	if !validName(packageName) {
		return fmt.Errorf(`invalid package %q, not in [a-z0-9]+`, packageName)
	}

	if h.skipgen {
		return nil
	}
	if !h.skipgendeps {
		if err := h.genDirDependencies(qualFilenames); err != nil {
			return err
		}
	}

	for _, lang := range h.langs {
		command := "wuffs-" + lang
		cmdArgs := []string{"gen", "-package_name", packageName}
		if h.genlinenum != cf.GenlinenumDefault {
			cmdArgs = append(cmdArgs, fmt.Sprintf("-genlinenum=%t", h.genlinenum))
		}
		cmdArgs = append(cmdArgs, qualFilenames...)
		stdout := &bytes.Buffer{}

		cmd := exec.Command(command, cmdArgs...)
		cmd.Stdin = nil
		cmd.Stdout = stdout
		cmd.Stderr = os.Stderr
		if err := cmd.Run(); err == nil {
			// No-op.
		} else if _, ok := err.(*exec.ExitError); ok {
			return fmt.Errorf("%s failed, args=%q", command, cmdArgs)
		} else {
			return err
		}
		out := stdout.Bytes()

		flatDirname := fmt.Sprintf("wuffs-%s", strings.Replace(dirname, "/", "-", -1))
		if err := h.genFile(flatDirname, lang, out); err != nil {
			return err
		}
	}
	if len(h.langs) > 0 && packageName != "base" {
		if err := h.genWuffs(dirname, qualFilenames); err != nil {
			return err
		}
	}
	return nil
}

func (h *genHelper) genDirDependencies(qualifiedFilenames []string) error {
	files, err := generate.ParseFiles(&h.tm, qualifiedFilenames, nil)
	if err != nil {
		return err
	}
	for _, f := range files {
		for _, n := range f.TopLevelDecls() {
			if n.Kind() != a.KUse {
				continue
			}
			useDirname := h.tm.ByID(n.AsUse().Path())
			useDirname, _ = t.Unescape(useDirname)
			if err := h.gen(useDirname, false); err != nil {
				return err
			}
		}
	}
	return h.gen("base", false)
}

func (h *genHelper) genFile(dirname string, lang string, out []byte) error {
	return writeFile(
		filepath.Join(h.wuffsRoot, "gen", lang, filepath.FromSlash(dirname)+"."+lang),
		out,
	)
}

func (h *genHelper) genWuffs(dirname string, qualifiedFilenames []string) error {
	files, err := generate.ParseFiles(&h.tm, qualifiedFilenames, &parse.Options{
		AllowDoubleUnderscoreNames: true,
	})
	if err != nil {
		return err
	}

	out := &bytes.Buffer{}
	fmt.Fprintf(out, "// Code generated by running \"wuffs gen\". DO NOT EDIT.\n\n")

	for _, f := range files {
		for _, n := range f.TopLevelDecls() {
			switch n.Kind() {
			case a.KConst:
				n := n.AsConst()
				if !n.Public() {
					continue
				}
				fmt.Fprintf(out, "pub const %s : %s = %v\n",
					n.QID().Str(&h.tm), n.XType().Str(&h.tm), n.Value().Str(&h.tm))

			case a.KFunc:
				n := n.AsFunc()
				if !n.Public() {
					continue
				}
				if n.Receiver().IsZero() {
					return fmt.Errorf("TODO: genWuffs for a free-standing function")
				}
				// TODO: look at n.Asserts().
				fmt.Fprintf(out, "pub func %s.%s%v(", n.Receiver().Str(&h.tm), n.FuncName().Str(&h.tm), n.Effect())
				for i, field := range n.In().Fields() {
					field := field.AsField()
					if i > 0 {
						fmt.Fprintf(out, ", ")
					}
					// TODO: what happens if the XType is from another package?
					// Similarly for the out-param.
					fmt.Fprintf(out, "%s: %s", field.Name().Str(&h.tm), field.XType().Str(&h.tm))
				}
				fmt.Fprintf(out, ") ")
				if o := n.Out(); o != nil {
					fmt.Fprintf(out, "%s ", o.Str(&h.tm))
				}
				fmt.Fprintf(out, "{ }\n")

			case a.KStatus:
				n := n.AsStatus()
				if !n.Public() {
					continue
				}
				fmt.Fprintf(out, "pub status %s\n", n.QID().Str(&h.tm))

			case a.KStruct:
				n := n.AsStruct()
				if !n.Public() {
					continue
				}
				fmt.Fprintf(out, "pub struct %s", n.QID().Str(&h.tm))
				if n.Classy() {
					fmt.Fprintf(out, "?")
				}
				if imps := n.Implements(); len(imps) > 0 {
					fmt.Fprintf(out, " implements ")
					for i, imp := range imps {
						if i > 0 {
							fmt.Fprintf(out, ", ")
						}
						fmt.Fprintf(out, "%s", imp.AsTypeExpr().Str(&h.tm))
					}
				}
				fmt.Fprintf(out, "()\n")
			}
		}
	}
	return h.genFile(dirname, "wuffs", out.Bytes())
}

func (h *genHelper) genlibAffected() error {
	for _, lang := range h.langs {
		command := "wuffs-" + lang
		args := []string{"genlib"}
		args = append(args, "-dstdir", filepath.Join(h.wuffsRoot, "gen", "lib", lang))
		args = append(args, "-srcdir", filepath.Join(h.wuffsRoot, "gen", lang))
		if lang == "c" {
			args = append(args, fmt.Sprintf("-ccompilers=%s", h.ccompilers))
		}
		args = append(args, h.affected...)
		cmd := exec.Command(command, args...)
		cmd.Stdout = os.Stdout
		cmd.Stderr = os.Stderr
		if err := cmd.Run(); err != nil {
			return err
		}
	}
	return nil
}
