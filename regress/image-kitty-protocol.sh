#!/bin/sh

# Kitty graphics layout, deletion, placeholders and replies.

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
import re
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import time
import zlib

tmux = [os.environ['TEST_TMUX'], '-u', '-Limage-protocol' + str(os.getpid()),
        '-f/dev/null']
outer = [os.environ['TEST_TMUX'], '-u', '-Limage-protocol-outer' + str(os.getpid()),
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
with open(command, 'rb') as f:
    os.write(1, f.read() + b'\\x1b[c')
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

def check(name, command, cursor, expected='', text=None, render=None, resize=None):
    output = directory / name
    command_file = directory / (name + '.input')
    command_file.write_text(command, encoding='utf-8')
    pane_command = 'python3 ' + shlex.quote(str(helper)) + ' ' + \
        shlex.quote(str(command_file)) + ' ' + shlex.quote(str(output))
    pane = run('new-window', '-d', '-P', '-F', '#{pane_id}', pane_command)
    try:
        deadline = time.monotonic() + 5
        while not output.exists():
            if time.monotonic() >= deadline:
                raise AssertionError(name + ': missing reply capture')
            time.sleep(0.05)
        reply = json.loads(output.read_text())
        assert reply == expected, (name, 'reply', repr(reply), repr(expected))
        if resize is not None:
            run('resize-window', '-t', pane, '-x', str(resize[0]), '-y', str(resize[1]))
        actual = run('display-message', '-pt', pane, '#{cursor_x},#{cursor_y}')
        assert actual == cursor, (name, 'cursor', actual, cursor)
        if text is not None:
            actual = run('capture-pane', '-pt', pane, '-S0', '-E0')
            assert actual == text, (name, 'text', actual, text)
        if render is not None:
            run('select-window', '-t', pane)
            attach = shlex.join(tmux + ['attach-session'])
            client = subprocess.check_output(outer + ['new-window', '-d', '-P',
                '-F', '#{pane_id}', attach], text=True).strip()
            try:
                deadline = time.monotonic() + 3
                while time.monotonic() < deadline:
                    actual = subprocess.check_output(outer + ['capture-pane', '-pe',
                        '-t', client, '-S0', '-E0'], text=True).rstrip('\n')
                    if re.search(render, actual):
                        break
                    time.sleep(0.05)
                assert re.search(render, actual), (name, 'render', repr(actual), render)
                return actual
            finally:
                subprocess.run(outer + ['kill-window', '-t', client], check=True)
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
        run('set', '-g', 'status', 'off')
        run('set', '-as', 'terminal-features', ',*:sixel@')
        run('set', '-as', 'terminal-features', ',*:kitty@')
        run('set', '-as', 'terminal-features', ',*:RGB')
        subprocess.run(outer + ['new-session', '-d', '-x', '40', '-y', '12'], check=True)
        subprocess.run(outer + ['set', '-g', 'status', 'off'], check=True)
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
        check('columns-only', origin + graphics('a=T,q=2,f=32,s=1,v=1,c=2', pixel),
              '7,3')
        check('rows-only', origin + graphics('a=T,q=2,f=32,s=1,v=1,r=2', pixel),
              '9,4')
        check('chunks', graphics(placement + ',m=1', '/wAA') + origin +
              graphics('m=0', '/w=='), '8,4')
        check('chunks-no-cursor', graphics(placement + ',C=1,m=1', '/wAA') + origin +
              graphics('m=0', '/w=='), '5,2')
        check('sixel', origin + '\033Pq"1;1;1;1#0;2;100;0;0#0@\033\\', '0,3')
        sixel = '\033Pq"1;1;8;16#0;2;100;100;100#0!8~-!8~-!8N\033\\'
        sixel_render = check('sixel-render', sixel, '0,1', render=r'38;2;')
        check('kitty-delete-preserves-sixel', sixel + graphics('a=d,d=A,q=2'),
              '0,1', render='^' + re.escape(sixel_render) + '$')
        check('put-reply', origin + graphics('a=t,q=2,f=32,s=1,v=1,i=7', pixel) +
              graphics('a=p,i=7,p=9,c=3,r=2'), '8,4', graphics('i=7,p=9', 'OK'))
        check('missing-reply', origin + graphics('a=p,i=7,p=9'), '5,2',
              graphics('i=7,p=9', 'ENOENT'))
        check('invalid-reply', origin + graphics('a=T,i=7,p=9,f=32,s=1,v=1', '!!!!'),
              '5,2', graphics('i=7,p=9', 'EINVAL'))
        check('quiet-error', origin + graphics('a=T,q=2,i=7,f=32,s=1,v=1', '!!!!'), '5,2')
        check('empty-decoded-payload', origin +
              graphics('a=T,i=7,f=32,s=1,v=1', '    '), '5,2',
              graphics('i=7', 'EINVAL'))
        check('empty-decoded-chunk',
              graphics(placement + ',m=1', '    ') + origin +
              graphics('m=0', pixel), '8,4')
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
              '\033[38;2;0;0;7m\U0010eeee\u0305\u030e', '1,0',
              text='\U0010eeee\u0305\u030e')
        # Uppercase deletion must leave other numbered and unnumbered placements.
        transmit = graphics('a=t,q=2,i=7,f=32,s=1,v=1', pixel)
        check('delete-one-placement', transmit + graphics('a=p,q=2,i=7,p=1,C=1') +
              graphics('a=p,q=2,i=7,p=2,C=1') + graphics('a=d,d=I,q=2,i=7,p=1') +
              graphics('a=p,i=7,p=2,C=1'), '0,0', graphics('i=7,p=2', 'OK'))
        check('delete-last-placement', transmit + graphics('a=p,q=2,i=7,p=1,C=1') +
              graphics('a=d,d=I,q=2,i=7,p=1') + graphics('a=p,i=7'), '0,0',
              graphics('i=7', 'ENOENT'))
        check('delete-retain-unnumbered', transmit + graphics('a=p,q=2,i=7,p=1,C=1') +
              graphics('a=p,q=2,i=7,C=1') + graphics('a=d,d=I,q=2,i=7,p=1') +
              graphics('a=p,i=7,C=1'), '0,0', graphics('i=7', 'OK'))
        check('delete-soft', transmit + graphics('a=p,q=2,i=7,p=1,C=1') +
              graphics('a=d,d=i,q=2,i=7,p=1') + graphics('a=p,i=7,C=1'),
              '0,0', graphics('i=7', 'OK'))
        for selector in ['A', 'Z']:
            check('delete-hard-' + selector, transmit + graphics('a=p,q=2,i=7,C=1,z=9') +
                  graphics('a=d,d=%s,z=9,q=2' % selector) + graphics('a=p,i=7'),
                  '0,0', graphics('i=7', 'ENOENT'))
        # A fully historical placement retains its source after visible deletion.
        check('delete-history', transmit + graphics('a=p,q=2,i=7,C=1') + '\n' * 13 +
              graphics('a=d,d=A,q=2') + graphics('a=p,i=7,C=1'), '0,11',
              graphics('i=7', 'OK'))
        # Delete controls must not inherit an unfinished upload's image ID.
        check('delete-aborts-upload', transmit +
              graphics('a=t,q=2,i=7,f=32,s=1,v=1,m=1', '/wAA') +
              graphics('a=d,d=I') + graphics('a=p,i=7,C=1'), '0,0',
              graphics('i=7', 'OK'))
        for selector, coordinates in [('C', ''), ('P', ',x=1,y=1'),
                                      ('Q', ',x=1,y=1,z=9'), ('X', ',x=1'),
                                      ('Y', ',y=1'), ('R', ',x=7,y=7')]:
            check('delete-selector-' + selector, transmit +
                  graphics('a=p,q=2,i=7,C=1,z=9') +
                  graphics('a=d,d=%s,q=2%s' % (selector, coordinates)) +
                  graphics('a=p,i=7'), '0,0', graphics('i=7', 'ENOENT'))
        check('delete-misses', transmit + graphics('a=p,q=2,i=7,C=1,z=9') +
              graphics('a=d,d=Q,x=1,y=1,z=8,q=2') +
              graphics('a=d,d=P,x=2,y=1,q=2') +
              graphics('a=p,i=7,C=1'), '0,0', graphics('i=7', 'OK'))
        check('delete-range-unused-source', transmit + graphics('a=d,d=R,x=7,y=7,p=99') +
              graphics('a=p,i=7'), '0,0', graphics('i=7', 'ENOENT'))
        white = base64.b64encode(b'\xff' * (24 * 32 * 4)).decode()
        virtual = graphics('a=T,q=2,U=1,i=7,p=1,f=32,s=24,v=32,c=3,r=2', white)
        ph = '\U0010eeee'
        colours = '\033[38;2;0;0;7m\033[58;2;0;0;1m'
        check('query-retains-prototype', virtual +
              graphics('a=q,q=2,U=1,i=7,p=1,f=32,s=1,v=1,c=1,r=1', pixel) +
              colours + ph + '\u0305\u030e', '1,0', render=r'48;2;255;255;255m')
        check('multiple-virtual', virtual + graphics('a=p,q=2,U=1,i=7,p=2,c=1,r=1') +
              colours + ph + '\u0305\u030e', '1,0', text=ph + '\u0305\u030e',
              render=r'255;255;255m')
        high_virtual = graphics('a=T,q=2,U=1,i=16777223,f=32,s=24,v=32,c=3,r=2', white)
        check('inherit-high-byte', high_virtual + '\033[38;2;0;0;7m' +
              ph + '\u0305\u0305\u030d' + ph, '2,0',
              text=ph + '\u0305\u0305\u030d' + ph, render=r'48;2;255;255;255m {2}')
        check('inherit-high-byte-row', high_virtual + '\033[38;2;0;0;7m' +
              ph + '\u0305\u0305\u030d' + ph + '\u0305', '2,0',
              render=r'48;2;255;255;255m {2}')
        check('inherit-high-byte-column', high_virtual + '\033[38;2;0;0;7m' +
              ph + '\u0305\u0305\u030d' + ph + '\u0305\u030d', '2,0',
              render=r'48;2;255;255;255m {2}')
        red = base64.b64encode(b'\xff\0\0\xff' * (24 * 32)).decode()
        check('explicit-high-byte', high_virtual +
              graphics('a=T,q=2,U=1,i=7,f=32,s=24,v=32,c=3,r=2', red) +
              '\033[38;2;0;0;7m' + ph + '\u0305\u0305\u030d' +
              ph + '\u0305\u030d\u0305', '2,0', render=r'48;2;255;0;0m')
        for name, change, diacritics in [
                ('colour', '\033[38;2;0;0;8m', ''),
                ('placement', '\033[58;2;0;0;1m', ''),
                ('row', '', '\u030d'),
                ('column', '', '\u0305\u030e')]:
            check('inherit-mismatch-' + name, high_virtual + '\033[38;2;0;0;7m' +
                  ph + '\u0305\u0305\u030d' + change + ph + diacritics, '2,0',
                  render=ph)
        check('palette-id', virtual + '\033[38;5;7m\033[58;5;1m' + ph, '1,0',
              render=r'255;255;255m')
        check('negative-virtual', virtual + graphics('a=p,q=2,U=1,i=7,p=1,c=3,r=2,z=-1') +
              colours + ph, '1,0', render=r'255;255;255m')
        check('placeholder-resize', virtual + colours + ph, '1,0',
              render=r'48;2;255;255;255m(?:\x1b\[[0-9;]+m)* \x1b\[(?:0|39)m',
              resize=(50, 12))
        for selector in ['a', 'z']:
            check('virtual-survives-' + selector, virtual + colours + ph +
                  graphics('a=d,d=%s,q=2' % selector), '1,0', render=r'255;255;255m')
        # Uppercase spatial deletion preserves the display and its prototype.
        for selector, coordinates in [('A', ''), ('C', ''), ('P', ',x=1,y=1'),
                                      ('Q', ',x=1,y=1,z=0'), ('X', ',x=1'),
                                      ('Y', ',y=1'), ('Z', ',z=0')]:
            check('virtual-survives-' + selector, virtual + colours + ph +
                  '\033[H' + graphics('a=d,d=%s,q=2%s' % (selector, coordinates)) +
                  '\033[2G' + ph + '\u0305\u030d', '2,0',
                  render=r'48;2;255;255;255m(?:\x1b\[[0-9;]+m)* {2}')
        # Deleting a prototype leaves its existing text-backed display intact.
        check('virtual-delete-display', virtual + colours + ph +
              graphics('a=d,d=I,i=7,p=1,q=2'), '1,0', render=r'255;255;255m')
        check('virtual-delete-prototype', virtual + graphics('a=d,d=i,i=7,p=1,q=2') +
              colours + ph, '1,0', render=ph)
        check('virtual-delete-last', virtual + graphics('a=d,d=I,i=7,p=1,q=2') +
              graphics('a=p,i=7'), '0,0', graphics('i=7', 'ENOENT'))
        check('virtual-delete-one', virtual +
              graphics('a=p,q=2,U=1,i=7,p=2,c=3,r=2') +
              graphics('a=d,d=I,i=7,p=1,q=2') +
              '\033[38;2;0;0;7m\033[58;2;0;0;2m' + ph, '1,0',
              render=r'48;2;255;255;255m')
        check('virtual-delete-range', virtual + graphics('a=d,d=r,x=7,y=7,q=2') +
              colours + ph, '1,0', render=ph)
        # Retransmission removes old displays and prototypes, retaining text.
        check('virtual-source-replaced', virtual + colours + ph +
              graphics('a=t,q=2,i=7,f=32,s=1,v=1', pixel), '1,0', text=ph, render=ph)
        check('virtual-replace-display', virtual + colours + ph +
              graphics('a=p,i=7,p=1,q=2,C=1,c=3,r=2'), '1,0',
              render=r'^(?:\x1b\[[0-9;]+m)*\x1b\[48;2;255;255;255m(?:\x1b\[[0-9;]+m)* ')
        check('placeholder-overwrite', virtual + colours + ph + '\033[HX', '1,0',
              text='X', render=r'X')
        check('placeholder-erase', virtual + colours + ph + '\033[H\033[2K', '0,0',
              text='', render=r'^\s*$')
        white_pixel = '/////w=='
        check('letterbox', graphics('a=T,q=2,f=32,s=1,v=1,c=2,r=2,C=1', white_pixel),
              '0,0', render='▄▄')
        check('pillarbox', graphics('a=T,q=2,f=32,s=1,v=1,c=4,r=1,C=1', white_pixel),
              '0,0', render=(r'^(?:\x1b\[[0-9;]+m)* '
                             r'(?:\x1b\[[0-9;]+m)*\x1b\[48;2;255;255;255m {2}'))
        offset_source = base64.b64encode(b'\xff' * (8 * 32 * 4)).decode()
        check('scaled-offset', graphics('a=T,q=2,f=32,s=8,v=32,c=1,r=1,X=4,C=1',
                                       offset_source), '0,0', render=r'12[78];12[78];12[78]m')
        square = base64.b64encode(b'\xff' * (8 * 8 * 4)).decode()
        check('vertical-offset', graphics('a=T,q=2,f=32,s=8,v=8,c=1,r=1,Y=8,C=1',
                                         square), '0,0', render='▄')
        check('offset-clamp', graphics('a=T,q=2,f=32,s=1,v=1,X=4294967295,C=1',
                                      white_pixel), '0,0')
        check('oversized-placement', transmit + graphics('a=p,i=7,c=65534,r=32767'),
              '0,0', graphics('i=7', 'EINVAL'))
        large_rgb = base64.b64encode(zlib.compress(b'\0' * (4096 * 4097 * 3))).decode()
        check('oversized-rgb', graphics('a=t,i=7,f=24,s=4096,v=4097,o=z', large_rgb),
              '0,0', graphics('i=7', 'EINVAL'))
        for action in ['f', 'a', 'c']:
            check('unsupported-action-' + action, graphics('a=%s,i=7' % action),
                  '0,0', graphics('i=7', 'ENOTSUP'))
        for key in ['P', 'Q', 'H', 'V']:
            check('unsupported-key-' + key, graphics('a=p,i=7,%s=1' % key),
                  '0,0', graphics('i=7', 'ENOTSUP'))
        check('negative-relative-offset', graphics('a=p,i=7,H=-1'),
              '0,0', graphics('i=7', 'ENOTSUP'))
        for medium in ['f', 't', 's']:
            check('unsupported-medium-' + medium, graphics('a=t,i=7,t=' + medium),
                  '0,0', graphics('i=7', 'ENOTSUP'))
        check('relative-defaults', transmit + graphics('a=p,i=7,P=0,Q=0,H=0,V=0,C=1'),
              '0,0', graphics('i=7', 'OK'))
        check('unsupported-number', graphics('a=t,I=7,f=32,s=1,v=1', pixel),
              '0,0', graphics('I=7', 'ENOTSUP'))
        check('id-and-number', graphics('a=t,i=7,I=8,f=32,s=1,v=1', pixel),
              '0,0', graphics('i=7,I=8', 'EINVAL'))
        check('unknown-extension', graphics('a=T,q=2,f=32,s=1,v=1,c=3,r=2,k=9', pixel),
              '3,2')
    finally:
        subprocess.run(tmux + ['kill-server'], stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL)
        subprocess.run(outer + ['kill-server'], stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL)
PY
