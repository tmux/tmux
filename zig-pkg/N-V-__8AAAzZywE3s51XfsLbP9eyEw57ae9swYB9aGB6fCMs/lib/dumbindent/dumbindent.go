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

// Package dumbindent formats C (and C-like) programs.
//
// It is similar in concept to pretty-printers like `indent` or `clang-format`.
// It is much dumber (it will not add or remove line breaks or otherwise
// re-flow lines of code just to fit within an 80 column limit) but it can
// therefore be much faster at the basic task of automatically indenting nested
// blocks. The output isn't 'perfect', but it's usually sufficiently readable
// if the input already has sensible line breaks.
//
// To quantify "much faster", on this one C file, `cmd/dumbindent` in this
// repository was 80 times faster than `clang-format`, even without a column
// limit:
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
// Apart from some rare and largely uninteresting exceptions, the dumbindent
// algorithm only considers:
//
//	∙ '{' and '}' curly braces,
//	∙ '(' and ')' round parentheses,
//	∙ '\n' line breaks,
//	∙ ' ' spaces and '\t' tabs that start or end a line, and
//	∙ strings, comments and preprocessor directives (in order to ignore any of
//	  the above special characters within them),
//
// Everything else is an opaque byte. Consider this input:
//
//	for (i = 0; i < 3; i++) {
//	j = 0;  // Ignore { in a comment.
//	if (i < j) { foo(); }
//	u = (v +
//	w);
//	}
//
// From the algorithm's point of view, this input is equivalent to:
//
//	....(.................).{
//	.................................
//	...(.....).{....()..}
//	....(...
//	.);
//	}
//
// The formatted output (using the default of 2 spaces per indent level) is:
//
//	....(.................).{
//	  .................................
//	  ...(.....).{....()..}
//	  ....(...
//	      .);
//	}
//
// Dumbindent adjusts lines horizontally (indenting) but not vertically (it
// does not break or un-break lines, or collapse consecutive blank lines),
// although it will remove blank lines at the end of the input. In the example
// above, it will not remove the "\n" between "u = (v +" and "w);", even though
// both lines are short.
//
// Each output line is indented according to the net number of open braces
// preceding it, although lines starting with close braces will outdent first,
// similar to `gofmt` style. A line which starts in a so-far-unbalanced open
// parenthesis, such as the "w);" line above, gets 2 additional indent levels.
//
// Horizontal adjustment only affects a line's leading white space (and will
// trim trailing white space). It does not affect white space within a line.
// Dumbindent does not parse the input as C/C++ source code.
//
// In particular, the algorithm does not solve C++'s "most vexing parse" or
// otherwise determine whether "x*y" is a multiplication or a type definition
// (where y is a pointer-to-x typed variable or function argument, such as
// "int*p"). For a type definition, where other formatting algorithms would
// re-write around the "*" as either "x* y" or "x *y", dumbindent will not
// insert spaces.
//
// Similarly, dumbindent will not correct this mis-indentation:
//
//	if (condition)
//	  goto fail;
//	  goto fail;
//
// Instead, when automatically or manually generating the input for dumbindent,
// it is recommended to always emit curly braces (again, similar to `gofmt`
// style), even for what would otherwise be 'one-liner' if statements.
//
// More commentary is at:
// https://nigeltao.github.io/blog/2020/dumbindent.html
package dumbindent

import (
	"bytes"
)

// 'Constants', but their type is []byte, not string.
var (
	backTick  = []byte("`")
	extern    = []byte("extern ")
	namespace = []byte("namespace ")
	starSlash = []byte("*/")

	newLines = []byte("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n")
	spaces   = []byte("                                ")
	tabs     = []byte("\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t")
)

// hangingBytes is a look-up table for updating the hanging variable.
var hangingBytes = [256]bool{
	'=':  true,
	'\\': true,
}

// Options are formatting options.
type Options struct {
	// Spaces, if positive, is the number of spaces per indent level. A
	// non-positive value means to use the default: 2 spaces per indent level.
	//
	// This field is ignored when Tabs is true.
	Spaces int

	// Tabs is whether to indent with tabs instead of spaces. If true, it's one
	// '\t' tab character per indent and the Spaces field is ignored.
	Tabs bool
}

