#!/bin/sh

# Bound prepared Kitty output, including its one-pixel border on each side.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PYTEST'
import sys
from image_test import image_test, kitty
from image_kitty import KittyTerminal

red = (255, 0, 0)
for vertical in (False, True):
    for length in (9998, 9999, 10000, 16000):
        columns, rows = length, 1
        if vertical:
            columns, rows = 1, length
        with image_test(sys.argv[1], sx=12, sy=16) as test:
            client = test.attach(kitty_graphics=True, cw=1, ch=1)
            terminal = KittyTerminal(sx=12, sy=16, cw=1, ch=1)
            test.emit(kitty('a=T,i=7,p=1,f=32,s=1,v=1,c=%d,r=%d,C=1' %
                (columns, rows), bytes((*red, 255))))
            terminal.feed(client.data)
            if length == 9998:
                terminal.check({(x, y): red
                    for y in range(min(rows, 16))
                    for x in range(min(columns, 12))})
                assert terminal.uploads == 1
                assert all(w <= 10000 and h <= 10000
                    for w, h, _ in terminal.images.values())
            else:
                terminal.check({})
                assert not terminal.images and terminal.uploads == 0

                # The source remains available for a smaller placement.
                test.emit(b'\033[H' +
                    kitty('a=p,i=7,p=1,c=1,r=1,C=1'))
                terminal.feed(client.data)
                terminal.check({(0, 0): red})
                assert terminal.uploads == 1


# The output limit must not reject a larger source with a small visible crop.
with image_test(sys.argv[1]) as test:
    client = test.attach(kitty_graphics=True, cw=1, ch=1)
    terminal = KittyTerminal(cw=1, ch=1)
    pixels = bytes(v for y in range(16000) for v in (y % 251, 64, 128, 255))
    test.emit(kitty('a=t,i=7,f=32,s=1,v=16000', pixels) +
        kitty('a=p,i=7,p=1,y=800,w=1,h=8,c=1,r=8,C=1'))
    terminal.feed(client.data)
    terminal.check({(0, y): ((800 + y) % 251, 64, 128) for y in range(8)})
    assert terminal.uploads == 1
PYTEST
