#!/bin/sh

# With a left side status line, images are drawn and erased in the pane's
# columns: a full redraw, a damaged area and a pane redraw put the image
# beside the side status line and do not erase the side status line.

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

def side_cells(terminal):
    return {(x, y): terminal.cells.get((x, y))
        for y in range(16) for x in range(side)}

with image_test(sys.argv[1]) as test:
    test.run("set-option", "-g", "side-status", "left")
    test.run("set-option", "-g", "side-status-width", str(side))
    client = test.attach()
    test.emit(b'\033[3;3H' + wire)

    # A full redraw.
    test.redraw()
    terminal = Terminal()
    terminal.feed(client.data)
    terminal.check(placed(side + 2, 2))
    cells = side_cells(terminal)
    assert all(cells[side - 1, y] not in (None, ' ') for y in range(16)), \
        'side status line not drawn'

    # Inserting a line above the image redraws the damaged area.
    test.emit(b'\033[3;1H\033[L')
    terminal.feed(client.data)
    terminal.check(placed(side + 2, 3))
    assert side_cells(terminal) == cells, 'side status erased by damage'

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
    terminal.check(placed(side + 2, 3))
    assert side_cells(terminal) == cells, 'side status erased by pane redraw'
PY
