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
	"flag"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"

	"github.com/google/wuffs/internal/cgen"

	cf "github.com/google/wuffs/cmd/commonflags"
)

func doGenlib(args []string) error {
	flags := flag.FlagSet{}
	ccompilersFlag := flags.String("ccompilers", cf.CcompilersDefault, cf.CcompilersUsage)
	dstdirFlag := flags.String("dstdir", "", "directory containing the object files ")
	srcdirFlag := flags.String("srcdir", "", "directory containing the C source files")
	if err := flags.Parse(args); err != nil {
		return err
	}
	args = flags.Args()

	filenames := []string(nil)
	for _, arg := range args {
		if arg != "base" {
			filenames = append(filenames, "wuffs-"+strings.Replace(filepath.ToSlash(arg), "/", "-", -1))
		} else {
			for _, subModule := range cgen.BaseSubModules {
				filenames = append(filenames, "wuffs-base-"+subModule)
			}
		}
	}

	if *dstdirFlag == "" {
		return fmt.Errorf("empty -dstdir flag")
	}
	if *srcdirFlag == "" {
		return fmt.Errorf("empty -srcdir flag")
	}

	for _, cc := range strings.Split(*ccompilersFlag, ",") {
		cc = strings.TrimSpace(cc)
		if cc == "" {
			continue
		}

		for _, dynamism := range []string{"static", "dynamic"} {
			outDir := filepath.Join(*dstdirFlag, cc+"-"+dynamism)
			if err := os.MkdirAll(outDir, 0755); err != nil {
				return err
			}
			if err := genObj(outDir, *srcdirFlag, cc, dynamism, filenames); err != nil {
				return err
			}
			if err := genLib(outDir, cc, dynamism, filenames); err != nil {
				return err
			}
		}
	}
	return nil
}

// TODO: are these extensions correct for non-Linux?
var (
	objExtensions = map[string]string{
		"dynamic": ".lo",
		"static":  ".o",
	}
	libExtensions = map[string]string{
		"dynamic": ".so",
		"static":  ".a",
	}
)

func genObj(outDir string, inDir string, cc string, dynamism string, filenames []string) error {
	for _, filename := range filenames {
		in := ""
		out := genlibOutFilename(outDir, dynamism, filename)

		args := []string(nil)
		args = append(args, "-O3", "-std=c99", "-DWUFFS_IMPLEMENTATION")

		const wuffsBasePrefix = "wuffs-base-"
		if strings.HasPrefix(filename, wuffsBasePrefix) {
			in = filepath.Join(inDir, "wuffs-base.c")
			suffix := filename[len(wuffsBasePrefix):]
			args = append(args,
				"-DWUFFS_CONFIG__MODULES",
				"-DWUFFS_CONFIG__MODULE__BASE__"+strings.ToUpper(suffix),
			)
		} else {
			in = filepath.Join(inDir, filename+".c")
		}

		if dynamism == "dynamic" {
			args = append(args, "-fPIC", "-DPIC")
		}
		args = append(args, "-c", "-o", out, in)

		cmd := exec.Command(cc, args...)
		cmd.Stdout = os.Stdout
		cmd.Stderr = os.Stderr
		if err := cmd.Run(); err != nil {
			return err
		}
		fmt.Printf("genlib: %s\n", out)
	}
	return nil
}

func genLib(outDir string, cc string, dynamism string, filenames []string) error {
	args := []string(nil)
	switch dynamism {
	case "dynamic":
		// TODO: add a "-Wl,-soname,libwuffs.so.1.2.3" argument?
		args = append(args, "-shared", "-fPIC", "-o")
	case "static":
		cc = "ar"
		args = append(args, "rc")
	}
	out := filepath.Join(outDir, "libwuffs"+libExtensions[dynamism])
	args = append(args, out)

	for _, filename := range filenames {
		args = append(args, genlibOutFilename(outDir, dynamism, filename))
	}

	cmd := exec.Command(cc, args...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	if err := cmd.Run(); err != nil {
		return err
	}
	fmt.Printf("genlib: %s\n", out)
	return nil
}

func genlibOutFilename(outDir string, dynamism string, filename string) string {
	return filepath.Join(outDir, filename+objExtensions[dynamism])
}
