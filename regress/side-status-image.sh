#!/bin/sh

# Images are drawn and erased in the pane's columns with either side status
# position and with the horizontal status off, above or below the window.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, sixel
from image_terminal import Terminal

side = 14
w, h = 16, 32
wire = sixel(w, h, [(100, 0, 0)], [0] * (w * h))

def placed(cx, cy):
    return {(cx * 8 + x, cy * 16 + y): (255, 0, 0)
        for y in range(h) for x in range(w)}

def side_cells(terminal, xoff, yoff, height):
    return {(x, y): terminal.cells.get((x, y))
        for y in range(yoff, yoff + height)
        for x in range(xoff, xoff + side)}

def check(position, status):
    with image_test(sys.argv[1]) as test:
        test.run("set-option", "-g", "side-status", position)
        test.run("set-option", "-g", "side-status-width", str(side))
        if status != "off":
            test.run("set-option", "-g", "status", "2")
            test.run("set-option", "-g", "status-position", status)
        xoff = side if position == "left" else 0
        yoff = 2 if status == "top" else 0
        height = 16 if status == "off" else 14
        side_x = 0 if position == "left" else 40 - side
        separator = side - 1 if position == "left" else side_x
        client = test.attach()
        test.emit(b'\033[3;3H' + wire)

        # A full redraw.
        test.redraw()
        terminal = Terminal()
        terminal.feed(client.data)
        terminal.check(placed(xoff + 2, yoff + 2))
        cells = side_cells(terminal, side_x, yoff, height)
        assert all(cells[separator, y] not in (None, ' ')
            for y in range(yoff, yoff + height)), \
            (position, status, 'side status line not drawn')

        # Inserting a line above the image redraws the damaged area.
        test.emit(b'\033[3;1H\033[L')
        terminal.feed(client.data)
        terminal.check(placed(xoff + 2, yoff + 3))
        assert side_cells(terminal, side_x, yoff, height) == cells, \
            (position, status, 'side status erased by damage')

        # Leaving copy mode redraws only the pane.
        client.clear()
        test.run("copy-mode")
        test.wait(lambda: test.run("display-message", "-p",
            "#{pane_in_mode}") == "1")
        test.run("send-keys", "-X", "cancel")
        test.wait(lambda: test.run("display-message", "-p",
            "#{pane_in_mode}") == "0")
        client.read(0.2)
        terminal.feed(client.data)
        terminal.check(placed(xoff + 2, yoff + 3))
        assert side_cells(terminal, side_x, yoff, height) == cells, \
            (position, status, 'side status erased by pane redraw')

for position in ("left", "right"):
    for status in ("off", "top", "bottom"):
        check(position, status)
PY
