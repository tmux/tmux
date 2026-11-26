#!/bin/sh

# Changing Kitty source rectangles reuses scaled source uploads.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, kitty
from image_kitty import KittyTerminal

colours = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255)]


def colour(x, y):
    """Source pixels distinguish both crop coordinates."""
    return colours[(x // 8 + y // 8) % len(colours)]


def source(w=64, h=256):
    """Upload one source without displaying it."""
    pixels = bytes(v for y in range(h) for x in range(w)
        for v in (*colour(x, y), 255))
    data = kitty('a=t,i=7,f=32,s=%d,v=%d' % (w, h), pixels)
    return data


def crop(x, y, placement=1):
    """Display a half-size source crop using a named placement."""
    data = kitty('a=p,i=7,p=%d,x=%d,y=%d,w=32,h=64,c=2,r=2,C=1' %
        (placement, x, y))
    return data


def expected(x, y, dx=0, dy=0, w=16, h=32, cw=8, ch=16):
    """Expected source pixels for the client cell dimensions."""
    return {(dx + xx, dy + yy): colour(x + xx * 16 // cw,
        y + yy * 32 // ch) for yy in range(h) for xx in range(w)}


with image_test(sys.argv[1]) as test:
    client = test.attach(kitty_graphics=True)
    terminal = KittyTerminal()
    test.emit(source() + crop(0, 0))
    terminal.feed(client.data)
    terminal.check(expected(0, 0))
    assert terminal.uploads == 1
    image = next(iter(terminal.placements))[0]
    width, height, _ = terminal.images[image]
    assert (width, height) == (18, 34), (width, height)
    uploads = terminal.uploads

    # Expand once, then scroll both source coordinates without uploading pixels.
    for x, y in [(8, 16), (16, 32), (24, 48), (0, 0)]:
        test.emit(b'\033[H' + crop(x, y))
        terminal.feed(client.data)
        terminal.check(expected(x, y))
        assert terminal.uploads == uploads + 1, (x, y, terminal.uploads)
    uploads = terminal.uploads

    # The initial prepared crop may also remain cached after expansion.
    assert any((w, h) == (34, 130) for w, h, _ in terminal.images.values())

    # A smaller source rectangle at the same scale uses the same upload.
    test.emit(b'\033[H' +
        kitty('a=p,i=7,p=1,x=8,y=16,w=16,h=32,c=1,r=1,C=1'))
    terminal.feed(client.data)
    terminal.check(expected(8, 16, w=8, h=16))
    assert terminal.uploads == uploads
    test.emit(b'\033[H' + crop(16, 32))
    terminal.feed(client.data)

    # Disjoint placements of the same source also share the upload.
    test.emit(b'\033[5;1H' + crop(8, 16, placement=2))
    terminal.feed(client.data)
    terminal.check(expected(16, 32) | expected(8, 16, dy=64))
    assert terminal.uploads == uploads

    # Overlap separates the shared upload and preserves placement order.
    test.emit(b'\033[H' + crop(8, 16, placement=2))
    terminal.feed(client.data)
    terminal.check(expected(8, 16))
    assert terminal.uploads == uploads + 2
    assert list(terminal.images) == sorted(terminal.images)
    test.redraw()
    terminal.feed(client.data)
    terminal.check(expected(8, 16))
    assert terminal.uploads == uploads + 2
    test.emit(b'\033[5;1H' + crop(8, 16, placement=2))
    terminal.feed(client.data)
    terminal.check(expected(16, 32) | expected(8, 16, dy=64))
    assert terminal.uploads == uploads + 2

    # Fractional origins retain the existing sampling behaviour.
    test.emit(b'\033[H' + crop(1, 3))
    terminal.feed(client.data)
    terminal.check(expected(1, 3) | expected(8, 16, dy=64))

    # Replacing the source must not reuse pixels from the previous source.
    test.emit(kitty('a=t,i=7,f=32,s=64,v=256', b'\xff' * (64 * 256 * 4)) +
        b'\033[H' + crop(0, 0))
    terminal.feed(client.data)
    terminal.check({(x, y): (255, 255, 255)
        for y in range(32) for x in range(16)})
    assert len(terminal.images) == 1


# Each client prepares the source for its own terminal cell dimensions.
with image_test(sys.argv[1]) as test:
    clients = [(test.attach(kitty_graphics=True), 8, 16),
        (test.attach(kitty_graphics=True, cw=10, ch=20), 10, 20)]
    terminals = [KittyTerminal(cw=cw, ch=ch) for _, cw, ch in clients]
    test.emit(source() + crop(0, 0))
    for (client, cw, ch), terminal in zip(clients, terminals):
        terminal.feed(client.data)
        terminal.check(expected(0, 0, w=2 * cw, h=2 * ch, cw=cw, ch=ch))
        image = next(iter(terminal.placements))[0]
        width, height, _ = terminal.images[image]
        assert (width, height) == (2 * cw + 2, 2 * ch + 2)
    test.emit(b'\033[H' + crop(8, 16))
    for (client, cw, ch), terminal in zip(clients, terminals):
        terminal.feed(client.data)
        terminal.check(expected(8, 16, w=2 * cw, h=2 * ch, cw=cw, ch=ch))
        assert terminal.uploads == 2


# A scaled source with fractional dimensions preserves crop sampling.
with image_test(sys.argv[1]) as test:
    client = test.attach(kitty_graphics=True)
    terminal = KittyTerminal()
    pixels = bytes(v for y in range(65) for x in range(47)
        for v in (x * 5 % 256, y * 3 % 256, (x + y) * 7 % 256, 255))
    test.emit(kitty('a=t,i=7,f=32,s=47,v=65', pixels))
    for step, (x, y) in enumerate([(0, 0), (15, 17), (30, 34)]):
        test.emit(b'\033[H' + kitty(
            'a=p,i=7,p=1,x=%d,y=%d,w=15,h=17,c=1,r=1,C=1' % (x, y)))
        terminal.feed(client.data)
        expected_pixels = {}
        for py in range(16):
            for px in range(8):
                offset = ((y + py * 17 // 16) * 47 + x + px * 15 // 8) * 4
                expected_pixels[px, py] = tuple(pixels[offset:offset + 3])
        terminal.check(expected_pixels)
        assert terminal.uploads == min(step + 1, 2)
    image = next(iter(terminal.placements))[0]
    width, height, _ = terminal.images[image]
    assert (width, height) == (28, 64)


# Expanding the whole source beyond the upload limit keeps a cropped canvas.
with image_test(sys.argv[1]) as test:
    client = test.attach(kitty_graphics=True)
    terminal = KittyTerminal()
    test.run('set-option', '-s', 'image-size-limit', '2048')
    test.emit(source(4, 4) + kitty('a=p,i=7,p=1,w=2,h=2,c=1,r=1,C=1'))
    terminal.feed(client.data)
    terminal.check({(x, y): colour(0, 0) for y in range(16) for x in range(8)})
    image = next(iter(terminal.placements))[0]
    width, height, _ = terminal.images[image]
    assert (width, height) == (10, 18)
    test.emit(b'\033[H' +
        kitty('a=p,i=7,p=1,x=1,y=1,w=2,h=2,c=1,r=1,C=1'))
    terminal.feed(client.data)
    terminal.check({(x, y): colour(1, 1) for y in range(16) for x in range(8)})
    image = next(iter(terminal.placements))[0]
    width, height, _ = terminal.images[image]
    assert (width, height) == (10, 18)
    assert terminal.uploads == 2
PY
