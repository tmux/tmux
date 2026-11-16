#!/bin/sh

# Kitty input crossing a scroll-region edge must be repainted through SIXEL,
# even when the terminal normally scrolls image pixels with its text.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, kitty
from image_terminal import Terminal

with image_test(sys.argv[1]) as test:
    test.run('set-option', '-as', 'terminal-features', ',*:imagescroll')
    client = test.attach()
    terminal = Terminal()
    colours = [(255, 0, 0), (0, 255, 0), (0, 0, 255)]
    data = bytes(c for colour in colours for c in (*colour, 255))
    expected = {(x, y): colours[(y - 32) // 16]
        for y in range(32, 80) for x in range(16, 48)}
    for operation in (b'\033[4;7r\033[S', b'\033[1;4r\033[T',
            b'\033[4;7r\033[4;1H\033M'):
        test.emit(b'\033[r\033[2J\033[3;3H' + kitty(
            'a=T,i=1,f=32,s=1,v=3,c=4,r=3,C=1', data))
        terminal.feed(client.data)
        terminal.check(expected)
        test.emit(operation)
        terminal.feed(client.data)
        terminal.check(expected)
PY
