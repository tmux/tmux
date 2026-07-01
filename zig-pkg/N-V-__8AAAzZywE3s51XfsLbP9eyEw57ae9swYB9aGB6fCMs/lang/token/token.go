// Copyright 2017 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package token

import (
	"errors"
	"fmt"
	"unicode/utf8"
)

const (
	maxID        = 1048575
	maxLine      = 1048575
	maxTokenSize = 1023
)

var backslashes = [256]byte{
	'"':  0x22 | 0x80,
	'\'': 0x27 | 0x80,
	'/':  0x2F | 0x80,
	'0':  0x00 | 0x80,
	'?':  0x3F | 0x80,
	'\\': 0x5C | 0x80,
	'a':  0x07 | 0x80,
	'b':  0x08 | 0x80,
	'e':  0x1B | 0x80,
	'f':  0x0C | 0x80,
	'n':  0x0A | 0x80,
	'r':  0x0D | 0x80,
	't':  0x09 | 0x80,
	'v':  0x0B | 0x80,
}

func Unescape(s string) (unescaped string, ok bool) {
	if len(s) < 2 {
		return "", false
	}
	switch s[0] {
	case '"':
		if s[len(s)-1] == '"' {
			s = s[1 : len(s)-1]
		} else {
			return "", false
		}
	case '\'':
		if s[len(s)-1] == '\'' {
			s = s[1 : len(s)-1]
		} else if (len(s) >= 4) && (s[len(s)-3] == '\'') &&
			((s[len(s)-2] == 'b') || (s[len(s)-2] == 'l')) &&
			(s[len(s)-1] == 'e') { // "be" or "le" suffix.
			s = s[1 : len(s)-3]
		} else {
			return "", false
		}
	default:
		return "", false
	}

	for i := 0; ; i++ {
		if i == len(s) {
			// There were no backslashes.
			return s, true
		}
		if s[i] == '\\' {
			break
		}
	}

	// There were backslashes.
	b := make([]byte, 0, len(s))
	for i := 0; i < len(s); {
		if s[i] != '\\' {
			b = append(b, s[i])
			i += 1
			continue
		} else if i >= (len(s) - 1) {
			// No-op.
		} else if x := backslashes[s[i+1]]; x != 0 {
			b = append(b, x&0x7F)
			i += 2
			continue
		} else if (s[i+1] == 'x') && (i < (len(s) - 3)) {
			u0 := unhex(s[i+2])
			u1 := unhex(s[i+3])
			u := (u0 << 4) | u1
			if 0 <= u {
				b = append(b, uint8(u))
				i += 4
				continue
			}
		} else if (s[i+1] == 'u') && (i < (len(s) - 5)) {
			u0 := unhex(s[i+2])
			u1 := unhex(s[i+3])
			u2 := unhex(s[i+4])
			u3 := unhex(s[i+5])
			u := (u0 << 12) | (u1 << 8) | (u2 << 4) | u3
			if (u >= 0) && utf8.ValidRune(u) {
				e := [utf8.UTFMax]byte{}
				n := utf8.EncodeRune(e[:], u)
				b = append(b, e[:n]...)
				i += 6
				continue
			}
		} else if (s[i+1] == 'U') && (i < (len(s) - 9)) {
			u0 := unhex(s[i+2])
			u1 := unhex(s[i+3])
			u2 := unhex(s[i+4])
			u3 := unhex(s[i+5])
			u4 := unhex(s[i+6])
			u5 := unhex(s[i+7])
			u6 := unhex(s[i+8])
			u7 := unhex(s[i+9])
			u := (u0 << 28) | (u1 << 24) | (u2 << 20) | (u3 << 16) |
				(u4 << 12) | (u5 << 8) | (u6 << 4) | u7
			if (u >= 0) && utf8.ValidRune(u) {
				e := [utf8.UTFMax]byte{}
				n := utf8.EncodeRune(e[:], u)
				b = append(b, e[:n]...)
				i += 10
				continue
			}
		}
		return "", false
	}
	return string(b), true
}

type Map struct {
	byName map[string]ID
	byID   []string
}

func (m *Map) Insert(name string) (ID, error) {
	if name == "" {
		return 0, nil
	}
	if id, ok := builtInsByName[name]; ok {
		return id, nil
	}
	if m.byName == nil {
		m.byName = map[string]ID{}
	}
	if id, ok := m.byName[name]; ok {
		return id, nil
	}

	id := nBuiltInIDs + ID(len(m.byID))
	if id > maxID {
		return 0, errors.New("token: too many distinct tokens")
	}
	m.byName[name] = id
	m.byID = append(m.byID, name)
	return id, nil
}

func (m *Map) ByName(name string) ID {
	if id, ok := builtInsByName[name]; ok {
		return id
	}
	if m.byName != nil {
		return m.byName[name]
	}
	return 0
}

func (m *Map) ByID(x ID) string {
	if x < nBuiltInIDs {
		return builtInsByID[x]
	}
	x -= nBuiltInIDs
	if uint(x) < uint(len(m.byID)) {
		return m.byID[x]
	}
	return ""
}

func unhex(c byte) int32 {
	switch {
	case 'A' <= c && c <= 'F':
		return int32(c) - ('A' - 10)
	case 'a' <= c && c <= 'f':
		return int32(c) - ('a' - 10)
	case '0' <= c && c <= '9':
		return int32(c) - '0'
	}
	return -1
}

func alpha(c byte) bool {
	return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') || (c == '_')
}

func alphaNumeric(c byte) bool {
	return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') || (c == '_') || ('0' <= c && c <= '9')
}

func hexaNumericUnderscore(c byte) bool {
	return ('A' <= c && c <= 'F') || ('a' <= c && c <= 'f') || (c == '_') || ('0' <= c && c <= '9')
}

