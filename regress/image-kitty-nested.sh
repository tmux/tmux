#!/bin/sh

# Native Kitty output remains within tmux's supported input subset when nested.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import shlex
import sys
import time
from image_test import image_test, kitty
from image_kitty import KittyTerminal

red, green, white = (255, 0, 0), (0, 255, 0), (255, 255, 255)


def upload(id, colour):
    """Store a pixel under an application image ID."""
    return kitty('a=t,i=%d,f=32,s=1,v=1' % id, bytes((*colour, 255)))


def put(id):
    """Place two columns using a named placement."""
    return kitty('a=p,i=%d,p=1,C=1,c=2,r=1' % id)


def rectangle(colour, x=0, w=16):
    """Expected pixels after both tmux servers have rendered."""
    return {(px + x, y): colour for y in range(16) for px in range(w)}


with image_test(sys.argv[1]) as outer, image_test(sys.argv[1]) as inner:
    client = outer.attach(kitty_graphics=True)
    terminal = KittyTerminal()
    inner.run('set-option', '-as', 'terminal-features', ',*:kittygraphics')
    command = 'env TMUX= TERM=tmux-256color ' + shlex.join(
        inner.server + ['attach-session', '-t', 'image'])
    outer.run('respawn-pane', '-k', command)
    inner.wait(lambda: inner.run('list-clients', '-F', '#{client_name}') != '')
    client.read(0.3)
    terminal.feed(client.data)

    def expect(data, expected):
        """Wait for input and both stages of graphical output to finish."""
        client.clear()
        inner.emit(data)
        end = time.monotonic() + 10
        while True:
            client.read(0.05)
            terminal.feed(client.data)
            client.data.clear()
            if terminal.render() == expected or time.monotonic() > end:
                break
        terminal.check(expected)

    # An older source first becomes visible after its overlapping neighbour.
    expect(upload(200, red) + upload(100, white) + put(100), rectangle(white))
    expect(put(200), rectangle(white))
    # Replacement preserves drawing order through both servers.
    expect(upload(200, green) + put(200), rectangle(white))
    expect(b'\033[1;2H' + put(100), rectangle(green, w=8) |
        rectangle(white, x=8))
    expect(kitty('a=d,d=I,i=100'), rectangle(green))
    expect(kitty('a=d,d=A'), {})
PY
