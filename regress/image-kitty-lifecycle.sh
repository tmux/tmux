#!/bin/sh

# Kitty placement recovery, reflow, alternate screens and pane-local IDs.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, kitty
from image_terminal import Terminal


def snapshot(test, client, expected, sx=40, sy=16):
    """Check visible image pixels after an independent redraw."""
    test.redraw()
    terminal = Terminal(sx=sx, sy=sy)
    terminal.feed(client.data)
    terminal.check(expected)


red, green, blue = (255, 0, 0), (0, 255, 0), (0, 0, 255)
raw = bytes(c for y in range(32) for x in range(32)
    for c in (*(red if x < 16 else blue), 255))
upload = kitty('a=t,i=7,f=32,s=32,v=32', raw)
place = kitty('a=p,i=7,p=9,C=1')
expected = {(x, y): red if x < 16 else blue
    for y in range(32) for x in range(32)}

with image_test(sys.argv[1]) as test:
    client = test.attach()
    test.emit(upload + place)
    for width in (1, 40, 2, 40):
        test.resize(client, width, 16)
        snapshot(test, client,
            {point: colour for point, colour in expected.items()
                if point[0] < width * 8}, sx=width)
    # Recovering a clipped named placement must still allow its replacement.
    test.emit(b'\033[4;5H' + place)
    snapshot(test, client,
        {(x + 32, y + 48): colour for (x, y), colour in expected.items()})

    # An alternate screen has its own grid, while the original grid is retained.
    test.emit(b'\033[?1049h')
    snapshot(test, client, {})
    test.emit(b'\033[H' + kitty('a=T,i=8,p=9,f=32,s=1,v=1,C=1,c=1,r=1',
        bytes((*green, 255))))
    snapshot(test, client,
        {(x, y): green for y in range(16) for x in range(8)})
    test.emit(b'\033[?1049l')
    snapshot(test, client,
        {(x + 32, y + 48): colour for (x, y), colour in expected.items()})

# Kitty image rows retain their width while adjacent wrapped text reflows.
with image_test(sys.argv[1], sx=16, sy=20) as test:
    client = test.attach()
    text = b'abcdefghijklmnopqrstuvwx'
    test.emit(text + b'\r\n' + upload + kitty('a=p,i=7,p=9') +
        b'\r\n' + text + b'\r\n')
    for width in (8, 16, 8, 16):
        test.resize(client, width, 20)
        top = (len(text) + width - 1) // width - int(test.run(
            'display-message', '-p', '#{history_size}'))
        assert top >= 0, top
        snapshot(test, client,
            {(x, y + top * 16): colour for (x, y), colour in expected.items()},
            sx=width, sy=20)
        captured = test.run('capture-pane', '-p', '-S', '-').replace('\n', '')
        assert captured == text.decode() * 2, captured

# Equal application image and placement IDs in separate panes are independent.
with image_test(sys.argv[1]) as test:
    client = test.attach()
    first = test.run('display-message', '-p', '#{pane_id}')
    second = test.run('split-window', '-d', '-P', '-F', '#{pane_id}',
        "stty raw -echo; printf '\033]2;SECOND\007'; exec cat")
    test.wait(lambda: test.run('display-message', '-pt', second,
        '#{pane_title}') == 'SECOND')
    placed = {}
    for pane, colour in [(first, red), (second, blue)]:
        test.run('select-pane', '-t', pane)
        test.emit(kitty('a=T,i=7,p=9,f=32,s=1,v=1,C=1,c=1,r=1',
            bytes((*colour, 255))))
        x, y = map(int, test.run('display-message', '-pt', pane,
            '#{pane_left},#{pane_top}').split(','))
        placed.update({(x * 8 + px, y * 16 + py): colour
            for py in range(16) for px in range(8)})
    snapshot(test, client, placed)
    test.run('select-pane', '-t', first)
    test.emit(kitty('a=T,i=7,p=9,f=32,s=1,v=1,C=1,c=1,r=1',
        bytes((*green, 255))))
    placed = {point: green if colour == red else colour
        for point, colour in placed.items()}
    snapshot(test, client, placed)
    test.run('kill-pane', '-t', second)
    snapshot(test, client,
        {(x, y): green for y in range(16) for x in range(8)})
    # Respawning removes the previous application's placements and sources.
    test.run('respawn-pane', '-k', '-t', first,
        "stty raw -echo; printf '\033]2;RESPAWNED\007'; exec cat")
    test.wait(lambda: test.run('display-message', '-p',
        '#{pane_title}') == 'RESPAWNED')
    snapshot(test, client, {})
    test.emit(kitty('a=p,i=7,p=9,C=1'))
    snapshot(test, client, {})
PY
