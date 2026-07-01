// Copyright 2018 The Wuffs Authors.
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
	"path/filepath"
	"sort"
	"strings"

	"github.com/google/wuffs/internal/cgen"

	cf "github.com/google/wuffs/cmd/commonflags"
)

func doGenrelease(args []string) error {
	flags := flag.FlagSet{}
	commitDateFlag := flags.String("commitdate", "", "git commit date the release was built from")
	gitRevListCountFlag := flags.Int("gitrevlistcount", 0, `git "rev-list --count" that the release was built from`)
	revisionFlag := flags.String("revision", "", "git revision the release was built from")
	versionFlag := flags.String("version", cf.VersionDefault, cf.VersionUsage)

	if err := flags.Parse(args); err != nil {
		return err
	}
	if (*gitRevListCountFlag < 0) || (0x7FFFFFFF < *gitRevListCountFlag) {
		return fmt.Errorf("bad -gitrevlistcount flag value %d", *gitRevListCountFlag)
	}
	if !cf.IsAlphaNumericIsh(*commitDateFlag) {
		return fmt.Errorf("bad -commitdate flag value %q", *commitDateFlag)
	}
	if !cf.IsAlphaNumericIsh(*revisionFlag) {
		return fmt.Errorf("bad -revision flag value %q", *revisionFlag)
	}
	v, ok := cf.ParseVersion(*versionFlag)
	if !ok {
		return fmt.Errorf("bad -version flag value %q", *versionFlag)
	}
	args = flags.Args()

	// Calculate the base directory.
	baseDir := ""
	for _, filename := range args {
		if filepath.Base(filename) != "wuffs-base.c" {
			continue
		}
		baseDir = filepath.Dir(filename)
	}
	if baseDir == "" {
		return fmt.Errorf("could not determine base directory")
	}
	baseDirSlash := baseDir + string(filepath.Separator)

	h := &genReleaseHelper{
		filesList:       nil,
		filesMap:        map[string]parsedCFile{},
		seen:            nil,
		commitDate:      *commitDateFlag,
		gitRevListCount: *gitRevListCountFlag,
		revision:        *revisionFlag,
		version:         v,
	}

	for _, filename := range args {
		if !strings.HasPrefix(filename, baseDirSlash) {
			return fmt.Errorf("filename %q is not under base directory %q", filename, baseDir)
		}
		relFilename := filename[len(baseDirSlash):]

		// std/tga was renamed to std/targa in September 2024 (Wuffs v0.4
		// alpha). Ignore the generated file from older versions.
		if relFilename == "wuffs-std-tga.c" {
			println(filename + " is obsolete, please delete it.")
			continue
		}

		s, err := os.ReadFile(filename)
		if err != nil {
			return err
		}

		if err := h.parse(relFilename, s); err != nil {
			return err
		}
	}
	sort.Strings(h.filesList)

	out := bytes.NewBuffer(nil)
	out.WriteString("#ifndef WUFFS_INCLUDE_GUARD\n")
	out.WriteString("#define WUFFS_INCLUDE_GUARD\n\n")
	out.WriteString(grSingleFileGuidance[1:]) // [1:] skips the initial '\n'.
	out.WriteString(grPragmaPush[1:])         // [1:] skips the initial '\n'.

	h.seen = map[string]bool{}
	for _, f := range h.filesList {
		if err := h.gen(out, f, 0, 0); err != nil {
			return err
		}
	}

	out.WriteString("#if defined(__cplusplus) && defined(WUFFS_BASE__HAVE_UNIQUE_PTR)\n\n")
	out.WriteString(cgen.EmbeddedString_AuxBaseHh.Trim())
	out.WriteString("\n")
	for _, f := range cgen.EmbeddedStrings_AuxNonBaseHhFiles {
		out.WriteString(f.Trim())
		out.WriteString("\n")
	}
	out.WriteString("#endif  // defined(__cplusplus) && defined(WUFFS_BASE__HAVE_UNIQUE_PTR)\n")
	out.WriteString("\n")
	out.WriteString(cgen.EmbeddedString_DropInSTBH.Trim())

	out.Write(grImplStartsHere)
	out.WriteString("\n")

	h.seen = map[string]bool{}
	for _, f := range h.filesList {
		if err := h.gen(out, f, 1, 0); err != nil {
			return err
		}
	}

	out.WriteString("#if defined(__cplusplus) && defined(WUFFS_BASE__HAVE_UNIQUE_PTR)\n\n")
	out.WriteString(cgen.EmbeddedString_AuxBaseCc.Trim())
	out.WriteString("\n")
	for _, f := range cgen.EmbeddedStrings_AuxNonBaseCcFiles {
		out.WriteString(f.Trim())
		out.WriteString("\n")
	}
	out.WriteString("#endif  // defined(__cplusplus) && defined(WUFFS_BASE__HAVE_UNIQUE_PTR)\n\n")
	out.WriteString("\n")
	out.WriteString(cgen.EmbeddedString_DropInSTBC.Trim())

	out.Write(grImplEndsHere)
	out.WriteString(grPragmaPop)
	out.WriteString("#endif  // WUFFS_INCLUDE_GUARD\n")

	os.Stdout.Write(out.Bytes())
	return nil
}

