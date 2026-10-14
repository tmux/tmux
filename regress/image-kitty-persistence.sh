#!/bin/sh

# Kitty placements survive text writes and erases, but follow screen clears
# and scrolling. Overwritten SIXEL pixels can reveal an older Kitty image.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import base64
import sys
from image_test import image_test, sixel
from image_terminal import Terminal


def graphics(control, data=b''):
    """Build a Kitty graphics APC with replies suppressed."""
    payload = b';' + base64.b64encode(data) if data else b''
    return b'\033_Gq=2,' + control.encode() + payload + b'\033\\'


red, blue = (255, 0, 0), (0, 0, 255)
origin = b'\033[3;3H'
upload = graphics('a=t,i=7,f=32,s=1,v=1', bytes((*red, 255)))
place = origin + graphics('a=p,i=7,p=9,c=4,r=3,C=1')
cells = {(x, y): red for y in range(2, 5) for x in range(2, 6)}


def snapshot(test, client, expected):
    """Check image pixels after a full redraw."""
    test.redraw()
    terminal = Terminal()
    terminal.feed(client.data)
    terminal.check(expected)
    return terminal


def pixels(cells):
    """Expand solid image cells into positioned pixels."""
    return {(x * 8 + px, y * 16 + py): colour
        for (x, y), colour in cells.items()
        for py in range(16) for px in range(8)}


cases = [
    ('ASCII text', origin + b'ab'),
    ('wide text', origin + '\u754c'.encode()),
    ('erase characters', b'\033[3;4H\033[2X'),
    ('erase line end', b'\033[4;5H\033[K'),
    ('erase line start', b'\033[4;4H\033[1K'),
    ('erase whole line', b'\033[4;4H\033[2K'),
    ('erase display end', b'\033[4;5H\033[J'),
    ('erase display start', b'\033[4;5H\033[1J'),
    ('erase whole display', b'\033[2J'),
]
with image_test(sys.argv[1]) as test:
    test.run('set-option', '-g', 'scroll-on-clear', 'off')
    client = test.attach()
    fallback = test.attach(graphical=False)
    test.emit(upload)
    for name, operation in cases:
        test.emit(b'\033[2J' + place)
        wanted = pixels(cells)
        terminal = snapshot(test, client, wanted)
        test.emit(operation)
        if name == 'erase whole display':
            wanted = {}
        try:
            terminal.feed(client.data)
            terminal.check(wanted)
            snapshot(test, client, wanted)
            # Fallback must still show the retained image after a line erase.
            term = Terminal()
            term.feed(fallback.data)
            for x, y in cells:
                assert term.cells[x, y][2] == (196 if wanted else None), (
                    x, y, term.cells[x, y])
        except AssertionError as e:
            raise AssertionError(name) from e

    # Erasing a SIXEL layer reveals the Kitty placement retained beneath it.
    wire = sixel(32, 48, [(0, 0, 100)], [0] * (32 * 48))
    test.emit(b'\033[2J' + place + origin + wire)
    terminal = snapshot(test, client,
        pixels({point: blue for point in cells}))
    test.emit(b'\033[4;1H\033[2K')
    wanted = pixels({(x, y): red if y == 3 else blue for x, y in cells})
    terminal.feed(client.data)
    terminal.check(wanted)
    snapshot(test, client, wanted)

    # Kitty rows move with a scroll region, without moving the rows outside it.
    test.emit(b'\033[2J' + place + b'\033[4;7r\033[S')
    wanted = pixels({(x, y - (y >= 3)): red
        for x, y in cells if y != 3})
    snapshot(test, client, wanted)
    test.emit(b'\033[r')

    # With scroll-on-clear enabled, Kitty images move into history.
    test.emit(b'\033[2J')
    test.run('clear-history')
    test.run('set-option', '-g', 'scroll-on-clear', 'on')
    test.emit(place + b'\033[2J')
    snapshot(test, client, {})
    assert int(test.run('display-message', '-p', '#{history_size}')) > 0
    test.run('copy-mode')
    test.run('send-keys', '-X', 'history-top')
    snapshot(test, client, pixels(cells))
    test.run('send-keys', '-X', 'cancel')
    test.run('clear-history')
    test.run('copy-mode')
    test.run('send-keys', '-X', 'history-top')
    snapshot(test, client, {})
PY
