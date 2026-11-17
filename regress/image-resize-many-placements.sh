#!/bin/sh

# Adapt the PR's many-placement recovery test to SIXEL: place images on
# separate rows so they remain live, then recover every clipped column.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, sixel, check_image

def check_rows(images, width):
    """Each placement has two coloured halves; rows may use several rasters."""
    row = x = 0
    for image in images:
        assert row < 64 and x + image[0] <= width, (row, x, image[:2])
        expected = [rgb[(row + (xx >= 64)) % 3]
            for xx in range(x, x + image[0])]
        check_image(image, image[0], 1, expected)
        x += image[0]
        if x == width:
            row += 1
            x = 0
    assert (row, x) == (64, 0), (row, x)


palette = [(100, 0, 0), (0, 100, 0), (0, 0, 100)]
rgb = [(255, 0, 0), (0, 255, 0), (0, 0, 255)]
with image_test(sys.argv[1], sx=8, sy=80) as test:
    client = test.attach()
    wire = b''
    for y in range(64):
        wire += ('\033[%d;1H' % (y + 1)).encode()
        wire += sixel(128, 1, palette,
            [y % 3] * 64 + [(y + 1) % 3] * 64)
    test.emit(wire)
    test.redraw()
    check_rows(client.images(), 64)

    test.resize(client, 16, 80)
    test.redraw()
    check_rows(client.images(), 128)
    test.redraw()
    check_rows(client.images(), 128)

PY
