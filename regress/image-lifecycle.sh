#!/bin/sh

# Stress placement cleanup and copied references, restore image rows after
# height changes, and switch between graphical and fallback output.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, sixel, check_image
from image_terminal import Terminal

palette = [(100, 0, 0), (0, 100, 0)]
rgb = [(255, 0, 0), (0, 255, 0)]
wire = sixel(16, 32, palette,
    [y >= 16 for y in range(32) for x in range(16)])
expected = [rgb[y >= 16] for y in range(32) for x in range(16)]

with image_test(sys.argv[1], sy=6) as test:
    client = test.attach()
    test.emit(wire)
    test.resize(client, 40, 2)
    test.redraw()
    images = client.images()
    assert len(images) == 1, [im[:2] for im in images]
    check_image(images[0], 16, 16, [rgb[1]] * (16 * 16))
    test.resize(client, 40, 6)
    test.redraw()
    assert len(client.images()) == 1
    check_image(client.images()[0], 16, 32, expected)

# Changing pixel size can change the selected backend without reattaching.
with image_test(sys.argv[1]) as test:
    client = test.attach()
    test.emit(wire)
    for _ in range(3):
        test.resize(client, 40, 16, cw=0, ch=0)
        test.redraw()
        assert not client.images(), 'SIXEL backend retained without pixel size'
        terminal = Terminal()
        terminal.feed(client.data)
        assert terminal.cells[0, 0][0] == '\u2584', terminal.cells[0, 0]
        test.resize(client, 40, 16)
        test.redraw()
        assert len(client.images()) == 1, 'SIXEL backend not restored'
        check_image(client.images()[0], 16, 32, expected)

# Keep one server alive through many overlapping placements, split spans,
# scrolling and clears, with copy-mode snapshots and resizes between batches.
with image_test(sys.argv[1]) as test:
    client = test.attach()
    overlay = sixel(16, 32, palette,
        [None if x % 2 else 1 for y in range(32) for x in range(16)])
    cycle = (b'\033[2J\033[H' + wire + b'\033[H' + overlay +
        b'\033[1;2H\033[@\033[P\033[X\033[2;1H\033[L\033[M\033[2S')
    for _ in range(10):
        test.emit(cycle * 8 + b'\033[2J\033[H' + wire)
        test.run('copy-mode')
        test.run('send-keys', '-X', 'cancel')
        test.resize(client, 1, 16)
        test.resize(client, 40, 16)
        test.redraw()
        terminal = Terminal()
        terminal.feed(client.data)
        terminal.check({(x, y): expected[y * 16 + x]
            for y in range(32) for x in range(16)})
        test.run('clear-history')
    test.emit(b'\033[2J')
    test.redraw()
    assert not client.images(), 'images survived the final clear'
PY
