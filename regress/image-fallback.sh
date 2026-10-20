#!/bin/sh

# SIXEL input is visible as text on ASCII and UTF-8 clients, including
# copy-mode selection and a graphical client with no known cell pixel size.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 -B - "$TEST_TMUX" <<'PY'
import re
import sys
from image_test import image_test, sixel
from image_terminal import Terminal

white = sixel(16, 16, [(100, 100, 100)], [0] * (16 * 16))

for utf8 in (False, True):
    with image_test(sys.argv[1]) as test:
        client = test.attach(graphical=False, utf8=utf8)
        char = b'\xe2\x96\x84' if utf8 else b'@'
        test.emit(white)
        assert not client.images(), 'graphical output on a fallback client'
        assert client.data.count(char) >= 2, (utf8, bytes(client.data))
        test.run('copy-mode')
        test.run('send-keys', '-X', 'history-top')
        test.run('send-keys', '-X', 'start-of-line')
        test.run('send-keys', '-X', 'begin-selection')
        test.run('send-keys', '-X', 'cursor-right')
        test.redraw()
        assert not client.images()
        assert client.data.count(char) >= 2, ('selection lost fallback', utf8)
        test.run('send-keys', '-X', 'cancel')
        test.emit(b'\033[2J\033[H')
        assert not client.images()
        # Clearing must also remove the image from later full redraws.
        test.redraw()
        assert char not in client.data, ('cleared fallback reappeared', utf8)

with image_test(sys.argv[1]) as test:
    client = test.attach(graphical=True, utf8=False, cw=0, ch=0)
    test.emit(white)
    assert not client.images(), 'SIXEL sent without a cell pixel size'
    # Default pixel sizes may leave padding, so the brightness character
    # need not be '@'. Remove terminal controls before checking the text.
    text = re.sub(rb'\033\[[0-?]*[ -/]*[@-~]', b'', client.data)
    text = re.sub(rb'\033\][^\007]*?(?:\007|\033\\)', b'', text)
    text = re.sub(rb'\033.', b'', text)
    assert any(bytes([ch]) in text for ch in b'.:-=+*#%@'), (
        'missing fallback with unknown pixel size', text)

# Every ASCII brightness character is reachable, in the right grid cell.
levels = [0, 12, 23, 34, 45, 56, 67, 78, 89, 100]
with image_test(sys.argv[1]) as test:
    client = test.attach(graphical=False, utf8=False)
    test.emit(sixel(80, 16, [(v, v, v) for v in levels],
        [x // 8 for y in range(16) for x in range(80)]))
    test.redraw()
    terminal = Terminal()
    terminal.feed(client.data)
    actual = ''.join(terminal.cells[x, 0][0] for x in range(10))
    assert actual == ' .:-=+*#%@', actual

# The lower-half character uses the upper sample as background and the
# lower sample as foreground. Check true colour and 256-colour mapping.
for truecolour in (False, True):
    with image_test(sys.argv[1]) as test:
        if truecolour:
            test.run('set-option', '-as', 'terminal-features',
                'screen-256color:RGB')
        client = test.attach(graphical=False)
        test.emit(b'\033[3;4H' + sixel(8, 16,
            [(100, 0, 0), (0, 0, 100)],
            [y >= 8 for y in range(16) for x in range(8)]))
        test.redraw()
        terminal = Terminal()
        terminal.feed(client.data)
        char, fg, bg, bold = terminal.cells[3, 2]
        assert char == '\u2584' and not bold, (char, fg, bg, bold)
        if truecolour:
            expected = ((0, 0, 255), (255, 0, 0))
        else:
            expected = (21, 196)
        assert (fg, bg) == expected, (truecolour, fg, bg)

# Eight-colour UTF-8 terminals use shades, with bold supplying extra levels
# unless Nobr disables it. Force the capabilities without extra terminfo files.
for nobr in (False, True):
    with image_test(sys.argv[1]) as test:
        overrides = 'screen:colors=8:Nobr=' if nobr else 'screen:colors=8:Nobr@'
        test.run('set-option', '-as', 'terminal-overrides', overrides)
        client = test.attach(graphical=False, term='screen')
        levels = [0, 14, 29, 43, 57, 71, 86, 100]
        test.emit(sixel(64, 16, [(v, v, v) for v in levels],
            [x // 8 for y in range(16) for x in range(64)]))
        test.redraw()
        terminal = Terminal()
        terminal.feed(client.data)
        chars = ''.join(terminal.cells[x, 0][0] for x in range(8))
        bold = [terminal.cells[x, 0][3] for x in range(8)]
        if nobr:
            assert chars == ' \u2591\u2591\u2592\u2592\u2593\u2593\u2588', chars
            assert not any(bold), bold
        else:
            assert chars == ' \u2591\u2591\u2592\u2592\u2593\u2588\u2588', chars
            assert bold == [False, False, True, False, True, False, False, True], bold
# Odd cell heights split a source pixel between the two fallback samples.
with image_test(sys.argv[1]) as test:
    test.run('set-option', '-as', 'terminal-features', 'screen-256color:RGB')
    client = test.attach(graphical=False, ch=15)
    test.emit(sixel(8, 15, [(100, 0, 0), (0, 0, 100)],
        [y >= 8 for y in range(15) for x in range(8)]))
    test.redraw()
    terminal = Terminal(ch=15)
    terminal.feed(client.data)
    char, fg, bg, bold = terminal.cells[0, 0]
    assert char == '\u2584' and not bold, (char, bold)
    assert (fg, bg) == ((17, 0, 238), (255, 0, 0)), (fg, bg)
PY
