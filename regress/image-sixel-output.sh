#!/bin/sh

# Check RGBA to SIXEL output, palette splitting, transparent edges, source
# clipping and repeated redraws at different terminal pixel sizes.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, sixel, check_image

palette = [(100, 0, 0), (0, 100, 0), (0, 0, 100), (100, 100, 100)]
rgb = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255)]
pixels = [2 * (y >= 16) + (x >= 8) for y in range(32) for x in range(16)]
expected = [rgb[c] for c in pixels]
wire = sixel(16, 32, palette, pixels)

with image_test(sys.argv[1]) as test:
    client = test.attach()
    test.emit(wire)
    assert len(client.images()) == 1
    check_image(client.images()[0], 16, 32, expected)
    for _ in range(3):
        test.redraw()
        assert len(client.images()) == 1
        check_image(client.images()[0], 16, 32, expected)

    # Keep the stored cell footprint when terminal cell pixels change.
    for cw, ch in [(16, 32), (12, 24), (12, 16)]:
        test.resize(client, 40, 16, cw=cw, ch=ch)
        w, h = cw * 2, ch * 2
        scaled = [expected[(y * 32 // h) * 16 + x * 16 // w]
            for y in range(h) for x in range(w)]
        assert len(client.images()) == 1
        check_image(client.images()[0], w, h, scaled)
    test.resize(client, 40, 16)
    check_image(client.images()[0], 16, 32, expected)

    # Removing one grid cell crops the original source, retaining its colours.
    test.emit(b'\033[HX')
    images = client.images()
    assert len(images) == 2, [im[:2] for im in images]
    check_image(images[0], 8, 16, [rgb[1]] * (8 * 16))
    check_image(images[1], 16, 16,
        [rgb[2 + (x >= 8)] for y in range(16) for x in range(16)])
    test.emit(b'\033[2J\033[H')
    assert not client.images()
    test.redraw()
    assert not client.images()

    # A partial last cell keeps its actual pixel size instead of stretching.
    test.emit(sixel(26, 26, [(100, 100, 100)], [0] * (26 * 26)))
    assert len(client.images()) == 1
    check_image(client.images()[0], 26, 26, [rgb[3]] * (26 * 26))

    # Transparent edges and holes must not acquire coloured pixels.
    pixels = [None if x == 0 or y == 0 or x == 7 or y == 15 else 0
        for y in range(16) for x in range(8)]
    test.emit(b'\033[2J\033[H' + sixel(8, 16, palette[:1], pixels))
    assert len(client.images()) == 1
    check_image(client.images()[0], 8, 16,
        [None if c is None else rgb[0] for c in pixels])

    # A skewed histogram still splits into black, grey and white entries.
    pixels = [0] * 10 + [1] * 10 + [2] * 80
    test.emit(b'\033[2J\033[H' +
        sixel(100, 1, [(0, 0, 0), (50, 50, 50), (100, 100, 100)], pixels))
    assert len(client.images()) == 1
    check_image(client.images()[0], 100, 1,
        [(0, 0, 0)] * 10 + [(128, 128, 128)] * 10 + [rgb[3]] * 80)

    # The right edge of a pane crops the source, rather than rescaling it.
    test.emit(b'\033[2J\033[1;40H' + wire)
    assert len(client.images()) == 1
    check_image(client.images()[0], 8, 32,
        [rgb[2 * (y >= 16)] for y in range(32) for x in range(8)])

# Different clients independently use the same image at their own cell size.
with image_test(sys.argv[1]) as test:
    large = test.attach()
    test.emit(wire)
    small = test.attach(cw=4, ch=8)
    fallback = test.attach(graphical=False)
    test.redraw()
    assert len(large.images()) == 1
    check_image(large.images()[0], 16, 32, expected)
    assert len(small.images()) == 1
    check_image(small.images()[0], 8, 16,
        [expected[y * 2 * 16 + x * 2]
            for y in range(16) for x in range(8)])
    assert not fallback.images()
    assert b'\xe2\x96\x84' in fallback.data, 'missing simultaneous fallback'
PY
