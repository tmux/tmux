// Copyright 2018 The Wuffs Authors.
//
//// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ----------------

// This program exercises the Rust GIF decoder at
// https://github.com/image-rs/image-gif
// which is the top result for https://crates.io/search?q=gif&sort=downloads
//
// Wuffs' C code doesn't depend on Rust per se, but this program gives some
// performance data for specific Rust GIF implementations. The equivalent Wuffs
// benchmarks (on the same test images) are run via:
//
// wuffs bench std/gif
//
// To run this program, do "cargo run --release" from the parent directory (the
// directory containing the Cargo.toml file).

// TODO: unify this program, bench-rust-gif-dot-rs, with bench-rust-gif, the
// other Rust GIF benchmark program. They are two separate programs because
// both libraries want to be named "gif", and the cargo package manager cannot
// handle duplicate names (https://github.com/rust-lang/cargo/issues/1311).

extern crate gif;
extern crate rustc_version_runtime;

use std::time::Instant;

const ITERSCALE: u64 = 10;
const REPS: u64 = 5;

fn main() {
    let version = rustc_version_runtime::version();
    print!(
        "# Rust {}.{}.{}\n",
        version.major,
        version.minor,
        version.patch
    );
    print!("#\n");
    print!("# The output format, including the \"Benchmark\" prefixes, is compatible with the\n");
    print!("# https://godoc.org/golang.org/x/perf/cmd/benchstat tool. To install it, first\n");
    print!("# install Go, then run \"go install golang.org/x/perf/cmd/benchstat\".\n");

    let mut dst = vec![0u8; 64 * 1024 * 1024];

    // The various magic constants below are copied from test/c/std/gif.c
    for i in 0..(1 + REPS) {
        bench(
            "1k_bw",
            &mut dst[..],
            include_bytes!("../../../test/data/pjw-thumbnail.gif"),
            i == 0, // warm_up
            32 * 32 * 1, // want_num_bytes
            2000, // iters_unscaled
        );

        bench(
            "1k_color_full_init",
            &mut dst[..],
            include_bytes!("../../../test/data/hippopotamus.regular.gif"),
            i == 0, // warm_up
            36 * 28 * 1, // want_num_bytes
            1000, // iters_unscaled
        );

        bench(
            "10k_bgra",
            &mut dst[..],
            include_bytes!("../../../test/data/hat.gif"),
            i == 0, // warm_up
            90 * 112 * 4, // want_num_bytes
            100, // iters_unscaled
        );

        bench(
            "10k_indexed",
            &mut dst[..],
            include_bytes!("../../../test/data/hat.gif"),
            i == 0, // warm_up
            90 * 112 * 1, // want_num_bytes
            100, // iters_unscaled
        );

        bench(
            "20k",
            &mut dst[..],
            include_bytes!("../../../test/data/bricks-gray.gif"),
            i == 0, // warm_up
            160 * 120 * 1, // want_num_bytes
            50, // iters_unscaled
        );

        bench(
            "100k_artificial",
            &mut dst[..],
            include_bytes!("../../../test/data/hibiscus.primitive.gif"),
            i == 0, // warm_up
            312 * 442 * 1, // want_num_bytes
            15, // iters_unscaled
        );

        bench(
            "100k_realistic",
            &mut dst[..],
            include_bytes!("../../../test/data/hibiscus.regular.gif"),
            i == 0, // warm_up
            312 * 442 * 1, // want_num_bytes
            10, // iters_unscaled
        );

        bench(
            "1000k_full_init",
            &mut dst[..],
            include_bytes!("../../../test/data/harvesters.gif"),
            i == 0, // warm_up
            1165 * 859, // want_num_bytes
            1, // iters_unscaled
        );

        bench(
            "anim_screencap",
            &mut dst[..],
            include_bytes!("../../../test/data/gifplayer-muybridge.gif"),
            i == 0, // warm_up
            4652198, // want_num_bytes
            1, // iters_unscaled
        );
    }
}

fn bench(
    name: &str, // Benchmark name.
    dst: &mut [u8], // Destination buffer.
    src: &[u8], // Source data.
    warm_up: bool, // Whether this is a warm up rep.
    want_num_bytes: u64, // Expected num_bytes per iteration.
    iters_unscaled: u64, // Base number of iterations.
) {
    let iters = iters_unscaled * ITERSCALE;
    let output_32_bit_color = name.ends_with("_bgra");
    let mut total_num_bytes = 0u64;

    let start = Instant::now();
    for _ in 0..iters {
        let n = decode(&mut dst[..], src, output_32_bit_color);
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
        "Benchmarkrust_gif_decode_{:16}   {:8}   {:12} ns/op   {:3}.{:03} MB/s\n",
        name,
        iters,
        elapsed_nanos / iters,
        kb_per_s / 1_000,
        kb_per_s % 1_000
    );
}

// decode returns the number of bytes processed.
fn decode(dst: &mut [u8], src: &[u8], output_32_bit_color: bool) -> u64 {
    let mut num_bytes = 0u64;
    let mut options = gif::DecodeOptions::new();
    if output_32_bit_color {
        options.set_color_output(gif::ColorOutput::RGBA);
    }
    let mut decoder = options.read_info(src).unwrap();
    while let Some(_) = decoder.next_frame_info().unwrap() {
        num_bytes += decoder.buffer_size() as u64;
        decoder.read_into_buffer(dst).unwrap();
    }
    num_bytes
}
