#!/bin/sh

# A cell changed under an image during a synchronized update must still show
# the image as text on a client without graphics when the update ends.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, kitty
from image_terminal import Terminal

with image_test(sys.argv[1]) as test:
    client = test.attach(graphical=False)
    test.emit(b'\033[3;3H' + kitty('a=T,i=7,f=32,s=1,v=1,c=4,r=3,C=1',
        bytes((255, 0, 0, 255))))
    test.redraw()
    terminal = Terminal()
    terminal.feed(client.data)
    before = terminal.cells[2, 2][0]
    assert before == '▄', before

    # Start and end the update in separate writes, so the cell is redrawn
    # when the update ends rather than with the rest of the pane.
    test.emit(b'\033[?2026h\033[3;3HX')
    terminal.feed(client.data)
    test.emit(b'\033[?2026l')
    terminal.feed(client.data)
    after = terminal.cells[2, 2][0]
    assert after == before, after
PY
