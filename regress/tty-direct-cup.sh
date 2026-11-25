#!/bin/sh

# Direct CUP must respect explicit overrides and changes to VT100 detection.

PATH=/bin:/usr/bin
LC_ALL=C.UTF-8
export PATH LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

. ./regress-common.inc
DIR=$(test_tmpdir) || exit 1
trap 'rm -rf "$DIR"' 0

python3 -B - "$TEST_TMUX" "$DIR" <<'PY'
import fcntl
import os
import re
import select
import struct
import subprocess
import sys
import termios
import time

tmux, directory = sys.argv[1:]
standard = r'\E[%i%p1%d;%p2%dH'
custom = r'\E]999;custom-cup\007' + standard
not_vt = '*:XT@:clear=plain-clear'
cases = [
    ('default', '', True, True),
    ('unrelated', '*:bold=bold', True, True),
    ('unmatched', 'not-this-terminal:cup=' + custom, True, True),
    ('standard override', '*:cup=' + standard, True, False),
    ('custom override', '*:cup=' + custom, True, False),
    ('not VT100', not_vt, False, False),
    ('removed CUP', '*:cup@', None, None),
    ('updated CUP', '', True, True),
    ('updated VT100', '', True, True),
]

for index, (name, overrides, vt100, direct) in enumerate(cases):
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

    def flags_are(vt100, direct):
        m = re.search(r'flags=0x([0-9a-f]+):', run('show-messages', '-T'))
        if m is None:
            return False
        flags = int(m[1], 16)
        return bool(flags & 0x20) == vt100 and bool(flags & 0x400) == direct

    def has_feature(feature):
        return feature in run('list-clients', '-F', '#{client_termfeatures}').split(',')

    try:
        run('new-session', '-d', '-s', 'cup', '-x', '80', '-y', '24',
            "printf '\033[4;12Hhello\033]2;ready\007'; exec sleep 100")
        run('set-option', '-g', 'status', 'off')
        run('set-option', '-s', 'terminal-features', '')
        run('set-option', '-s', 'terminal-overrides', overrides)
        deadline = time.monotonic() + 10
        while run('display-message', '-p', '#{pane_title}') != 'ready':
            assert time.monotonic() < deadline, (name, 'pane not ready')
            time.sleep(.01)

        pid, fd = os.forkpty()
        if pid == 0:
            os.environ['TERM'] = 'xterm-256color'
            fcntl.ioctl(0, termios.TIOCSWINSZ,
                        struct.pack('HHHH', 24, 80, 0, 0))
            os.execv(tmux, server + ['attach-session', '-t', 'cup'])

        if name == 'removed CUP':
            wait_for(lambda: b'terminal does not support cup' in output)
            continue

        wait_for(lambda: flags_are(vt100, direct))
        if name == 'custom override':
            wait_for(lambda: b'\033]999;custom-cup\007' in output)
        elif direct:
            wait_for(lambda: b'hello' in output)
            assert b'custom-cup' not in output, (name, bytes(output))

        if name == 'updated CUP':
            run('set-option', '-s', 'terminal-overrides', '*:cup=' + custom)
            # Apply the override with secondary DA while startup queries are open.
            os.write(fd, b'\033[>85;1;0c')
            wait_for(lambda: has_feature('ignorefkeys') and flags_are(True, False))
            run('refresh-client')
            wait_for(lambda: b'\033]999;custom-cup\007' in output)

            # Removing the option does not restore the stored capability.
            # Primary DA adds another feature and finishes the startup queries.
            run('set-option', '-s', 'terminal-overrides', '')
            os.write(fd, b'\033[?65;21c')
            wait_for(lambda: has_feature('margins'))
            assert flags_are(True, False), (name, 'direct CUP re-enabled')
            assert 'custom-cup' in run('show-messages', '-T'), name
        elif name == 'updated VT100':
            run('set-option', '-s', 'terminal-overrides', not_vt)
            os.write(fd, b'\033[?65;21c')
            wait_for(lambda: has_feature('margins') and flags_are(False, False))
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
