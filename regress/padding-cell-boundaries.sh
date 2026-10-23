#!/bin/sh

# Check the stored grid and the output to an attached terminal separately.
# Each edit must preserve the text and backgrounds outside its range, and
# leave every tab or wide character with exactly the padding it owns.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import os
import re
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path

binary = sys.argv[1]
width = 24

# Positions and colour ranges are zero based. Expected lines are independent
# of the tab representation and include the columns of wide characters.
cases = [
    ('tab-insert-middle', 'ab\tXYZ', 4, '\033[2@', 'ab        XYZ', (4, 6)),
    ('tab-insert-leading', 'ab\tXYZ', 2, '\033[@', 'ab       XYZ', (2, 3)),
    ('tab-insert-after', 'ab\tXYZ', 8, '\033[@', 'ab       XYZ', (8, 9)),
    ('tab-insert-right-edge', '0123456789012345\tZ', 0, '\033[2@',
     '  0123456789012345      ', (0, 2)),
    ('insert-whole-line', 'abcdef', 0, '\033[999@', '', (0, 24)),
    ('insert-to-edge', 'abcdef', 3, '\033[999@', 'abc', (3, 24)),
    ('insert-exactly-to-edge', 'abcdef', 3, '\033[21@', 'abc', (3, 24)),
    ('insert-with-gap', 'abcdefghijklmnopqrstuvwx', 16, '\033[6@',
     'abcdefghijklmnop      qr', (16, 22)),
    ('insert-wide-in-gap', 'ab界XYZ              界', 15, '\033[7@',
     'ab界XYZ', (15, 22)),
    ('insert-tab-to-edge', 'ab\tXYZ', 4, '\033[999@', 'ab', (4, 24)),
    ('insert-wide-to-edge', 'ab界XYZ', 3, '\033[999@', 'ab', (3, 24)),
    ('insert-last-column', 'abcdefghijklmnopqrstuvwx', 23, '\033[999@',
     'abcdefghijklmnopqrstuvw', (23, 24)),
    ('insert-zero-count', 'abc', 1, '\033[0@', 'a bc', (1, 2)),
    ('tab-delete-middle', 'ab\tXYZ', 4, '\033[2P', 'ab    XYZ', (22, 24)),
    ('tab-delete-end', 'ab\tXYZ', 1, '\033[3P', 'a    XYZ', (21, 24)),
    ('tab-delete-leading', 'ab\tXYZ', 2, '\033[P', 'ab     XYZ', (23, 24)),
    ('tab-delete-whole', 'ab\tXYZ', 2, '\033[6P', 'abXYZ', (18, 24)),
    ('tab-erase-middle', 'ab\tXYZ', 4, '\033[2X', 'ab      XYZ', (4, 6)),
    ('tab-erase-end', 'ab\tXYZ', 1, '\033[3X', 'a       XYZ', (1, 4)),
    ('tab-erase-leading', 'ab\tXYZ', 2, '\033[X', 'ab      XYZ', (2, 3)),
    ('tab-erase-whole', 'ab\tXYZ', 2, '\033[6X', 'ab      XYZ', (2, 8)),
    ('tab-delete-to-edge', '0123456789012345\tZ', 20, '\033[999P',
     '0123456789012345        ', (20, 24)),
    ('tab-erase-to-edge', '0123456789012345\tZ', 20, '\033[999X',
     '0123456789012345        ', (20, 24)),
    ('wide-insert-middle', 'ab界XYZ', 3, '\033[@', 'ab   XYZ', (3, 4)),
    ('wide-insert-right-edge', '0123456789012345678901界', 0, '\033[@',
     ' 0123456789012345678901 ', (0, 1)),
    ('wide-delete-middle', 'ab界XYZ', 3, '\033[P', 'ab XYZ', (23, 24)),
    ('wide-delete-end', 'ab界XYZ', 1, '\033[2P', 'a XYZ', (22, 24)),
    ('wide-erase-middle', 'ab界XYZ', 3, '\033[X', 'ab  XYZ', (3, 4)),
    ('wide-erase-end', 'ab界XYZ', 1, '\033[2X', 'a   XYZ', (1, 3)),
    ('write-ascii-in-tab', 'ab\tXYZ', 4, 'Q', 'ab  Q   XYZ', (4, 5)),
    ('write-wide-in-tab', 'ab\tXYZ', 4, '語', 'ab  語  XYZ', (4, 6)),
    ('write-wide-leading', 'ab界XYZ', 2, '語', 'ab語XYZ', (2, 4)),
    ('write-wide-end', 'ab界XYZ', 1, '語', 'a語 XYZ', (1, 3)),
    ('write-wide-both-ends', 'ab界界Z', 3, '語', 'ab 語 Z', (3, 5)),
    ('write-wide-two-tabs', '\t\tZ', 7, '語', '       語       Z', (7, 9)),
    ('write-wide-at-edge', '0123456789012345678901界', 22, '語',
     '0123456789012345678901語', (22, 24)),
    ('irm-tab-middle', 'ab\tXYZ', 4, '\033[4hQ\033[4l',
     'ab  Q    XYZ', (4, 5)),
    ('irm-wide-in-tab', 'ab\tXYZ', 4, '\033[4h語\033[4l',
     'ab  語    XYZ', (4, 6)),
    ('irm-tab-leading', 'ab\tXYZ', 2, '\033[4hQ\033[4l',
     'abQ      XYZ', (2, 3)),
    ('irm-tab-after', 'ab\tXYZ', 8, '\033[4hQ\033[4l',
     'ab      QXYZ', (8, 9)),
    ('irm-wide-middle', 'ab界XYZ', 3, '\033[4hQ\033[4l',
     'ab Q XYZ', (3, 4)),
    ('irm-wide-in-wide', 'ab界XYZ', 3, '\033[4h語\033[4l',
     'ab 語 XYZ', (3, 5)),
    ('irm-wide-leading', 'ab界XYZ', 2, '\033[4hQ\033[4l',
     'abQ界XYZ', (2, 3)),
    ('irm-wide-right-edge', '0123456789012345678901界', 0,
     '\033[4hQ\033[4l', 'Q0123456789012345678901 ', (0, 1)),
    ('irm-tab-right-edge', '0123456789012345\tZ', 0,
     '\033[4h語\033[4l', '語0123456789012345      ', (0, 2)),
    ('irm-repeated-right-edge', '0123456789012345\tZ', 0,
     '\033[4hQR\033[4l', 'QR0123456789012345      ', (0, 2)),
    ('irm-both-ends', 'ab界012345678901234567界', 3,
     '\033[4hQ\033[4l', 'ab Q 012345678901234567 ', (3, 4)),
    ('irm-last-column', 'abcdefghijklmnopqrstuvwx', 23,
     '\033[4hQ\033[4l', 'abcdefghijklmnopqrstuvwQ', (23, 24)),
    ('irm-combining-in-tab', 'ab\tXYZ', 4, '\033[4h✔️\033[4l',
     'ab  ✔️    XYZ', (4, 6)),
    ('irm-combining-right-edge', '012345678901234567890界Z', 0,
     '\033[4h✔️\033[4l', '✔️012345678901234567890 ', (0, 2)),
]
styles = [
    ('default', '\033[0m', 8, '\033[0m', 8),
    ('basic', '\033[44m', 4, '\033[43m', 3),
    ('indexed', '\033[48;5;123m', 0x100007b, '\033[43m', 3),
    ('rgb', '\033[48;2;12;34;56m', 0x20c2238, '\033[43m', 3),
]
rows = [(case, style) for style in styles for case in cases]


