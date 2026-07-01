// Copyright 2020 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package dumbindent

import (
	"os"
	"testing"
)

func TestFormatBytes(tt *testing.T) {
	testCases := []struct {
		src  string
		want string
	}{{
		// Leading and trailing space.
		src:  "\t\tx y  \n",
		want: "x y\n",
	}, {
		// Braces.
		src:  "foo{\nbar\n    }\nbaz\n",
		want: "foo{\n  bar\n}\nbaz\n",
	}, {
		// Braces with initial indent, giving every output line +3 spaces.
		src:  "   foo{\nbar\n    }\nbaz\n",
		want: "   foo{\n     bar\n   }\n   baz\n",
	}, {
		// Parentheses.
		src:  "i = (j +\nk)\np = q\n",
		want: "i = (j +\n    k)\np = q\n",
	}, {
		// Hanging assignment.
		src:  "int x =\ny;\n",
		want: "int x =\n    y;\n",
	}, {
		// Hanging assignment with slash-slash comment.
		src:  "int x = // comm.\ny;\n",
		want: "int x = // comm.\n    y;\n",
	}, {
		// Consecutive blank lines.
		src:  "f {\n\n\ng;\n\n\nh;\n\n\n}\n\n",
		want: "f {\n\n\n  g;\n\n\n  h;\n\n\n}\n",
	}, {
		// Single-quote string.
		src:  "a = '{'\nb = {\nc = 0\n",
		want: "a = '{'\nb = {\n  c = 0\n",
	}, {
		// Double-quote string.
		src:  "a = \"{\"\nb = {\nc = 0\n",
		want: "a = \"{\"\nb = {\n  c = 0\n",
	}, {
		// Back-tick string.
		src:  "a['key'] = `{\n\n\nX` ; \nb = {\nc = 0\n",
		want: "a['key'] = `{\n\n\nX` ;\nb = {\n  c = 0\n",
	}, {
		// Slash-star comment.
		src:  "\n   a['key'] = /*{\n\n\nX*/ ; \nb = {\nc = 0\n",
		want: "a['key'] = /*{\n\n\nX*/ ;\nb = {\n  c = 0\n",
	}, {
		// Nested blocks with label.
		src:  "if (b) {\nlabel:\nswitch (i) {\ncase 0:\nj = k\nbreak;\n}\n}\n",
		want: "if (b) {\n  label:\n  switch (i) {\n    case 0:\n    j = k\n    break;\n  }\n}\n",
	}, {
		// One-liner if statement.
		src:  "if (x) { goto fail; }\n",
		want: "if (x) { goto fail; }\n",
	}, {
		// Leading blank lines.
		src:  "\n\n\n  x = y;",
		want: "x = y;\n",
	}, {
		// Namespaces.
		src:  "namespace A {\nint f() {\nreturn 0;\n}\n}\n",
		want: "namespace A {\nint f() {\n  return 0;\n}\n}\n",
	}, {
		// No break between "{" and "//".
		src:  "if (b) {  // Blah.\nreturn;\n",
		want: "if (b) {  // Blah.\n  return;\n",
	}, {
		// Simple compound literal.
		src:  "T x = {0};",
		want: "T x = {0};\n",
	}}

	for i, tc := range testCases {
		if got := string(FormatBytes(nil, []byte(tc.src), nil)); got != tc.want {
			tt.Fatalf("i=%d, src=%q:\ngot  %q\nwant %q", i, tc.src, got, tc.want)
		}
	}
}

func TestTabs(tt *testing.T) {
	const src = "a {\nb\n}\n"
	got := string(FormatBytes(nil, []byte(src), &Options{Tabs: true}))
	want := "a {\n\tb\n}\n"
	if got != want {
		tt.Fatalf("\ngot  %q\nwant %q", got, want)
	}
}

func ExampleFormatBytes() {
	const src = `
for (i = 0; i < 3; i++) {
j = 0;  // Ignore { in a comment.
if (i < j) { foo(); }
u = (v +
w);
}
`

	os.Stdout.Write(FormatBytes(nil, []byte(src), nil))

	// Output:
	// for (i = 0; i < 3; i++) {
	//   j = 0;  // Ignore { in a comment.
	//   if (i < j) { foo(); }
	//   u = (v +
	//       w);
	// }
}
