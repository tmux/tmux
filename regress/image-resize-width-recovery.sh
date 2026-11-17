#!/bin/sh

# The PR's SIXEL fixture must recover all columns clipped at placement time.
# A PTY gives a known cell size; allow restored rows to use several rasters.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 -B - "$TEST_TMUX" <<'PY'
from pathlib import Path
import re
import sys
from image_test import image_test

wire = Path('monkey-2.sixel.txt').read_bytes()
match = re.search(rb'"1;1;(\d+);(\d+)', wire)
assert match is not None
w, h = map(int, match.groups())

with image_test(sys.argv[1], sx=12, sy=40) as test:
    client = test.attach()
    test.emit(wire)
    test.redraw()
    images = client.images()
    assert len(images) == 1, [im[:2] for im in images]
    assert images[0][:2] == (96, h), images[0][:2]

    test.resize(client, 80, 40)
    test.redraw()
    images = client.images()
    assert images, 'no image after widening'
    x = y = 0
    row_h = None
    for im in images:
        if x == 0:
            row_h = im[1]
        assert im[1] == row_h, (im[:2], row_h)
        x += im[0]
        assert x <= w, (x, w)
        if x == w:
            y += row_h
            x = 0
    assert (x, y) == (0, h), ('image stayed clipped', x, y, w, h)
PY
