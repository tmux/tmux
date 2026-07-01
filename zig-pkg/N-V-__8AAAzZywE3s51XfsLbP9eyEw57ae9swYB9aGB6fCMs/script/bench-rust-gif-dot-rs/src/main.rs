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

// This program does not work (as of October 2019), because the upstream
// https://github.com/Geal/gif.rs library does not build.
//
// See https://github.com/Geal/gif.rs/issues/5

// ----------------

// This program exercises the Rust GIF decoder at
// https://github.com/Geal/gif.rs
//
// Wuffs' C code doesn't depend on Rust or Nom (a parser combinator library
// written in Rust) per se, but this program gives some performance data for
// specific Rust GIF implementations. The equivalent Wuffs benchmarks (on the
// same test image) are run via:
//
// wuffs bench -mimic -focus=wuffs_gif_decode_1000k,mimic_gif_decode_1000k std/gif
//
// The "1000k" is because the test image (harvesters.gif) has approximately 1
// million pixels.
//
// The Wuffs benchmark reports megabytes per second. This program reports
// megapixels per second. The two concepts should be equivalent, since GIF
// images' pixel data are always 1 byte per pixel indices into a color palette.
// However, the gif.rs library only lets you decode 3 bytes per pixel (RGB),
// not 1 byte per pixel. This automatic palette look-up is arguably a defect in
// its API: decoding a 1,000 × 1,000 pixel image requires (2,000,000 - 768)
// more bytes this way, per frame, and animated GIFs have multiple frames.
//
// To run this program, do "cargo run --release > /dev/null" from the parent
// directory (the directory containing the Cargo.toml file). See the eprint
// comment below for why we redirect to /dev/null.

// TODO: unify this program, bench-rust-gif-dot-rs, with bench-rust-gif, the
// other Rust GIF benchmark program. They are two separate programs because
// both libraries want to be named "gif", and the cargo package manager cannot
// handle duplicate names (https://github.com/rust-lang/cargo/issues/1311).

extern crate gif;

use std::time::Instant;

// These constants are hard-coded to the harvesters.gif test image. As of
// 2018-01-27, the https://github.com/Geal/gif.rs library's src/lib.rs' Decoder
// implementation is incomplete, and doesn't expose enough API to calculate
// these from the source GIF image.
const WIDTH: usize = 1165;
const HEIGHT: usize = 859;
const BYTES_PER_PIXEL: usize = 3; // Red, green, blue.
const NUM_BYTES: usize = WIDTH * HEIGHT * BYTES_PER_PIXEL;
const COLOR_TABLE_ELEMENT_COUNT: u16 = 256;
const COLOR_TABLE_OFFSET: usize = 13;
const GRAPHIC_BLOCK_OFFSET: usize = 782;
const FIRST_PIXEL: u8 = 1; // Top left pixel's red component is 0x01.
const LAST_PIXEL: u8 = 3; // Bottom right pixel's blue component is 0x03.

fn main() {
    let mut dst = [0; NUM_BYTES];
    let src = include_bytes!("../../../test/data/harvesters.gif");

    let start = Instant::now();

    const REPS: u32 = 50;
    for _ in 0..REPS {
        decode(&mut dst[..], src);
    }

    let elapsed = start.elapsed();
    let elapsed_nanos = elapsed.as_secs() * 1_000_000_000 + (elapsed.subsec_nanos() as u64);

    let total_pixels: u64 = ((WIDTH * HEIGHT) as u64) * (REPS as u64);
    let kp_per_s: u64 = total_pixels * 1_000_000 / elapsed_nanos;

    // Use eprint instead of print because the https://github.com/Geal/gif.rs
    // library can print its own debugging messages (this is filed as
    // https://github.com/Geal/gif.rs/issues/4). When running this program, it
    // can be useful to redirect stdout (but not stderr) to /dev/null.
    eprint!(
        "gif.rs  {:3}.{:03} megapixels/second  github.com/Geal/gif.rs\n",
        kp_per_s / 1_000,
        kp_per_s % 1_000
    );
}

fn decode(dst: &mut [u8], src: &[u8]) {
    // Set up the hard-coded coherence check, executed below.
    dst[0] = 0xFE;
    dst[NUM_BYTES - 1] = 0xFE;

    let (_, colors) =
        gif::parser::color_table(&src[COLOR_TABLE_OFFSET..], COLOR_TABLE_ELEMENT_COUNT).unwrap();

    let (_, block) = gif::parser::graphic_block(&src[GRAPHIC_BLOCK_OFFSET..]).unwrap();

    let rendering = match block {
        gif::parser::Block::GraphicBlock(_, x) => x,
        _ => panic!("not a graphic block"),
    };

    let (code_size, blocks) = match rendering {
        gif::parser::GraphicRenderingBlock::TableBasedImage(_, x, y) => (x, y),
        _ => panic!("not a table based image"),
    };

    let num_bytes = gif::lzw::decode_lzw(&colors, code_size as usize, blocks, dst).unwrap();

    if num_bytes != NUM_BYTES {
        panic!("wrong num_bytes")
    }

    // A hard-coded coherence check that we decoded the pixel data correctly.
    if (dst[0] != FIRST_PIXEL) || (dst[NUM_BYTES - 1] != LAST_PIXEL) {
        panic!("wrong dst pixels")
    }
}