var (
	grImplStartsHere = []byte("\n// ‼ WUFFS C HEADER ENDS HERE.\n#ifdef WUFFS_IMPLEMENTATION\n")
	grImplEndsHere   = []byte("#endif  // WUFFS_IMPLEMENTATION\n")
	grIncludeQuote   = []byte("#include \"")
	grNN             = []byte("\n\n")
	grVOverride      = []byte("// ¡ Some code generation programs can override WUFFS_VERSION.\n")
	grVEnd           = []byte(`#define WUFFS_VERSION_STRING "0.0.0+0.00000000"`)
	grWmrAbove       = []byte("// ¡ WUFFS MONOLITHIC RELEASE DISCARDS EVERYTHING ABOVE.\n")
	grWmrBelow       = []byte("// ¡ WUFFS MONOLITHIC RELEASE DISCARDS EVERYTHING BELOW.\n")
)

const grSingleFileGuidance = `
// Wuffs ships as a "single file C library" or "header file library" as per
// https://github.com/nothings/stb/blob/master/docs/stb_howto.txt
//
// To use that single file as a "foo.c"-like implementation, instead of a
// "foo.h"-like header, #define WUFFS_IMPLEMENTATION before #include'ing or
// compiling it.

`

const grPragmaPush = `
// Wuffs' C code is generated automatically, not hand-written. These warnings'
// costs outweigh the benefits.
//
// The "elif defined(__clang__)" isn't redundant. While vanilla clang defines
// __GNUC__, clang-cl (which mimics MSVC's cl.exe) does not.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunreachable-code"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#if defined(__cplusplus)
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wunreachable-code"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-parameter"
#if defined(__cplusplus)
#pragma clang diagnostic ignored "-Wold-style-cast"
#endif
#endif

`

const grPragmaPop = `
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(__clang__)
#pragma clang diagnostic pop
#endif

`

type parsedCFile struct {
	includes []string
	// fragments[0] is the header, fragments[1] is the implementation.
	fragments [2][]byte
}

type genReleaseHelper struct {
	filesList       []string
	filesMap        map[string]parsedCFile
	seen            map[string]bool
	commitDate      string
	gitRevListCount int
	revision        string
	version         cf.Version
}

func (h *genReleaseHelper) parse(relFilename string, s []byte) error {
	if _, ok := h.filesMap[relFilename]; ok {
		return fmt.Errorf("duplicate %q", relFilename)
	}

	f := parsedCFile{}

	if i := bytes.Index(s, grWmrAbove); i < 0 {
		return fmt.Errorf("could not find %q in %s", grWmrAbove, relFilename)
	} else {
		f.includes = parseIncludes(s[:i])
		s = s[i+len(grWmrAbove):]
	}

	if i := bytes.LastIndex(s, grWmrBelow); i < 0 {
		return fmt.Errorf("could not find %q in %s", grWmrBelow, relFilename)
	} else {
		s = s[:i]
	}

	if i := bytes.Index(s, grImplStartsHere); i < 0 {
		return fmt.Errorf("could not find %q in %s", grImplStartsHere, relFilename)
	} else {
		f.fragments[0], s = bytes.TrimSpace(s[:i]), s[i+len(grImplStartsHere):]
	}

	if i := bytes.LastIndex(s, grImplEndsHere); i < 0 {
		return fmt.Errorf("could not find %q in %s", grImplEndsHere, relFilename)
	} else {
		f.fragments[1] = bytes.TrimSpace(s[:i])
	}

	if relFilename == "wuffs-base.c" && (h.version != cf.Version{}) {
		if subs, err := h.substituteWuffsVersion(f.fragments[0]); err != nil {
			return err
		} else {
			f.fragments[0] = bytes.TrimSpace(subs)
		}
	}

	h.filesList = append(h.filesList, relFilename)
	h.filesMap[relFilename] = f
	return nil
}

