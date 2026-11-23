#!/bin/sh

# Direct RGB must use the RGB feature's capabilities and respect overrides.

PATH=/bin:/usr/bin
LC_ALL=C.UTF-8
export PATH LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101
command -v tic >/dev/null || exit 101

. ./regress-common.inc
DIR=$(test_tmpdir) || exit 1
trap 'rm -rf "$DIR"' 0

python3 -B - "$TEST_TMUX" "$DIR" <<'PY'
import fcntl
import os
from pathlib import Path
import re
import select
import struct
import subprocess
import sys
import termios
import time

from image_terminal import Terminal

tmux, directory = sys.argv[1:]
fg = r'\E[38;2;%p1%d;%p2%d;%p3%dm'
bg = r'\E[48;2;%p1%d;%p2%d;%p3%dm'
custom_fg = r'\E]999;custom-fg\007' + fg
custom_bg = r'\E]999;custom-bg\007' + bg
terminfo = directory + '/terminfo'
subprocess.run(['tic', '-x', '-o', terminfo, '-'], input=(
    'tmux-direct-rgb-test|direct RGB test terminal,\n'
    '\tsetrgbf=\\E]999;native-fg\\007' + fg + ',\n'
    '\tsetrgbb=\\E]999;native-bg\\007' + bg + ',\n'
    '\tuse=xterm-256color,\n'), text=True, check=True)
cases = [
    ('native', '', '', True, False),
    ('feature', '*:RGB', '', True, True),
    ('unrelated', '*:RGB', '*:bold=bold', True, True),
    ('unmatched', '*:RGB', 'not-this-terminal:setrgbf=' + custom_fg, True, True),
    ('standard override', '*:RGB', '*:setrgbf=' + fg, True, False),
    ('custom fg', '*:RGB', '*:setrgbf=' + custom_fg, True, False),
    ('custom bg', '*:RGB', '*:setrgbb=' + custom_bg, True, False),
    ('removed fg', '*:RGB', '*:setrgbf@', False, False),
    ('removed bg', '*:RGB', '*:setrgbb@', False, False),
    ('updated', '*:RGB', '', True, True),
    ('collisions', '*:RGB', '', True, True),
]

# Exercise alternating collisions, replacement, and black's initially zero key.
colliding = []
for colour in range(4096):
    hash = colour * 2654435761 & 0xffffffff
    hash ^= hash >> 16
    if hash % 32 == 0:
        colliding.append((colour >> 16, colour >> 8 & 255, colour & 255))
        if len(colliding) == 3:
            break
assert len(colliding) == 3
sequence = [colliding[i] for i in ([0, 1] * 16 + [0, 2] * 16 + [0, 1, 2] * 12)]
collision_input = Path(directory) / 'collisions'
collision_input.write_bytes(b''.join(
    (('\033[38;2;%d;%d;%dm' % colour) +
     ('\033[48;2;%d;%d;%dm@' % colour)).encode() for colour in sequence))


