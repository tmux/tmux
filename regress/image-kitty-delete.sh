#!/bin/sh

# Kitty deletion selects placements and preserves other images and text.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, kitty, sixel
from image_terminal import Terminal


def rectangle(colour, xoff=0, yoff=0):
    """Expected pixels for a one-column, two-row placement."""
    return {(x + xoff, y + yoff): colour for y in range(32) for x in range(8)}


def snapshot(test, client, expected):
    """Check image positions and colours after a full redraw."""
    test.redraw()
    terminal = Terminal()
    terminal.feed(client.data)
    terminal.check(expected)
    return terminal


red, green, blue, white = (255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255)
uploads = kitty('a=t,i=7,f=32,s=1,v=1', bytes((*red, 255))) + \
    kitty('a=t,i=8,f=32,s=1,v=1', bytes((*green, 255)))
put = 'a=p,i=7,C=1,c=1,r=2'
display = b'\033[H' + kitty(put + ',p=1') + b'\033[1;3H' + \
    kitty(put + ',p=2,z=1') + b'\033[1;5H' + \
    kitty('a=p,i=8,p=1,C=1,c=1,r=2,z=1') + b'\033[1;7H' + \
    kitty('a=T,f=32,s=1,v=1,C=1,c=1,r=2', bytes((*blue, 255))) + \
    b'\033[1;9H' + sixel(8, 32, [(100, 100, 100)], [0] * 256) + b'\033[2;3H'
pictures = dict(first=rectangle(red), second=rectangle(red, 16),
    other=rectangle(green, 32), anonymous=rectangle(blue, 48),
    sixel=rectangle(white, 64))
cases = [
    ('one placement', kitty('a=d,d=i,i=7,p=1'),
        ['second', 'other', 'anonymous', 'sixel']),
    ('one image', kitty('a=d,d=i,i=7'), ['other', 'anonymous', 'sixel']),
    ('placement zero', kitty('a=d,d=i,i=7,p=0'),
        ['other', 'anonymous', 'sixel']),
    ('placement IDs belong to each image', kitty('a=d,d=i,i=8,p=1'),
        ['first', 'second', 'anonymous', 'sixel']),
    ('missing image', kitty('a=d,d=I,i=999'), list(pictures)),
    ('missing placement', kitty('a=d,d=I,i=7,p=999'), list(pictures)),
    ('missing image ID', kitty('a=d,d=I'), list(pictures)),
    ('all images', kitty('a=d,d=a'), ['sixel']),
    ('default selector', kitty('a=d'), ['sixel']),
    ('all ignores image and placement IDs', kitty('a=d,d=A,i=7,p=1'),
        ['sixel']),
    ('repeated deletion', kitty('a=d,d=I,i=7,p=1') * 2,
        ['second', 'other', 'anonymous', 'sixel']),
    ('selector resets', kitty('a=d,d=i,i=7') + kitty('a=d'), ['sixel']),
    ('cursor position', kitty('a=d,d=c'),
        ['first', 'other', 'anonymous', 'sixel']),
    ('cell position', kitty('a=d,d=p,x=1,y=2'),
        ['second', 'other', 'anonymous', 'sixel']),
    ('anonymous placement', kitty('a=d,d=P,x=7,y=1'),
        ['first', 'second', 'other', 'sixel']),
    ('column', kitty('a=d,d=X,x=5'),
        ['first', 'second', 'anonymous', 'sixel']),
    ('row', kitty('a=d,d=Y,y=2'), ['sixel']),
    ('row boundary', kitty('a=d,d=Y,y=3'), list(pictures)),
    ('range includes both endpoints', kitty('a=d,d=R,x=7,y=8'),
        ['anonymous', 'sixel']),
    ('range ignores image and placement IDs', kitty('a=d,d=r,x=7,y=7,i=8,p=1'),
        ['other', 'anonymous', 'sixel']),
    ('range excludes lower ID', kitty('a=d,d=R,x=8,y=9'),
        ['first', 'second', 'anonymous', 'sixel']),
    ('reversed range', kitty('a=d,d=R,x=8,y=7'), list(pictures)),
    ('spatial selection ignores IDs', kitty('a=d,d=P,x=1,y=1,i=8,p=99'),
        ['second', 'other', 'anonymous', 'sixel']),
    ('cell and layer', kitty('a=d,d=q,x=3,y=2,z=1'),
        ['first', 'other', 'anonymous', 'sixel']),
    ('cell and default layer', kitty('a=d,d=Q,x=1,y=1'),
        ['second', 'other', 'anonymous', 'sixel']),
    ('cell layer mismatch', kitty('a=d,d=Q,x=1,y=1,z=1'), list(pictures)),
    ('anonymous cell and layer', kitty('a=d,d=Q,x=7,y=1,z=0'),
        ['first', 'second', 'other', 'sixel']),
    ('layer', kitty('a=d,d=z,z=1'), ['first', 'anonymous', 'sixel']),
    ('default layer preserves SIXEL', kitty('a=d,d=Z'),
        ['second', 'other', 'sixel']),
    ('missing layer', kitty('a=d,d=Z,z=-2147483648'), list(pictures)),
    ('layer ignores image and placement IDs', kitty('a=d,d=Z,z=1,i=7,p=1'),
        ['first', 'anonymous', 'sixel']),
]
for position in ['d=P,x=0,y=1', 'd=P,x=1,y=0', 'd=P,x=1', 'd=P,y=1',
                 'd=P,x=41,y=1', 'd=P,x=1,y=17', 'd=X,x=4294967295',
                 'd=Y,y=4294967295', 'd=Q,x=0,y=1,z=0',
                 'd=Q,x=1,y=0,z=0', 'd=Q,x=41,y=1,z=0']:
    cases.append(('invalid position ' + position, kitty('a=d,' + position),
        list(pictures)))
