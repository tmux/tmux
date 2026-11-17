#!/bin/sh

# Pane prompts remain above Kitty images during damage and full redraws.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import shlex
import sys
from image_test import image_test, kitty
from image_terminal import Terminal
from image_kitty import KittyTerminal


for native in (False, True):
    for position, row in [('top', 0), ('bottom', 3)]:
        with image_test(sys.argv[1], sy=4) as test:
            client = test.attach(kitty_graphics=native)
            test.run('set-option', '-g', 'status-position', position)
            test.emit(kitty('a=t,i=7,f=32,s=1,v=1', b'\xff\0\0\xff'))
            test.run('command-prompt', '-P', '-C', '-b', '-l', '-p', 'PROMPT',
                'display-message %1')
            test.redraw()
            terminal = KittyTerminal(sy=4) if native else Terminal(sy=4)
            terminal.feed(client.data)
            expected = {(x, y): (255, 0, 0) for y in range(64)
                for x in range(32) if y // 16 != row}

            client.clear()
            test.marker += 1
            title = 'image-%d' % test.marker
            data = kitty('a=p,i=7,C=1,c=4,r=4') + \
                b'\033]2;' + title.encode() + b'\007'
            # Feed the pane directly, bypassing its open prompt.
            test.run('pipe-pane', '-I', 'printf %%s ' +
                shlex.quote(data.decode('ascii')))
            test.wait(lambda: test.run('display-message', '-p',
                '#{pane_title}') == title)
            client.read(0.2)
            test.run('pipe-pane')
            terminal.feed(client.data)
            terminal.check(expected)
            assert ''.join(terminal.cells[x, row][0] for x in range(6)) \
                == 'PROMPT', terminal.cells
            test.redraw()
            terminal.feed(client.data)
            terminal.check(expected)
            assert ''.join(terminal.cells[x, row][0] for x in range(6)) \
                == 'PROMPT', terminal.cells
PY
