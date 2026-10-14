#!/bin/sh

# Kitty graphics uploads, chunking, queries, placements and replies.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
export TEST_TMUX
command -v python3 >/dev/null || exit 101

python3 -B - <<'PY'
import base64
import json
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
import time
import zlib

tmux = [os.environ['TEST_TMUX'], '-u', '-Limage-protocol' + str(os.getpid()),
        '-f/dev/null']

# Run a command on the test server.
def run(*args):
    return subprocess.check_output(tmux + list(args), text=True).strip()

# Build a Kitty graphics APC sequence.
def graphics(control, payload=''):
    return '\033_G' + control + (';' + payload if payload else '') + '\033\\'

# Read until a following device-attributes reply, including when q=2 is used.
reader = '''
import json, os, select, sys, time, tty
command, output = sys.argv[1:]
tty.setraw(0)
with open(command, 'rb') as f:
    while True:
        data = f.read(65536)
        if not data:
            break
        while data:
            data = data[os.write(1, data):]
os.write(1, b'\\x1b[c')
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

# Check replies, cursor movement and unchanged text and title.
def check(name, command, expected, cursor='5,2', timeout=5):
    output = directory / name
    command_file = directory / (name + '.input')
    with command_file.open('w', encoding='utf-8') as f:
        f.write(origin)
        if isinstance(command, str):
            f.write(command)
        else:
            f.writelines(command)
    pane_command = 'python3 -B ' + shlex.quote(str(helper)) + ' ' + \
        shlex.quote(str(command_file)) + ' ' + shlex.quote(str(output))
    pane = run('new-window', '-d', '-P', '-F', '#{pane_id}', pane_command)
    try:
        deadline = time.monotonic() + timeout
        while not output.exists():
            if time.monotonic() >= deadline:
                raise AssertionError(name + ': missing reply capture')
            time.sleep(0.05)
        reply = json.loads(output.read_text())
        assert reply == expected, (name, 'reply', repr(reply), repr(expected))
        actual = run('display-message', '-pt', pane, '#{cursor_x},#{cursor_y}')
        assert actual == cursor, (name, 'cursor', actual, cursor)
        assert run('capture-pane', '-pt', pane) == '', (name, 'text')
        title = run('display-message', '-pt', pane, '#{pane_title}')
        assert title == 'kitty-test', (name, 'title', title)
        return reply
    finally:
        run('kill-window', '-t', pane)

with tempfile.TemporaryDirectory(prefix='tmux-kitty-protocol-') as tmp:
    directory = Path(tmp)
    helper = directory / 'read-reply.py'
    helper.write_text(reader)
    origin = '\033]2;kitty-test\007\033[3;6H'
    try:
        run('new-session', '-d', '-x', '40', '-y', '12')
        if run('display-message', '-p', '#{image_support}') == '0':
            raise SystemExit(101)
        run('set', '-g', 'status', 'off')
        pixel = '/wAA/w=='
        transmit = 'a=t,i=7,f=32,s=1,v=1'
        ok = graphics('i=7', 'OK')
        invalid = graphics('i=7', 'EINVAL')
        check('transmit', graphics(transmit, pixel), ok)
        check('defaults', graphics('i=7,s=1,v=1', pixel), ok)
        check('rgb', graphics('a=t,i=7,f=24,s=1,v=1', '/wAA'), ok)
        check('unpadded', graphics(transmit, '/wAA/w'), ok)
        check('placement-id-reply', graphics(transmit + ',p=9', pixel),
              graphics('i=7,p=9', 'OK'))
        check('chunks', graphics(transmit + ',m=1', '/wAA') +
              graphics('m=0', '/w=='), ok)
        check('nonzero-padding', graphics(transmit + ',m=1', '/wAA') +
              graphics('m=0', '/z=='), invalid)
        check('empty-decoded-chunk', graphics(transmit + ',m=1', '    ') +
              graphics('m=0', pixel), ok)
        check('empty-final-chunk', graphics(transmit + ',m=1', pixel) +
              graphics('m=0'), ok)
        check('incomplete', graphics(transmit + ',m=1', '/wAA'), '')
        check('failed-chunk', graphics(transmit + ',m=1', '/wAA') +
              graphics('m=0', '!!!!') + graphics(transmit, pixel), invalid + ok)
        check('quiet-ok', graphics(transmit + ',q=1', pixel), '')
        check('quiet-two', graphics(transmit + ',q=2', pixel), '')
        check('quiet-one-error', graphics(transmit + ',q=1', '!!!!'), invalid)
        check('quiet-two-error', graphics(transmit + ',q=2', '!!!!'), '')
        check('quiet-continuation', graphics(transmit + ',q=2,m=1', '/wAA') +
              graphics('m=0', '/w=='), '')
        check('quiet-control-error', graphics('a=t,q=2,i=7,s=bad'), '')
        check('anonymous', graphics('a=t,f=32,s=1,v=1', pixel), '')
        check('anonymous-error', graphics('a=t,f=32,s=1,v=1', '!!!!'), '')
        check('query', graphics('a=q,f=32,s=1,v=1', pixel),
              graphics('i=0', 'OK'))
        check('query-chunks', graphics('a=q,i=7,f=32,s=1,v=1,m=1', '/wAA') +
              graphics('m=0', '/w=='), ok)
        check('replace', graphics(transmit, pixel) +
              graphics(transmit, 'AP8A/w=='), ok + ok)
        check('transmit-and-place',
              graphics('a=T,i=7,f=32,s=1,v=1', pixel), ok, cursor='6,2')
        check('transmit-and-place-sized',
              graphics('a=T,i=7,f=32,s=1,v=1,c=3,r=2', pixel),
              ok, cursor='8,3')
        check('place-stored', graphics(transmit + ',q=2', pixel) +
              graphics('a=p,i=7'), ok, cursor='6,2')
        check('place-no-cursor', graphics(transmit + ',q=2', pixel) +
              graphics('a=p,i=7,C=1,c=3,r=2'), ok)
        check('place-named', graphics(transmit + ',q=2', pixel) +
              graphics('a=p,i=7,p=9,C=1'), graphics('i=7,p=9', 'OK'))
        check('place-missing', graphics('a=p,i=7'),
              graphics('i=7', 'ENOENT'))
        check('query-does-not-store',
              graphics('a=q,q=2,i=7,f=32,s=1,v=1', pixel) +
              graphics('a=p,i=7'), graphics('i=7', 'ENOENT'))
        check('place-chunks', graphics('a=T,i=7,f=32,s=1,v=1,'
              'c=3,r=2,C=1,m=1', '/wAA') + graphics('m=0', '/w=='), ok)
        for n, geometry in enumerate(['x=1', 'y=1', 'c=65536', 'r=65536',
                                      'c=65534,r=32767']):
            check('invalid-placement-' + str(n),
                  graphics(transmit + ',q=2', pixel) +
                  graphics('a=p,i=7,C=1,' + geometry), invalid)
        check('failed-transmit-keeps-source',
              graphics(transmit + ',q=2', pixel) +
              graphics('a=T,i=7,f=32,s=1,v=1,x=1,C=1', pixel) +
              graphics('a=p,i=7,C=1'), invalid + ok)
        check('place-at-bottom', '\033[12;6H' +
              graphics('a=T,i=7,f=32,s=1,v=1,c=3,r=2', pixel),
              ok, cursor='8,11')
        check('stationary-at-bottom', '\033[12;6H' +
              graphics('a=T,i=7,f=32,s=1,v=1,c=3,r=2,C=1', pixel),
              ok, cursor='5,11')
        for n, payload in enumerate(['', '    ', '!!!!', '====', '/w',
                                     '/wAA/wAA']):
            check('invalid-payload-' + str(n),
                  graphics(transmit, payload), invalid)
        for control in ['f=12,s=1,v=1', 'f=32,s=0,v=1', 'f=32,s=1,v=0',
                        'f=32,s=bad,v=1', 'f=32,s=4294967296,v=1',
                        'f=32,s=1,v=1,o=x']:
            check('invalid-control-' + control,
                  graphics('a=t,i=7,' + control, pixel), invalid)
        for action in ['f', 'a', 'c']:
            check('unsupported-action-' + action,
                  graphics('a=%s,i=7' % action), graphics('i=7', 'ENOTSUP'))
        for medium in ['f', 't', 's']:
            check('unsupported-medium-' + medium,
                  graphics('a=t,i=7,t=' + medium), graphics('i=7', 'ENOTSUP'))
        check('unsupported-number', graphics('a=t,I=7,f=32,s=1,v=1', pixel),
              graphics('I=7', 'ENOTSUP'))
        check('id-and-number', graphics('a=t,i=7,I=8,f=32,s=1,v=1', pixel),
              graphics('i=7,I=8', 'EINVAL'))
        check('unknown-extension', graphics(transmit + ',k=9', pixel), ok)
        check('unknown-extension-string',
              graphics(transmit + ',k=unused', pixel), ok)
        check('trailing-comma', graphics(transmit + ',', pixel), ok)
        check('maximum-ids',
              graphics('a=t,i=4294967295,p=4294967295,f=32,s=1,v=1', pixel),
              graphics('i=4294967295,p=4294967295', 'OK'))
        for n, control in enumerate(['s=', 's=-1', 's=1x', 'a=tt', 't=dd',
                                     'o=zz', '=1', 'ss=1', ',s=1',
                                     's=1,,v=1']):
            check('malformed-control-' + str(n),
                  graphics('i=7,' + control, pixel), invalid)
        check('failed-control-resets-transfer',
              graphics(transmit + ',m=1', '/wAA') +
              graphics('m=0,s=bad') + graphics(transmit, pixel), invalid + ok)
        check('quiet-resets', graphics(transmit + ',q=2', pixel) +
              graphics(transmit, pixel), ok)
        check('oversized-dimensions',
              graphics('a=t,i=7,f=32,s=4294967295,v=4294967295', pixel),
              invalid)
        check('delete-aborts-upload', graphics(transmit + ',q=2,m=1', '/wAA') +
              graphics('a=d,d=I,q=2') + graphics(transmit, pixel), ok)

        # Successful deletion sends no reply and never moves the cursor.
        stored = graphics(transmit + ',q=2', pixel)
        put = 'a=p,q=2,i=7,C=1'
        first = graphics(put + ',p=1')
        second = graphics(put + ',p=2')
        missing = graphics('i=7', 'ENOENT')
        check('delete-soft-image', stored + first + graphics('a=d,d=i,i=7') +
              graphics('a=p,i=7,C=1'), ok)
        check('delete-hard-image', stored + first + graphics('a=d,d=I,i=7') +
              graphics('a=p,i=7,C=1'), missing)
        check('delete-one-placement', stored + first + second +
              graphics('a=d,d=I,i=7,p=1') + graphics('a=p,i=7,p=2,C=1'),
              graphics('i=7,p=2', 'OK'))
        check('delete-last-placement', stored + first +
              graphics('a=d,d=I,i=7,p=1') + graphics('a=p,i=7,C=1'), missing)
        check('delete-retain-unnumbered', stored + first + graphics(put) +
              graphics('a=d,d=I,i=7,p=1') + graphics('a=p,i=7,C=1'), ok)
        check('delete-missing-placement', stored + first +
              graphics('a=d,d=I,i=7,p=99') + graphics('a=p,i=7,C=1'), ok)
        check('delete-upload-only', stored + graphics('a=d,d=I,i=7') +
              graphics('a=p,i=7,C=1'), missing)
        check('delete-soft-all', stored + first + graphics('a=d,d=a') +
              graphics('a=p,i=7,C=1'), ok)
        check('delete-hard-all', stored + first + graphics('a=d,d=A') +
              graphics('a=p,i=7,C=1'), missing)
        check('delete-all-retains-unplaced', stored + graphics('a=d,d=A') +
              graphics('a=p,i=7,C=1'), ok)
        check('delete-history-retains-data', stored + first + '\033[12S' +
              graphics('a=d,d=A') + graphics('a=p,i=7,C=1'), ok)
        check('delete-history-by-id', stored + first + '\033[12S' +
              graphics('a=d,d=I,i=7') + graphics('a=p,i=7,C=1'), missing)
        for selector in ['A', 'I']:
            check('delete-saved-screen-' + selector, stored + first +
                  '\033[?1049h' + graphics('a=d,d=' + selector + ',i=7') +
                  '\033[?1049l' + graphics('a=p,i=7,C=1'), ok)
        check('delete-abort-does-not-inherit-id', stored +
              graphics(transmit + ',q=2,m=1', '/wAA') +
              graphics('a=d,d=I') + graphics('a=p,i=7,C=1'), ok)
        check('unsupported-delete', stored + graphics('a=d,d=?,i=7'),
              graphics('i=7', 'ENOTSUP'))
        check('malformed-delete', stored + graphics('a=d,i=7,d=ii'), invalid)

        # Position selectors use one-based cells; range selectors use image IDs.
        selectors = [('c', ''), ('p', ',x=6,y=3'), ('x', ',x=6'),
                     ('y', ',y=3'), ('r', ',x=7,y=7')]
        for selector, controls in selectors:
            for hard in (False, True):
                how = selector.upper() if hard else selector
                check('delete-selector-' + how, stored + first +
                      graphics('a=d,d=' + how + controls) +
                      graphics('a=p,i=7,C=1'), missing if hard else ok)
        check('delete-range-unplaced', stored + graphics('a=d,d=R,x=7,y=7') +
              graphics('a=p,i=7,C=1'), missing)
        check('delete-soft-range-unplaced', stored +
              graphics('a=d,d=r,x=7,y=7') + graphics('a=p,i=7,C=1'), ok)
        check('delete-spatial-retains-other-placement', stored + first +
              '\033[3;8H' + second + graphics('a=d,d=X,x=6') +
              graphics('a=p,i=7,C=1'), ok, cursor='7,2')
        check('delete-spatial-miss-retains-data', stored + first +
              graphics('a=d,d=P,x=7,y=3') + graphics('a=p,i=7,C=1'), ok)
        check('delete-spatial-retains-history', stored + first + '\033[12S' +
              graphics('a=d,d=P,x=6,y=3') + graphics('a=p,i=7,C=1'), ok)

        # Abort immediately when chunked data exceeds 64 MiB, then recover.
        block = base64.b64encode(b'\0' * (512 * 1024)).decode()
        commands = [graphics(transmit + ',q=1,m=1', block)]
        commands.extend([graphics('m=1', block)] * 127)
        commands.extend([graphics('m=1', 'AAAAAA=='),
            graphics('a=t,i=8,f=32,s=1,v=1', pixel)])
        check('accumulated-size-limit', commands,
              invalid + graphics('i=8', 'OK'), timeout=30)

        # Compressed uploads require a test build with zlib.
        compressed = base64.b64encode(zlib.compress(b'\xff\0\0\xff')).decode()
        control = transmit + ',o=z'
        for format, raw in [(24, b'\xff\0\0'), (32, b'\xff\0\0\xff')]:
            payload = base64.b64encode(zlib.compress(raw)).decode()
            check('compressed-' + str(format),
                  graphics('a=t,i=7,f=%d,s=1,v=1,o=z' % format, payload),
                  ok)
        split = len(compressed) // 8 * 4
        check('compressed-chunks', graphics(control + ',m=1',
              compressed[:split]) + graphics('m=0', compressed[split:]), ok)
        check('compressed-truncated', graphics(control, compressed[:-4]),
              invalid)
        check('compressed-wrong-size',
              graphics(control.replace('s=1', 's=2'), compressed), invalid)
        large = base64.b64encode(zlib.compress(
            b'\0' * (4096 * 4097 * 3))).decode()
        check('oversized-rgb',
              graphics('a=t,i=7,f=24,s=4096,v=4097,o=z', large), invalid)

        # PNG uploads require a test build with libpng.
        png = b'\x89PNG\r\n\x1a\n'
        for kind, data in [
                (b'IHDR', struct.pack('>2I5B', 8, 16, 8, 6, 0, 0, 0)),
                (b'IDAT', zlib.compress((b'\0' + b'\xff\0\0\xff' * 8) * 16)),
                (b'IEND', b'')]:
            png += struct.pack('>I', len(data)) + kind + data + \
                struct.pack('>I', zlib.crc32(kind + data))
        payload = base64.b64encode(png).decode()
        control = 'a=t,i=7,f=100'
        check('png', graphics(control, payload), ok)
        truncated = base64.b64encode(png[:40]).decode()
        check('png-truncated', graphics(control, truncated), invalid)
        damaged = bytearray(png)
        damaged[29] ^= 1
        check('png-bad-header-crc',
              graphics(control, base64.b64encode(damaged).decode()), invalid)
        split = len(payload) // 8 * 4
        check('png-chunks', graphics(control + ',m=1', payload[:split]) +
              graphics('m=0', payload[split:]), ok)
        payload = base64.b64encode(zlib.compress(png)).decode()
        control += ',o=z'
        sized = control + ',S=' + str(len(png))
        check('compressed-png', graphics(sized, payload), ok)
        split = len(payload) // 8 * 4
        check('compressed-png-chunks',
              graphics(sized + ',m=1', payload[:split]) +
              graphics('m=0', payload[split:]), ok)
        check('compressed-png-missing-size', graphics(control, payload),
              invalid)
        check('compressed-png-wrong-size',
              graphics(control + ',S=' + str(len(png) + 1), payload),
              invalid)
        check('compressed-png-oversized',
              graphics(control + ',S=67108865', payload), invalid)
        check('compressed-png-truncated', graphics(sized, payload[:-4]),
              invalid)
        check('decoder-controls-reset', graphics(sized + ',q=2', payload) +
              graphics('a=T,i=7,s=1,v=1', pixel), ok, cursor='6,2')
    finally:
        subprocess.run(tmux + ['kill-server'], stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL)
PY
