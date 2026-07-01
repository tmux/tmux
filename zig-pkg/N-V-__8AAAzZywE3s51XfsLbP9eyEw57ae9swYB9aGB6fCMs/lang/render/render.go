// Copyright 2017 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package render

// TODO: render an *ast.Node instead of a []token.Token, as this will better
// allow automated refactoring tools, not just automated formatting. For now,
// rendering tokens is easier, especially with respect to tracking comments.

import (
	"errors"
	"io"

	t "github.com/google/wuffs/lang/token"
)

var newLine = []byte{'\n'}

func max(a, b int) int {
	if a > b {
		return a
	}
	return b
}

func Render(w io.Writer, tm *t.Map, src []t.Token, comments []string) (err error) {
	if len(src) == 0 {
		return nil
	}

	const maxIndent = 0xFFFF
	indent := 0
	buf := make([]byte, 0, 1024)
	commentLine := uint32(0)

	inStruct := false
	varNameLength := uint32(0)

	prevLine := src[0].Line - 1
	prevLineHanging := false

	for len(src) > 0 {
		// Find the tokens in this line.
		line := src[0].Line
		i := 1
		for ; i < len(src) && src[i].Line == line; i++ {
			if i == len(src) || src[i].Line != line {
				break
			}
		}
		lineTokens := src[:i]
		src = src[i:]

		// Print any previous comments.
		commentIndent := indent
		if prevLineHanging {
			commentIndent += 2
		}
		for ; commentLine < line; commentLine++ {
			buf = buf[:0]
			buf = appendComment(buf, comments, commentLine, commentIndent, true)
			if len(buf) == 0 {
				continue
			}
			if commentLine > prevLine+1 {
				if _, err = w.Write(newLine); err != nil {
					return err
				}
			}
			buf = append(buf, '\n')
			if _, err = w.Write(buf); err != nil {
				return err
			}
			varNameLength = 0
			prevLine = commentLine
		}

		// Strip any trailing semi-colons.
		hanging := prevLineHanging
		prevLineHanging = true
		for len(lineTokens) > 0 && lineTokens[len(lineTokens)-1].ID == t.IDSemicolon {
			prevLineHanging = false
			lineTokens = lineTokens[:len(lineTokens)-1]
		}
		if len(lineTokens) == 0 {
			continue
		}

		// Collapse one or more blank lines to just one.
		if prevLine < line-1 {
			if _, err = w.Write(newLine); err != nil {
				return err
			}
			varNameLength = 0
		}

		// Render any leading indentation. If this line starts with a close
		// token, outdent it so that it hopefully aligns vertically with the
		// line containing the matching open token.
		buf = buf[:0]
		indentAdjustment := 0
		if id := lineTokens[0].ID; id == t.IDCloseDoubleCurly {
			// No-op.
		} else if id.IsClose() {
			indentAdjustment--
		} else if hanging && ((id != t.IDOpenCurly) && (id != t.IDOpenDoubleCurly)) {
			indentAdjustment += 2
		}
		buf = appendTabs(buf, indent+indentAdjustment)

		// Apply or update varNameLength.
		if len(lineTokens) < 4 {
			varNameLength = 0
		} else {
			id0 := lineTokens[0].ID
			id1 := lineTokens[1].ID
			if (id0 == t.IDPri) || (id0 == t.IDPub) {
				inStruct = id1 == t.IDStruct
				if id1 != t.IDConst {
					varNameLength = 0
				}
			}
			if (id1 == t.IDConst) || (id0 == t.IDVar) || inStruct {
				if varNameLength == 0 {
					varNameLength = measureVarNameLength(tm, lineTokens, src)
				}
				name := ""
				if colon := findColon(lineTokens); colon >= 0 {
					for _, lt := range lineTokens[:colon] {
						name = tm.ByID(lt.ID)
						buf = append(buf, name...)
						buf = append(buf, ' ')
					}
					for i := uint32(len(name)); i < varNameLength; i++ {
						buf = append(buf, ' ')
					}
					lineTokens = lineTokens[colon:]
				}
			} else {
				varNameLength = 0
			}
		}

		// Render the lineTokens.
		prevID, prevIsTightRight := t.ID(0), false
		for _, tok := range lineTokens {
			if prevID == t.IDEq || (prevID != 0 && !prevIsTightRight && !tok.ID.IsTightLeft()) {
				// The "(" token's tight-left-ness is context dependent. For
				// "f(x)", the "(" is tight-left. For "a * (b + c)", it is not.
				if tok.ID != t.IDOpenParen || !isCloseIdentStrLiteralQuestion(tm, prevID) {
					buf = append(buf, ' ')
				}
			}

			if s := tm.ByID(tok.ID); (s == "") || (s[0] < '0') || ('9' < s[0]) {
				buf = append(buf, s...)
			} else {
				buf = appendNum(buf, s)
			}

			if tok.ID == t.IDOpenCurly {
				if indent == maxIndent {
					return errors.New("render: too many \"{\" tokens")
				}
				indent++
			} else if tok.ID == t.IDCloseCurly {
				if indent == 0 {
					return errors.New("render: too many \"}\" tokens")
				}
				indent--
			}

			prevIsTightRight = tok.ID.IsTightRight()
			// The "+" and "-" tokens' tight-right-ness is context dependent.
			// The unary flavor is tight-right, the binary flavor is not.
			if prevID != 0 && tok.ID.IsUnaryOp() && tok.ID.IsBinaryOp() {
				// Token-based (not ast.Node-based) heuristic for whether the
				// operator looks unary instead of binary.
				prevIsTightRight = !isCloseIdentLiteral(tm, prevID)
			}

			prevID = tok.ID
		}

		buf = appendComment(buf, comments, line, 0, false)
		buf = append(buf, '\n')
		if _, err = w.Write(buf); err != nil {
			return err
		}
		commentLine = line + 1
		prevLine = line
		lastID := lineTokens[len(lineTokens)-1].ID
		prevLineHanging = prevLineHanging &&
			(lastID != t.IDOpenCurly) && (lastID != t.IDOpenDoubleCurly)
	}

	// Print any trailing comments.
	for ; uint(commentLine) < uint(len(comments)); commentLine++ {
		buf = buf[:0]
		buf = appendComment(buf, comments, commentLine, indent, true)
		if len(buf) > 0 {
			if commentLine > prevLine+1 {
				if _, err = w.Write(newLine); err != nil {
					return err
				}
			}
			buf = append(buf, '\n')
			if _, err = w.Write(buf); err != nil {
				return err
			}
			prevLine = commentLine
		}
	}

	return nil
}

