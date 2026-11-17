#!/bin/sh

# Kitty transfer state, fragmented APCs, interleaved text and failed uploads.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, kitty
from image_terminal import Terminal


def check(test, client, expected):
    """Check immediate output and independently redrawn image pixels."""
    terminal = Terminal()
    terminal.feed(client.data)
    terminal.check(expected)
    test.redraw()
    terminal = Terminal()
    terminal.feed(client.data)
    terminal.check(expected)


red, blue = (255, 0, 0), (0, 0, 255)
with image_test(sys.argv[1]) as test:
    test.run('set-option', '-g', 'scroll-on-clear', 'off')
    client = test.attach()

    # Many separately encoded chunks reconstruct exactly the original pixels.
    w, h = 64, 32
    colours = [red if (x // 8 + y // 8) % 2 else blue
        for y in range(h) for x in range(w)]
    raw = bytes(c for pixel in colours for c in (*pixel, 255))
    command = kitty('a=T,i=7,p=9,C=1,f=32,s=64,v=32,m=1', raw[:128])
    for offset in range(128, len(raw), 128):
        more = int(offset + 128 < len(raw))
        command += kitty('m=%d' % more, raw[offset:offset + 128])
    test.emit(command)
    check(test, client, {(x, y): colours[y * w + x]
        for y in range(h) for x in range(w)})

    # A query must neither replace the existing source nor draw its own pixels.
    test.emit(kitty('a=q,i=7,f=32,s=1,v=1', b'\0\xff\0\xff') +
        b'\033[2J\033[H' + kitty('a=p,i=7,p=9,C=1'))
    expected = {(x, y): colours[y * w + x]
        for y in range(h) for x in range(w)}
    check(test, client, expected)

    # Failed decoding must leave the earlier source and its placement intact.
    for control, data in [('f=24,s=1,v=1', b'\xff\0'),
                           ('f=32,s=1,v=1,o=z', b'not zlib'),
                           ('f=100', b'not PNG')]:
        test.emit(kitty('a=T,i=7,p=9,C=1,' + control, data))
        test.redraw()
        terminal = Terminal()
        terminal.feed(client.data)
        terminal.check(expected)
        test.emit(b'\033[2J\033[H' + kitty('a=p,i=7,p=9,C=1'))
        check(test, client, expected)

    # Text and cursor movement between chunks do not reset the upload.
    test.emit(b'\033[2J\033[H' + kitty(
        'a=T,i=7,p=9,f=32,s=1,v=1,c=2,r=2,m=1', b'\xff\0\0'))
    assert not client.images(), 'incomplete upload drew an image'
    assert test.run('display-message', '-p', '#{cursor_x},#{cursor_y}') == '0,0'
    test.emit(b'between\033[3;5H')
    test.emit(kitty('m=0', b'\xff'))
    check(test, client, {(x, y): red
        for y in range(32, 64) for x in range(32, 48)})
    assert test.run('display-message', '-p', '#{cursor_x},#{cursor_y}') == '6,3'
    assert test.run('capture-pane', '-p') == 'between'

    # Split the introducer, controls, payload and terminator across writes.
    test.emit(b'\033[2J\033[H')
    wire = kitty('a=T,i=7,p=9,C=1,f=32,s=1,v=1,c=1,r=1', b'\0\0\xff\xff')
    cuts = [1, 2, 3, wire.index(b';'), wire.index(b';') + 3, len(wire) - 1]
    offset = 0
    for end in cuts:
        test.send(wire[offset:end])
        test.wait(lambda: test.run('capture-pane', '-p', '-P') ==
            wire[:end].decode())
        if end < len(wire) - 1:
            assert not client.images(), 'unfinished APC drew an image'
        offset = end
    # The input parser can dispatch the APC when it receives the ST's ESC.
    client.read(0.2)
    terminal = Terminal()
    terminal.feed(client.data)
    test.emit(wire[offset:])
    terminal.feed(client.data)
    expected = {(x, y): blue for y in range(16) for x in range(8)}
    terminal.check(expected)
    test.redraw()
    terminal = Terminal()
    terminal.feed(client.data)
    terminal.check(expected)
    assert test.run('capture-pane', '-p') == '', 'APC bytes appeared as text'
PY
