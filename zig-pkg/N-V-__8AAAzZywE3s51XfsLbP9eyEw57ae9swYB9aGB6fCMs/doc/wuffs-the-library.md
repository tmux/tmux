# Wuffs the Library

The Wuffs project is both a programming language and a standard library written
in that programming language (and then e.g. transpiled to C). For more details
on the former, see the [Wuffs the Language](/doc/wuffs-the-language.md)
document. As for Wuffs the Library (also known as "std"):

- Source code is in [`/std`](/std).
- API documentation is in [`/doc/std`](/doc/std).
- Example programs are in [`/example`](/example). A simple way to get started
  is to run the `build-example.sh` script in the top level directory.
- Unit tests are in [`/test`](/test).
- Fuzz tests are in [`/fuzz`](/fuzz).

It is perfectly feasible to use Wuffs the Library as a C library, without
depending on Wuffs the Language tools (such as its compiler). Although the
standard library's source code is in Wuffs the Language, the transpiled form is
also checked into the repository. Using the standard library's C form would be
like using any other third party C library. It's just not hand-written C.

To do so, you need to download only one file, from the
[`/release/c`](/release/c) directory. Wuffs the Library's C form ships as a
[single file C
library](https://github.com/nothings/stb/blob/master/docs/stb_howto.txt),
targeting the C99 standard. That file acts as either a traditional `.c` or `.h`
file depending on the presence or absence of a `WUFFS_IMPLEMENTATION` macro
definition.

Most of the example programs treat it as a `.c` file. The
[`/example/toy-genlib`](/example/toy-genlib) program treats it as a `.h` file,
and requires a separate step (running `wuffs genlib` beforehand) to build the
library implementation (a `libwuffs.a` or `libwuffs.so` file).
