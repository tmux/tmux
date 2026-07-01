# Changelog


## Work In Progress

The LICENSE has changed from a single license (Apache 2) to a dual license
(Apache 2 or MIT, at your option).

- Added `base.bitvec256`.
- Added `base.optional_u63`.
- Added `base.hasher_bitvec256`.
- Added `base.hasher_u32` `update!` and `checksum_u32` methods.
- Added `base.hasher_u64`.
- Added `compact_retaining` and `dst_history_retain_length`.
- Added `example/toy-aux-image`.
- Added `example/mzcat`.
- Added `get_quirk(key: u32) u64`.
- Added `std/crc64`.
- Added `std/etc2`.
- Added `std/jpeg`.
- Added `std/lzip`.
- Added `std/lzma`.
- Added `std/netpbm`.
- Added `std/qoi`.
- Added `std/sha256`.
- Added `std/thumbhash`.
- Added `std/vp8`.
- Added `std/webp`.
- Added `std/xxhash32`.
- Added `std/xxhash64`.
- Added `std/xz`.
- Added `WUFFS_BASE__QUIRK_QUALITY`.
- Added `WUFFS_CONFIG__DISABLE_MSVC_CPU_ARCH__X86_64_FAMILY`.
- Added `WUFFS_CONFIG__DST_PIXEL_FORMAT__ENABLE_ALLOWLIST`.
- Added `WUFFS_CONFIG__ENABLE_DROP_IN_REPLACEMENT__STB`.
- Added `WUFFS_CONFIG__ENABLE_MSVC_CPU_ARCH__X86_64_V2`.
- Added `WUFFS_CONFIG__ENABLE_MSVC_CPU_ARCH__X86_64_V3`.
- Added `wuffs_base__status__is_truncated_input_error`.
- Changed `lzw.set_literal_width` to `lzw.set_quirk`.
- Changed `set_quirk_enabled!(quirk: u32, enabled: bool)` to `set_quirk!(key:
  u32, value: u64) status`.
- Deprecated `std/lzw.decoder.flush`.
- Fixed `PIXEL_FORMAT__YA_{NON,}PREMUL` constant values.
- Generated constants now default to unsigned.
- Halved the sizeof `wuffs_foo__bar::unique_ptr`.
- Let `std/png` decode PNG color type 4 to `PIXEL_FORMAT__YA_NONPREMUL` (two
  channels) instead of `PIXEL_FORMAT__BGRA_NONPREMUL` (four channels).
- Reassigned `lib/base38` alphabet and numbers.
- Removed the `std/gif -> std/lzw` dependency.
- Removed `endwhile` keyword.
- Removed `example/bzcat`.
- Renamed `std/tga` to `std/targa`.
- Set image decoder pixel width and height inclusive maximum to `0xFF_FFFF`,
  down from `0x7FFF_FFFF`.

