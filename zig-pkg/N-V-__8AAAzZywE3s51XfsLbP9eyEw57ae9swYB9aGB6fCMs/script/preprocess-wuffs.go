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

// Deprecated: unused as of commit 695a6815 "Remove gif.config_decoder".

// TODO: consider renaming this from script/preprocess-wuffs.go to
// cmd/wuffspreprocess, making it a "go install"able command line tool.

package main

// preprocess-wuffs.go generates target Wuffs files based on source Wuffs
// files. It is conceptually similar to, but weaker than, the C language's
// preprocessor's #ifdef mechanism. It is a stand-alone tool, not built into
// the Wuffs compiler. It runs relatively infrequently (not at every compile)
// and preprocessed output is checked into the repository.
//
// Preprocessing is separate from compilation, unlike C, for multiple reasons:
//
//  - It simplifies the Wuffs language per se, which makes it easier to write
//    other tools for Wuffs programs, such as an independent implementation of
//    the type checker or a tool that converts from Wuffs programs to the input
//    format of a formal verifier.
//
//  - Having an explicit file containing the preprocessed output helps keep the
//    programmer aware of the cost (increased source size is correlated with
//    increased binary size) of generating code. Other programming languages
//    make it very easy (in terms of lines of code written and checked in),
//    possibly too easy, to produce lots of object code, especiallly when
//    monomorphizing favors run-time performance over binary size.
//
//  - Writing the generated code to disk can help debug that generated code.
//
// It is the programmer's responsibility to re-run the preprocessor to
// re-generate the target files whenever the source file changes, similar to
// the Go language's "go generate" (https://blog.golang.org/generate).
// Naturally, this can be automated to some extent, e.g. through Makefiles or
// git hooks (when combined with the -fail-on-change flag).
//
// --------
//
// Usage:
//
//    go run preprocess-wuffs.go a.wuffs b*.wuffs dir3 dir4
//
// This scans all of the files or directories (recursively, albeit skipping
// dot-files) passed for Wuffs-preprocessor directives (see below). If no files
// or directories are passed, it scans ".", the current working directory.
//
// The optional -fail-on-change flag means to fail (with a non-zero exit code)
// if the target files' contents would change.
//
// --------
//
// Directives:
//
// Preprocessing is line-based, and lines of interest all start with optional
// whitespace and then "//#", slash slash hash, e.g. "//#USE etc".
//
// The first directive must be #USE, which mentions the name of this program
// and then lists the files to generate.
//
// Other directives are grouped into blocks:
//  - One or more "#WHEN FOO filename1 filename2" lines, and then
//  - One "#DONE FOO" line.
//
// The "FOO" names are arbitrary but must be unique (in a file), preventing
// nested blocks. A good text editor can also quickly cycle through the #WHEN
// and #DONE directives for any given block by searching for that unique name.
// By convention, the names look like "PREPROC123". The "123" suffix is for
// uniqueness. The names' ordering, number-suffixed or not, does not matter.
//
// A #WHEN's filenames detail which target files are active: the subset of the
// #USE directive's filenames that the subsequent lines (up until the next
// #WHEN or #DONE) apply to. A #WHEN's filenames may be empty, in which case
// the subsequent lines are part of the source file but none of the generated
// target files.
//
// A #REPLACE directive adds a simple find/replace filter to the active
// targets, applied to every subsequent generated line. A target may have
// multiple filters, which are applied sequentially. Filters are conceptually
// similar to a sed script, but the mechanism is trivial: for each input line,
// the first exact sub-string match (if any) is replaced.
//
// Lines that aren't directives (that don't start with whitespace then "//#")
// are simply copied (after per-target filtering) to either all active targets
// (when within a block) or to all targets (otherwise).
//
// The ## directive (e.g. "//## apple banana") is, like all directives, a "//"
// comment in the source file, but the "//##" is stripped and the remainder
// ("apple banana") is treated as a non-directive line, copied and filtered per
// the previous paragraph.
//
// For an example, look for "PREPROC" in the std/gif/decode_gif.wuffs file, and
// try "diff std/gif/decode_{gif,config}.wuffs".

import (
	"bytes"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"sort"
	"strings"

	"github.com/google/wuffs/lang/render"

	t "github.com/google/wuffs/lang/token"
)

