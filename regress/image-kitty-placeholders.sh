#!/bin/sh

# Kitty Unicode placeholders resolve to images and follow their text cells.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, kitty
from image_terminal import Terminal
from image_kitty import KittyTerminal

ph = '\U0010eeee'
marks = ['\u0305', '\u030d', '\u030e']
colours = [(255, 0, 0), (0, 255, 0), (0, 0, 255),
    (0, 255, 255), (255, 0, 255), (255, 255, 0)]
raw = bytes(c for y in range(32) for x in range(24)
    for c in (*colours[y // 16 * 3 + x // 8], 255))


def cell(*values):
    """Encode explicit row, column and high image ID byte marks."""
    return (ph + ''.join(marks[value] for value in values)).encode()


def ids(image=7, placement=1):
    """Encode image and placement IDs in foreground and underline colours."""
    return ('\033[38;2;%d;%d;%dm\033[58;2;%d;%d;%dm' % (
        image >> 16 & 255, image >> 8 & 255, image & 255,
        placement >> 16 & 255, placement >> 8 & 255, placement & 255)).encode()


def virtual(image=7, placement=1, columns=3, rows=2, pixels=raw, w=24, h=32):
    """Transmit pixels and create an invisible virtual placement."""
    return kitty('a=T,U=1,i=%d,p=%d,f=32,s=%d,v=%d,c=%d,r=%d' % (
        image, placement, w, h, columns, rows), pixels)


def rectangle(colour, x=0, y=0, w=8, h=16):
    """Expected image pixels at a terminal pixel position."""
    return {(x + xx, y + yy): colour for yy in range(h) for xx in range(w)}


pattern = {}
for i, colour in enumerate(colours):
    pattern.update(rectangle(colour, i % 3 * 8, i // 3 * 16))
text = ids() + cell(0) + cell() * 2 + b'\033[2;1H' + cell(1) + cell() * 2

for mode in ('sixel', 'kitty', 'fallback'):
    with image_test(sys.argv[1]) as test:
        test.run('set-option', '-as', 'terminal-features',
            ',screen-256color:RGB')
        client = test.attach(graphical=mode != 'fallback',
            kitty_graphics=mode == 'kitty')
        terminal = KittyTerminal() if mode == 'kitty' else Terminal()

        def expect(expected, hidden=True):
            """Check the current output and a full redraw through each backend."""
            for redraw in (False, True):
                if redraw:
                    test.redraw()
                terminal.feed(client.data)
                if mode != 'fallback':
                    terminal.check(expected)
                elif expected:
                    cells = {(x // 8, y // 16) for x, y in expected}
                    for x, y in cells:
                        samples = []
                        for half in range(2):
                            pixels = [expected.get((x * 8 + xx,
                                y * 16 + half * 8 + yy), (0, 0, 0))
                                for yy in range(8) for xx in range(8)]
                            samples.append(tuple(sum(p[c] for p in pixels)
                                // 64 for c in range(3)))
                        actual = terminal.cells[x, y]
                        assert actual[:3] == ('\u2584', samples[1], samples[0]), (
                            mode, x, y, actual, samples)
                else:
                    assert all(c[0] != '\u2584' for c in terminal.cells.values())
                if redraw and hidden:
                    assert ph.encode() not in client.data, ('visible placeholder', mode)

        def check(data, expected, hidden=True):
            """Clear earlier text and check a new placeholder display."""
            test.emit(b'\033[0m\033[2J\033[H' + data)
            expect(expected, hidden)

        # Virtual uploads are invisible and do not move the cursor.
        check(virtual(), {})
        assert test.run('display-message', '-p', '#{cursor_x},#{cursor_y}') == '0,0'

        # Missing columns inherit from the left cell, independently on each row.
        check(virtual() + text, pattern)
        assert test.run('capture-pane', '-p').count(ph) == 6

        # Explicit source cells and underline colour select the right prototype.
        check(virtual() + kitty('a=p,U=1,i=7,p=2,c=1,r=1') +
            ids() + cell(1, 2) + cell(0, 1),
            rectangle(colours[5]) | rectangle(colours[1], x=8))
        check(virtual() + b'\033[38;5;7m\033[58;5;1m' + cell(1, 0),
            rectangle(colours[3]))

        # The third mark supplies the high image ID byte. Missing marks inherit it.
        high = 7 + (1 << 24)
        for following in (cell(), cell(0), cell(0, 1)):
            check(virtual(high) + ids() + cell(0, 0, 1) + following,
                rectangle(colours[0]) | rectangle(colours[1], x=8))
        check(virtual(high) + ids() + cell(0, 0, 1) +
            kitty('a=d,d=i,i=%d,p=1' % high) +
            kitty('a=p,U=1,i=%d,p=1,c=3,r=2' % high) + cell(),
            rectangle(colours[0]) | rectangle(colours[1], x=8))
        red = bytes((*colours[0], 255))
        check(virtual(high) + virtual(pixels=red * (24 * 32)) + ids() +
            cell(0, 0, 1) + cell(0, 1, 0),
            rectangle(colours[0]) | rectangle(colours[0], x=8))

        # Queries do not replace an existing virtual placement's geometry.
        check(virtual() + kitty('a=q,U=1,i=7,p=1,f=32,s=1,v=1,c=1,r=1', red) +
            ids() + cell(0, 2), rectangle(colours[2]))

        # Completed chunks create the same virtual placement as one upload.
        check(kitty('a=T,U=1,i=7,p=1,f=32,s=24,v=32,c=3,r=2,m=1', raw[:512]) +
            kitty('m=0', raw[512:]) + text, pattern)
        for invalid in (cell(2, 0), cell(0, 0) + '\u0300'.encode(),
                cell(0, 0, 0) + marks[0].encode()):
            check(virtual() + ids() + invalid, {}, hidden=False)

        # All-visible deletion leaves the prototypes and their text displays.
        for selector in ('a', 'A'):
            check(virtual() + ids() + cell(0, 0) + kitty('a=d,d=' + selector) +
                cell(0, 1), rectangle(colours[0]) | rectangle(colours[1], x=8))

        # Deleting or replacing a prototype retains its existing text display.
        check(virtual() + ids() + cell(0, 0) + kitty('a=d,d=i,i=7,p=1') +
            cell(0, 1), rectangle(colours[0]), hidden=False)
        check(virtual() + ids() + cell(0, 2) +
            kitty('a=p,U=1,i=7,p=1,c=1,r=1') + cell(0, 2),
            rectangle(colours[2]), hidden=False)

        # Replacing source pixels discards old displays but leaves their text.
        check(virtual() + ids() + cell(0, 0) +
            kitty('a=t,i=7,f=32,s=1,v=1', red), {}, hidden=False)
        assert ph in test.run('capture-pane', '-p')

        # Text writes and erases remove the matching image cells.
        for erase in (b'X', b'\033[X', b'\033[2K', b'\033[2J'):
            check(virtual() + ids() + cell(0, 0) + b'\033[H' + erase, {})

        # Inserting and deleting cells moves placeholders with their text.
        check(virtual() + ids() + cell(0) + cell() * 2 + b'\033[H\033[@',
            rectangle(colours[0], x=8) | rectangle(colours[1], x=16) |
            rectangle(colours[2], x=24))
        test.emit(b'\033[1;2H\033[P')
        expect(rectangle(colours[1], x=8) | rectangle(colours[2], x=16))

        # Inserting lines also moves placements which cross the scroll region edge.
        check(virtual() + text + b'\033[2;12r\033[2;1H\033[L',
            {point if point[1] < 16 else (point[0], point[1] + 16): colour
                for point, colour in pattern.items()})
        test.emit(b'\033[r')

        # Virtual placements preserve aspect ratio and centre the image.
        white = b'\xff' * 4
        block = ids() + cell(0) + cell() + b'\033[2;1H' + cell(1) + cell()
        check(virtual(columns=2, rows=2, pixels=white, w=1, h=1) + block,
            rectangle((255, 255, 255), y=8, w=16, h=16))
        check(virtual(columns=4, rows=1, pixels=white, w=1, h=1) +
            ids() + cell(0) + cell() * 3,
            rectangle((255, 255, 255), x=8, w=16, h=16))

        # A copied grid retains placeholder spans and their source positions.
        check(virtual() + text, pattern)
        test.run('copy-mode')
        test.redraw()
        terminal.feed(client.data)
        test.run('send-keys', '-X', 'cancel')
        test.redraw()
        expect(pattern)

        # Narrowing clips image cells without reflowing their source positions.
        test.resize(client, 2, 16)
        terminal.sx = 2
        terminal.pixels = {point: colour for point, colour in terminal.pixels.items()
            if point[0] < 16}
        expect({point: colour for point, colour in pattern.items() if point[0] < 16})
        test.resize(client, 40, 16)
        terminal.sx = 40
        expect(pattern)

        # Evicting a source also drops retained virtual placements safely.
        test.run('set-option', '-s', 'image-total-count-limit', '2')
        check(virtual(8, columns=1, rows=1, pixels=white * 128, w=8, h=16) +
            ids(8) + cell(0, 0),
            rectangle((255, 255, 255)))
PY