for index, (name, features, overrides, rgb, direct) in enumerate(cases):
    server = [tmux, '-S', directory + '/socket-%d' % index, '-f/dev/null']
    pid = fd = None
    output = bytearray()

    def run(*args):
        p = subprocess.run(server + list(args), capture_output=True,
                           text=True, timeout=10)
        assert p.returncode == 0, (name, args, p.stderr)
        return p.stdout.strip()

    def drain():
        while select.select([fd], [], [], .01)[0]:
            try:
                data = os.read(fd, 65536)
            except OSError:
                break
            if not data:
                break
            output.extend(data)

    def wait_for(predicate):
        deadline = time.monotonic() + 10
        while True:
            drain()
            if predicate():
                return
            assert time.monotonic() < deadline, (name, 'timed out', bytes(output))
            time.sleep(.02)

    def flags_are(rgb, direct):
        m = re.search(r'flags=0x([0-9a-f]+):', run('show-messages', '-T'))
        if m is None:
            return False
        flags = int(m[1], 16)
        return bool(flags & 0x10) == rgb and bool(flags & 0x800) == direct

    def has_feature(feature):
        return feature in run('list-clients', '-F', '#{client_termfeatures}').split(',')

    def check_collision_cells():
        terminal = Terminal(sx=80, sy=24)
        positions = [(i % terminal.sx, i // terminal.sx)
                     for i in range(len(sequence))]
        offset = 0

        def cells_ready():
            nonlocal offset
            terminal.feed(output[offset:])
            offset = len(output)
            return all(terminal.cells.get(pos, (' ',))[0] == '@'
                       for pos in positions)

        wait_for(cells_ready)
        actual = [terminal.cells[pos][1:3] for pos in positions]
        assert actual == [(c, c) for c in sequence], (actual, bytes(output))

    try:
        run('new-session', '-d', '-s', 'rgb', '-x', '80', '-y', '24',
            "printf '\033[38;2;1;127;255m\033[48;2;254;128;0mRGB"
            "\033[0m\033]2;ready\007'; exec sleep 100")
        run('set-option', '-g', 'status', 'off')
        run('set-option', '-s', 'terminal-features', features)
        run('set-option', '-s', 'terminal-overrides', overrides)
        deadline = time.monotonic() + 10
        while run('display-message', '-p', '#{pane_title}') != 'ready':
            assert time.monotonic() < deadline, (name, 'pane not ready')
            time.sleep(.01)

        pid, fd = os.forkpty()
        if pid == 0:
            os.environ['TERM'] = 'tmux-direct-rgb-test'
            os.environ['TERMINFO'] = terminfo
            os.environ.pop('COLORTERM', None)
            fcntl.ioctl(0, termios.TIOCSWINSZ,
                        struct.pack('HHHH', 24, 80, 0, 0))
            os.execv(tmux, server + ['attach-session', '-t', 'rgb'])

        wait_for(lambda: flags_are(rgb, direct) and b'RGB' in output)
        if rgb:
            assert b'\033[38;2;1;127;255m' in output, (name, bytes(output))
            assert b'\033[48;2;254;128;0m' in output, (name, bytes(output))
        for marker in ('native-fg', 'native-bg', 'custom-fg', 'custom-bg'):
            expected = (name == 'native' and marker.startswith('native-') or
                        name == 'custom fg' and marker == 'custom-fg' or
                        name == 'custom bg' and marker == 'custom-bg')
            assert (marker.encode() in output) == expected, (name, marker, bytes(output))

        if name == 'updated':
            run('set-option', '-s', 'terminal-overrides', '*:setrgbf=' + custom_fg)
            # A new DA feature reapplies overrides to the existing terminal.
            os.write(fd, b'\033[?65;21c')
            wait_for(lambda: has_feature('margins') and flags_are(True, False))
            run('refresh-client')
            wait_for(lambda: b'\033]999;custom-fg\007' in output)

            # Removing the option leaves the custom capability in place.
            run('set-option', '-s', 'terminal-overrides', '')
            os.write(fd, b'\033[>85;1;0c')
            wait_for(lambda: has_feature('ignorefkeys'))
            assert flags_are(True, False), (name, 'direct RGB re-enabled')
            assert 'custom-fg' in run('show-messages', '-T'), name
        elif name == 'collisions':
            # Feed colours after attach, so they also exercise incremental output.
            run('respawn-pane', '-k',
                "cat '%s'; printf '\033[0m'; exec sleep 100" % collision_input)
            check_collision_cells()
            output.clear()
            run('refresh-client')
            check_collision_cells()
    finally:
        if fd is not None:
            drain()
        subprocess.run(['/bin/sh', '-c',
            '. ./regress-common.inc; stop_server "$@"', 'sh'] + server,
            check=True, timeout=15)
        if fd is not None:
            os.close(fd)
        if pid is not None:
            os.waitpid(pid, 0)
PY
