#!/bin/sh

# Check that cached colours and dithering survive clipping and client changes,
# and that a visible crop is drawn when the whole scaled image is too large.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, sixel, check_image

levels = [0, 14, 29, 43, 57, 71, 86, 100]
palette = [(r, g, b) for r in levels for g in levels for b in levels]
wire = sixel(32, 16, palette, list(range(512)))

with image_test(sys.argv[1]) as test:
    first = test.attach()
    test.emit(wire)
    original = first.images()
    assert len(original) == 1 and original[0][:2] == (32, 16)

    # Clients with the same cell size draw identical indexed pixels.
    second = test.attach()
    test.redraw()
    assert first.images() == second.images() == original

    # A different cell size must not replace the first clients' result.
    small = test.attach(cw=4, ch=8)
    test.redraw()
    scaled = small.images()
    assert len(scaled) == 1 and scaled[0][:2] == (16, 8)
    assert first.images() == second.images() == original

    # Overwrite the first cell. The remaining pixels retain the full palette
    # and dithering, rather than converting the smaller rectangle again.
    test.emit(b'\033[HX')
    expected = [original[0][2][y * 32 + x]
        for y in range(16) for x in range(8, 32)]
    for client in (first, second):
        images = client.images()
        assert len(images) == 1
        check_image(images[0], 24, 16, expected)
    small_expected = [scaled[0][2][y * 16 + x]
        for y in range(8) for x in range(4, 16)]
    images = small.images()
    assert len(images) == 1
    check_image(images[0], 12, 8, small_expected)

    # Changing back to an earlier cell size reuses the same indexed pixels.
    test.resize(small, 40, 16)
    test.redraw()
    for client in (first, second, small):
        images = client.images()
        assert len(images) == 1
        check_image(images[0], 24, 16, expected)

    # Replacing the source must not draw pixels from the deleted image.
    test.emit(b'\033[2J\033[H' +
        sixel(8, 16, [(100, 0, 0)], [0] * (8 * 16)))
    for client in (first, second, small):
        images = client.images()
        assert len(images) == 1
        check_image(images[0], 8, 16, [(255, 0, 0)] * (8 * 16))

with image_test(sys.argv[1], sx=1) as test:
    client = test.attach()
    test.emit(sixel(16, 16, [(100, 0, 0)], [0] * (16 * 16)))
    # The whole image would be 12000 pixels wide, beyond the SIXEL limit.
    # Its visible first cell is still valid and must be converted directly.
    test.resize(client, 1, 16, cw=6000, ch=16)
    test.redraw()
    images = client.images()
    assert len(images) == 1
    check_image(images[0], 6000, 16, [(255, 0, 0)] * (6000 * 16))
PY
