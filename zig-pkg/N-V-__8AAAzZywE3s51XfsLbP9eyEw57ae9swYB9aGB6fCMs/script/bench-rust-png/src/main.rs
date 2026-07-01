// Copyright 2021 The Wuffs Authors.
//
//// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ----------------

// This program exercises the Rust PNG decoder at
// https://github.com/image-rs/image-png
// which is the top result for https://crates.io/search?q=png&sort=downloads
//
// Wuffs' C code doesn't depend on Rust per se, but this program gives some
// performance data for specific Rust PNG implementations. The equivalent Wuffs
// benchmarks (on the same test images) are run via:
//
// wuffs bench std/png
//
// To run this program, do "cargo run --release" from the parent directory (the
// directory containing the Cargo.toml file).

extern crate png;
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

    let mut dst0 = vec![0u8; 64 * 1024 * 1024];
    let mut dst1 = vec![0u8; 64 * 1024 * 1024];

    // The various magic constants below are copied from test/c/std/png.c
    for i in 0..(1 + REPS) {
        bench(
            "19k_8bpp",
            &mut dst0[..],
            &mut dst1[..],
            include_bytes!("../../../test/data/bricks-gray.no-ancillary.png"),
            i == 0,        // warm_up
            160 * 120 * 1, // want_num_bytes = 19_200
            50,            // iters_unscaled
        );

        bench(
            "40k_24bpp",
            &mut dst0[..],
            &mut dst1[..],
            include_bytes!("../../../test/data/hat.png"),
            i == 0,       // warm_up
            90 * 112 * 4, // want_num_bytes = 40_320
            30,           // iters_unscaled
        );

        bench(
            "77k_8bpp",
            &mut dst0[..],
            &mut dst1[..],
            include_bytes!("../../../test/data/bricks-dither.png"),
            i == 0,        // warm_up
            160 * 120 * 4, // want_num_bytes = 76_800
            30,            // iters_unscaled
        );

        bench(
            "552k_32bpp_verify_checksum",
            &mut dst0[..],
            &mut dst1[..],
            include_bytes!("../../../test/data/hibiscus.primitive.png"),
            i == 0,        // warm_up
            312 * 442 * 4, // want_num_bytes = 551_616
            4,             // iters_unscaled
        );

        bench(
            "4002k_24bpp",
            &mut dst0[..],
            &mut dst1[..],
            include_bytes!("../../../test/data/harvesters.png"),
            i == 0,         // warm_up
            1165 * 859 * 4, // want_num_bytes = 4_002_940
            1,              // iters_unscaled
        );
    }
}

fn bench(
    name: &str,          // Benchmark name.
    dst0: &mut [u8],     // Destination buffer #0.
    dst1: &mut [u8],     // Destination buffer #1.
    src: &[u8],          // Source data.
    warm_up: bool,       // Whether this is a warm up rep.
    want_num_bytes: u64, // Expected num_bytes per iteration.
    iters_unscaled: u64, // Base number of iterations.
) {
    let iters = iters_unscaled * ITERSCALE;
    let mut total_num_bytes = 0u64;

    let start = Instant::now();
    for _ in 0..iters {
        let n = decode(&mut dst0[..], &mut dst1[..], src);
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
        "Benchmarkrust_png_decode_image_{:16}   {:8}   {:12} ns/op   {:3}.{:03} MB/s\n",
        name,
        iters,
        elapsed_nanos / iters,
        kb_per_s / 1_000,
        kb_per_s % 1_000
    );
}

// decode returns the number of bytes processed.
fn decode(dst0: &mut [u8], dst1: &mut [u8], src: &[u8]) -> u64 {
    let mut decoder = png::Decoder::new(src);
    decoder.set_transformations(png::Transformations::normalize_to_color8());
    let mut reader = decoder.read_info().unwrap();
    let info = reader.next_frame(dst0).unwrap();
    let num_bytes = info.buffer_size() as u64;
    if info.color_type == png::ColorType::Grayscale {
        // No conversion necessary.
        return num_bytes;
    } else if info.color_type == png::ColorType::Rgb {
        // Convert RGB => BGRA.
        let new_size = ((num_bytes / 3) * 4) as usize;
        rgb_to_bgra(&dst0[..num_bytes as usize], &mut dst1[..new_size]);
        return new_size as u64;
    } else if info.color_type == png::ColorType::Rgba {
        // Convert RGBA => BGRA.
        for i in 0..((num_bytes / 4) as usize) {
            let d = dst0[(4 * i) + 0];
            dst0[(4 * i) + 0] = dst0[(4 * i) + 2];
            dst0[(4 * i) + 2] = d;
        }
        return num_bytes;
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
