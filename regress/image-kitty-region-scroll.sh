#!/bin/sh

# Kitty placements follow terminal scrolls only with imagescroll enabled.
# Check partial redraws after scrolling as well as placement reuse.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, kitty
from image_kitty import KittyTerminal
from image_terminal import Terminal

red = (255, 0, 0)


def rectangle(y, h=48):
    """Expected pixels for the visible part of the image."""
    return {(x, py): red for py in range(y, y + h) for x in range(16)}


for scrolls in (False, True):
    with image_test(sys.argv[1]) as test:
        test.run('set-option', '-as', 'terminal-features',
            ',*:imagescroll' + ('' if scrolls else '@'))
        client = test.attach(kitty_graphics=True)
        terminal = KittyTerminal(scroll_images=scrolls)
        test.emit(b'\033[5;1H' + kitty(
            'a=T,i=1,f=32,s=1,v=1,c=2,r=3,C=1', bytes((*red, 255))))
        terminal.feed(client.data)
        terminal.check(rectangle(64))
        uploads = terminal.uploads
        # A linefeed at the bottom of the scroll region moves the image up.
        test.emit(b'\033[3;12r\033[12;1H\n')
        terminal.commands.clear()
        terminal.feed(client.data)
        terminal.check(rectangle(48))
        placed = any(c.get(b'a') == b'p' for c in terminal.commands)
        assert placed != scrolls, ('imagescroll placement reuse', scrolls)
        assert terminal.uploads == uploads
        # Redraw one row after the terminal moved the cached placements.
        test.emit(b'\033[4;2Htext under image')
        terminal.feed(client.data)
        terminal.check(rectangle(48))
        # Reverse index and explicit scrolling move the same placements.
        test.emit(b'\033[3;1H\033M')
        terminal.feed(client.data)
        terminal.check(rectangle(64))
        test.emit(b'\033[3S')
        terminal.feed(client.data)
        terminal.check(rectangle(32, 32))
        test.emit(b'\033[2T')
        terminal.feed(client.data)
        terminal.check(rectangle(64, 32))
        # Deletion after scrolling must leave no stale terminal placements.
        test.emit(kitty('a=d,d=A'))
        terminal.feed(client.data)
        terminal.check({})
        assert not terminal.placements and not terminal.images

# A placement crossing a scroll-region edge remains fixed. This requires a
# repaint even when the terminal normally moves images with scrolling.
for graphics in (False, True):
    with image_test(sys.argv[1]) as test:
        test.run('set-option', '-as', 'terminal-features', ',*:imagescroll')
        client = test.attach(kitty_graphics=graphics)
        if graphics:
            terminal = KittyTerminal(scroll_images=True)
        else:
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
