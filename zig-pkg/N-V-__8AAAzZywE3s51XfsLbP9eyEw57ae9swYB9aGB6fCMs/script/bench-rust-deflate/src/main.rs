// Copyright 2019 The Wuffs Authors.
//
//// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ----------------

// This program exercises the Rust Deflate decoder at
// https://github.com/alexcrichton/flate2-rs
// which is the top result for https://crates.io/search?q=flate&sort=downloads
//
// Wuffs' C code doesn't depend on Rust per se, but this program gives some
// performance data for specific Rust Deflate implementations. The equivalent
// Wuffs benchmarks (on the same test files) are run via:
//
// wuffs bench std/deflate
//
// To run this program, do "cargo run --release" from the parent directory (the
// directory containing the Cargo.toml file).

extern crate flate2;
extern crate rustc_version_runtime;

use std::io::Read;
use std::time::Instant;

const ITERSCALE: u64 = 20;
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

    // The various magic constants below are copied from test/c/std/deflate.c
    for i in 0..(1 + REPS) {
        bench(
            "1k_full_init",
            &mut dst[..],
            &include_bytes!("../../../test/data/romeo.txt.gz")[20..550],
            i == 0, // warm_up
            942, // want_num_bytes
            2000, // iters_unscaled
        );

        bench(
            "10k_full_init",
            &mut dst[..],
            &include_bytes!("../../../test/data/midsummer.txt.gz")[24..5166],
            i == 0, // warm_up
            11065, // want_num_bytes
            300, // iters_unscaled
        );

        bench(
            "100k_just_one_read",
            &mut dst[..],
            &include_bytes!("../../../test/data/pi.txt.gz")[17..48335],
            i == 0, // warm_up
            100003, // want_num_bytes
            30, // iters_unscaled
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
    let mut total_num_bytes = 0u64;

    let start = Instant::now();
    for _ in 0..iters {
        let n = decode(&mut dst[..], src);
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
        "Benchmarkrust_deflate_decode_{:16}   {:8}   {:12} ns/op   {:3}.{:03} MB/s\n",
        name,
        iters,
        elapsed_nanos / iters,
        kb_per_s / 1_000,
        kb_per_s % 1_000
    );
}

// decode returns the number of bytes processed.
fn decode(dst: &mut [u8], src: &[u8]) -> u64 {
    let mut num_bytes = 0u64;
    let mut decoder = flate2::read::DeflateDecoder::new(src);
    loop {
        let n = decoder.read(dst).unwrap();
        if n == 0 {
            break;
        }
        num_bytes += n as u64;
    }
    num_bytes
}
