# Auxiliary Code

Wuffs is a [memory-safe](/doc/note/memory-safety.md) programming language,
achieving that safety in part because Wuffs code is
[hermetic](/doc/note/hermeticity.md) and doesn't even have the *capability* to
dynamically allocate and free memory. Wuffs code is also transpiled to C (which
is very portable and easy to bind to other, higher-level languages), but C
lacks modern conveniences like a built-in string type that's safe and easy to
use.

Wuffs' C/C++ form (a "single file library") also contains auxiliary C++ code
(in the `wuffs_aux` namespace) that compensates for that. For example, the JSON
decoder that is written in the memory-safe Wuffs language works with low-level
[tokens](/doc/note/tokens.md). The high-level `wuffs_aux::DecodeJson` auxiliary
function instead uses e.g. a `std::string` and a `double` for the JSON inputs
`"foo\tbar"` and `0.3`. This API is more *convenient* for the programmer who
uses Wuffs-the-library, but there are several trade-offs:

- Technically, `0.99999999999999999` and `1.0` are different JSON values, and
  the low-level Wuffs JSON decoder can distinguish them, but the high-level
  `wuffs_aux` JSON decoder will treat them identically (as a C++ `double`).
- The low-level API is more modular and can work with a variety of
  StringToDouble implementations (which converts the string `"0.3"`, unquoted
  in the JSON input, to the number `0.3`). The high-level API always uses
  Wuffs' own StringToDouble implementation, which makes one particular choice
  on the [binary size versus runtime
  performance](https://github.com/google/double-conversion/issues/137)
  frontier. Other choices are viable too, especially when integrating with an
  existing C/C++ project that already uses another StringToDouble library.
- In the worst case (when the JSON input is just one giant string), `wuffs_aux`
  requires `O(N)` memory, where `N` is the input length. In comparison, the
  [example/jsonptr](/example/jsonptr/jsonptr.cc) program, which works with the
  low-level token API, can process arbitrarily long input in `O(1)` memory.
- The auxiliary code is hand-written C++. It's carefully written and there's
  [not a lot of it](/internal/cgen/auxiliary), but unlike code written in the
  Wuffs language, its memory-safety is not enforced by the Wuffs toolchain.
- For simplicity, it assumes that all I/O can be performed synchronously. This
  is trivially true if the input is already entirely in memory (e.g. as a
  `std::string` or a `std::vector`). However, the auxiliary code should not be
  used on e.g. the GUI main thread if it could block on network I/O (and make
  the GUI unresponsive). In such cases, use the low-level Wuffs API instead.

Similarly, decoding an image using the written-in-Wuffs low-level API involves
[multiple steps](/doc/note/memory-safety.md#allocation-free-apis) and the
`wuffs_aux::DecodeImage` high-level API provides something more convenient,
albeit with similar trade-offs.

Grepping the [examples directory](/example) for `wuffs_aux` should reveal code
examples with and without using the auxiliary code library.