func parseIncludes(s []byte) (ret []string) {
	for remaining := []byte(nil); len(s) > 0; s, remaining = remaining, nil {
		if i := bytes.IndexByte(s, '\n'); i >= 0 {
			s, remaining = s[:i+1], s[i+1:]
		}
		if len(s) == 0 || s[0] != '#' || !bytes.HasPrefix(s, grIncludeQuote) {
			continue
		}
		s = s[len(grIncludeQuote):]
		if len(s) < 2 || s[len(s)-2] != '"' || s[len(s)-1] != '\n' {
			continue
		}
		s = s[:len(s)-2]

		ret = append(ret, string(s))
	}
	sort.Strings(ret)
	return ret
}

func (h *genReleaseHelper) gen(w *bytes.Buffer, relFilename string, which int, depth uint32) error {
	if depth > 1024 {
		return fmt.Errorf("genrelease recursion depth too large")
	}
	depth++

	if strings.HasPrefix(relFilename, "./") {
		relFilename = relFilename[2:]
	}

	if h.seen[relFilename] {
		return nil
	}
	f, ok := h.filesMap[relFilename]
	if !ok {
		return fmt.Errorf("cannot resolve %q", relFilename)
	}

	// Process the files in #include-ee before #include-er order.
	for _, inc := range f.includes {
		if err := h.gen(w, inc, which, depth); err != nil {
			return err
		}
	}

	w.Write(f.fragments[which])
	w.WriteString("\n\n")
	h.seen[relFilename] = true
	return nil
}

func (h *genReleaseHelper) substituteWuffsVersion(s []byte) ([]byte, error) {
	ret := []byte(nil)
	if i := bytes.Index(s, grVOverride); i < 0 {
		return nil, fmt.Errorf("could not find %q in %s", grVOverride, "wuffs-base.c")
	} else {
		ret = append(ret, s[:i]...)
		s = s[i+len(grVOverride):]
	}

	cut := []byte(nil)
	if i := bytes.Index(s, grNN); i < 0 {
		return nil, fmt.Errorf(`could not find "\n\n" near WUFFS_VERSION`)
	} else {
		cut, s = s[:i], s[i+len(grNN):]
	}

	if !bytes.HasSuffix(cut, grVEnd) {
		return nil, fmt.Errorf("%q did not end with %q", cut, grVEnd)
	}

	w := bytes.NewBuffer(nil)
	fmt.Fprintf(w, `// WUFFS_VERSION was overridden by "wuffs gen -version"`)
	if h.revision != "" && h.commitDate != "" {
		fmt.Fprintf(w, " based on revision\n// %s committed on %s", h.revision, h.commitDate)
	}

	commitDate := "0"
	if h.commitDate != "" {
		commitDate = strings.Replace(h.commitDate, "-", "", -1)
	}

	buildMetadata := ""
	if h.gitRevListCount != 0 {
		buildMetadata = fmt.Sprintf("+%d.%s", h.gitRevListCount, commitDate)
	}

	fmt.Fprintf(w, `.
#define WUFFS_VERSION 0x%09X
#define WUFFS_VERSION_MAJOR %d
#define WUFFS_VERSION_MINOR %d
#define WUFFS_VERSION_PATCH %d
#define WUFFS_VERSION_PRE_RELEASE_LABEL %q
#define WUFFS_VERSION_BUILD_METADATA_COMMIT_COUNT %d
#define WUFFS_VERSION_BUILD_METADATA_COMMIT_DATE %s
#define WUFFS_VERSION_STRING %q

`, h.version.Uint64(), h.version.Major, h.version.Minor, h.version.Patch,
		h.version.Extension, h.gitRevListCount, commitDate,
		h.version.String()+buildMetadata)

	ret = append(ret, w.Bytes()...)
	ret = append(ret, s...)
	return ret, nil
}
