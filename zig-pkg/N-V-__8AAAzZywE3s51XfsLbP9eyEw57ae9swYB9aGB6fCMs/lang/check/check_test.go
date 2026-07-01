// Copyright 2017 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package check

import (
	"bytes"
	"fmt"
	"math/big"
	"reflect"
	"sort"
	"strings"
	"testing"

	"github.com/google/wuffs/lang/builtin"
	"github.com/google/wuffs/lang/parse"
	"github.com/google/wuffs/lang/render"

	a "github.com/google/wuffs/lang/ast"
	t "github.com/google/wuffs/lang/token"
)

func compareToWuffsfmt(tm *t.Map, tokens []t.Token, comments []string, src string) error {
	buf := &bytes.Buffer{}
	if err := render.Render(buf, tm, tokens, comments); err != nil {
		return err
	}
	got := strings.Split(buf.String(), "\n")
	want := strings.Split(src, "\n")
	for line := 1; len(got) != 0 || len(want) != 0; line++ {
		if len(got) == 0 {
			w := strings.TrimSpace(want[0])
			return fmt.Errorf("difference at line %d:\n%s\n%s", line, "", w)
		}
		if len(want) == 0 {
			g := strings.TrimSpace(got[0])
			return fmt.Errorf("difference at line %d:\n%s\n%s", line, g, "")
		}
		if g, w := strings.TrimSpace(got[0]), strings.TrimSpace(want[0]); g != w {
			return fmt.Errorf("difference at line %d:\n%s\n%s", line, g, w)
		}
		got = got[1:]
		want = want[1:]
	}
	return nil
}

func TestCheck(tt *testing.T) {
	const filename = "test.wuffs"
	src := strings.TrimSpace(`
		pri struct foo(
			i : base.i32,
		)

		pri func foo.bar() {
			var x : base.u8
			var y : base.i32
			var z : base.u64[..= 123]
			var a : array[4] base.u8
			var b : base.bool

			var p : base.i32
			var q : base.i32[0 ..= 8]

			x = 0
			x = 1 + (x * 0)
			y = -y - 1
			y = this.i
			b = not true

			y = x as base.i32

			assert true

			while.label p == q,
				pre true,
				inv true,
				post p <> q,
			{
				// Redundant, but shows the labeled jump syntax.
				continue.label
			}.label
		}
	`) + "\n"

	tm := &t.Map{}

	tokens, comments, err := t.Tokenize(tm, filename, []byte(src))
	if err != nil {
		tt.Fatalf("Tokenize: %v", err)
	}

	file, err := parse.Parse(tm, filename, tokens, nil)
	if err != nil {
		tt.Fatalf("Parse: %v", err)
	}

	if err := compareToWuffsfmt(tm, tokens, comments, src); err != nil {
		tt.Fatalf("compareToWuffsfmt: %v", err)
	}

	c, err := Check(tm, []*a.File{file}, nil)
	if err != nil {
		tt.Fatalf("Check: %v", err)
	}

	// There are 2 funcs in this package: the implicit foo.reset method, and
	// the explicit foo.bar method.
	nFuncs := 0
	for qqid := range c.funcs {
		if qqid[0] == 0 {
			nFuncs++
		}
	}
	if nFuncs != 2 {
		tt.Fatalf("c.funcs: got %d elements, want 2", len(c.funcs))
	}

	qqid := t.QQID{0, tm.ByName("foo"), tm.ByName("bar")}
	fooBar := c.funcs[qqid]
	if fooBar == nil {
		tt.Fatalf("c.funcs: no entry for %q", qqid.Str(tm))
	}
	fooBarLocalVars := c.localVars[qqid]
	if fooBarLocalVars == nil {
		tt.Fatalf("c.localVars: no entry for %q", qqid.Str(tm))
	}

	if got, want := fooBar.QQID().Str(tm), "foo.bar"; got != want {
		tt.Fatalf("func name: got %q, want %q", got, want)
	}

	got := [][2]string(nil)
	for id, typ := range fooBarLocalVars {
		got = append(got, [2]string{
			id.Str(tm),
			typ.Str(tm),
		})
	}
	sort.Slice(got, func(i, j int) bool {
		return got[i][0] < got[j][0]
	})

	want := [][2]string{
		{"a", "array[4] base.u8"},
		{"args", "args"},
		{"b", "base.bool"},
		{"coroutine_resumed", "base.bool"},
		{"p", "base.i32"},
		{"q", "base.i32[0 ..= 8]"},
		{"this", "ptr foo"},
		{"x", "base.u8"},
		{"y", "base.i32"},
		{"z", "base.u64[..= 123]"},
	}
	if !reflect.DeepEqual(got, want) {
		tt.Fatalf("\ngot  %v\nwant %v", got, want)
	}
}

