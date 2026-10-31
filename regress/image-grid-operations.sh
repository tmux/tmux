#!/bin/sh

# Check image positions and erases, line operations, scroll regions,
# history cleanup and reflow next to wrapped text.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, sixel
from image_terminal import Terminal

palette = [(x * 30, y * 40, 100) for y in range(3) for x in range(4)]
rgb = [tuple((c * 255 + 50) // 100 for c in colour) for colour in palette]
wire = sixel(32, 48, palette,
    [y // 16 * 4 + x // 8 for y in range(48) for x in range(32)])
cells = {(x + 2, y + 2): rgb[y * 4 + x]
    for y in range(3) for x in range(4)}

def pixels(cells):
    """Expand solid image cells into positioned pixels."""
    return {(x * 8 + px, y * 16 + py): colour
        for (x, y), colour in cells.items()
        for py in range(16) for px in range(8)}

def snapshot(test, client, expected):
    """Check a full redraw independently of previous terminal contents."""
    test.redraw()
    terminal = Terminal()
    terminal.feed(client.data)
    terminal.check(pixels(expected))
    return terminal

cases = [
    ('erase characters', b'\033[3;4H\033[2X',
        {(x, y): c for (x, y), c in cells.items() if y != 2 or x not in (3, 4)}),
    ('erase line end', b'\033[4;5H\033[K',
        {(x, y): c for (x, y), c in cells.items() if y != 3 or x < 4}),
    ('erase line start', b'\033[4;4H\033[1K',
        {(x, y): c for (x, y), c in cells.items() if y != 3 or x > 3}),
    ('erase whole line', b'\033[4;4H\033[2K',
        {(x, y): c for (x, y), c in cells.items() if y != 3}),
    ('erase display end', b'\033[4;5H\033[J',
        {(x, y): c for (x, y), c in cells.items() if y < 3 or y == 3 and x < 4}),
    ('erase display start', b'\033[4;5H\033[1J',
        {(x, y): c for (x, y), c in cells.items() if y > 3 or y == 3 and x > 4}),
    ('erase whole display', b'\033[2J', {}),
    ('insert line', b'\033[4;1H\033[L',
        {(x, y + (y >= 3)): c for (x, y), c in cells.items()}),
    ('delete line', b'\033[4;1H\033[M',
        {(x, y - (y > 3)): c for (x, y), c in cells.items() if y != 3}),
    ('scroll region up', b'\033[3;6r\033[S',
        {(x, y - 1): c for (x, y), c in cells.items() if y > 2}),
    ('scroll region down', b'\033[3;6r\033[T',
        {(x, y + 1): c for (x, y), c in cells.items()}),
    ('reverse index', b'\033[3;6r\033[3;1H\033M',
        {(x, y + 1): c for (x, y), c in cells.items()}),
]
with image_test(sys.argv[1]) as test:
    client = test.attach()
    for name, operation, expected in cases:
        test.emit(b'\033[r\033[2J\033[3;3H' + wire)
        terminal = snapshot(test, client, cells)
        test.emit(operation)
        if name.startswith('erase'):
            # Check actual erase commands, before a full refresh can hide a bug.
            terminal.feed(client.data)
            terminal.check(pixels(expected))
        try:
            snapshot(test, client, expected)
        except AssertionError as e:
            raise AssertionError(name) from e

# Image lines stay whole while the surrounding text reflows.
with image_test(sys.argv[1], sx=16, sy=20) as test:
    client = test.attach()
    text = b'abcdefghijklmnopqrstuvwx'
    small = sixel(32, 32, palette,
        [y // 16 * 4 + x // 8 for y in range(32) for x in range(32)])
    test.emit(text + b'\r\n' + small + text + b'\r\n')
    for width in (8, 16, 8, 16):
        test.resize(client, width, 20)
        test.redraw()
        terminal = Terminal(sx=width, sy=20)
        terminal.feed(client.data)
        # Reflow can move the leading text into history to retain the cursor.
        top = (len(text) + width - 1) // width - int(test.run(
            'display-message', '-p', '#{history_size}'))
        assert top >= 0, top
        expected = {(x, y + top): rgb[y * 4 + x]
            for y in range(2) for x in range(4)}
        terminal.check(pixels(expected))
        captured = test.run('capture-pane', '-p', '-S', '-').replace('\n', '')
        assert captured == text.decode() * 2, captured

# Evicted or explicitly cleared history must not leave images in copy mode.
for evict in (False, True):
    with image_test(sys.argv[1], sy=6) as test:
        if evict:
            test.run('set-option', '-g', 'history-limit', '2')
            test.run('new-window', '-k', '-t', 'image:0',
                "stty raw -echo; printf '\033]2;READY2\007'; exec cat")
            test.wait(lambda: test.run('display-message', '-p',
                '#{pane_title}') == 'READY2')
        client = test.attach()
        test.emit(wire)
        assert client.images(), 'image missing before scrolling into history'
        test.emit(b'\033[12S')
        test.run('copy-mode')
        test.run('send-keys', '-X', 'history-top')
        test.redraw()
        if evict:
            assert not client.images(), 'evicted history retained an image'
        else:
            assert client.images(), 'image missing before clearing history'
            test.run('send-keys', '-X', 'cancel')
            test.run('clear-history')
            test.run('copy-mode')
            test.run('send-keys', '-X', 'history-top')
            test.redraw()
            assert not client.images(), 'cleared history retained an image'
PY
