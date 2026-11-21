#!/bin/sh

# Replace overlapping Kitty uploads together, leaving other uploads cached.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, kitty
from image_kitty import KittyTerminal

red, green, blue = (255, 0, 0), (0, 255, 0), (0, 0, 255)
white, yellow, grey = (255, 255, 255), (255, 255, 0), (128, 128, 128)


def upload(id, colour):
    """Upload new pixels under an application image ID."""
    return kitty('a=t,i=%d,f=32,s=1,v=1' % id, bytes((*colour, 255)))


def put(id, x=0, y=0, c=3, r=2):
    """Place an image at a cell position."""
    return ('\033[%d;%dH' % (y + 1, x + 1)).encode() + kitty(
        'a=p,i=%d,p=1,c=%d,r=%d,C=1' % (id, c, r))


def rectangle(colour, x=0, y=0, w=24, h=32):
    """Expected pixels at a terminal pixel position."""
    return {(x + xx, y + yy): colour for yy in range(h) for xx in range(w)}


def image_id(terminal, colour):
    """Find the current terminal upload for a solid colour."""
    return next(id for id, (_, _, data) in terminal.images.items()
        if tuple(data[:3]) == colour)


with image_test(sys.argv[1]) as test:
    client = test.attach(kitty_graphics=True)
    terminal = KittyTerminal()
    test.emit(upload(1, red) + upload(2, green) + upload(3, blue) +
        upload(4, white) + put(1) + put(2, x=2) + put(3, x=4, r=1) +
        put(4, x=10, c=2))
    terminal.feed(client.data)
    expected = rectangle(red) | rectangle(green, x=16) | \
        rectangle(blue, x=32, h=16) | rectangle(white, x=80, w=16)
    terminal.check(expected)
    assert terminal.uploads == 4
    untouched = image_id(terminal, white)
    test.redraw()
    terminal.feed(client.data)
    terminal.check(expected)
    assert terminal.uploads == 4

    # A overlaps B, B overlaps C, but A does not overlap C.
    # Replacing C must reach A through B, and restore their unaffected rows.
    test.emit(upload(3, yellow) + put(3, x=4, r=1))
    terminal.feed(client.data)
    expected = rectangle(red) | rectangle(green, x=16) | \
        rectangle(yellow, x=32, h=16) | rectangle(white, x=80, w=16)
    terminal.check(expected)
    assert terminal.uploads == 7, terminal.uploads
    assert image_id(terminal, white) == untouched
    group_ids = {image_id(terminal, colour) for colour in (red, green, yellow)}

    # Replacing a disjoint image leaves the overlapping group cached.
    test.emit(upload(4, grey) + put(4, x=10, c=2))
    terminal.feed(client.data)
    expected = rectangle(red) | rectangle(green, x=16) | \
        rectangle(yellow, x=32, h=16) | rectangle(grey, x=80, w=16)
    terminal.check(expected)
    assert terminal.uploads == 8
    assert group_ids <= terminal.images.keys()

    # A partial text redraw reuses every upload and retains the other rows.
    test.emit(b'\033[2;2Htext behind the images')
    terminal.feed(client.data)
    terminal.check(expected)
    test.redraw()
    terminal.feed(client.data)
    terminal.check(expected)
    assert terminal.uploads == 8

    # Removing the middle image does not require replacing its neighbours.
    test.emit(kitty('a=d,d=I,i=2'))
    terminal.feed(client.data)
    expected = rectangle(red) | rectangle(yellow, x=32, h=16) | \
        rectangle(grey, x=80, w=16)
    terminal.check(expected)
    assert terminal.uploads == 8

# Previously disjoint cached IDs may be in a different order. Scrolling one
# image over the other must repair the order without needing new source pixels.
with image_test(sys.argv[1]) as test:
    client = test.attach(kitty_graphics=True)
    terminal = KittyTerminal()
    test.emit(upload(1, red) + upload(2, blue) + put(1, c=2, r=3) +
        put(2, y=6, c=2, r=3))
    terminal.feed(client.data)
    terminal.check(rectangle(red, w=16, h=48) |
        rectangle(blue, y=96, w=16, h=48))
    test.emit(upload(1, yellow) + put(1, c=2, r=3))
    terminal.feed(client.data)
    terminal.check(rectangle(yellow, w=16, h=48) |
        rectangle(blue, y=96, w=16, h=48))
    assert terminal.uploads == 3
    assert image_id(terminal, yellow) > image_id(terminal, blue)
    test.emit(b'\033[3;12r\033[4S')
    terminal.feed(client.data)
    expected = rectangle(yellow, w=16, h=48) | \
        rectangle(blue, y=32, w=16, h=48)
    terminal.check(expected)
    assert terminal.uploads == 5
    assert image_id(terminal, yellow) < image_id(terminal, blue)
    test.redraw()
    terminal.feed(client.data)
    terminal.check(expected)
    assert terminal.uploads == 5

# Different placements of one source retain their own stacking and cache entry.
with image_test(sys.argv[1]) as test:
    client = test.attach(kitty_graphics=True)
    terminal = KittyTerminal()
    pixels = bytes((*red, 255, *blue, 255))
    test.emit(kitty('a=t,i=1,f=32,s=2,v=1', pixels) +
        kitty('a=p,i=1,p=99,x=1,w=1,c=2,r=2,C=1') +
        kitty('a=p,i=1,p=1,x=0,w=1,c=2,r=2,C=1'))
    terminal.feed(client.data)
    terminal.check(rectangle(red, w=16))
    assert terminal.uploads == 2
    test.redraw()
    terminal.feed(client.data)
    terminal.check(rectangle(red, w=16))
    assert terminal.uploads == 2
    test.emit(kitty('a=p,i=1,p=99,x=1,w=1,c=2,r=2,C=1'))
    terminal.feed(client.data)
    terminal.check(rectangle(blue, w=16))
    assert terminal.uploads == 4
    assert image_id(terminal, red) < image_id(terminal, blue)
    test.emit(kitty('a=d,d=i,i=1,p=99'))
    terminal.feed(client.data)
    terminal.check(rectangle(red, w=16))
    assert terminal.uploads == 4
    test.resize(client, 10, 16)
    terminal.sx = 10
    terminal.feed(client.data)
    terminal.check(rectangle(red, w=16))

PY