func TestConstValues(tt *testing.T) {
	const filename = "test.wuffs"
	testCases := map[string]int64{
		"i = 42": 42,

		"i = +7": +7,
		"i = -7": -7,

		"b = false":         0,
		"b = not false":     1,
		"b = not not false": 0,

		"i = 10  + 3": 13,
		"i = 10  - 3": 7,
		"i = 10  * 3": 30,
		"i = 10  / 3": 3,
		"i = 10 << 3": 80,
		"i = 10 >> 3": 1,
		"i = 10  & 3": 2,
		"i = 10  | 3": 11,
		"i = 10  ^ 3": 9,

		"b = 10 <> 3": 1,
		"b = 10  < 3": 0,
		"b = 10 <= 3": 0,
		"b = 10 == 3": 0,
		"b = 10 >= 3": 1,
		"b = 10  > 3": 1,

		"b = false and true": 0,
		"b = false  or true": 1,
	}

	tm := &t.Map{}
	for s, wantInt64 := range testCases {
		src := "pri func foo() {\n"
		if s[0] == 'b' {
			src += "var b : base.bool\n"
		} else {
			src += "var i : base.i32\n"
		}
		src += s + "\n}\n"

		tokens, _, err := t.Tokenize(tm, filename, []byte(src))
		if err != nil {
			tt.Errorf("%q: Tokenize: %v", s, err)
			continue
		}

		file, err := parse.Parse(tm, filename, tokens, nil)
		if err != nil {
			tt.Errorf("%q: Parse: %v", s, err)
			continue
		}

		c, err := Check(tm, []*a.File{file}, nil)
		if err != nil {
			tt.Errorf("%q: Check: %v", s, err)
			continue
		}

		// There is 1 func in this package: foo.
		nFuncs := 0
		for qqid := range c.funcs {
			if qqid[0] == 0 {
				nFuncs++
			}
		}
		if nFuncs != 1 {
			tt.Errorf("%q: Funcs: got %d elements, want 1", s, len(c.funcs))
			continue
		}

		foo := c.funcs[t.QQID{0, 0, tm.ByName("foo")}]
		if foo == nil {
			tt.Errorf("%q: cannot look up func foo", s)
			continue
		}
		body := foo.Body()
		if len(body) != 2 {
			tt.Errorf("%q: Body: got %d elements, want 1", s, len(body))
			continue
		}
		if body[0].Kind() != a.KVar {
			tt.Errorf("%q: Body[0]: got %s, want %s", s, body[0].Kind(), a.KVar)
			continue
		}
		if body[1].Kind() != a.KAssign {
			tt.Errorf("%q: Body[1]: got %s, want %s", s, body[1].Kind(), a.KAssign)
			continue
		}

		got := body[1].AsAssign().RHS().ConstValue()
		want := big.NewInt(wantInt64)
		if got == nil || want == nil || got.Cmp(want) != 0 {
			tt.Errorf("%q: got %v, want %v", s, got, want)
			continue
		}
	}
}

func TestBitMask(tt *testing.T) {
	testCases := [][2]uint64{
		{0, 0},
		{1, 1},
		{2, 3},
		{3, 3},
		{4, 7},
		{5, 7},
		{50, 63},
		{500, 511},
		{5000, 8191},
		{50000, 65535},
		{500000, 524287},
		{1<<7 - 2, 1<<7 - 1},
		{1<<7 - 1, 1<<7 - 1},
		{1<<7 + 0, 1<<8 - 1},
		{1<<7 + 1, 1<<8 - 1},
		{1<<8 - 2, 1<<8 - 1},
		{1<<8 - 1, 1<<8 - 1},
		{1<<8 + 0, 1<<9 - 1},
		{1<<8 + 1, 1<<9 - 1},
		{1<<32 - 2, 1<<32 - 1},
		{1<<32 - 1, 1<<32 - 1},
		{1<<32 + 0, 1<<33 - 1},
		{1<<32 + 1, 1<<33 - 1},
		{1<<64 - 2, 1<<64 - 1},
		{1<<64 - 1, 1<<64 - 1},
	}

	for _, tc := range testCases {
		got := bitMask(big.NewInt(0).SetUint64(tc[0]).BitLen())
		want := big.NewInt(0).SetUint64(tc[1])
		if got.Cmp(want) != 0 {
			tt.Errorf("roundUpToPowerOf2Minus1(%v): got %v, want %v", tc[0], got, tc[1])
		}
	}
}

func TestBuiltInTypeMap(tt *testing.T) {
	if got, want := len(builtInTypeMap), len(builtin.Types); got != want {
		tt.Fatalf("lengths: got %d, want %d", got, want)
	}
	tm := t.Map{}
	for _, s := range builtin.Types {
		id := tm.ByName(s)
		if id == 0 {
			tt.Fatalf("ID(%q): got 0, want non-0", s)
		}
		if _, ok := builtInTypeMap[id]; !ok {
			tt.Fatalf("no builtInTypeMap entry for %q", s)
		}
	}
}
