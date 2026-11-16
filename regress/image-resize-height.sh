#!/bin/sh

# Height reduction discards image rows without leaving spans behind.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, kitty, sixel
from image_terminal import Terminal


def snapshot(test, client, expected, sy):
    """Check image pixels after a full redraw at the current height."""
    test.redraw()
    terminal = Terminal(sy=sy)
    terminal.feed(client.data)
    terminal.check(expected)


red, green = (255, 0, 0), (0, 255, 0)
for protocol in ('kitty', 'sixel'):
    if protocol == 'kitty':
        image = kitty('a=T,f=32,s=1,v=1,c=2,r=5,C=1', bytes((*red, 255)))
        lower = kitty('a=T,f=32,s=1,v=1,c=2,r=1,C=1', bytes((*green, 255)))
    else:
        image = sixel(16, 80, [(100, 0, 0)], [0] * (16 * 80))
        lower = sixel(16, 16, [(0, 100, 0)], [0] * (16 * 16))
    for alternate in (False, True):
        with image_test(sys.argv[1]) as test:
            client = test.attach()
            if alternate:
                test.emit(b'\033[?1049h')
            test.emit(b'\033[9;3H' + image + b'\033[14;3H' + lower +
                b'\033[H')
            expected = {(x, y): red for y in range(8 * 16, 13 * 16)
                for x in range(16, 32)}
            expected.update({(x, y): green for y in range(13 * 16, 14 * 16)
                for x in range(16, 32)})
            snapshot(test, client, expected, 16)

            # One placement is clipped and the other is discarded entirely.
            test.resize(client, 40, 10)
            snapshot(test, client, {(x, y): red for y in range(8 * 16, 10 * 16)
                for x in range(16, 32)}, 10)

            # Discarded rows must not keep the remaining placement fixed.
            test.emit(b'\033[8;10r\033[S\033[r')
            snapshot(test, client, {(x, y): red for y in range(7 * 16, 9 * 16)
                for x in range(16, 32)}, 10)
PY