var (
	focFlag = flag.Bool("fail-on-change", false,
		"fail (with a non-zero exit code) if the target files' contents would change")
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	flag.Parse()

	if flag.NArg() == 0 {
		if err := filepath.Walk(".", walk); err != nil {
			return err
		}
	} else {
		for i := 0; i < flag.NArg(); i++ {
			arg := flag.Arg(i)
			switch dir, err := os.Stat(arg); {
			case err != nil:
				return err
			case dir.IsDir():
				if err := filepath.Walk(arg, walk); err != nil {
					return err
				}
			default:
				if err := do(arg); err != nil {
					return err
				}
			}
		}
	}

	sortedFilenames := []string(nil)
	for filename := range globalTargets {
		sortedFilenames = append(sortedFilenames, filename)
	}
	sort.Strings(sortedFilenames)
	for _, filename := range sortedFilenames {
		contents := globalTargets[filename]
		if x, err := os.ReadFile(filename); (err == nil) && bytes.Equal(x, contents) {
			fmt.Printf("gen unchanged:  %s\n", filename)

			continue
		}
		if *focFlag {
			return fmt.Errorf("fail-on-change: %s\n", filename)
		}
		if err := writeFile(filename, contents); err != nil {
			return fmt.Errorf("writing %s: %v", filename, err)
		}
		fmt.Printf("gen wrote:      %s\n", filename)
	}

	return nil
}

func isWuffsFile(info os.FileInfo) bool {
	name := info.Name()
	return !info.IsDir() && !strings.HasPrefix(name, ".") && strings.HasSuffix(name, ".wuffs")
}

func walk(filename string, info os.FileInfo, err error) error {
	if (err == nil) && isWuffsFile(info) {
		err = do(filename)
	}
	// Don't complain if a file was deleted in the meantime (i.e. the directory
	// changed concurrently while running this program).
	if (err != nil) && !os.IsNotExist(err) {
		return err
	}
	return nil
}

var (
	directiveDone    = []byte(`//#DONE `)
	directiveHash    = []byte(`//## `)
	directiveReplace = []byte(`//#REPLACE `)
	directiveUse     = []byte(`//#USE "go run preprocess-wuffs.go" TO MAKE `)
	directiveWhen    = []byte(`//#WHEN `)

	_with_ = []byte(" WITH ")
	space  = []byte(" ")

	// globalTargets map from filenames to contents.
	globalTargets = map[string][]byte{}
)

type target struct {
	buffer  *bytes.Buffer
	filters []filter
}

func (t *target) write(s []byte) {
	for _, f := range t.filters {
		i := bytes.Index(s, f.find)
		if i < 0 {
			continue
		}
		x := []byte(nil)
		x = append(x, s[:i]...)
		x = append(x, f.replace...)
		x = append(x, s[i+len(f.find):]...)
		s = x
	}
	t.buffer.Write(s)
}

type filter struct {
	find    []byte
	replace []byte
}

func do(filename string) error {
	src, err := os.ReadFile(filename)
	if err != nil {
		return err
	}
	if !bytes.Contains(src, directiveUse) {
		return nil
	}
	localTargets := map[string]*target(nil)
	activeTargets := []*target(nil)
	usedBlockNames := map[string]bool{}
	blockName := []byte(nil) // Typically something like "PREPROC123".
	prefix := []byte(nil)    // Source file contents up to the "//#USE" directive.

	for remaining := src; len(remaining) > 0; {
		line := remaining
		if i := bytes.IndexByte(remaining, '\n'); i >= 0 {
			line, remaining = remaining[:i+1], remaining[i+1:]
		} else {
			remaining = nil
		}

		ppLine := parsePreprocessorLine(line)
		if ppLine == nil {
			if localTargets == nil {
				prefix = append(prefix, line...)
			} else {
				for _, t := range activeTargets {
					t.write(line)
				}
			}
			continue
		}

		if bytes.HasPrefix(ppLine, directiveUse) {
			if localTargets != nil {
				return fmt.Errorf("multiple #USE directives")
			}
			err := error(nil)
			localTargets, err = parseUse(filename, ppLine[len(directiveUse):])
			if err != nil {
				return err
			}

			activeTargets = activeTargets[:0]
			for _, t := range localTargets {
				activeTargets = append(activeTargets, t)
				t.write(prefix)
			}
			prefix = nil
			continue
		}

		if localTargets == nil {
			return fmt.Errorf("missing #USE directive")
		}

		switch {
		case bytes.HasPrefix(ppLine, directiveDone):
			arg := ppLine[len(directiveDone):]
			if blockName == nil {
				return fmt.Errorf("bad #DONE directive without #WHEN directive")
			} else if !bytes.Equal(blockName, arg) {
				return fmt.Errorf("bad directive name: %q", arg)
			}
			activeTargets = activeTargets[:0]
			for _, t := range localTargets {
				activeTargets = append(activeTargets, t)
			}
			blockName = nil

		case bytes.HasPrefix(ppLine, directiveHash):
			indent := []byte(nil)
			if i := bytes.IndexByte(line, '/'); i >= 0 {
				indent = line[:i]
			}
			if blockName == nil {
				for _, t := range localTargets {
					t.buffer.Write(indent)
					t.write(ppLine[len(directiveHash):])
					t.buffer.WriteByte('\n')
				}
			} else {
				for _, t := range activeTargets {
					t.buffer.Write(indent)
					t.write(ppLine[len(directiveHash):])
					t.buffer.WriteByte('\n')
				}
			}

		case bytes.HasPrefix(ppLine, directiveReplace):
			f := parseReplace(ppLine[len(directiveReplace):])
			if (f.find == nil) || (f.replace == nil) {
				return fmt.Errorf("bad #REPLACE directive: %q", ppLine)
			}
			for _, t := range activeTargets {
				t.filters = append(t.filters, f)
			}

		case bytes.HasPrefix(ppLine, directiveWhen):
			args := bytes.Split(ppLine[len(directiveWhen):], space)
			if len(args) == 0 {
				return fmt.Errorf("bad #WHEN directive: %q", ppLine)
			}
			if blockName == nil {
				blockName = args[0]
				if bn := string(blockName); usedBlockNames[bn] {
					return fmt.Errorf("duplicate directive name: %q", bn)
				} else {
					usedBlockNames[bn] = true
				}
			} else if !bytes.Equal(blockName, args[0]) {
				return fmt.Errorf("bad directive name: %q", args[0])
			}

			dir := filepath.Dir(filename)
			activeTargets = activeTargets[:0]
			for _, arg := range args[1:] {
				t := localTargets[filepath.Join(dir, string(arg))]
				if t == nil {
					return fmt.Errorf("bad #WHEN filename: %q", arg)
				}
				activeTargets = append(activeTargets, t)
			}

		default:
			return fmt.Errorf("bad directive: %q", ppLine)
		}
	}

	if blockName != nil {
		return fmt.Errorf("missing #DONE directive: %q", blockName)
	}

	for absFilename, t := range localTargets {
		globalTargets[absFilename] = wuffsfmt(t.buffer.Bytes())
	}
	return nil
}

