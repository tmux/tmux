#!/bin/sh

# Kitty placeholder rows remain cell-aligned when the terminal becomes narrower.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import shlex
import sys
from image_test import image_test

ph = '\U0010eeee\u0305'

with image_test(sys.argv[1], sx=10, sy=4) as outer, \
        image_test(sys.argv[1], sx=10, sy=4) as inner:
    client = outer.attach(graphical=False)
    command = 'env TMUX= TERM=tmux-256color ' + shlex.join(
        inner.server + ['-u', 'attach-session', '-t', 'image'])
    outer.run('respawn-pane', '-k', command)
    inner.wait(lambda: inner.run('list-clients', '-F', '#{client_name}') != '')

    # Unresolved placeholders also retain their positions during reflow.
    inner.emit((ph * 10).encode())
    outer.wait(lambda: outer.run('capture-pane', '-p').splitlines()[0] == ph * 10)
    for width in (5, 10):
        outer.resize(client, width, 4)
        inner.wait(lambda: inner.run('display-message', '-p',
            '#{window_width}') == str(width))
        outer.wait(lambda: outer.run('capture-pane', '-p').splitlines()[0] == ph * width)
        lines = outer.run('capture-pane', '-p').splitlines()
        assert len(lines) == 1, (width, lines)
PY
