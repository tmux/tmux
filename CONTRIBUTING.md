# Code contributions

When this file is silent, match the surrounding tmux code.

## Before making changes

- Ask before starting a large or architectural change.
- Keep each patch as small and self-contained as practical. Separate cleanup,
  refactoring, performance work, and unrelated fixes.
- Preserve compatibility unless the change explicitly requires otherwise.
- Solve the present problem. Do not add speculative options, syntax, APIs,
  wrappers, or abstractions for possible future use.
- Reuse existing tmux mechanisms and cleanup paths rather than adding parallel
  ones.
- Read the complete final diff, not only the commits that produced it, and
  remove unrelated changes.
- Write a capitalized, sentence-style commit subject ending with a period.
  Most commits need no body; add one when useful rationale or context does not
  fit naturally in the subject.

## C style

- Use eight-column tabs for block indentation. Indent continuation lines by
  four additional spaces after any leading indentation tabs. Never put spaces
  before tabs. Keep code under 80 columns. Reshape code, add a useful helper or
  temporary, or shorten an obvious local name rather than forcing awkward
  wrapping.
- Put a space after language keywords (`if (x)`, `switch (x)`), but not before
  function-call parentheses. Space binary operators, not unary operators.
- Put control-statement opening braces on the same line. Omit braces for a
  single-statement body that fits on one physical line. If the body spans
  multiple physical lines or contains multiple statements, use braces.
- Put a function's return type on its own line and its opening brace on the
  next line. Keep `} else` on one line.
- Keep declarations together at the start of the function. Block declarations
  are exceptional. Align variable names after the complete type, following
  nearby tmux declarations.
- Put each structure member on a separate declaration line.
- Use C comments, not `//`. Explanatory prose comments are complete sentences.
  Explain purpose, policy, invariants, or non-obvious constraints; remove
  useless comments rather than narrating straightforward code. Public or
  substantial functions normally have a short header comment.
- Use tmux forms such as `return (value);`, `sizeof *ptr`, and `NULL` for
  pointers. Derive allocation and copy sizes from the destination object.
- Prefer `strlcpy`, `strlcat`, `xsnprintf`, `xasprintf`, `xmalloc`, `xcalloc`,
  and `xreallocarray` as appropriate. Never use `strcpy`.
- In normal tmux C files, include `<sys/types.h>` first. Order the remaining
  includes as other system headers, standard-library headers, then `"tmux.h"`,
  with blank lines between groups.
- Keep private types, data, comparators, and functions in their implementation
  file and make them `static`. Add only genuinely cross-file interfaces to
  `tmux.h`; group prototypes under the implementing file and omit parameter
  names there.
- Prefix nonlocal symbols and callbacks with the owning subsystem. Use tmux's
  established terms and concise, descriptive names.

## Design and implementation

- Put mutable per-object state on the client, session, window, pane, mode, or
  other object that owns it; do not use a function-local `static` merely for
  convenience. Immutable lookup tables, API return storage, and caches with
  deliberately process-wide semantics are different.
- Operate on the explicit target, not an incidental active object. Do not add a
  fallback that hides an impossible state; fix the invariant or fail clearly.
- Prefer early validation and returns to deeply nested control flow.
- Use direct collection operations such as `RB_FIND` and safe traversal macros
  rather than open-coded searches or deletion loops.
- Keep structures compact, especially frequently allocated cells and entries.
  Do not widen fields or duplicate state without a demonstrated need.
- Avoid trivial forwarding wrappers and unnecessary parallel implementations.
  Inline simple work or unify existing paths when doing so makes ownership and
  behavior clearer.
- Keep APIs shaped around actual callers. Pass the object or enum actually
  needed, not a loosely related object or an `int` that conceals the type.
- Cache repeated work in hot paths, but do not introduce derived state merely
  for hypothetical performance.
- Check fallible library calls. When a command reports an error with
  `cmdq_error`, return `CMD_RETURN_ERROR`. Make ownership of allocated `cause`
  strings and other resources explicit, and reuse an existing cleanup path when
  it naturally owns them.

## Commands, documentation, and tests

- Keep usage strings exact: required and optional arguments must match the
  implementation, and arguments are not written as `<argument>`.
- Document every user-visible command, flag, option, format, and behavior in
  `tmux.1`. Follow nearby semantic mdoc usage, including macros such as `.Nm`,
  `.Ic`, `.Fl`, `.Ar`, and `.Ql`. Normally put each sentence on its own source
  line and start a new paragraph when the subject changes.
- Use one precise term for each concept. Prefer plain wording and describe
  behavior rather than marketing or suggested use cases.
- Keep declarative tables, registries, command lists, and flag strings sorted
  where the surrounding code does.
- Run `git diff --check` before submitting.
- Build without warnings and run the relevant regression tests. For behavior
  changes, add a focused regression when practical. Cover applicable error,
  boundary, and cleanup paths without broadening the patch.
- New C source and header files normally need the ISC header. For regression
  scripts, generated files, imported compatibility code, and other file types,
  follow neighboring files. Add your name and email to a file's copyright
  header after substantial copyrightable changes, not for a few lines.

## AI assistance

AI assistance from GitHub, Anthropic, OpenAI, Google, and DeepSeek requires no
special disclosure.

For any other provider, disclose its name and product at the top of the
pull-request description and link to a public statement that the provider
does not assert copyright in the output or assigns its rights to the user.
Without such a statement, AI may assist with analysis or test planning but may
produce only plainly noncopyrightable changes.