func wuffsfmt(src []byte) []byte {
	tm := &t.Map{}
	tokens, comments, err := t.Tokenize(tm, "placeholder.filename", src)
	if err != nil {
		return src
	}
	dst := &bytes.Buffer{}
	if err := render.Render(dst, tm, tokens, comments); err != nil {
		return src
	}
	return dst.Bytes()
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

func parsePreprocessorLine(line []byte) []byte {
	// Look for "//#", slash slash hash.
	line = stripLeadingWhitespace(line)
	if (len(line) >= 3) && (line[0] == '/') && (line[1] == '/') && (line[2] == '#') {
		return bytes.TrimSpace(line)
	}
	return nil
}

func parseReplace(ppLine []byte) filter {
	s0, ppLine := parseString(ppLine)
	if s0 == nil {
		return filter{}
	}
	if !bytes.HasPrefix(ppLine, _with_) {
		return filter{}
	}
	ppLine = ppLine[len(_with_):]
	s1, ppLine := parseString(ppLine)
	if (s1 == nil) || (len(ppLine) != 0) {
		return filter{}
	}
	return filter{
		find:    s0,
		replace: s1,
	}
}

func parseString(line []byte) (s []byte, remaining []byte) {
	line = stripLeadingWhitespace(line)
	if (len(line) == 0) || (line[0] != '"') {
		return nil, line
	}
	line = line[1:]
	i := bytes.IndexByte(line, '"')
	if i < 0 {
		return nil, line
	}
	if bytes.IndexByte(line[:i], '\\') >= 0 {
		return nil, line
	}
	return line[:i], line[i+1:]
}

func parseUse(srcFilename string, ppLine []byte) (map[string]*target, error) {
	absSrcFilename := filepath.Clean(srcFilename)
	localTargets := map[string]*target{}
	dir := filepath.Dir(srcFilename)
	for _, relFilename := range bytes.Split(ppLine, space) {
		if len(relFilename) == 0 {
			continue
		}
		if !validFilename(relFilename) {
			return nil, fmt.Errorf("invalid filename: %q", string(relFilename))
		}
		absFilename := filepath.Join(dir, string(relFilename))
		if _, ok := globalTargets[absFilename]; ok {
			return nil, fmt.Errorf("duplicate filename: %q", absFilename)
		}
		if absFilename == absSrcFilename {
			return nil, fmt.Errorf("self-referential filename: %q", absFilename)
		}

		buf := &bytes.Buffer{}
		buf.WriteString(
			"// This file was automatically generated by \"preprocess-wuffs.go\".\n\n")
		buf.WriteString("// --------\n\n")
		localTargets[absFilename] = &target{buffer: buf}
	}
	return localTargets, nil
}

func stripLeadingWhitespace(s []byte) []byte {
	for (len(s) > 0) && (s[0] <= ' ') {
		s = s[1:]
	}
	return s
}

func validFilename(s []byte) bool {
	if (len(s) == 0) || (s[0] == '.') {
		return false
	}
	for _, c := range s {
		if (c <= ' ') || (c == '/') || (c == '\\') || (c == ':') {
			return false
		}
	}
	return true
}