// FormatBytes formats the C (or C-like) program in src, appending the result
// to dst, and returns that longer slice.
//
// It is valid to pass a dst slice (such as nil) whose unused capacity,
// cap(dst) - len(dst), is too short to hold the formatted program. In this
// case, a new slice will be allocated and returned.
//
// Passing a nil opts is valid and equivalent to passing &Options{}.
func FormatBytes(dst []byte, src []byte, opts *Options) []byte {
	indentBytes := spaces
	indentCount := 2
	if opts != nil {
		if opts.Tabs {
			indentBytes = tabs
			indentCount = 1
		} else if opts.Spaces > 0 {
			indentCount = opts.Spaces
		}
	}

	// Count the number of leading spaces (or tabs) on the first line. Every
	// output line starts with that much indent, before further indentation
	// from unbalanced braces and parentheses.
	initialIndent := countInitialOccurrences(src, indentBytes[0])

	src = trimLeadingWhiteSpaceAndNewLines(src)
	if len(src) == 0 {
		return dst
	} else if len(dst) == 0 {
		dst = make([]byte, 0, len(src)+(len(src)/2))
	}

	nBlankLines := 0 // The number of preceding blank lines.
	nBraces := 0     // The number of unbalanced '{'s.
	nParens := 0     // The number of unbalanced '('s.
	hanging := false // Whether the previous non-blank line ends with '=' or '\\'.
	preproc := false // Whether we're in a #preprocessor line.

	for line, remaining := src, []byte(nil); len(src) > 0; src = remaining {
		src = trimLeadingWhiteSpace(src)
		line, remaining = src, nil
		if i := bytes.IndexByte(line, '\n'); i >= 0 {
			line, remaining = line[:i], line[i+1:]
		}
		lineLength := len(line)

		// Strip any blank lines at the end of the file.
		if len(line) == 0 {
			nBlankLines++
			continue
		}
		if nBlankLines > 0 {
			dst = appendRepeatedBytes(dst, newLines, nBlankLines)
			nBlankLines = 0
		}

		// Handle preprocessor lines (#ifdef, #pragma, etc).
		if preproc || (line[0] == '#') {
			indent := initialIndent
			if preproc {
				indent += indentCount * 2
			}
			line = trimTrailingWhiteSpace(line)
			dst = appendRepeatedBytes(dst, indentBytes, indent)
			dst = append(dst, line...)
			dst = append(dst, '\n')
			hanging = false
			preproc = lastNonWhiteSpace(line) == '\\'
			continue
		}

		closeBraces := 0

		// Don't indent for `extern "C" {` or `namespace foo {`.
		if ((line[0] == 'e') && hasPrefixAndBrace(line, extern)) ||
			((line[0] == 'n') && hasPrefixAndBrace(line, namespace)) {
			nBraces--

		} else {
			// Account for leading '}'s before we print the line's indentation.
			for ; (closeBraces < len(line)) && line[closeBraces] == '}'; closeBraces++ {
			}
			nBraces -= closeBraces

			// Because the "{" in "extern .*{" and "namespace .*{" is had no
			// net effect on nBraces, the matching "}" can cause the nBraces
			// count to dip below zero. Correct for that here.
			if nBraces < 0 {
				nBraces = 0
			}
		}

		// Output indentation. Dumbindent's default, 2 spaces per indent level,
		// roughly approximates clang-format's default style.
		indent := initialIndent
		if nBraces > 0 {
			indent += indentCount * nBraces
		}
		if (nParens > 0) || hanging {
			indent += indentCount * 2
		}
		dst = appendRepeatedBytes(dst, indentBytes, indent)

		// Output the leading '}'s.
		dst = append(dst, line[:closeBraces]...)
		line = line[closeBraces:]

		// Adjust the state according to the braces and parentheses within the
		// line (except for those in comments and strings).
		last := lastNonWhiteSpace(line)
	loop:
		for {
			for i, c := range line {
				switch c {
				case '{':
					nBraces++
				case '}':
					nBraces--
				case '(':
					nParens++
				case ')':
					nParens--

				case '/':
					if (i + 1) >= len(line) {
						break
					}
					if line[i+1] == '/' {
						// A slash-slash comment. Skip the rest of the line.
						last = lastNonWhiteSpace(line[:i])
						break loop
					} else if line[i+1] == '*' {
						// A slash-star comment.
						dst = append(dst, line[:i+2]...)
						restOfLine := line[i+2:]
						restOfSrc := src[lineLength-len(restOfLine):]
						dst, line, remaining = handleRaw(dst, restOfSrc, starSlash)
						last = lastNonWhiteSpace(line)
						continue loop
					}

				case '"', '\'':
					// A cooked string, whose contents are backslash-escaped.
					suffix := skipCooked(line[i+1:], c)
					dst = append(dst, line[:len(line)-len(suffix)]...)
					line = suffix
					continue loop

				case '`':
					// A raw string.
					dst = append(dst, line[:i+1]...)
					restOfLine := line[i+1:]
					restOfSrc := src[lineLength-len(restOfLine):]
					dst, line, remaining = handleRaw(dst, restOfSrc, backTick)
					last = lastNonWhiteSpace(line)
					continue loop
				}
			}
			break loop
		}
		hanging = hangingBytes[last]

		// Output the line (minus any trailing space).
		line = trimTrailingWhiteSpace(line)
		dst = append(dst, line...)
		dst = append(dst, "\n"...)
	}
	return dst
}

