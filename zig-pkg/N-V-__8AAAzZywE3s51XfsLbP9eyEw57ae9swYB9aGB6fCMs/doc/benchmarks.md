# Benchmarks

`wuffs bench -mimic` summarized throughput numbers for various codecs are
below. Higher is better.

"Mimic" tests check that Wuffs' output mimics (i.e. exactly matches) other
libraries' output. "Mimic" benchmarks give the numbers for those other
libraries, as shipped with Debian. These were measured on a Debian Bullseye
system as of September 2022, which meant these compiler versions:

- clang/llvm 11.0.1
- gcc 10.2.1

and these popular "mimic" library versions, all written in C:

- libbz2 1.0.8
- libgif 5.1.9
- libpng 1.6.37
- zlib 1.2.11 (we'll call this "ztl" or "zlib the library", as opposed to "zlib
  the format")

and these alternative "mimic" library versions, again all written in C:

- [libdeflate](https://github.com/ebiggers/libdeflate) 1.7.1
- [libspng](https://github.com/randy408/libspng) 0.7.3
- [lodepng](https://github.com/lvandeve/lodepng) 20220717
- [miniz](https://github.com/richgel999/miniz) 2.2.0
- [stb](https://github.com/nothings/stb) 2.27

Unless otherwise stated, the numbers below were taken as of Wuffs git commit
315b2e52 "wuffs gen -version=0.3.0-rc.1", the first Wuffs v0.3 release
candidate. As for the CPU model:

    $ cat /proc/cpuinfo | grep model.name | uniq
    model name: Intel(R) Core(TM) i5-10210U CPU @ 1.60GHz


## Reproducing

The benchmark programs aim to be runnable "out of the box" without any
configuration or installation. For example, to run the `std/zlib` benchmarks:

    git clone https://github.com/google/wuffs.git
    cd wuffs
    gcc -O3 test/c/std/zlib.c
    ./a.out -bench
    rm a.out

A comment near the top of that `.c` file says how to run the mimic benchmarks.

The output of those benchmark programs is compatible with the
[benchstat](https://godoc.org/golang.org/x/perf/cmd/benchstat) tool. For
example, that tool can calculate confidence intervals based on multiple
benchmark runs, or calculate p-values when comparing numbers before and after a
code change. To install it, first install Go, then run `go install
golang.org/x/perf/cmd/benchstat`.


## wuffs bench

As mentioned above, individual benchmark programs can be run manually. However,
the canonical way to run the benchmarks (across multiple compilers and multiple
packages like GIF and PNG) for Wuffs' standard library is to use the `wuffs`
command line tool, as it will also re-generate (transpile) the C code whenever
you edit the `std/*/*.wuffs` code. Running `go install -v
github.com/google/wuffs/cmd/...` will install the Wuffs tools. After that, you
can say

    wuffs bench

or

    wuffs bench -mimic std/deflate

or

    wuffs bench -ccompilers=gcc -reps=3 -focus=wuffs_gif_decode_20k std/gif


## Clang versus GCC

On some of the benchmarks below, clang performs noticeably worse (e.g. 1.3x
slower) than gcc, on the same C code. A relatively simple reproduction was
filed as [LLVM bug 35567](https://bugs.llvm.org/show_bug.cgi?id=35567).


## CPU Scaling

CPU power management can inject noise in benchmark times. On a Linux system,
power management can be controlled with:

    # Query.
    cpupower --cpu all frequency-info --policy
    # Turn on.
    sudo cpupower frequency-set --governor powersave
    # Turn off.
    sudo cpupower frequency-set --governor performance


---

# Adler-32

The `1k`, `10k`, etc. numbers are approximately how many bytes are hashed.

    name                          speed      vs_mimic

    wuffs_adler32_10k/clang11     22.3 GB/s   6.4x
    wuffs_adler32_100k/clang11    26.4 GB/s   7.5x

    wuffs_adler32_10k/gcc10       21.9 GB/s   6.3x
    wuffs_adler32_100k/gcc10      22.4 GB/s   6.4x

    mimicztl_adler32_10k           3.50 GB/s  1.0x
    mimicztl_adler32_100k          3.52 GB/s  1.0x

    mimiclibdeflate_adler32_10k   50.4 GB/s  14.4x
    mimiclibdeflate_adler32_100k  49.4 GB/s  14.0x


# Bzip2

The `1k`, `10k`, etc. numbers are approximately how many bytes there are in the
decoded output.

    name                             speed      vs_mimic

    wuffs_bzip2_decode_10k/clang11   63.0 MB/s  1.86x
    wuffs_bzip2_decode_100k/clang11  48.9 MB/s  1.62x

    wuffs_bzip2_decode_10k/gcc10     61.1 MB/s  1.81x
    wuffs_bzip2_decode_100k/gcc10    49.1 MB/s  1.62x

    mimic_bzip2_decode_10k           33.8 MB/s  1.00x
    mimic_bzip2_decode_100k          30.2 MB/s  1.00x


# CRC-32

The `1k`, `10k`, etc. numbers are approximately how many bytes are hashed.

    name                             speed      vs_mimic

    wuffs_crc32_ieee_10k/clang11     14.7 GB/s   9.1x
    wuffs_crc32_ieee_100k/clang11    21.6 GB/s  13.3x

    wuffs_crc32_ieee_10k/gcc10       14.9 GB/s   9.2x
    wuffs_crc32_ieee_100k/gcc10      23.8 GB/s  14.7x

    mimicztl_crc32_ieee_10k           1.62 GB/s  1.0x
    mimicztl_crc32_ieee_100k          1.62 GB/s  1.0x

    mimiclibdeflate_crc32_ieee_10k   24.6 GB/s  15.2x
    mimiclibdeflate_crc32_ieee_100k  25.4 GB/s  15.7x


# Deflate

The `1k`, `10k`, etc. numbers are approximately how many bytes there are in the
decoded output.

The `full_init` vs `part_init` suffixes are whether
[`WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED`](/doc/note/initialization.md#partial-zero-initialization)
is unset or set.

    name                                                speed     vs_mimic

    wuffs_deflate_decode_1k_full_init/clang11           195 MB/s  0.81x
    wuffs_deflate_decode_1k_part_init/clang11           226 MB/s  0.93x
    wuffs_deflate_decode_10k_full_init/clang11          409 MB/s  1.41x
    wuffs_deflate_decode_10k_part_init/clang11          418 MB/s  1.47x
    wuffs_deflate_decode_100k_just_one_read/clang11     521 MB/s  1.48x
    wuffs_deflate_decode_100k_many_big_reads/clang11    330 MB/s  1.19x

    wuffs_deflate_decode_1k_full_init/gcc10             183 MB/s  0.76x
    wuffs_deflate_decode_1k_part_init/gcc10             217 MB/s  0.90x
    wuffs_deflate_decode_10k_full_init/gcc10            402 MB/s  1.39x
    wuffs_deflate_decode_10k_part_init/gcc10            414 MB/s  1.43x
    wuffs_deflate_decode_100k_just_one_read/gcc10       522 MB/s  1.48x
    wuffs_deflate_decode_100k_many_big_reads/gcc10      330 MB/s  1.19x

    mimicztl_deflate_decode_1k_full_init                241 MB/s  1.00x
    mimicztl_deflate_decode_10k_full_init               289 MB/s  1.00x
    mimicztl_deflate_decode_100k_just_one_read          352 MB/s  1.00x
    mimicztl_deflate_decode_100k_many_big_reads         277 MB/s  1.00x

    mimiclibdeflate_deflate_decode_1k_full_init         326 MB/s  1.35x
    mimiclibdeflate_deflate_decode_10k_full_init        503 MB/s  1.74x
    mimiclibdeflate_deflate_decode_100k_just_one_read   507 MB/s  1.44x

    mimicminiz_deflate_decode_1k_full_init              204 MB/s  0.85x
    mimicminiz_deflate_decode_10k_full_init             252 MB/s  0.87x
    mimicminiz_deflate_decode_100k_just_one_read        287 MB/s  0.82x

    go_deflate_decode_1k_full_init                       71 MB/s  0.29x
    go_deflate_decode_10k_full_init                     120 MB/s  0.42x
    go_deflate_decode_100k_just_one_read                135 MB/s  0.38x

    rust_deflate_decode_1k_full_init                    165 MB/s  0.68x
    rust_deflate_decode_10k_full_init                   259 MB/s  0.90x
    rust_deflate_decode_100k_just_one_read              272 MB/s  0.77x

To reproduce the libdeflate or miniz numbers, look in
`test/c/mimiclib/deflate-gzip-zlib.c`. For Go 1.19, run `go run main.go` in
`script/bench-go-deflate`. For Rust 1.48 / flate2 1.0.24 / miniz\_oxide 0.5.3,
run `cargo run --release` in `script/bench-rust-deflate`.

Historical (Wuffs v0.2; 2019) numbers for 32-bit ARMv7 (2012 era Samsung Exynos
5 Chromebook), Debian Stretch (2017):

    name                                             speed     vs_mimic

    wuffs_deflate_decode_1k_full_init/clang5         30.4MB/s  0.60x
    wuffs_deflate_decode_1k_part_init/clang5         37.9MB/s  0.74x
    wuffs_deflate_decode_10k_full_init/clang5        72.8MB/s  0.81x
    wuffs_deflate_decode_10k_part_init/clang5        76.2MB/s  0.85x
    wuffs_deflate_decode_100k_just_one_read/clang5   96.5MB/s  0.82x
    wuffs_deflate_decode_100k_many_big_reads/clang5  81.1MB/s  0.90x

    wuffs_deflate_decode_1k_full_init/gcc6           31.6MB/s  0.62x
    wuffs_deflate_decode_1k_part_init/gcc6           39.9MB/s  0.78x
    wuffs_deflate_decode_10k_full_init/gcc6          69.6MB/s  0.78x
    wuffs_deflate_decode_10k_part_init/gcc6          72.4MB/s  0.81x
    wuffs_deflate_decode_100k_just_one_read/gcc6     87.3MB/s  0.74x
    wuffs_deflate_decode_100k_many_big_reads/gcc6    73.8MB/s  0.82x

    mimic_deflate_decode_1k                          51.0MB/s  1.00x
    mimic_deflate_decode_10k                         89.7MB/s  1.00x
    mimic_deflate_decode_100k_just_one_read           118MB/s  1.00x
    mimic_deflate_decode_100k_many_big_reads         90.0MB/s  1.00x


# GIF

The `1k`, `10k`, etc. numbers are approximately how many pixels there are in
the decoded image. For example, the `test/data/harvesters.*` images are 1165 ×
859, approximately 1000k pixels.

The `bgra` vs `indexed` suffixes are whether to decode to 4 bytes (BGRA or
RGBA) or 1 byte (a palette index) per pixel, even if the underlying file format
gives 1 byte per pixel.

The `full_init` vs `part_init` suffixes are whether
[`WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED`](/doc/note/initialization.md#partial-zero-initialization)
is unset or set.

The libgif library doesn't export any API for decode-to-BGRA or decode-to-RGBA,
so there are no mimic numbers to compare to for the `bgra` suffix.

    name                                         speed      vs_mimic

    wuffs_gif_decode_1k_bw/clang11                758 MB/s  4.38x
    wuffs_gif_decode_1k_color_full_init/clang11   176 MB/s  1.96x
    wuffs_gif_decode_1k_color_part_init/clang11   213 MB/s  2.37x
    wuffs_gif_decode_10k_bgra/clang11             786 MB/s  n/a
    wuffs_gif_decode_10k_indexed/clang11          208 MB/s  1.98x
    wuffs_gif_decode_20k/clang11                  262 MB/s  2.47x
    wuffs_gif_decode_100k_artificial/clang11      580 MB/s  3.52x
    wuffs_gif_decode_100k_realistic/clang11       225 MB/s  2.18x
    wuffs_gif_decode_1000k_full_init/clang11      229 MB/s  2.16x
    wuffs_gif_decode_1000k_part_init/clang11      229 MB/s  2.16x
    wuffs_gif_decode_anim_screencap/clang11      1.31 GB/s  6.58x

    wuffs_gif_decode_1k_bw/gcc10                  659 MB/s  3.80x
    wuffs_gif_decode_1k_color_full_init/gcc10     175 MB/s  1.94x
    wuffs_gif_decode_1k_color_part_init/gcc10     202 MB/s  2.24x
    wuffs_gif_decode_10k_bgra/gcc10               768 MB/s  n/a
    wuffs_gif_decode_10k_indexed/gcc10            202 MB/s  1.92x
    wuffs_gif_decode_20k/gcc10                    250 MB/s  2.36x
    wuffs_gif_decode_100k_artificial/gcc10        564 MB/s  3.42x
    wuffs_gif_decode_100k_realistic/gcc10         217 MB/s  2.11x
    wuffs_gif_decode_1000k_full_init/gcc10        221 MB/s  2.08x
    wuffs_gif_decode_1000k_part_init/gcc10        221 MB/s  2.08x
    wuffs_gif_decode_anim_screencap/gcc10        1.31 GB/s  6.58x

    mimic_gif_decode_1k_bw                        173 MB/s  1.00x
    mimic_gif_decode_1k_color_full_init            90 MB/s  1.00x
    mimic_gif_decode_10k_indexed                  105 MB/s  1.00x
    mimic_gif_decode_20k                          106 MB/s  1.00x
    mimic_gif_decode_100k_artificial              165 MB/s  1.00x
    mimic_gif_decode_100k_realistic               103 MB/s  1.00x
    mimic_gif_decode_1000k_full_init              106 MB/s  1.00x
    mimic_gif_decode_anim_screencap               199 MB/s  1.00x

    go_gif_decode_1k_bw                           167 MB/s  0.97x
    go_gif_decode_1k_color_full_init               63 MB/s  0.70x
    go_gif_decode_10k_bgra                        216 MB/s  n/a
    go_gif_decode_10k_indexed                      92 MB/s  0.88x
    go_gif_decode_20k                             102 MB/s  0.96x
    go_gif_decode_100k_artificial                 202 MB/s  1.22x
    go_gif_decode_100k_realistic                  102 MB/s  0.99x
    go_gif_decode_1000k_full_init                 103 MB/s  0.97x
    go_gif_decode_anim_screencap                  237 MB/s  1.19x

    rust_gif_decode_1k_bw                         362 MB/s  2.09x
    rust_gif_decode_1k_color_full_init            113 MB/s  1.26x
    rust_gif_decode_10k_bgra                      333 MB/s  n/a
    rust_gif_decode_10k_indexed                   101 MB/s  0.96x
    rust_gif_decode_20k                           119 MB/s  1.12x
    rust_gif_decode_100k_artificial               248 MB/s  1.50x
    rust_gif_decode_100k_realistic                113 MB/s  1.10x
    rust_gif_decode_1000k_full_init               115 MB/s  1.08x
    rust_gif_decode_anim_screencap                513 MB/s  2.58x

To reproduce the Go 1.19 numbers, run `go run main.go` in
`script/bench-go-gif`. For Rust 1.48 / gif 0.11.4, run `cargo run --release` in
`script/bench-rust-gif`.

Historical (Wuffs v0.2; 2019) numbers for 32-bit ARMv7 (2012 era Samsung Exynos
5 Chromebook), Debian Stretch (2017):

    name                                             speed     vs_mimic

    wuffs_gif_decode_1k_bw/clang5                    49.1MB/s  1.76x
    wuffs_gif_decode_1k_color_full_init/clang5       22.3MB/s  1.35x
    wuffs_gif_decode_1k_color_part_init/clang5       27.4MB/s  1.66x
    wuffs_gif_decode_10k_bgra/clang5                  157MB/s  n/a
    wuffs_gif_decode_10k_indexed/clang5              42.0MB/s  1.79x
    wuffs_gif_decode_20k/clang5                      49.3MB/s  1.68x
    wuffs_gif_decode_100k_artificial/clang5           132MB/s  2.62x
    wuffs_gif_decode_100k_realistic/clang5           47.8MB/s  1.62x
    wuffs_gif_decode_1000k_full_init/clang5          46.4MB/s  1.62x
    wuffs_gif_decode_1000k_part_init/clang5          46.4MB/s  1.62x
    wuffs_gif_decode_anim_screencap/clang5            243MB/s  4.03x

    wuffs_gif_decode_1k_bw/gcc6                      46.6MB/s  1.67x
    wuffs_gif_decode_1k_color_full_init/gcc6         20.1MB/s  1.22x
    wuffs_gif_decode_1k_color_part_init/gcc6         24.2MB/s  1.47x
    wuffs_gif_decode_10k_bgra/gcc6                    124MB/s  n/a
    wuffs_gif_decode_10k_indexed/gcc6                34.8MB/s  1.49x
    wuffs_gif_decode_20k/gcc6                        43.8MB/s  1.49x
    wuffs_gif_decode_100k_artificial/gcc6             123MB/s  2.44x
    wuffs_gif_decode_100k_realistic/gcc6             42.7MB/s  1.44x
    wuffs_gif_decode_1000k_full_init/gcc6            41.6MB/s  1.45x
    wuffs_gif_decode_1000k_part_init/gcc6            41.7MB/s  1.45x
    wuffs_gif_decode_anim_screencap/gcc6              227MB/s  3.76x

    mimic_gif_decode_1k_bw                           27.9MB/s  1.00x
    mimic_gif_decode_1k_color                        16.5MB/s  1.00x
    mimic_gif_decode_10k_indexed                     23.4MB/s  1.00x
    mimic_gif_decode_20k                             29.4MB/s  1.00x
    mimic_gif_decode_100k_artificial                 50.4MB/s  1.00x
    mimic_gif_decode_100k_realistic                  29.5MB/s  1.00x
    mimic_gif_decode_1000k                           28.7MB/s  1.00x
    mimic_gif_decode_anim_screencap                  60.3MB/s  1.00x


# Gzip (Deflate + CRC-32)

The `1k`, `10k`, etc. numbers are approximately how many bytes there are in the
decoded output.

    name                              speed     vs_mimic

    wuffs_gzip_decode_10k/clang11     420 MB/s  1.71x
    wuffs_gzip_decode_100k/clang11    527 MB/s  1.81x

    wuffs_gzip_decode_10k/gcc10       427 MB/s  1.74x
    wuffs_gzip_decode_100k/gcc10      550 MB/s  1.89x

    mimicztl_gzip_decode_10k          246 MB/s  1.00x
    mimicztl_gzip_decode_100k         291 MB/s  1.00x

    mimiclibdeflate_gzip_decode_10k   494 MB/s  2.01x
    mimiclibdeflate_gzip_decode_100k  496 MB/s  1.70x


# LZW

The `1k`, `10k`, etc. numbers are approximately how many bytes there are in the
decoded output.

The libgif library doesn't export its LZW decoder in its API, so there are no
mimic numbers to compare to.

    name                           speed     vs_mimic

    wuffs_lzw_decode_20k/clang11   293 MB/s  n/a
    wuffs_lzw_decode_100k/clang11  489 MB/s  n/a

    wuffs_lzw_decode_20k/gcc10     259 MB/s  n/a
    wuffs_lzw_decode_100k/gcc10    516 MB/s  n/a


# PNG

The `1k`, `10k`, etc. numbers are approximately how many bytes there are in the
decoded image. For example, the `test/data/harvesters.*` images are 1165 × 859,
approximately 1000k pixels and hence 4000k bytes at 4 bytes per pixel.

The `full_init` vs `part_init` suffixes are whether
[`WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED`](/doc/note/initialization.md#partial-zero-initialization)
is unset or set.

libpng's "simplified API" doesn't provide a way to ignore the checksum. We copy
the `verify_checksum` numbers for a 1.00x baseline.

    name                                                        speed     vs_mimic

    wuffs_png_decode_image_19k_8bpp/clang11                     279 MB/s  2.29x
    wuffs_png_decode_image_40k_24bpp/clang11                    323 MB/s  2.20x
    wuffs_png_decode_image_77k_8bpp/clang11                     984 MB/s  2.66x
    wuffs_png_decode_image_552k_32bpp_ignore_checksum/clang11   871 MB/s  3.01x
    wuffs_png_decode_image_552k_32bpp_verify_checksum/clang11   837 MB/s  2.90x
    wuffs_png_decode_image_4002k_24bpp/clang11                  331 MB/s  1.55x

    wuffs_png_decode_image_19k_8bpp/gcc10                       277 MB/s  2.27x
    wuffs_png_decode_image_40k_24bpp/gcc10                      343 MB/s  2.33x
    wuffs_png_decode_image_77k_8bpp/gcc10                      1.00 GB/s  2.70x
    wuffs_png_decode_image_552k_32bpp_ignore_checksum/gcc10     914 MB/s  3.16x
    wuffs_png_decode_image_552k_32bpp_verify_checksum/gcc10     870 MB/s  3.01x
    wuffs_png_decode_image_4002k_24bpp/gcc10                    353 MB/s  1.65x

    mimiclibpng_png_decode_image_19k_8bpp                       122 MB/s  1.00x
    mimiclibpng_png_decode_image_40k_24bpp                      147 MB/s  1.00x
    mimiclibpng_png_decode_image_77k_8bpp                       370 MB/s  1.00x
    mimiclibpng_png_decode_image_552k_32bpp_verify_checksum     289 MB/s  1.00x
    mimiclibpng_png_decode_image_4002k_24bpp                    214 MB/s  1.00x

    mimiclibspng_png_decode_image_19k_8bpp                      125 MB/s  1.02x
    mimiclibspng_png_decode_image_40k_24bpp                     155 MB/s  1.05x
    mimiclibspng_png_decode_image_77k_8bpp                      384 MB/s  1.04x
    mimiclibspng_png_decode_image_552k_32bpp_ignore_checksum    461 MB/s  1.60x
    mimiclibspng_png_decode_image_552k_32bpp_verify_checksum    392 MB/s  1.36x
    mimiclibspng_png_decode_image_4002k_24bpp                   225 MB/s  1.05x

    mimiclodepng_png_decode_image_19k_8bpp                      138 MB/s  1.13x
    mimiclodepng_png_decode_image_40k_24bpp                     166 MB/s  1.13x
    mimiclodepng_png_decode_image_77k_8bpp                      404 MB/s  1.09x
    mimiclodepng_png_decode_image_552k_32bpp_verify_checksum    258 MB/s  0.89x
    mimiclodepng_png_decode_image_4002k_24bpp                   165 MB/s  0.77x

    mimicstb_png_decode_image_19k_8bpp                          150 MB/s  1.23x
    mimicstb_png_decode_image_40k_24bpp                         166 MB/s  1.13x
    mimicstb_png_decode_image_77k_8bpp                          443 MB/s  1.20x
    mimicstb_png_decode_image_552k_32bpp_ignore_checksum        288 MB/s  1.00x
    mimicstb_png_decode_image_4002k_24bpp                       163 MB/s  0.76x

    go_png_decode_image_19k_8bpp                                 92 MB/s  0.75x
    go_png_decode_image_40k_24bpp                               107 MB/s  0.73x
    go_png_decode_image_77k_8bpp                                207 MB/s  0.56x
    go_png_decode_image_552k_32bpp_verify_checksum              246 MB/s  0.85x
    go_png_decode_image_4002k_24bpp                             116 MB/s  0.54x

    rust_png_decode_image_19k_8bpp                              187 MB/s  1.53x
    rust_png_decode_image_40k_24bpp                             260 MB/s  1.77x
    rust_png_decode_image_77k_8bpp                              330 MB/s  0.89x
    rust_png_decode_image_552k_32bpp_verify_checksum            299 MB/s  1.03x
    rust_png_decode_image_4002k_24bpp                           264 MB/s  1.23x

To reproduce the Go 1.19 numbers, run `go run main.go` in
`script/bench-go-png`. For Rust 1.48 / png 0.17.5 / deflate 1.0.0 /
miniz\_oxide 0.5.3, run `cargo run --release` in `script/bench-rust-png`.


# Zlib (Deflate + Adler-32)

The `1k`, `10k`, etc. numbers are approximately how many bytes there are in the
decoded output.

    name                              speed     vs_mimic

    wuffs_zlib_decode_10k/clang11     410 MB/s  1.53x
    wuffs_zlib_decode_100k/clang11    505 MB/s  1.56x

    wuffs_zlib_decode_10k/gcc10       431 MB/s  1.61x
    wuffs_zlib_decode_100k/gcc10      548 MB/s  1.70x

    mimicztl_zlib_decode_10k          268 MB/s  1.00x
    mimicztl_zlib_decode_100k         323 MB/s  1.00x

    mimiclibdeflate_zlib_decode_10k   497 MB/s  1.85x
    mimiclibdeflate_zlib_decode_100k  499 MB/s  1.54x

    mimicminiz_zlib_decode_10k        237 MB/s  0.88x
    mimicminiz_zlib_decode_100k       272 MB/s  0.84x


---

Updated on September 2022.
