#!/bin/sh

# Splitting a tab must preserve image cells outside the actual edit. Check
# the pixels on a graphical client and the text on a fallback client.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, sixel
from image_terminal import Terminal

red = sixel(64, 16, [(100, 0, 0)], [0] * (64 * 16))
for sync in (False, True):
    for edit, width, char in ((b'Q', 1, 'Q'),
            ('界'.encode(), 2, '界'), (b'\033[2X', 2, ' ')):
        with image_test(sys.argv[1]) as test:
            graphic = test.attach()
            fallback = test.attach(graphical=False)
            test.emit(b'\033[3;1H\033[4m        \033[0m\033[3;1H\tT')
            test.emit(b'\033[3;1H' + red)
            test.redraw()
            pixels = Terminal()
            pixels.feed(graphic.data)
            before = dict(pixels.pixels)
            assert len(before) == 64 * 16, len(before)
            text = Terminal()
            text.feed(fallback.data)
            assert text.cells[0, 2][0] == '▄', text.cells

            begin = b'\033[?2026h' if sync else b''
            end = b'\033[?2026l' if sync else b''
            test.emit(begin + b'\033[3;5H' + edit + end)
            pixels.feed(graphic.data)
            expected = {p: c for p, c in before.items()
                if not 32 <= p[0] < (4 + width) * 8}
            pixels.check(expected)
            text.feed(fallback.data)
            assert text.cells[4, 2][0] == char, (sync, edit, text.cells)
            for x in range(8):
                if not 4 <= x < 4 + width:
                    assert text.cells[x, 2][0] == '▄', (sync, edit, x)

            test.redraw()
            pixels.feed(graphic.data)
            pixels.check(expected)
PY
