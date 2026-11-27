#!/bin/sh

# Split prepared Kitty canvases without exceeding terminal dimension limits.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PYTEST'
import sys
from image_test import image_test, kitty
from image_kitty import KittyTerminal


def colour(x, y):
    """Distinguish pixels on both sides of each upload boundary."""
    return (x % 251, y % 251, (x + y) % 251)


for vertical in (False, True):
    for length in (9998, 10000, 10020, 20000):
        w, h, cw, ch = length, 2, 2, 2
        if vertical:
            w, h, ch = 2, length, 20
            if length == 9998:
                ch = 4999
        sx, sy = max(4, w // cw), max(4, h // ch)
        with image_test(sys.argv[1], sx=sx, sy=sy) as test:
            client = test.attach(kitty_graphics=True, cw=cw, ch=ch)
            terminal = KittyTerminal(sx=sx, sy=sy, cw=cw, ch=ch)
            pixels = bytes(v for y in range(h) for x in range(w)
                for v in (*colour(x, y), 255))
            test.emit(kitty('a=T,i=7,f=32,s=%d,v=%d,c=%d,r=%d,C=1' %
                (w, h, w // cw, h // ch), pixels))
            terminal.feed(client.data)
            assert all(ww <= 10000 and hh <= 10000
                for ww, hh, _ in terminal.images.values())
            visible = {(x, y): colour(x, y)
                for y in range(h) for x in range(w)}
            terminal.check(visible)
            assert sum(ww * hh for ww, hh, _ in
                terminal.images.values()) == w * h
            uploads = terminal.uploads
            assert uploads == (length + 9999) // 10000
            test.redraw()
            terminal.feed(client.data)
            terminal.check(visible)
            assert terminal.uploads == uploads

            if length <= 10000:
                continue

            # Cover a cell on each side of an upload boundary.
            x, y, fw, fh = 4999, 0, 2, 1
            if vertical:
                x, y, fw, fh = 0, 499, 1, 2
            client.clear()
            pane = test.run('new-pane', '-d', '-P', '-F', '#{pane_id}',
                '-B', 'none', '-x', str(fw), '-y', str(fh),
                '-X', str(x), '-Y', str(y), 'stty raw -echo; exec cat')

            def read_terminal():
                """Read new commands and compose the visible pixels."""
                terminal.feed(client.data)
                client.data.clear()
                result = terminal.render()
                return result

            covered = {(px, py): rgb for (px, py), rgb in visible.items()
                if not (x * cw <= px < (x + fw) * cw and
                    y * ch <= py < (y + fh) * ch)}
            test.wait(lambda: read_terminal() == covered)
            test.run('kill-pane', '-t', pane)
            test.wait(lambda: read_terminal() == visible)
            assert all(ww <= 10000 and hh <= 10000
                for ww, hh, _ in terminal.images.values())

            # A newer image overlaps both upload rectangles.
            test.emit(('\033[%d;%dH' % (y + 1, x + 1)).encode() +
                kitty('a=T,i=8,f=32,s=1,v=1,c=%d,r=%d,C=1' % (fw, fh),
                    bytes((0, 0, 255, 255))))
            terminal.feed(client.data)
            overlap = dict(covered)
            overlap.update({(px, py): (0, 0, 255)
                for py in range(y * ch, (y + fh) * ch)
                for px in range(x * cw, (x + fw) * cw)})
            terminal.check(overlap)
            test.redraw()
            terminal.feed(client.data)
            terminal.check(overlap)
            test.emit(kitty('a=d,d=I,i=8'))
            terminal.feed(client.data)
            terminal.check(visible)


# Each client prepares rectangles for its own cell pixel size.
with image_test(sys.argv[1], sx=4, sy=500) as test:
    clients = [test.attach(kitty_graphics=True, cw=2, ch=ch)
        for ch in (18, 24)]
    test.emit(kitty('a=T,i=7,f=32,s=1,v=1,c=1,r=500,C=1',
        bytes((255, 0, 0, 255))))
    for client, ch in zip(clients, (18, 24)):
        terminal = KittyTerminal(sx=4, sy=500, cw=2, ch=ch)
        terminal.feed(client.data)
        terminal.check({(x, y): (255, 0, 0)
            for y in range(500 * ch) for x in range(2)})
        assert terminal.uploads == (500 * ch + 9999) // 10000
        assert sum(w * h for w, h, _ in terminal.images.values()) == 1000 * ch
        assert all(w <= 10000 and h <= 10000
            for w, h, _ in terminal.images.values())


# A tall cursor-moving image keeps its upper rows in history.
with image_test(sys.argv[1], sx=12, sy=16) as test:
    client = test.attach(kitty_graphics=True, cw=2, ch=20)
    terminal = KittyTerminal(sx=12, sy=16, cw=2, ch=20)

    def read_terminal():
        """Read new commands and compose the visible pixels."""
        terminal.feed(client.data)
        client.data.clear()
        result = terminal.render()
        return result

    def viewport(offset):
        """Expected pixels for a vertical position within the source."""
        return {(x, y): colour(0, offset + y)
            for y in range(320) for x in range(2)}

    pixels = bytes(v for y in range(16000) for v in (*colour(0, y), 255))
    test.emit(kitty('a=T,i=7,f=32,s=1,v=16000,c=1,r=800', pixels))
    assert read_terminal() == viewport(15680)
    test.run('copy-mode')
    test.run('send-keys', '-X', 'history-top')
    test.wait(lambda: read_terminal() == viewport(0))
    test.run('send-keys', '-X', '-N', '495', 'scroll-down')
    test.wait(lambda: read_terminal() == viewport(9900))
    test.run('send-keys', '-X', 'cancel')
    test.wait(lambda: read_terminal() == viewport(15680))
    assert all(ww <= 10000 and hh <= 10000
        for ww, hh, _ in terminal.images.values())
PYTEST
