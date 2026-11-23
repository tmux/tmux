#!/bin/sh

# Fold controls must preserve visible image spans and hide collapsed ones.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, kitty
from image_terminal import Terminal

for text in (b'', b'A\xe4\xb8\xadD'):
    with image_test(sys.argv[1]) as test:
        client = test.attach()
        fallback = test.attach(graphical=False)
        test.run('set-option', '-g', 'copy-mode-position-format', '')
        test.run('set-option', '-g', 'copy-mode-exit-status-format', '')
        test.run('set-option', '-g', 'scroll-on-clear', 'off')
        test.emit(b'\033]133;A\007p> \033]133;B\007image\r\n'
            b'\033]133;C\007' + text + b'\r' +
            kitty('a=T,i=7,f=32,s=1,v=1,c=4,r=3,C=1', b'\xff\0\0\xff') +
            b'\033[5;1H\033]133;D;0\007\033]133;A\007NEXT> '
            b'\033]133;B\007')

        def check(visible, margin=0, row=1):
            """Check graphical pixels and fallback cells after a full redraw."""
            test.redraw()
            terminal = Terminal()
            terminal.feed(client.data)
            expected = {(margin * 8 + x, row * 16 + y): (255, 0, 0)
                for y in range(48) for x in range(32)} if visible else {}
            terminal.check(expected)
            terminal = Terminal()
            terminal.feed(fallback.data)
            cell = terminal.cells.get((margin, row), (' ',))[0]
            assert (cell == '▄') == visible, (visible, margin, row, cell)

        test.run('copy-mode')
        check(True)
        test.run('send-keys', '-X', 'fold-view-toggle')
        check(True, margin=3)
        test.run('send-keys', '-X', 'fold-view-toggle')
        check(True)
        test.run('send-keys', '-X', 'cancel')
        test.run('copy-mode', '-c')
        check(False)
        test.run('send-keys', '-X', 'expand-output', '-a')
        check(True, margin=3)

        # Images below all text must survive trimming of trailing blank rows.
        test.run('send-keys', '-X', 'cancel')
        test.emit(kitty('a=d,d=A') + b'\033[2J\033[Hplain\033[9;1H' +
            kitty('a=T,i=8,f=32,s=1,v=1,c=4,r=3,C=1', b'\xff\0\0\xff'))
        test.run('copy-mode', '-U')
        check(True, margin=3, row=8)
PY
