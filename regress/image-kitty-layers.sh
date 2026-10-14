#!/bin/sh

# Kitty layers and source ordering through SIXEL and fallback output.

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


def put(id, z=0, controls=''):
    """Place a stored image in two columns without moving the cursor."""
    return kitty('a=p,i=%d,C=1,c=2,r=1,z=%d' % (id, z) + controls)


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
    test.emit(put(200, 2147483646) + put(100, 2147483647))
    terminal = check(test, client, terminal, rectangle(white))
    # Different layers override source order, including at the signed limits.
    test.emit(b'\033[2J\033[H' + put(100, 1) + put(200, 2))
    terminal = check(test, client, terminal, rectangle(red))
    test.emit(b'\033[2J\033[H' + put(100, -2147483648) + put(200, -1))
    terminal = check(test, client, terminal, rectangle(red))

    # A retransmit-and-place also inherits the old source's drawing order.
    test.emit(b'\033[2J\033[H' + upload(200, red) + upload(100, white) +
        put(200) + put(100) + kitty('a=T,i=100,f=32,s=1,v=1,C=1,c=2,r=1',
        bytes((*green, 255))))
    # 200 was recreated above, so it is newer than 100 despite retransmission.
    terminal = check(test, client, terminal, rectangle(red))

    # Replacing a placement can change its layer in either direction.
    test.emit(b'\033[2J\033[H' + put(100, 5, ',p=9') + put(200, 4, ',p=9'))
    terminal = check(test, client, terminal, rectangle(green))
    test.emit(put(100, 3, ',p=9'))
    terminal = check(test, client, terminal, rectangle(red))
    test.emit(put(100, 6, ',p=9'))
    terminal = check(test, client, terminal, rectangle(green))

    # Placements of the same source use placement order, including after reflow.
    pixels = bytes((*red, 255, *blue, 255))
    test.emit(b'\033[2J\033[H' + kitty('a=t,i=300,f=32,s=2,v=1', pixels) +
        put(300, 0, ',p=99,x=1,w=1') + put(300, 0, ',p=1,x=0,w=1'))
    terminal = check(test, client, terminal, rectangle(red))
    test.run('resize-window', '-x', '10')
    test.redraw()
    terminal = Terminal()
    terminal.feed(client.data)
    terminal.check(rectangle(red))
    test.run('resize-window', '-x', '40')

    # SIXEL stays above Kitty at every layer, regardless of arrival order.
    for z in [-1, 0, 2147483647]:
        for first in (True, False):
            graphics = sixel(16, 16, [(100, 100, 100)], [0] * 256)
            commands = put(200, z) + b'\033[H' + graphics if first else \
                graphics + b'\033[H' + put(200, z)
            test.emit(b'\033[2J\033[H' + commands)
            check(test, client, Terminal(), rectangle(white))
    test.emit(b'\033[2J\033[H' + put(100, -1) + put(200, -2))
    check(test, client, Terminal(), rectangle(green))

    # A negative image retains its source alignment around text and decoration.
    pixels = bytes(value for y in range(16) for x in range(24)
        for value in (*(red if x < 8 else green if x < 16 else blue), 255))
    test.emit(b'\033[2J\033[H' + kitty('a=T,i=400,f=32,s=24,v=16,C=1,z=-1',
        pixels))
    terminal = check(test, client, Terminal(), rectangle(red, w=8) |
        rectangle(green, 8, w=8) | rectangle(blue, 16, w=8))
    test.emit(b'\033[1;2HX')
    terminal = check(test, client, terminal, rectangle(red, w=8) |
        rectangle(blue, 16, w=8))
    assert terminal.cells[1, 0][0] == 'X', terminal.cells[1, 0]
    test.emit(b'\033[1;2H\033[4m \033[0m')
    terminal = check(test, client, terminal, rectangle(red, w=8) |
        rectangle(blue, 16, w=8))
    test.emit(b'\033[1;2H ')
    terminal = check(test, client, terminal, rectangle(red, w=8) |
        rectangle(green, 8, w=8) | rectangle(blue, 16, w=8))

    # Negative images sit above explicit cell backgrounds unless below -2**30.
    for z in [-1, -1073741824, -1073741825, -2147483648]:
        test.emit(b'\033[2J\033[H\033[44m  \033[0m\033[H' + put(200, z))
        test.redraw()
        term = Terminal()
        term.feed(fallback.data)
        if z >= -1073741824:
            assert term.cells[0, 0][1:3] == (196, 196), term.cells[0, 0]
            image = Terminal()
            image.feed(client.data)
            image.check(rectangle(red))
        else:
            assert term.cells[0, 0][0] == ' ' and term.cells[0, 0][2] == 4, \
                term.cells[0, 0]
            image = Terminal()
            image.feed(client.data)
            image.check({})
        # Deep negative layers remain visible on default-background spaces.
        test.emit(b'\033[2J\033[H' + put(200, z))
        check(test, client, Terminal(), rectangle(red))

    # Changing the background hides and reveals a retained placement.
    test.emit(b'\033[2J\033[H\033[44m  \033[0m\033[H' +
        put(200, -2147483648, ',p=9'))
    terminal = check(test, client, Terminal(), {})
    test.emit(b'\033[H  ')
    terminal = check(test, client, terminal, rectangle(red))
    fallback_term = Terminal()
    fallback_term.feed(fallback.data)
    assert fallback_term.cells[0, 0][1:3] == (196, 196), \
        fallback_term.cells[0, 0]
    test.emit(b'\033[H\033[44m  \033[0m')
    terminal = check(test, client, terminal, {})
    test.emit(b'\033[H' + put(200, 0, ',p=9'))
    check(test, client, terminal, rectangle(red))

    # Fallback retains text above negative layers; layer zero covers it.
    for z in [-1, 0]:
        test.emit(b'\033[2J\033[HX' + b'\033[H' + put(200, z))
        test.redraw()
        term = Terminal()
        term.feed(fallback.data)
        if z < 0:
            assert term.cells[0, 0][0] == 'X', term.cells[0, 0]
        else:
            assert term.cells[0, 0][1:3] == (196, 196), term.cells[0, 0]
PY