The dot points below probably aren't of interest unless you're _writing_ Wuffs
code (instead of writing C/C++ code that _uses_ Wuffs' standard library).

- Added `if.likely` and `if.unlikely`.
- Added `io_forget_history`.
- Added `slice_var as nptr array[etc] etc` conversion.
- Added read-only type decorators: `roarray`, `roslice` and `rotable`.
- Banned recursive function calls.
- Renamed `base` `min/max` argument from `a` to `no_more/less_than`.
- Wuffs struct private data now needs a "+" between the "()" pairs.
- `wuffsfmt` double-indents hanging lines and each indent is now 4 spaces (not
  a tab).


## 2024-04-19 version 0.3.4

- Have `std/png` ignore tRNS chunks for color types 4 (YA) and 6 (RGBA).


## 2023-04-08 version 0.3.3

The `wuffs_base__parse_number_f64` function has been further optimized.


## 2023-04-07 version 0.3.2

The `std/bmp` and `std/nie` image decoders' `decode_frame` method now allow
decoding to a pixel buffer that's smaller than the source image. This makes
these two image decoders consistent with the other ones in `std`.


## 2023-03-04 version 0.3.1

For a *closed* `io_reader`, the standard library now returns `"#truncated
input"` instead of `"$short read"`. Importantly, this is an error, not a
suspension.

The `wuffs_base__parse_number_f64` function's Simple Decimal Conversion
fallback algorithm has been optimized.


## 2023-01-26 version 0.3.0

The headline feature is that we have a production quality PNG decoder. It's
also [the fastest, safest PNG decoder in the
world](https://nigeltao.github.io/blog/2021/fastest-safest-png-decoder.html).
There's also a [memory-safe, zero-allocation JSON
decoder](https://nigeltao.github.io/blog/2020/jsonptr.html).

The dot points below probably aren't of interest unless you're upgrading from
Wuffs version 0.2.

- Added `0b` prefixed binary numbers.
- Added `WUFFS_BASE__PIXEL_BLEND__SRC_OVER`.
- Added `WUFFS_BASE__PIXEL_FORMAT__BGR_565`.
- Added `WUFFS_CONFIG__MODULE__BASE__ETC` sub-modules.
- Added `auxiliary` code.
- Added `base` library support for UTF-8.
- Added `base` library support for `atoi`-like string conversion.
- Added `choose` and `choosy`.
- Added `cpu_arch`.
- Added `doc/logo`.
- Added `endwhile` syntax.
- Added `example/bzcat`.
- Added `example/cbor-to-json`.
- Added `example/convert-to-nia`.
- Added `example/imageviewer`.
- Added `example/json-to-cbor`.
- Added `example/jsonfindptrs`.
- Added `example/jsonptr`.
- Added `example/sdl-imageviewer`.
- Added `slice base.u8 peek/poke` methods.
- Added `std/bmp`.
- Added `std/bzip2`.
- Added `std/cbor`.
- Added `std/json`.
- Added `std/nie`.
- Added `std/png`.
- Added `std/tga`.
- Added `std/wbmp`.
- Added `tell_me_more?` mechanism.
- Added SIMD.
- Added alloc functions.
- Added colons to const syntax.
- Added double-curly blocks.
- Added interfaces.
- Added iterate advance parameter.
- Added single-quoted strings.
- Added slice `uintptr_low_12_bits` method.
- Added tokens.
- Changed `gif.decoder_workbuf_len_max_incl_worst_case` from 1 to 0.
- Changed default C compilers from `clang-5.0,gcc` to `clang,gcc`.
- Changed the C formatting style; removed the `-cformatter` flag.
- Changed what the `std/gif` benchmarks actually measure.
- Made `wuffs_base__pixel_format` a struct.
- Made `wuffs_base__pixel_subsampling` a struct.
- Made `wuffs_base__status` a struct.
- Prohibited iterate loops inside coroutines.
- Removed `ack_metadata_chunk?`.
- Removed `wuffs_base__frame_config__blend`.
- Renamed I/O `available` methods to `length`.
- Renamed `decode_io_writer?` methods to `transform_io?`.
- Renamed `example/library` to `example/toy-genlib`.
- Renamed `load` and `store` to `peek` and `poke`.
- Renamed `{read,writ}er_io_position` to `{read,writ}er_position`.
- Renamed `set_ignore_checksum!` as a quirk.
- Renamed `swizzle_interleaved!` to `swizzle_interleaved_from_slice!`.
- Renamed warnings to notes.


## 2019-12-19 version 0.2.0

The headline feature is that the GIF decoder is now of production quality.
There is now API for overall metadata (e.g. ICCP color profiles) and to
recreate each frame (width, height, BGRA pixels, timing, etc.) of a GIF
animation, instead of version 0.1's proof-of-concept GIF decoder API, which
just gave you a one-dimensional stream of palette indexes. It also now accepts
a variety of GIF images that are invalid, when strictly following the GIF
specifiction, but are nonetheless accepted by other real world GIF
implementations. The Wuffs GIF decoder has also been optimized to be about 1.5x
faster than Wuffs version 0.1 and about 2x faster than giflib (the C library).

The Wuffs GIF decoder is being trialled by Skia, the 2-D graphics library used
by both the Android operating system and the Chromium web browser.

Work also proceeds on the NIE and RAC file formats, but both are still
experimental and may change later in backwards incompatible ways.

The dot points below probably aren't of interest unless you're upgrading from
Wuffs version 0.1.

- Renamed Puffs to Wuffs.
- Ship as a "single file C library"
- Added a `skipgendeps` flag.
- Added a `nullptr` literal and `nptr T` type.
- Added `io_bind` and `io_limit` keywords.
- Added a `use` keyword.
- Added a `yield` keyword.
- Made the `return` value mandatory; added `ok` literal.
- Restricted `var` statements to the top of functions.
- Dropped the `= RHS` out of `var x T = RHS`.
- Changed func out-type from struct to bare type.
- Renamed the implicit `in` variable to `args`.
- Added an implicit `coroutine_resumed` variable.
- Added `std/adler32`, `std/crc32` and `std/gzip`.
- Added `std/gif` quirks.
- Spun `std/lzw` out of `std/gif`.
- Spun `std/zlib` out of `std/flate`.
- Let the `std/gzip` and `std/zlib` decoder ignore checksums.
- Renamed `std/flate` to `std/deflate`.
- Renamed `!=` to `<>`; `!` is now only for impure functions.
- Renamed `~+` to `~mod+`; added `~mod-`, `~sat+` and `~sat-`.
- Removed `&^`.
- Renamed `$(etc)` to `[etc]`.
- Renamed `[i..j]` to `[i ..= j]`, consistent with Rust syntax.
- Renamed `[i:j]` to `[i .. j]`, consistent with Rust syntax.
- Renamed `x T` to `x: T`, consistent with Rust syntax.
- Renamed `[N] T` and `[] T` types to `array[N] T` and `slice T`.
- Renamed `while:label` to `while.label`.
- Renamed `u32`, `buf1`, etc to `base.u32`, `base.io_buffer`, etc.
- Renamed `unread_u8?` to `undo_byte!`; added `can_undo_byte`.
- Renamed some `decode?` methods to `decode_io_writer?`.
- Replaced `= try foo` with `=? foo`.
- Prohibited effect-ful subexpressions.
- Redesigned iterate blocks.
- Added `{frame,image,pixel}_config` and `pixel_buffer` types.
- Added a `reset` method.
- Added `peek_uxx`, `skip_fast` and `write_fast_uxx` methods.
- Renamed some `read_uxx` methods as `read_uxx_as_uyy`.
- Report image metadata such as ICCP and XMP.
- Added I/O positions.
- Tweaked how I/O marks and limits work.
- Tweaked `io_buffer` / `io_reader` distinction in C and Wuffs.
- Added extra fields (uninitialized internal buffers) to structs.
- Supported animated (not just single frame) and interlaced GIFs.
- Marked some internal status codes as private.
- Removed closed-for-read/write built-in status codes.
- Changed the string messages for built-in status codes.
- Changed `error "foo"` to `"#foo"` or `base."#bar"`.
- Added warnings as another status code category.
- Made the status type a `const char *`, not an `int32_t`.
- Disallowed `__double_underscore` prefixed names.
- Added fuzz tests.
- Added `WUFFS_CONFIG__MODULES`.
- Added `WUFFS_CONFIG__STATIC_FUNCTIONS`.
- Added some C++ convenience methods.
- Added some Go and Rust benchmarks.
- Made struct implementations private (opaque pointers).
- Sped up the `mimic_deflate_xxx` benchmarks.
- Fix compile errors under MSVC (Microsoft Visual C/C++).
- Moved the base38 package from `lang/base38` to `lib/base38`.
- Added a stand-alone `lib/interval` package.
- Added NIE file format spec.
- Added RAC file format spec and Go implementation.


## 2017-11-16 version 0.1.0

- [Initial open source
  release](https://groups.google.com/d/topic/puffslang/2z61mNTAMns/discussion),
  under the "Puffs" name.


---

Updated on April 2023.
