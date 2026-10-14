#!/bin/sh

# Transparent overlap preserves underlying pixels; opaque background modes
# replace them. Check positions, redraws, resize and erasing stacked images.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, sixel
from image_terminal import Terminal

w, h = 24, 32
holes = [None if x // 8 == 1 and y < 16 or (x + y) % 3 == 0 else 0
    for y in range(h) for x in range(w)]
base = sixel(w, h, [(100, 0, 0)], [0] * (w * h))

for p2 in (0, 1, 2):
    with image_test(sys.argv[1]) as test:
        client = test.attach()
        test.emit(b'\033[3;3H' + base + b'\033[3;3H' +
            sixel(w, h, [(0, 0, 100)], holes, p2=p2))
        background = (255, 0, 0) if p2 == 1 else (0, 0, 0)
        expected = {(x + 16, y + 32): (0, 0, 255)
            if holes[y * w + x] == 0 else background
            for y in range(h) for x in range(w)}
        for width in (40, 4, 40):
            test.resize(client, width, 16)
            test.redraw()
            terminal = Terminal(sx=width)
            terminal.feed(client.data)
            terminal.check({point: c for point, c in expected.items()
                if point[0] < width * 8})
        # One erase must remove the same cell from every overlapping placement.
        test.emit(b'\033[3;3H\033[X')
        terminal.feed(client.data)
        expected = {point: c for point, c in expected.items()
            if not (16 <= point[0] < 24 and 32 <= point[1] < 48)}
        terminal.check(expected)
        test.redraw()
        terminal = Terminal()
        terminal.feed(client.data)
        terminal.check(expected)

with image_test(sys.argv[1]) as test:
    client = test.attach()
    test.emit(sixel(24, 32, [(100, 0, 0)], [None] * (24 * 32)))
    assert not client.images(), 'fully transparent image produced output'
    test.redraw()
    assert not client.images(), 'fully transparent image survived redraw'
PY