func zeroOneUnderscore(c byte) bool {
	return (c == '_') || ('0' <= c && c <= '1')
}

func numeric(c byte) bool {
	return ('0' <= c && c <= '9')
}

func numericUnderscore(c byte) bool {
	return (c == '_') || ('0' <= c && c <= '9')
}

func hasPrefix(a []byte, s string) bool {
	if len(s) == 0 {
		return true
	}
	if len(a) < len(s) {
		return false
	}
	a = a[:len(s)]
	for i := range a {
		if a[i] != s[i] {
			return false
		}
	}
	return true
}

// checkNumericUnderscores rejects consecutive or trailing underscores.
func checkNumericUnderscores(a []byte) bool {
	prevUnderscore := false
	for _, c := range a {
		currUnderscore := c == '_'
		if prevUnderscore && currUnderscore {
			return false
		}
		prevUnderscore = currUnderscore
	}
	return !prevUnderscore
}

func Tokenize(m *Map, filename string, src []byte) (tokens []Token, comments []string, retErr error) {
	line := uint32(1)
loop:
	for i := 0; i < len(src); {
		c := src[i]

		if c <= ' ' {
			if c == '\n' {
				if len(tokens) > 0 && tokens[len(tokens)-1].ID.IsImplicitSemicolon(m) {
					tokens = append(tokens, Token{IDSemicolon, line})
				}
				if line == maxLine {
					return nil, nil, fmt.Errorf("token: too many lines in %q", filename)
				}
				line++
			}
			i++
			continue
		}

		if (c == '"') || (c == '\'') {
			quote := c
			j := i + 1
			for j < len(src) {
				c = src[j]
				j++
				if c == quote {
					break
				} else if c == '\\' {
					if quote == '"' {
						return nil, nil, fmt.Errorf("token: backslash in \"-string at %s:%d", filename, line)
					}
				} else if c == '\n' {
					return nil, nil, fmt.Errorf("token: expected final %c in string at %s:%d", quote, filename, line)
				} else if c < ' ' {
					return nil, nil, fmt.Errorf("token: control character in string at %s:%d", filename, line)
				}
			}

			hasEndian := (quote == '\'') && (j < (len(src) - 2)) &&
				((src[j] == 'b') || (src[j] == 'l')) &&
				(src[j+1] == 'e')
			if hasEndian {
				j += 2
			}

			if j-i > maxTokenSize {
				return nil, nil, fmt.Errorf("token: string too long at %s:%d", filename, line)
			}
			s := string(src[i:j])
			if quote == '\'' {
				if unescaped, ok := Unescape(s); !ok {
					return nil, nil, fmt.Errorf("token: invalid '-string at %s:%d", filename, line)
				} else if (len(unescaped) > 1) && !hasEndian {
					return nil, nil, fmt.Errorf("token: multi-byte '-string needs be or le suffix at %s:%d", filename, line)
				}
			}

			id, err := m.Insert(s)
			if err != nil {
				return nil, nil, err
			}
			tokens = append(tokens, Token{id, line})
			i = j
			continue
		}

		if alpha(c) {
			j := i + 1
			for ; j < len(src) && alphaNumeric(src[j]); j++ {
				if j-i == maxTokenSize {
					return nil, nil, fmt.Errorf("token: identifier too long at %s:%d", filename, line)
				}
			}
			id, err := m.Insert(string(src[i:j]))
			if err != nil {
				return nil, nil, err
			}
			tokens = append(tokens, Token{id, line})
			i = j
			continue
		}

		if numeric(c) {
			// TODO: 0b11 binary numbers.
			j, isDigit := i+1, numericUnderscore
			if c == '0' && j < len(src) {
				if next := src[j]; next == 'x' || next == 'X' {
					j, isDigit = j+1, hexaNumericUnderscore
				} else if next == 'b' || next == 'B' {
					j, isDigit = j+1, zeroOneUnderscore
				} else if numeric(next) {
					return nil, nil, fmt.Errorf("token: legacy octal syntax at %s:%d", filename, line)
				}
			}
			for ; j < len(src) && isDigit(src[j]); j++ {
				if j-i == maxTokenSize {
					return nil, nil, fmt.Errorf("token: constant too long at %s:%d", filename, line)
				}
			}
			if !checkNumericUnderscores(src[i:j]) {
				return nil, nil, fmt.Errorf("token: invalid numeric literal at %s:%d", filename, line)
			}
			id, err := m.Insert(string(src[i:j]))
			if err != nil {
				return nil, nil, err
			}
			tokens = append(tokens, Token{id, line})
			i = j
			continue
		}

		if c == '/' && i+1 < len(src) && src[i+1] == '/' {
			h := i
			i += 2
			for ; i < len(src) && src[i] != '\n'; i++ {
			}
			for uint32(len(comments)) < line {
				comments = append(comments, "")
			}
			comments = append(comments, string(src[h:i]))
			continue
		}

		if id := squiggles[c]; id != 0 {
			i++
			tokens = append(tokens, Token{id, line})
			continue
		}
		for _, x := range lexers[c] {
			if hasPrefix(src[i+1:], x.suffix) {
				i += len(x.suffix) + 1
				tokens = append(tokens, Token{x.id, line})
				continue loop
			}
		}

		msg := ""
		if c <= 0x7F {
			msg = fmt.Sprintf("byte '\\x%02X' (%q)", c, c)
		} else {
			msg = fmt.Sprintf("non-ASCII byte '\\x%02X'", c)
		}
		return nil, nil, fmt.Errorf("token: unrecognized %s at %s:%d", msg, filename, line)
	}
	return tokens, comments, nil
}
