#!/bin/sh

# Shared compressed crops retain their uploads through clipping and hiding.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import os
import sys
from image_test import image_test, kitty
from image_kitty import KittyTerminal


def colour(x, y):
    """Source pixels distinguish both crop coordinates."""
    return (x // 8 * 60, y // 16 * 60, 128)


def crop(x, y, placement=1):
    """Display a source crop under a named placement."""
    data = kitty('a=p,i=7,p=%d,x=%d,y=%d,w=16,h=32,c=2,r=2,C=1' %
        (placement, x, y))
    return data


def expected(x, y, dy=0):
    """Expected pixels at the terminal's one-to-one source scale."""
    return {(px, py + dy): colour(x + px, y + py)
        for py in range(32) for px in range(16)}


with image_test(sys.argv[1]) as test:
    client = test.attach(kitty_graphics=True, reply_da=False)
    os.write(client.fd, b'\033_Gi=32;OK\033\\\033[?65cZ')
    test.wait(lambda: test.run('capture-pane', '-p') == 'Z')
    terminal = KittyTerminal()

    def read_terminal():
        """Read new commands and compose the visible graphics."""
        terminal.feed(client.data)
        client.data.clear()
        pixels = terminal.render()
        return pixels

    pixels = bytes(v for y in range(64) for x in range(32)
        for v in (*colour(x, y), 255))
    test.emit(b'\033[H' + kitty('a=t,i=7,f=32,s=32,v=64', pixels) +
        crop(0, 0))
    assert read_terminal() == expected(0, 0)
    test.emit(b'\033[H' + crop(8, 16))
    assert read_terminal() == expected(8, 16)
    assert terminal.uploads == 2
    test.emit(b'\033[5;1H' + crop(0, 32, placement=2))
    visible = expected(8, 16) | expected(0, 32, dy=64)
    assert read_terminal() == visible
    assert terminal.uploads == 2
    assert len({image for image, _ in terminal.placements}) == 1
    headers = [c for c in terminal.commands if b'f' in c]
    assert len(headers) == 2 and all(c.get(b'o') == b'z' for c in headers)

    # Account for shared pixels once when applying the retention budget.
    count = len(terminal.images)
    size = sum(w * h * 4 for w, h, _ in terminal.images.values())
    test.run('set-option', '-s', 'image-total-size-limit', str(size))
    test.run('new-window', 'stty raw -echo; exec cat')
    test.wait(lambda: read_terminal() == {})
    assert len(terminal.images) == count
    test.run('select-window', '-t', ':0')
    test.wait(lambda: read_terminal() == visible)
    assert len(terminal.images) == count and terminal.uploads == 2

    floating = test.run('new-pane', '-d', '-P', '-F', '#{pane_id}',
        '-B', 'none', '-x', '2', '-y', '2', '-X', '0', '-Y', '0',
        'stty raw -echo; exec cat')
    test.wait(lambda: read_terminal() == expected(0, 32, dy=64))
    test.run('kill-pane', '-t', floating)
    test.wait(lambda: read_terminal() == visible)
    assert terminal.uploads == 2

    # An idle refresh can evict shared uploads; redisplay prepares them again.
    test.run('select-window', '-t', ':1')
    test.wait(lambda: read_terminal() == {})
    test.run('set-option', '-s', 'image-total-size-limit', '1')
    test.run('refresh-client', '-t', client.name)
    test.wait(lambda: read_terminal() == {} and not terminal.images)
    test.run('set-option', '-s', 'image-total-size-limit', str(size))
    test.run('select-window', '-t', ':0')
    test.wait(lambda: read_terminal() == visible)
    assert terminal.uploads == 4
PY
