# Axiom Listing

This file lists Wuffs' [axioms](/doc/note/assertions.md#axioms): a fixed list
of built-in, self-evident rules to create new facts by combining existing
facts. For example, given numerical-typed expressions `a`, `b` and `c`, the two
facts that `a < c` and `c <= b` together imply that `a < b`: less-than-ness is
transitive. That rule is named by the string "a < b: a < c; c <= b"`.

This file is not just documentation. It is [parsed](/lang/check/gen.go) to give
the list of axioms built into the Wuffs compiler. Run `go generate` after
editing this list.

---

- `"a < b: b > a"`
- `"a < b: a < c; c < b"`
- `"a < b: a < c; c == b"`
- `"a < b: a == c; c < b"`
- `"a < b: a < c; c <= b"`
- `"a < b: a <= c; c < b"`

---

- `"a > b: b < a"`

---

- `"a <= b: a == b"`
- `"a <= b: b >= a"`
- `"a <= b: a <= c; c <= b"`
- `"a <= b: a <= c; c == b"`
- `"a <= b: a == c; c <= b"`

---

- `"a >= b: a == b"`
- `"a >= b: b <= a"`
- `"a >= b: a >= (b + c); 0 <= c"`

---

- `"a < (b + c): a < c; 0 <= b"`
- `"a < (b + c): a < (b0 + c0); b0 <= b; c0 <= c"`

---

- `"a <= (a + b): 0 <= b"`

---

- `"(a + b) <= c: a <= (c - b)"`

---

- `"(a - b) < c: a < c; 0 <= b"`