def cup(y, x=0):
    return f'\033[{y + 1};{x + 1}H'


def run(server, *args):
    return subprocess.check_output(server + list(args), text=True)


def common(helper, *args):
    subprocess.check_call(['/bin/sh', '-c', '. ./regress-common.inc; "$@"',
                           'sh', helper, *args])


def wait_marker(server, marker):
    common('wait_for_text', marker, *server,
           'capture-pane', '-p', '-t', 'test:0.0')


def cells_for_text(text):
    cells = []
    for char in re.findall('✔️|.', text):
        cells.append(char)
        if char in ('界', '語', '✔️'):
            cells.append('')
    assert len(cells) <= width, text
    return cells + [' '] * (width - len(cells))


def check_grid(server, where):
    raw = run(server, 'capture-pane', '-pR', '-t', 'test:0.0')
    cells = {}
    for line in raw.splitlines():
        match = re.search(
            r'C (\d+),(\d+) data=\((\d+),\d+,(.*?)\) '
            r'flags=([^\[]+)\[.*? bg=[^\[]+\[([0-9a-f]+)\]', line)
        if match:
            y, x, size = map(int, match.group(1, 2, 3))
            cells[y, x] = (size, match[4], match[5], int(match[6], 16))
    for y, (case, style) in enumerate(rows):
        name, seed, pos, edit, expected, changed = case
        label, seed_sgr, seed_bg, edit_sgr, edit_bg = style
        context = f'{where}: {label}/{name}'
        actual = []
        x = 0
        while x < width:
            size, data, flags, bg = cells[y, x]
            assert 'PADDING' not in flags, (context, x, 'orphan padding')
            assert size > 0 and x + size <= width, (context, x, size)
            for padding in range(x + 1, x + size):
                assert 'PADDING' in cells[y, padding][2], (
                    context, x, padding, 'missing padding')
            if 'TAB' in flags:
                actual.extend([' '] * size)
            else:
                actual.append(data)
                actual.extend([''] * (size - 1))
            x += size
        assert actual == cells_for_text(expected), (context, actual, expected)
        for x in range(width):
            bg = edit_bg if changed[0] <= x < changed[1] else seed_bg
            assert cells[y, x][3] == bg, (
                context, x, 'background', hex(cells[y, x][3]), hex(bg))


