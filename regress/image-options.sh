#!/bin/sh

# Runtime image size, memory and count limits with small uploads.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import sys
import zlib
from image_test import image_test, kitty, png, sixel
from image_terminal import Terminal


def snapshot(test, client, expected):
    """Check the image pixels after a full redraw."""
    test.redraw()
    terminal = Terminal()
    terminal.feed(client.data)
    terminal.check(expected)


red, green, blue = (255, 0, 0), (0, 255, 0), (0, 0, 255)
raw = bytes((*red, 255)) * 256
with image_test(sys.argv[1]) as test:
    client = test.attach()
    for option, value in [('image-size-limit', 64 * 1024 * 1024),
                          ('image-total-count-limit', 1000),
                          ('image-total-size-limit', 1024 * 1024 * 1024)]:
        assert int(test.run('show-options', '-sv', option)) == value, option
    test.run('set-option', '-s', 'image-size-limit', '1024')
    expected = {(x, y): red for y in range(16) for x in range(16)}

    # Chunked uploads can fill the limit exactly and reject the next bytes.
    command = kitty('a=T,i=1,f=32,s=16,v=16,C=1,m=1', raw[:512]) + \
        kitty('m=0', raw[512:])
    test.emit(command)
    snapshot(test, client, expected)
    test.emit(b'\033[2J\033[H' + command.replace(b'v=16', b'v=17') +
        kitty('a=p,i=1,C=1'))
    snapshot(test, client, expected)
    test.emit(b'\033[2J\033[H' +
        kitty('a=T,i=2,f=32,s=16,v=17,C=1,m=1', raw[:512]) +
        kitty('m=1', raw[512:]) + kitty('m=0', bytes((*red, 255))) +
        kitty('a=p,i=2,C=1'))
    snapshot(test, client, {})

    # Compressed raw and PNG data must also respect the decoded pixel limit.
    for height in (16, 17):
        pixels = bytes((*red, 255)) * 16 * height
        commands = [kitty('a=T,f=32,s=16,v=%d,o=z,C=1' % height,
            zlib.compress(pixels)), kitty('a=T,f=100,C=1',
            png(16, height, [bytes((*red, 255)) * 16] * height)),
            sixel(16, height, [(100, 0, 0)], [0] * (16 * height))]
        for command in commands:
            test.emit(b'\033[2J\033[H' + command)
            snapshot(test, client, expected if height == 16 else {})

    # Lowering the limit during an upload rejects the buffered excess.
    test.emit(kitty('a=t,i=3,f=32,s=16,v=16,m=1', raw[:768]))
    test.run('set-option', '-s', 'image-size-limit', '512')
    test.emit(kitty('m=0', raw[768:]) + kitty('a=p,i=3,C=1'))
    snapshot(test, client, {})
    test.emit(kitty('a=T,f=32,s=1,v=1,c=1,r=1,C=1', bytes((*green, 255))))
    snapshot(test, client, {(x, y): green
        for y in range(16) for x in range(8)})

    # Transfer buffers grow correctly with a limit that is not a power of two.
    test.run('set-option', '-s', 'image-size-limit', '6000')
    pixels = bytes((*red, 255)) * 1500
    test.emit(b'\033[2J\033[H' +
        kitty('a=T,f=32,s=25,v=60,C=1,m=1', pixels[:3000]) +
        kitty('m=0', pixels[3000:]))
    snapshot(test, client, {(x, y): red
        for y in range(60) for x in range(25)})

# Sources share their pixels with placements but count as separate images.
for option, limit in [('image-total-count-limit', 4),
                      ('image-total-size-limit', 8)]:
    with image_test(sys.argv[1]) as test:
        client = test.attach()
        test.run('set-option', '-s', option, str(limit))
        for image_id, colour in [(1, red), (2, green)]:
            test.emit(('\033[%d;1H' % image_id).encode() +
                kitty('a=T,i=%d,f=32,s=1,v=1,c=1,r=1,C=1' % image_id,
                    bytes((*colour, 255))))
        snapshot(test, client, {(x, y): red if y < 16 else green
            for y in range(32) for x in range(8)})
        test.run('set-option', '-s', option, str(limit // 2))
        test.emit(b'\033[3;1H' +
            kitty('a=T,i=3,f=32,s=1,v=1,c=1,r=1,C=1', bytes((*blue, 255))))
        snapshot(test, client, {(x, y): blue
            for y in range(32, 48) for x in range(8)})
PY
