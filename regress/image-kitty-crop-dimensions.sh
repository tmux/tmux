#!/bin/sh

# Expanded Kitty sources must fit each terminal dimension, including borders.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, kitty
from image_kitty import KittyTerminal


def colour(i):
    """Distinguish source pixels along the changing crop axis."""
    return (i % 251, i * 3 % 251, 64)


for vertical in (False, True):
    for length in (9998, 9999, 14000):
        w, h, cw, ch = length, 1, 2, 1
        if vertical:
            w, h, cw, ch = 1, length, 1, 2
        with image_test(sys.argv[1]) as test:
            client = test.attach(kitty_graphics=True, cw=cw, ch=ch)
            terminal = KittyTerminal(cw=cw, ch=ch)
            pixels = bytes(v for i in range(length) for v in (*colour(i), 255))
            test.emit(kitty('a=t,i=7,f=32,s=%d,v=%d' % (w, h), pixels))
            for step, offset in enumerate((0, 2, 4)):
                x, y = offset, 0
                if vertical:
                    x, y = 0, offset
                test.emit(b'\033[H' + kitty(
                    'a=p,i=7,p=1,x=%d,y=%d,w=%d,h=%d,c=1,r=1,C=1' %
                    (x, y, cw, ch)))
                terminal.feed(client.data)
                terminal.check({(px, py): colour(offset + px + py)
                    for py in range(ch) for px in range(cw)})
                assert all(ww <= 10000 and hh <= 10000
                    for ww, hh, _ in terminal.images.values())
                uploads = step + 1
                if length == 9998:
                    uploads = min(step + 1, 2)
                assert terminal.uploads == uploads, (vertical, length, step)
PY
