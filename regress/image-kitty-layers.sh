#!/bin/sh

# Kitty output layers must not depend on upload, placement or redraw order.

PATH=/bin:/usr/bin
TERM=screen
export TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
export TEST_TMUX
command -v python3 >/dev/null || exit 0

python3 - <<'PY'
import base64
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import time

binary = str(Path(os.environ['TEST_TMUX']).resolve())
inner = [binary, '-Lkitty-layers-inner' + str(os.getpid()), '-f/dev/null']
outer = [binary, '-Lkitty-layers-outer' + str(os.getpid()), '-f/dev/null']


# Run one command against the selected test server.
def run(server, *args):
    result = subprocess.check_output(server + list(args), text=True).strip()
    return result


# Build a graphics command that leaves the cursor unchanged.
def graphics(control, payload=''):
    return '\033_G' + control + (';' + payload if payload else '') + '\033\\'


# Replay uploads and placements using Kitty's internal-ID tie breaker.
def scene(data):
    images, placements = {}, {}
    loading = bytearray()
    upload = None
    for match in re.finditer(rb'\033_G([^\033]*)\033\\', data):
        control, _, payload = match[1].partition(b';')
        fields = dict(item.split(b'=', 1) for item in control.split(b','))
        action = fields.get(b'a', b't')
        image_id = int(fields.get(b'i', 0))
        if action == b't':
            if image_id:
                upload = image_id
                loading.clear()
            loading.extend(base64.b64decode(payload))
            if fields.get(b'm', b'0') == b'0':
                # The one-pixel fixture has a duplicated border on each side.
                assert len(loading) == 36, len(loading)
                images[upload] = (len(images), bytes(loading[16:20]))
        elif action == b'p':
            placements[image_id, int(fields[b'p'])] = int(fields.get(b'z', 0))
        elif action == b'd':
            if b'p' in fields:
                placements.pop((image_id, int(fields[b'p'])), None)
            else:
                placements = {key: z for key, z in placements.items()
                              if key[0] != image_id}
    if not placements:
        return None, len(images)
    top = max(placements, key=lambda key: (placements[key], images[key[0]][0]))
    return images[top[0]][1], len(images)


# Wait for a rendered scene, rather than only the application's input write.
def expect(name, colour, uploads):
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline:
        actual, count = scene(capture.read_bytes())
        if actual == colour and count >= uploads:
            return
        time.sleep(0.05)
    raise AssertionError((name, actual, colour, count, uploads))


red = bytes((255, 0, 0, 255))
white = bytes((255, 255, 255, 255))
older = graphics('a=t,q=2,i=200,f=32,s=1,v=1', base64.b64encode(red).decode())
newer = graphics('a=t,q=2,i=100,f=32,s=1,v=1', base64.b64encode(white).decode())
put_older = graphics('a=p,q=2,C=1,i=200,c=1,r=1')
put_newer = graphics('a=p,q=2,C=1,i=100,c=1,r=1')
commands = [older + newer + put_newer, put_older,
            older + put_older,
            graphics('a=d,d=I,q=2,i=200') + older + put_older,
            put_older.replace('C=1', 'C=1,z=2147483646') +
            put_newer.replace('C=1', 'C=1,z=2147483647')]

with tempfile.TemporaryDirectory(prefix='tmux-kitty-layers-') as tmp:
    directory = Path(tmp)
    capture = directory / 'output'
    capture.touch()
    helper = directory / 'emit.py'
    helper.write_text('import os, sys, time\n'
                      'commands = ' + repr(commands) + '\n'
                      'for index, command in enumerate(commands):\n'
                      '    if index: sys.stdin.readline()\n'
                      '    os.write(1, command.encode())\n'
                      'time.sleep(30)\n')
    try:
        run(inner, 'new-session', '-d', '-x', '40', '-y', '12',
            shlex.join(['python3', str(helper)]))
        if run(inner, 'display-message', '-p', '#{image_support}') == '0':
            raise SystemExit(0)
        run(inner, 'set', '-g', 'status', 'off')
        run(inner, 'set', '-as', 'terminal-features', ',*:kitty')
        pane = run(outer, 'new-session', '-dP', '-F', '#{pane_id}',
                   '-x', '40', '-y', '12', 'exec sh')
        run(outer, 'set', '-g', 'status', 'off')
        run(outer, 'pipe-pane', '-t', pane, '-O',
            'cat >' + shlex.quote(str(capture)))
        run(outer, 'send-keys', '-t', pane, '-l', shlex.join(inner + ['attach']))
        run(outer, 'send-keys', '-t', pane, 'Enter')
        expect('newer-placed-first', white, 1)
        run(inner, 'send-keys', 'Enter')
        expect('older-uploaded-last', white, 2)
        run(inner, 'refresh-client')
        time.sleep(0.2)
        expect('full-redraw', white, 2)
        run(inner, 'send-keys', 'Enter')
        expect('older-retransmitted', white, 3)
        run(inner, 'send-keys', 'Enter')
        expect('older-recreated', red, 4)
        run(inner, 'send-keys', 'Enter')
        expect('maximum-z-order', white, 6)
    finally:
        for server in (outer, inner):
            subprocess.run(server + ['kill-server'], stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL)
PY
