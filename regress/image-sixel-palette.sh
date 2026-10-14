#!/bin/sh

# Reduce a 512-colour cube to a SIXEL palette. Check colour error, opaque
# pixels, palette size, extremes and reproducible output after redraws.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 -B - "$TEST_TMUX" <<'PY'
import base64
import sys
from image_test import image_test, sixel

levels = [0, 14, 29, 43, 57, 71, 86, 100]
palette = [(r, g, b) for r in levels for g in levels for b in levels]
expected = [tuple((c * 255 + 50) // 100 for c in p) for p in palette]
with image_test(sys.argv[1]) as test:
    client = test.attach()
    test.emit(sixel(32, 16, palette, list(range(512))))
    images = client.images()
    assert len(images) == 1 and images[0][:2] == (32, 16)
    assert images[0][2] == expected, 'original SIXEL palette changed'
    test.redraw()
    assert client.images() == images, 'retained palette changed on redraw'

    # Kitty input exercises RGBA quantization rather than SIXEL retention.
    rgba = bytes(c for pixel in expected for c in (*pixel, 255))
    test.emit(b'\033[2J\033[H\033_Ga=T,q=2,f=32,s=32,v=16;' +
        base64.b64encode(rgba) + b'\033\\')
    images = client.images()
    assert len(images) == 1 and images[0][:2] == (32, 16)
    actual = images[0][2]
    assert all(c is not None for c in actual), 'dithering introduced holes'
    assert 128 <= len(set(actual)) <= 256, len(set(actual))
    errors = [abs(a - e) for p, q in zip(actual, expected) for a, e in zip(p, q)]
    assert max(errors) <= 80, max(errors)
    assert sum(errors) / len(errors) <= 16, sum(errors) / len(errors)
    assert max(actual[0]) <= 30 and min(actual[-1]) >= 225, (
        'black/white extremes lost', actual[0], actual[-1])
    for _ in range(3):
        test.redraw()
        assert client.images() == images, 'palette or dithering changed on redraw'
PY
