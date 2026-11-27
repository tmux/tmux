#!/bin/sh

# Reuse Kitty uploads when moving images, including images with padding.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, kitty
from image_kitty import KittyTerminal

cases = [
    'x=0,y=0,w=16,h=32,c=2,r=2',
    'x=1,y=3,w=15,h=31,c=2,r=2',
    'x=1,y=3,w=15,h=31,c=2,r=2,X=3,Y=5',
    'x=0,y=0,w=13,h=17',
    'x=0,y=0,w=1,h=1,c=1,r=1',
]
pixels = bytes(v for y in range(64) for x in range(32)
    for v in (x * 7 % 256, y * 13 % 256, (x + y) * 5 % 256, 255))

with image_test(sys.argv[1]) as test:
    client = test.attach(kitty_graphics=True)
    terminal = KittyTerminal()
    test.run('set-option', '-s', 'image-size-limit', str(len(pixels)))
    test.emit(kitty('a=t,i=7,f=32,s=32,v=64,m=1', pixels[:3072]) +
        kitty('m=1', pixels[3072:6144]) + kitty('m=0', pixels[6144:]))
    for controls in cases:
        put = kitty('a=p,i=7,p=1,C=1,' + controls)
        test.emit(b'\033[H' + put)
        terminal.feed(client.data)
        before = terminal.render()
        assert before, controls
        uploads = terminal.uploads
        for x, y in [(4, 3), (8, 5), (0, 0)]:
            test.emit(('\033[%d;%dH' % (y + 1, x + 1)).encode() + put)
            terminal.feed(client.data)
            terminal.check({(px + x * 8, py + y * 16): rgb
                for (px, py), rgb in before.items()})
            assert terminal.uploads == uploads, controls
        test.redraw()
        terminal.feed(client.data)
        terminal.check(before)
        assert terminal.uploads == uploads, controls

    # A different source rectangle must still prepare different pixels.
    test.emit(b'\033[H' + kitty('a=p,i=7,p=1,x=8,w=1,h=1,c=1,r=1,C=1'))
    terminal.feed(client.data)
    assert terminal.uploads == uploads + 1
    terminal.check({(x, y): (56, 0, 40) for y in range(16) for x in range(8)})
PY
