#!/bin/sh

# Kitty cursor movement, acknowledgements and query isolation.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
export TEST_TMUX
command -v python3 >/dev/null || exit 0

python3 - <<'PY'
import base64
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import time
import zlib

tmux = [os.environ['TEST_TMUX'], '-u', '-Limage-protocol' + str(os.getpid()),
        '-f/dev/null']

def run(*args):
    return subprocess.check_output(tmux + list(args), text=True).strip()

def graphics(control, payload=''):
    return '\033_G' + control + (';' + payload if payload else '') + '\033\\'

# Read until a following device-attributes reply, including when q=2 is used.
reader = '''
import json, os, select, sys, termios, time, tty
command, output = sys.argv[1:]
tty.setraw(0)
os.write(1, command.encode() + b'\\x1b[c')
reply = b''
deadline = time.monotonic() + 3
while time.monotonic() < deadline:
    if select.select([0], [], [], 0.1)[0]:
        reply += os.read(0, 4096)
        if b'\\x1b[?' in reply and reply.endswith(b'c'):
            break
else:
    raise SystemExit('missing device-attributes reply')
with open(output, 'w') as f:
    json.dump(reply.split(b'\\x1b[?')[0].decode(), f)
time.sleep(30)
'''

def check(name, command, cursor, expected='', text=None):
    output = directory / name
    pane_command = 'python3 ' + shlex.quote(str(helper)) + ' ' + \
        shlex.quote(command) + ' ' + shlex.quote(str(output))
    pane = run('new-window', '-d', '-P', '-F', '#{pane_id}', pane_command)
    try:
        deadline = time.monotonic() + 5
        while not output.exists():
            if time.monotonic() >= deadline:
                raise AssertionError(name + ': missing reply capture')
            time.sleep(0.05)
        reply = json.loads(output.read_text())
        assert reply == expected, (name, 'reply', repr(reply), repr(expected))
        actual = run('display-message', '-pt', pane, '#{cursor_x},#{cursor_y}')
        assert actual == cursor, (name, 'cursor', actual, cursor)
        if text is not None:
            actual = run('capture-pane', '-pt', pane, '-S0', '-E0')
            assert actual == text, (name, 'text', actual, text)
    finally:
        run('kill-window', '-t', pane)

with tempfile.TemporaryDirectory(prefix='tmux-kitty-protocol-') as tmp:
    directory = Path(tmp)
    helper = directory / 'read-reply.py'
    helper.write_text(reader)
    try:
        run('new-session', '-d', '-x', '40', '-y', '12')
        if run('display-message', '-p', '#{image_support}') == '0':
            raise SystemExit(0)
        origin = '\033[3;6H'  # Column 6, row 3 (zero-based 5,2).
        pixel = '/wAA/w=='
        placement = 'a=T,q=2,f=32,s=1,v=1,c=3,r=2'
        check('placement', origin + graphics(placement, pixel), '8,4')
        check('no-cursor', origin + graphics(placement + ',C=1', pixel), '5,2')
        check('clipped', '\033[3;39H' + graphics(placement, pixel), '39,4')
        check('scrolled', '\033[12;6H' + graphics(placement, pixel), '8,11')
        check('transmit', origin + graphics('a=t,q=2,f=32,s=1,v=1,i=7', pixel), '5,2')
        check('virtual', origin + graphics(placement + ',U=1,i=7', pixel), '5,2')
        check('put', origin + graphics('a=t,q=2,f=32,s=1,v=1,i=7', pixel) +
              graphics('a=p,q=2,i=7,c=3,r=2'), '8,4')
        check('chunks', graphics(placement + ',m=1', '/wAA') + origin +
              graphics('m=0', '/w=='), '8,4')
        check('sixel', origin + '\033Pq"1;1;1;1#0;2;100;0;0#0@\033\\', '0,3')
        check('put-reply', origin + graphics('a=t,q=2,f=32,s=1,v=1,i=7', pixel) +
              graphics('a=p,i=7,p=9,c=3,r=2'), '8,4', graphics('i=7,p=9', 'OK'))
        check('missing-reply', origin + graphics('a=p,i=7,p=9'), '5,2',
              graphics('i=7,p=9', 'ENOENT'))
        check('invalid-reply', origin + graphics('a=T,i=7,p=9,f=32,s=1,v=1', '!!!!'),
              '5,2', graphics('i=7,p=9', 'EINVAL'))
        check('quiet-error', origin + graphics('a=T,q=2,i=7,f=32,s=1,v=1', '!!!!'), '5,2')
        check('quiet-ok', origin + graphics('a=t,q=1,i=7,f=32,s=1,v=1', pixel), '5,2')
        check('quiet-one-error', origin + graphics('a=t,q=1,i=7,f=32,s=1,v=1', '!!!!'),
              '5,2', graphics('i=7', 'EINVAL'))
        check('quiet-control-error', origin + graphics('a=t,q=2,i=7,z=bad'), '5,2')
        check('anonymous', origin + graphics('a=t,f=32,s=1,v=1', pixel), '5,2')
        check('anonymous-error', origin + graphics('a=t,f=32,s=1,v=1', '!!!!'), '5,2')
        check('anonymous-placement', origin + graphics('a=T,p=9,f=32,s=1,v=1,c=3,r=2',
                                                      pixel), '8,4')
        check('delete-no-reply', origin + graphics('a=t,q=2,i=7,f=32,s=1,v=1', pixel) +
              graphics('a=d,d=I,i=7'), '5,2')
        for format, raw in [(24, b'\xff\0\0'), (32, b'\xff\0\0\xff')]:
            payload = base64.b64encode(zlib.compress(raw)).decode()
            check('compressed-' + str(format), origin +
                  graphics('a=T,q=2,f=%d,s=1,v=1,c=3,r=2,o=z' % format, payload), '8,4')
        check('query', origin + graphics('a=q,f=32,s=1,v=1', pixel), '5,2',
              graphics('i=0', 'OK'))
        check('query-not-stored', origin + graphics('a=q,q=2,i=7,f=32,s=1,v=1', pixel) +
              graphics('a=p,i=7'), '5,2', graphics('i=7', 'ENOENT'))
        # A query must not change an existing virtual image. Column 2 is
        # valid for the original three-column placement, but not the query.
        check('query-virtual', graphics(placement + ',U=1,i=7', pixel) +
              graphics('a=q,q=2,U=1,i=7,f=32,s=1,v=1,c=1,r=1', pixel) +
              '\033[38;2;0;0;7m\U0010eeee\u0305\u030e', '1,0', text='')
    finally:
        subprocess.run(tmux + ['kill-server'], stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL)
PY
