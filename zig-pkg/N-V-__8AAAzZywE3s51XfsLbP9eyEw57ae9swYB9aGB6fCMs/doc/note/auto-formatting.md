# Auto-Formatting

Within this repository:
- Hand-written `C` code is formatted by `clang-format-9 -style=Chromium`.
- Auto-generated `C` code is formatted by [`dumbindent`](/cmd/dumbindent),
  which isn't as 'pretty' in some sense, but is [substantially
  faster](https://github.com/google/wuffs/blob/f935120aa6aef88b5f5fc04784f89d90dd901921/cmd/dumbindent/main.go#L35-L51),
  making the edit-compile-run cycle more productive.
- `Go` code is formatted by `gofmt`.
- `Wuffs` code is formatted by [`wuffsfmt`](/cmd/wuffsfmt).

Some C code has empty `//` line-comments, which look superfluous at first, but
force clang-format to break the line. This ensures one element per line (in a
long list) or having a function's name (not just its type) start a line. For
example, `grep ^wuffs_base__utf_8_ release/c/wuffs-unsupported-snapshot.c` (and
note the `^`) gives you a rough overview of Wuffs' UTF-8 related functions,
because of the forced line breaks:

```
size_t  //
wuffs_base__utf_8__encode(wuffs_base__slice_u8 dst, uint32_t code_point);
```
