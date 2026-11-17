#!/bin/sh

# Check RGB, RGBA and PNG pixels, including compression and transparency.
# These tests require libpng and zlib in the tmux build.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import sys
import zlib
from image_test import image_test, kitty, png
from image_terminal import Terminal


def rows(data, width, height, channels):
    """Split raster bytes into PNG rows."""
    size = width * channels
    return [data[y * size:(y + 1) * size] for y in range(height)]


def check(test, client, command, expected):
    """Check newly decoded pixels and their later redraw."""
    test.emit(b'\033[2J\033[H' + command)
    terminal = Terminal()
    terminal.feed(client.data)
    terminal.check(expected)
    test.redraw()
    terminal = Terminal()
    terminal.feed(client.data)
    terminal.check(expected)


colours = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255)]
w, h = 16, 32
rgb = [colours[2 * (y >= 16) + (x >= 8)]
    for y in range(h) for x in range(w)]
expected = {(x, y): rgb[y * w + x] for y in range(h) for x in range(w)}
raw_rgb = bytes(c for pixel in rgb for c in pixel)
raw_rgba = bytes(c for pixel in rgb for c in (*pixel, 255))
png_rgb = png(w, h, rows(raw_rgb, w, h, 3), colour_type=2)
png_rgba = png(w, h, rows(raw_rgba, w, h, 4))

with image_test(sys.argv[1]) as test:
    test.run('set-option', '-g', 'scroll-on-clear', 'off')
    client = test.attach()
    for format, data in [(24, raw_rgb), (32, raw_rgba),
                         (100, png_rgb), (100, png_rgba)]:
        for compressed in (False, True):
            control = 'a=T,i=7,C=1,f=%d' % format
            if format != 100:
                control += ',s=16,v=32'
            payload = data
            if compressed:
                control += ',o=z,S=%d' % len(data)
                payload = zlib.compress(data)
            try:
                check(test, client, kitty(control, payload), expected)
            except AssertionError as e:
                raise AssertionError((format, compressed)) from e

    # Palette PNGs retain their colours and transparent palette entries.
    indexes = bytes(2 * (y >= 16) + (x >= 8)
        for y in range(h) for x in range(w))
    palette = bytes(c for pixel in colours for c in pixel)
    data = png(w, h, rows(indexes, w, h, 1), colour_type=3,
        chunks=[(b'PLTE', palette), (b'tRNS', b'\xff\0\xff\xff')])
    visible = {point: colour for point, colour in expected.items()
        if not (point[0] >= 8 and point[1] < 16)}
    check(test, client, kitty('a=T,i=7,f=100,C=1', data), visible)

    # Grayscale and grayscale-alpha PNGs expand to the shared RGBA format.
    gray = bytes(255 if y >= 16 else 0 for y in range(h) for x in range(w))
    gray_expected = {(x, y): (gray[y * w + x],) * 3
        for y in range(h) for x in range(w)}
    data = png(w, h, rows(gray, w, h, 1), colour_type=0)
    check(test, client, kitty('a=T,i=7,f=100,C=1', data), gray_expected)
    gray_alpha = bytes(c for y in range(h) for x in range(w)
        for c in (gray[y * w + x], 255 if x < 8 else 0))
    data = png(w, h, rows(gray_alpha, w, h, 2), colour_type=4)
    check(test, client, kitty('a=T,i=7,f=100,C=1', data),
        {point: colour for point, colour in gray_expected.items()
            if point[0] < 8})

    # Test both an entirely transparent cell and holes inside a visible cell.
    rgba = bytes(c for y in range(h) for x in range(w)
        for c in (*rgb[y * w + x], 0 if (x >= 8 and y < 16) or
            (x + y) % 3 == 0 else 255))
    visible = {point: colour for point, colour in expected.items()
        if rgba[(point[1] * w + point[0]) * 4 + 3] != 0}
    for format, data in [(32, rgba), (100, png(w, h, rows(rgba, w, h, 4)))]:
        control = 'a=T,i=7,f=%d,C=1' % format
        if format == 32:
            control += ',s=16,v=32'
        check(test, client, kitty(control, data), visible)
        empty = b'\xff\0\0\0' * (w * h)
        data = empty if format == 32 else png(w, h, rows(empty, w, h, 4))
        check(test, client, kitty(control, data), {})

    # SIXEL represents partial alpha as a mixture of coloured and empty pixels.
    test.run('set-option', '-as', 'terminal-features', 'screen-256color:RGB')
    fallback = test.attach(graphical=False)
    rgba = b'\xff\0\0\x40' * 64 + b'\xff\0\0\xc0' * 64
    for format, data in [(32, rgba), (100, png(8, 16, rows(rgba, 8, 16, 4)))]:
        control = 'a=T,i=7,f=%d,C=1' % format
        if format == 32:
            control += ',s=8,v=16'
        test.emit(b'\033[2J\033[H' + kitty(control, data))
        for redraw in (False, True):
            if redraw:
                test.redraw()
            terminal = Terminal()
            terminal.feed(client.data)
            upper = sum(y < 8 for x, y in terminal.pixels)
            lower = len(terminal.pixels) - upper
            assert 12 <= upper <= 20 and 44 <= lower <= 52, (upper, lower)
            assert all(colour == (255, 0, 0)
                for colour in terminal.pixels.values()), terminal.pixels
            terminal = Terminal()
            terminal.feed(fallback.data)
            char, fg, bg, bold = terminal.cells[0, 0]
            assert char == '\u2584' and not bold, (char, bold)
            assert (fg, bg) == ((192, 0, 0), (64, 0, 0)), (fg, bg)
PY
