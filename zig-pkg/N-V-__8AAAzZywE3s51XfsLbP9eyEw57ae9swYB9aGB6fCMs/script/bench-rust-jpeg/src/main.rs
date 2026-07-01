// Copyright 2023 The Wuffs Authors.
//
//// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ----------------

// This program exercises the Rust JPEG decoder at
// https://github.com/image-rs/jpeg-decoder
// which is a popular result for https://crates.io/search?q=jpeg&sort=downloads
//
// Wuffs' C code doesn't depend on Rust per se, but this program gives some
// performance data for specific Rust JPEG implementations. The equivalent Wuffs
// benchmarks (on the same test images) are run via:
//
// wuffs bench std/jpeg
//
// To run this program: "RUSTFLAGS='-C target-cpu=native' cargo run --release"
// from the parent directory (the directory containing the Cargo.toml file).

extern crate jpeg_decoder;
extern crate rustc_version_runtime;

use std::time::Instant;
use std::convert::TryInto;

const ITERSCALE: u64 = 50;
const REPS: u64 = 5;

fn main() {
    let version = rustc_version_runtime::version();
    print!(
        "# Rust {}.{}.{}\n",
        version.major, version.minor, version.patch,
    );
    print!("#\n");
    print!("# The output format, including the \"Benchmark\" prefixes, is compatible with the\n");
    print!("# https://godoc.org/golang.org/x/perf/cmd/benchstat tool. To install it, first\n");
    print!("# install Go, then run \"go install golang.org/x/perf/cmd/benchstat\".\n");

    let mut dst1 = vec![0u8; 64 * 1024 * 1024];

    // The various magic constants below are copied from test/c/std/jpeg.c
    for i in 0..(1 + REPS) {
        bench(
            "19k_8bpp",
            &mut dst1[..],
            include_bytes!("../../../test/data/bricks-gray.jpeg"),
            i == 0, // warm_up
            160 * 120 * 1, // want_num_bytes = 19_200
            100, // iters_unscaled
        );

        bench(
            "30k_24bpp_progressive",
            &mut dst1[..],
            include_bytes!("../../../test/data/peacock.progressive.jpeg"),
            i == 0, // warm_up
            100 * 75 * 4, // want_num_bytes = 30_000
            50, // iters_unscaled
        );

        bench(
            "30k_24bpp_sequential",
            &mut dst1[..],
            include_bytes!("../../../test/data/peacock.default.jpeg"),
            i == 0, // warm_up
            100 * 75 * 4, // want_num_bytes = 30_000
            50, // iters_unscaled
        );

        bench(
            "77k_24bpp",
            &mut dst1[..],
            include_bytes!("../../../test/data/bricks-color.jpeg"),
            i == 0, // warm_up
            160 * 120 * 4, // want_num_bytes = 30_000
            30, // iters_unscaled
        );

        bench(
            "552k_24bpp_420",
            &mut dst1[..],
            include_bytes!("../../../test/data/hibiscus.regular.jpeg"),
            i == 0, // warm_up
            312 * 442 * 4, // want_num_bytes = 551_616
            5, // iters_unscaled
        );

        bench(
            "552k_24bpp_444",
            &mut dst1[..],
            include_bytes!("../../../test/data/hibiscus.primitive.jpeg"),
            i == 0, // warm_up
            312 * 442 * 4, // want_num_bytes = 551_616
            5, // iters_unscaled
        );

        bench(
            "4002k_24bpp",
            &mut dst1[..],
            include_bytes!("../../../test/data/harvesters.jpeg"),
            i == 0, // warm_up
            1165 * 859 * 4, // want_num_bytes = 4_002_940
            1, // iters_unscaled
        );
    }
}

fn bench(
    name: &str, // Benchmark name.
    dst1: &mut [u8], // Destination buffer #1.
    src: &[u8], // Source data.
    warm_up: bool, // Whether this is a warm up rep.
    want_num_bytes: u64, // Expected num_bytes per iteration.
    iters_unscaled: u64, // Base number of iterations.
) {
    let iters = iters_unscaled * ITERSCALE;
    let mut total_num_bytes = 0u64;

    let start = Instant::now();
    for _ in 0..iters {
        let n = decode(&mut dst1[..], src);
        if n != want_num_bytes {
            panic!("num_bytes: got {}, want {}", n, want_num_bytes);
        }
        total_num_bytes += n;
    }
    let elapsed = start.elapsed();

    let elapsed_nanos = (elapsed.as_secs() * 1_000_000_000) + (elapsed.subsec_nanos() as u64);
    let kb_per_s: u64 = total_num_bytes * 1_000_000 / elapsed_nanos;

    if warm_up {
        return;
    }

    print!(
        "Benchmarkrust_jpeg_decode_image_{:16}   {:8}   {:12} ns/op   {:3}.{:03} MB/s\n",
        name,
        iters,
        elapsed_nanos / iters,
        kb_per_s / 1_000,
        kb_per_s % 1_000
    );
}

// decode returns the number of bytes processed.
fn decode(dst1: &mut [u8], src: &[u8]) -> u64 {
    let mut decoder = jpeg_decoder::Decoder::new(src);
    let pixels = decoder.decode().expect("failed to decode image");
    let metadata = decoder.info().unwrap();
    let w = metadata.width as u64;
    let h = metadata.height as u64;
    if metadata.pixel_format == jpeg_decoder::PixelFormat::L8 {
        // No conversion necessary.
        return w * h;
    } else if metadata.pixel_format == jpeg_decoder::PixelFormat::RGB24 {
        // Convert RGB => BGRA.
        rgb_to_bgra(&pixels, &mut dst1[..(w * h * 4) as usize]);
        return w * h * 4;
    }
    // Returning 0 should lead to a panic (when want_num_bytes != 0).
    0
}

/// Copy `src` (treated as 3-byte chunks) into `dst`
/// (treated as 4-byte chunks), filling out `dst` by adding
/// a `0xff` "alpha value" at the end of each entry.
///
/// # Panics:
///
/// Will panic if
///
/// * The length of `src` is not a multiple of 3.
/// * The length of `dst` is not a multiple of 4.
/// * `src` and `dst` do not have the same length in chunks.
#[inline]
pub fn rgb_to_bgra(src: &[u8], dst: &mut [u8]) {
    let nsrc = src.len();
    let ndst = dst.len();
    assert_eq!(0, nsrc % 3);
    assert_eq!(0, ndst % 4);
    assert_eq!(nsrc, (ndst / 4) * 3);
    for (s, d) in src.chunks_exact(3).zip(dst.chunks_exact_mut(4)) {
        let s: &[u8; 3] = s.try_into().unwrap();
        let d: &mut [u8; 4] = d.try_into().unwrap();
        // R
        d[0] = s[2];
        // G
        d[1] = s[1];
        // B
        d[2] = s[0];
        // A
        d[3] = 0xff;
    }
}