with tempfile.TemporaryDirectory(prefix='tmux-padding-', dir='/tmp') as directory:
    root = Path(directory)
    os.environ['TMUX_TMPDIR'] = str(root)
    config = root / 'tmux.conf'
    config.write_text('set -g status off\nset -g window-size manual\n'
                      'set -g default-terminal screen\n'
                      'set -g default-shell /bin/sh\n')
    seeds = root / 'seeds'
    edits = root / 'edits'
    seeds.write_text(''.join(
        cup(y) + style[1] + '\033[2K' + case[1]
        for y, (case, style) in enumerate(rows)) +
        cup(len(rows)) + '\033[0mSEED-READY')
    edits.write_text(''.join(
        cup(y, case[2]) + style[3] + case[3]
        for y, (case, style) in enumerate(rows)) +
        cup(len(rows)) + '\033[0m\033[2KEDIT-DONE')

    for mode in ('redraw', 'live-full', 'live-split'):
        suffix = str(os.getpid()) + '-' + mode
        inner = [binary, '-Lpadding-inner-' + suffix, '-f', str(config)]
        outer = [binary, '-Lpadding-outer-' + suffix, '-f', str(config)]
        columns = 49 if mode == 'live-split' else width
        command = (
            shlex.join(inner + ['wait-for', 'start']) + '; cat ' +
            shlex.quote(str(seeds)) + '; ' +
            shlex.join(inner + ['wait-for', 'edit']) + '; cat ' +
            shlex.quote(str(edits)) + '; exec sleep 60')
        try:
            run(inner, 'new-session', '-d', '-s', 'test', '-x', str(columns),
                '-y', str(len(rows) + 1), command)
            if mode == 'live-split':
                run(inner, 'split-window', '-dh', '-l', '24', 'exec sleep 60')
            run(inner, 'wait-for', '-S', 'start')
            wait_marker(inner, 'SEED-READY')
            if mode == 'redraw':
                run(inner, 'wait-for', '-S', 'edit')
                wait_marker(inner, 'EDIT-DONE')
            run(outer, 'new-session', '-d', '-s', 'test', '-x', str(columns),
                '-y', str(len(rows) + 1), shlex.join(inner + ['attach-session']))
            if mode != 'redraw':
                wait_marker(outer, 'SEED-READY')
                run(inner, 'wait-for', '-S', 'edit')
            wait_marker(outer, 'EDIT-DONE')
            check_grid(inner, mode + ' grid')
            check_grid(outer, mode + ' terminal')
            print(f'{mode}: {len(rows)} text, padding and colour checks passed')
        finally:
            for server in (outer, inner):
                common('stop_server', *server)
PY