with image_test(sys.argv[1]) as test:
    test.run('set-option', '-g', 'scroll-on-clear', 'off')
    client = test.attach()
    fallback = test.attach(graphical=False)
    for name, command, retained in cases:
        test.emit(b'\033[2J' + uploads + display)
        original = {point: colour for image in pictures.values()
            for point, colour in image.items()}
        terminal = snapshot(test, client, original)
        cursor = test.run('display-message', '-p', '#{cursor_x},#{cursor_y}')
        test.emit(command)
        wanted = {point: colour for name in retained
            for point, colour in pictures[name].items()}
        try:
            terminal.feed(client.data)
            terminal.check(wanted)
            snapshot(test, client, wanted)
            assert test.run('display-message', '-p',
                '#{cursor_x},#{cursor_y}') == cursor
            assert not fallback.images(), 'graphics sent to fallback client'
        except AssertionError as e:
            raise AssertionError(name) from e

    # A cell selects every overlapping placement, including covered images.
    test.emit(b'\033[2J\033[H' + uploads + kitty(put + ',p=1') +
        kitty('a=p,i=8,p=1,C=1,c=1,r=2') + b'\033[1;3H' + kitty(put + ',p=2'))
    terminal = snapshot(test, client, rectangle(green) | rectangle(red, 16))
    test.emit(kitty('a=d,d=P,x=1,y=2'))
    terminal.feed(client.data)
    terminal.check(rectangle(red, 16))
    snapshot(test, client, rectangle(red, 16))

    # Cell-and-layer deletion selects covered images and every matching overlap.
    test.emit(b'\033[2J\033[H' + uploads + kitty(put + ',p=1,z=0') +
        kitty(put + ',p=2,z=0') + kitty('a=p,i=8,C=1,c=1,r=2,z=1'))
    snapshot(test, client, rectangle(green))
    test.emit(kitty('a=d,d=q,x=1,y=1,z=0') + kitty('a=d,d=i,i=8'))
    snapshot(test, client, {})

    # Deleting an image restores the text which was updated beneath it.
    test.emit(b'\033[2J\033[H' + uploads + kitty(put + ',p=1') + b'\033[HX')
    assert test.run('capture-pane', '-p') == 'X'
    test.emit(kitty('a=d,d=I,i=7'))
    snapshot(test, client, {})
    for attached in [client, fallback]:
        terminal = Terminal()
        terminal.feed(attached.data)
        assert terminal.cells[0, 0][0] == 'X', terminal.cells[0, 0]

# A visible part selects its whole placement, including the part in history.
with image_test(sys.argv[1]) as test:
    client = test.attach()
    test.emit(uploads + b'\033[H' + kitty(put + ',p=1') + b'\033[S')
    snapshot(test, client, {point: colour
        for point, colour in rectangle(red, yoff=-16).items() if point[1] >= 0})
    test.emit(kitty('a=d,d=P,x=1,y=1'))
    snapshot(test, client, {})
    test.run('copy-mode')
    test.run('send-keys', '-X', 'history-top')
    snapshot(test, client, {})
    test.run('send-keys', '-X', 'cancel')

# Deletion on the alternate screen leaves the saved primary placements intact.
with image_test(sys.argv[1]) as test:
    client = test.attach()
    test.emit(uploads + b'\033[H' + kitty(put + ',p=1') + b'\033[?1049h\033[H' +
        kitty(put + ',p=2'))
    snapshot(test, client, rectangle(red))
    test.emit(kitty('a=d,d=R,x=7,y=7,i=999,p=99'))
    snapshot(test, client, {})
    test.emit(b'\033[?1049l')
    snapshot(test, client, rectangle(red))
    test.emit(b'\033[1;3H' + kitty(put + ',p=2'))
    snapshot(test, client, rectangle(red) | rectangle(red, 16))

# Layer deletion includes placements which have scrolled entirely into history.
with image_test(sys.argv[1]) as test:
    client = test.attach()
    test.emit(uploads + b'\033[H' + kitty(put + ',p=1,z=5') + b'\033[16S')
    snapshot(test, client, {})
    test.emit(kitty('a=d,d=Z,z=5'))
    test.run('copy-mode')
    test.run('send-keys', '-X', 'history-top')
    snapshot(test, client, {})
    test.run('send-keys', '-X', 'cancel')
PY
