#!/bin/sh

# Image ordering and overlapping Kitty placements through SIXEL and fallback.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, kitty, sixel
from image_terminal import Terminal


red, green, blue, white = (255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255)


def upload(id, colour):
    """Upload a solid pixel under a Kitty image ID."""
    return kitty('a=t,i=%d,f=32,s=1,v=1' % id, bytes((*colour, 255)))


def put(id, controls=''):
    """Place a stored image in two columns without moving the cursor."""
    return kitty('a=p,i=%d,C=1,c=2,r=1' % id + controls)


def rectangle(colour, xoff=0, yoff=0, w=16, h=16):
    """Expected image pixels for a solid rectangle."""
    return {(x + xoff, y + yoff): colour for y in range(h) for x in range(w)}


def check(test, client, terminal, expected):
    """Check incremental output and a full redraw of the same scene."""
    terminal.feed(client.data)
    terminal.check(expected)
    test.redraw()
    terminal = Terminal()
    terminal.feed(client.data)
    terminal.check(expected)
    return terminal


with image_test(sys.argv[1]) as test:
    test.run('set-option', '-g', 'scroll-on-clear', 'off')
    client = test.attach()
    fallback = test.attach(graphical=False)
    terminal = Terminal()

    # The PR's tie-breaker cases: upload order wins over placement and app IDs.
    test.emit(upload(200, red) + upload(100, white) + put(100))
    terminal = check(test, client, terminal, rectangle(white))
    test.emit(put(200))
    terminal = check(test, client, terminal, rectangle(white))
    # Replacing the older source preserves its place in the drawing order.
    test.emit(upload(200, green) + put(200))
    terminal = check(test, client, terminal, rectangle(white))
    test.emit(kitty('a=d,d=I,i=200') + upload(200, red) + put(200))
    terminal = check(test, client, terminal, rectangle(red))
    # A retransmit-and-place also inherits the old source's drawing order.
    test.emit(b'\033[2J\033[H' + upload(200, red) + upload(100, white) +
        put(200) + put(100) + kitty('a=T,i=100,f=32,s=1,v=1,C=1,c=2,r=1',
        bytes((*green, 255))))
    # 200 was recreated above, so it is newer than 100 despite retransmission.
    terminal = check(test, client, terminal, rectangle(red))

    # Placements of the same source use placement order, including after reflow.
    pixels = bytes((*red, 255, *blue, 255))
    test.emit(b'\033[2J\033[H' + kitty('a=t,i=300,f=32,s=2,v=1', pixels) +
        put(300, ',p=99,x=1,w=1') + put(300, ',p=1,x=0,w=1'))
    terminal = check(test, client, terminal, rectangle(red))
    test.run('resize-window', '-x', '10')
    test.redraw()
    terminal = Terminal()
    terminal.feed(client.data)
    terminal.check(rectangle(red))
    test.run('resize-window', '-x', '40')

    # Replacing a named placement puts it after the other source placements.
    test.emit(put(300, ',p=99,x=1,w=1'))
    terminal = check(test, client, terminal, rectangle(blue))
    test.emit(kitty('a=d,d=i,i=300,p=99'))
    terminal = check(test, client, terminal, rectangle(red))

    # Both protocols use source creation order, even when placed later.
    graphics = sixel(16, 16, [(100, 100, 100)], [0] * 256)
    test.emit(b'\033[2J\033[H' + upload(400, red) + graphics +
        b'\033[H' + put(400))
    terminal = check(test, client, Terminal(), rectangle(white))
    # Replacing the older Kitty source leaves the newer SIXEL on top.
    test.emit(upload(400, green) + put(400))
    terminal = check(test, client, terminal, rectangle(white))
    test.emit(b'\033[2J\033[H' + graphics + b'\033[H' +
        upload(401, red) + put(401))
    terminal = check(test, client, Terminal(), rectangle(red))
    term = Terminal()
    term.feed(fallback.data)
    assert term.cells[0, 0][1:3] == (196, 196), term.cells[0, 0]
    # Removing the newer Kitty placement reveals the retained SIXEL image.
    test.emit(kitty('a=d,d=I,i=401'))
    terminal = check(test, client, terminal, rectangle(white))
    term = Terminal()
    term.feed(fallback.data)
    assert term.cells[0, 0][1:3] == (231, 231), term.cells[0, 0]

    # Transparency preserves the lower image across redraws and deletion.
    pixels = bytes(value for y in range(16) for x in range(16)
        for value in (*blue, 255 if x < 8 else 0))
    for base, colour in [(upload(500, red) + put(500), red), (graphics, white)]:
        test.emit(b'\033[2J\033[H' + base + b'\033[H' +
            kitty('a=T,i=501,f=32,s=16,v=16,C=1', pixels))
        terminal = check(test, client, Terminal(),
            rectangle(blue, w=8) | rectangle(colour, 8, w=8))
        test.emit(kitty('a=d,d=I,i=501'))
        terminal = check(test, client, terminal, rectangle(colour))

    # Rejected layers cannot replace stored pixels or remove placements.
    test.emit(b'\033[2J\033[H' + upload(600, red) + put(600))
    terminal = check(test, client, Terminal(), rectangle(red))
    for action in ('t', 'T', 'p', 'q', 'd'):
        test.emit(kitty('a=%s,i=600,p=9,f=32,s=1,v=1,C=1,d=I,z=1' %
            action, bytes((*green, 255))))
        terminal = check(test, client, terminal, rectangle(red))
    test.emit(kitty('a=p,i=600,p=9,C=1,c=2,r=1,z=0'))
    terminal = check(test, client, terminal, rectangle(red))

    # Images at the default layer cover text and explicit backgrounds.
    test.emit(b'\033[2J\033[H\033[44mX \033[0m\033[H' + put(600))
    check(test, client, Terminal(), rectangle(red))
    term = Terminal()
    term.feed(fallback.data)
    assert term.cells[0, 0][1:3] == (196, 196), term.cells[0, 0]
PY