func appendComment(buf []byte, comments []string, line uint32, indent int, otherwiseEmpty bool) []byte {
	if uint(line) < uint(len(comments)) {
		if com := comments[line]; com != "" {
			// Strip trailing spaces.
			for len(com) > 0 {
				if com[len(com)-1] != ' ' {
					break
				}
				com = com[:len(com)-1]
			}

			if otherwiseEmpty {
				buf = appendTabs(buf, indent)
			} else {
				buf = append(buf, "  "...)
			}
			buf = append(buf, com...)
		}
	}
	return buf
}

func appendNum(buf []byte, s string) []byte {
	groupLen := uint32(6)
	if (len(s) >= 2) && (s[0] == '0') {
		if (s[1] == 'X') || (s[1] == 'x') {
			buf = append(buf, "0x"...)
			s = s[2:]
			groupLen = 4
		} else if (s[1] == 'B') || (s[1] == 'b') {
			buf = append(buf, "0b"...)
			s = s[2:]
			groupLen = 4
		}
	}

	nonUnderscores := uint32(0)
	for i := 0; i < len(s); i++ {
		c := s[i]
		if c == '_' {
			continue
		}
		nonUnderscores++
	}
	digitsUntilGroup := nonUnderscores % groupLen
	if digitsUntilGroup == 0 {
		digitsUntilGroup = groupLen
	}

	for i := 0; i < len(s); i++ {
		c := s[i]
		if c == '_' {
			continue
		} else if 'a' <= c {
			c -= 'a' - 'A'
		}

		if digitsUntilGroup > 0 {
			digitsUntilGroup--
		} else {
			digitsUntilGroup = groupLen - 1
			buf = append(buf, '_')
		}
		buf = append(buf, c)
	}
	return buf
}

func appendTabs(buf []byte, nTabs int) []byte {
	nSpaces := 4 * nTabs // Hard-code 4 spaces per tab.
	if nSpaces > 0 {
		const spaces = "" + // 256 spaces.
			"                                                                " +
			"                                                                " +
			"                                                                " +
			"                                                                "
		for ; nSpaces > len(spaces); nSpaces -= len(spaces) {
			buf = append(buf, spaces...)
		}
		buf = append(buf, spaces[:nSpaces]...)
	}
	return buf
}

func isCloseIdentLiteral(tm *t.Map, x t.ID) bool {
	return x.IsClose() || x.IsIdent(tm) || x.IsLiteral(tm)
}

func isCloseIdentStrLiteralQuestion(tm *t.Map, x t.ID) bool {
	return x.IsClose() || x.IsIdent(tm) || x.IsDQStrLiteral(tm) ||
		x.IsSQStrLiteral(tm) || (x == t.IDQuestion)
}

func findColon(lineTokens []t.Token) int {
	for i, lt := range lineTokens {
		if lt.ID == t.IDColon {
			return i
		}
	}
	return -1
}

func measureVarNameLength(tm *t.Map, lineTokens []t.Token, remaining []t.Token) uint32 {
	x := findColon(lineTokens)
	if x <= 0 {
		return 0
	}

	line := lineTokens[0].Line
	length := len(tm.ByID(lineTokens[x-1].ID))
	for (len(remaining) > x) &&
		(remaining[0].Line == line+1) &&
		(remaining[x].Line == line+1) &&
		(remaining[x].ID == t.IDColon) {

		line = remaining[0].Line
		length = max(length, len(tm.ByID(remaining[x-1].ID)))

		remaining = remaining[x+1:]
		for len(remaining) > 0 && remaining[0].Line == line {
			remaining = remaining[1:]
		}
	}
	return uint32(length)
}
