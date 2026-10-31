#!/bin/sh

# Native Kitty uploads, geometry, placement ordering, redraw and cleanup.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, kitty, sixel
from image_kitty import KittyTerminal

red, green, blue, white = (255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255)


def upload(id, colour):
    """Store a single pixel without placing it."""
    return kitty('a=t,i=%d,f=32,s=1,v=1' % id, bytes((*colour, 255)))


def put(id, controls=''):
    """Place two columns and one row without moving the cursor."""
    return kitty('a=p,i=%d,C=1,c=2,r=1' % id + controls)


def rectangle(colour, x=0, y=0, w=16, h=16):
    """Expected solid pixels at a terminal pixel position."""
    return {(x + xx, y + yy): colour for yy in range(h) for xx in range(w)}


with image_test(sys.argv[1]) as test:
    client = test.attach(kitty_graphics=True)
    terminal = KittyTerminal()
    test.emit(upload(200, red) + upload(100, white) + put(100))
    terminal.feed(client.data)
    terminal.check(rectangle(white))
    assert terminal.uploads == 1
    test.emit(put(200))
    terminal.feed(client.data)
    terminal.check(rectangle(white))
    assert terminal.uploads == 3, 'overlapping upper image was not replaced'
    test.redraw()
    terminal.feed(client.data)
    terminal.check(rectangle(white))
    assert terminal.uploads == 3, 'full redraw uploaded pixels again'
    # The older image keeps its order when replaced, but not when deleted.
    test.emit(upload(200, green) + put(200))
    terminal.feed(client.data)
    terminal.check(rectangle(white))
    test.emit(kitty('a=d,d=I,i=200') + upload(200, red) + put(200))
    terminal.feed(client.data)
    terminal.check(rectangle(red))
    # Native placements survive text writes, then disappear on deletion.
    test.emit(b'\033[Htext underneath')
    terminal.feed(client.data)
    terminal.check(rectangle(red))
    test.emit(kitty('a=d,d=A'))
    terminal.feed(client.data)
    terminal.check({})
    assert not terminal.images and not terminal.placements
    # SIXEL-origin images use the same backend and ordering.
    test.emit(b'\033[H' + sixel(16, 16, [(0, 0, 100)], [0] * 256))
    terminal.feed(client.data)
    terminal.check(rectangle(blue))
    test.emit(b'\033[H' + upload(300, red) + put(300))
    terminal.feed(client.data)
    terminal.check(rectangle(red))
    # A different window must not retain native placements from the first.
    client.clear()
    test.run('new-window', 'stty raw -echo; exec cat')
    client.read(0.3)
    terminal.feed(client.data)
    terminal.check({})
    assert not terminal.images, 'hidden uploads were retained in the terminal'
    client.clear()
    test.run('select-window', '-t', ':0')
    client.read(0.3)
    terminal.feed(client.data)
    terminal.check(rectangle(red))
    # Transparency is preserved across separately uploaded images.
    test.emit(kitty('a=T,i=400,f=32,s=1,v=1,c=2,r=1,C=1',
        bytes((0, 0, 255, 128))))
    terminal.feed(client.data)
    blended = rectangle((127, 0, 128))
    terminal.check(blended)
    # Detach deletes tmux's uploads before leaving the alternate screen.
    client.clear()
    test.run('detach-client', '-t', client.name)
    try:
        client.read(0.5)
    except RuntimeError as error:
        assert str(error) == 'attached client exited', error
    terminal.feed(client.data)
    terminal.check({})
    assert not terminal.images

# Source position, source size, subcell offsets, transparent padding and clips.
with image_test(sys.argv[1]) as test:
    other = test.attach(kitty_graphics=True, cw=10, ch=20)
    client = test.attach(kitty_graphics=True)
    terminal = KittyTerminal()
    term2 = KittyTerminal(cw=10, ch=20)
    data = bytes(v for y in range(8) for x in range(12)
        for v in ((red if x < 6 else green) if y < 4 else blue) + (255,))
    test.emit(kitty('a=T,i=1,f=32,s=12,v=8,x=6,y=0,w=6,h=4,'
        'c=2,r=2,X=3,Y=5,C=1', data))
    terminal.feed(client.data)
    expected = rectangle(green, x=3, y=5, w=13, h=27)
    terminal.check(expected)
    term2.feed(other.data)
    expected2 = rectangle(green, x=4, y=7, w=16, h=33)
    term2.check(expected2)
    # Inserting text cells must not move Kitty placements.
    test.emit(b'\033[1;2H\033[@')
    terminal.feed(client.data)
    terminal.check(expected)
    term2.feed(other.data)
    term2.check(expected2)
    uploads = terminal.uploads, term2.uploads
    test.redraw()
    terminal.feed(client.data)
    term2.feed(other.data)
    terminal.check(expected)
    term2.check(expected2)
    assert uploads == (terminal.uploads, term2.uploads)
    # A changed cell pixel size discards the old upload and prepares it again.
    test.resize(other, 40, 16, cw=12, ch=24)
    term2.cw, term2.ch = 12, 24
    term2.feed(other.data)
    term2.check(rectangle(green, x=5, y=8, w=19, h=40))
    assert term2.uploads > uploads[1]
PY