// hasPrefixAndBrace returns whether line starts with prefix and after that
// contains a '{'.
func hasPrefixAndBrace(line []byte, prefix []byte) bool {
	return bytes.HasPrefix(line, prefix) &&
		bytes.IndexByte(line[len(prefix):], '{') >= 0
}

// trimLeadingWhiteSpaceAndNewLines converts "\t\n  foo bar " to "foo bar ".
func trimLeadingWhiteSpaceAndNewLines(s []byte) []byte {
	for (len(s) > 0) && ((s[0] == ' ') || (s[0] == '\t') || (s[0] == '\n')) {
		s = s[1:]
	}
	return s
}

// trimLeadingWhiteSpace converts "\t\t  foo bar " to "foo bar ".
func trimLeadingWhiteSpace(s []byte) []byte {
	for (len(s) > 0) && ((s[0] == ' ') || (s[0] == '\t')) {
		s = s[1:]
	}
	return s
}

// trimTrailingWhiteSpace converts "\t\t  foo bar " to "\t\t  foo bar".
func trimTrailingWhiteSpace(s []byte) []byte {
	for (len(s) > 0) && ((s[len(s)-1] == ' ') || (s[len(s)-1] == '\t')) {
		s = s[:len(s)-1]
	}
	return s
}

// appendRepeatedBytes appends number copies of a byte, assuming that
// repeatedBytes' elements are all the same byte.
func appendRepeatedBytes(dst []byte, repeatedBytes []byte, number int) []byte {
	for number > 0 {
		n := number
		if n > len(repeatedBytes) {
			n = len(repeatedBytes)
		}
		dst = append(dst, repeatedBytes[:n]...)
		number -= n
	}
	return dst
}

// countInitialOccurrences returns the length of the longest prefix of s that
// consists entirely of x. Passing ("aaabacdaa", 'a') returns 3.
func countInitialOccurrences(s []byte, x byte) int {
	for i, c := range s {
		if c != x {
			return i
		}
	}
	return len(s)
}

// lastNonWhiteSpace returns the 'z' in "abc xyz  ". It returns '\x00' if s
// consists entirely of spaces or tabs.
func lastNonWhiteSpace(s []byte) byte {
	for i := len(s) - 1; i >= 0; i-- {
		if x := s[i]; (x != ' ') && (x != '\t') {
			return x
		}
	}
	return 0
}

// skipCooked converts `ijk \" lmn" pqr` to ` pqr`.
func skipCooked(s []byte, quote byte) (suffix []byte) {
	for i := 0; i < len(s); {
		if x := s[i]; x == quote {
			return s[i+1:]
		} else if x != '\\' {
			i += 1
		} else if (i + 1) < len(s) {
			i += 2
		} else {
			break
		}
	}
	return nil
}

// handleRaw copies a raw string from restOfSrc to dst, re-calculating the
// (line, remaining) pair afterwards.
func handleRaw(dst []byte, restOfSrc []byte, endQuote []byte) (retDst []byte, line []byte, remaining []byte) {
	end := bytes.Index(restOfSrc, endQuote)
	if end < 0 {
		end = len(restOfSrc)
	} else {
		end += len(endQuote)
	}
	dst = append(dst, restOfSrc[:end]...)
	line, remaining = restOfSrc[end:], nil
	if i := bytes.IndexByte(line, '\n'); i >= 0 {
		line, remaining = line[:i], line[i+1:]
	}
	return dst, line, remaining
}
