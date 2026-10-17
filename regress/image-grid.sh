#!/bin/sh

# Images follow grid movement, overwrites, clear, reflow and copy mode.
# Check output pixels so lost or incorrect source coordinates are detected.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, sixel, check_image

palette = [(100, 0, 0), (0, 100, 0), (0, 0, 100), (100, 100, 100)]
rgb = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255)]
pixels = [2 * (y >= 16) + (x >= 8) for y in range(32) for x in range(16)]
wire = sixel(16, 32, palette, pixels)

with image_test(sys.argv[1]) as test:
    client = test.attach()
    # A scroll preserves the second source row and puts the first in history.
    test.emit(wire + b'\033[1S')
    assert len(client.images()) == 1
    check_image(client.images()[0], 16, 16,
        [rgb[2 + (x >= 8)] for y in range(16) for x in range(16)])
    client.clear()
    test.run('copy-mode')
    test.run('send-keys', '-X', 'history-top')
    client.read(0.3)
    assert any(im[:2] == (16, 32) for im in client.images())
    image = next(im for im in client.images() if im[:2] == (16, 32))
    check_image(image, 16, 32, [rgb[c] for c in pixels])
    test.run('send-keys', '-X', 'cancel')

    # Insert a blank cell inside the image, then delete the gap again.
    pixels = [x >= 8 for y in range(16) for x in range(16)]
    test.emit(b'\033[2J\033[H' + sixel(16, 16, palette[:2], pixels))
    test.emit(b'\033[1;2H\033[@')
    images = client.images()
    assert len(images) == 2, [im[:2] for im in images]
    for im, colour in zip(images, rgb[:2]):
        check_image(im, 8, 16, [colour] * (8 * 16))
    test.emit(b'\033[1;2H\033[P')
    images = client.images()
    assert len(images) == 2, [im[:2] for im in images]
    for im, colour in zip(images, rgb[:2]):
        check_image(im, 8, 16, [colour] * (8 * 16))

    # A double-width character overwrites both image cells.
    test.emit(b'\033[H' + '界'.encode())
    assert not client.images()
    assert test.run('capture-pane', '-p').startswith('界')
    test.redraw()
    assert not client.images()

    # Narrowing keeps image rows whole; widening restores the clipped column.
    pixels = [2 * (y >= 16) + (x >= 8) for y in range(32) for x in range(16)]
    test.emit(b'\033[2J\033[H' + wire)
    test.resize(client, 1, 16)
    assert len(client.images()) == 1
    check_image(client.images()[0], 8, 32,
        [rgb[2 * (y >= 16)] for y in range(32) for x in range(8)])
    test.resize(client, 40, 16)
    assert len(client.images()) == 1
    check_image(client.images()[0], 16, 32, [rgb[c] for c in pixels])

    # Switching the alternate screen does not retain the other screen's image.
    test.emit(b'\033[?1049h')
    assert not client.images()
    test.redraw()
    assert not client.images()
    test.emit(b'\033[?1049l')
    assert len(client.images()) == 1
    check_image(client.images()[0], 16, 32, [rgb[c] for c in pixels])
PY
