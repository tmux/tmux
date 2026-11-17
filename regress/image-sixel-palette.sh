#!/bin/sh

# Reduce a 512-colour cube to a SIXEL palette. Check colour error, opaque
# pixels, palette size, extremes and reproducible output after redraws.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, kitty, sixel

levels = [0, 14, 29, 43, 57, 71, 86, 100]
palette = [(r, g, b) for r in levels for g in levels for b in levels]
expected = [tuple((c * 255 + 50) // 100 for c in p) for p in palette]
raw = bytes(c for rgb in expected for c in (*rgb, 255))
commands = [sixel(32, 16, palette, list(range(512))),
    kitty('a=T,i=7,f=32,s=32,v=16,C=1', raw)]
for command in commands:
    with image_test(sys.argv[1]) as test:
        client = test.attach()
        test.emit(command)
        images = client.images()
        assert len(images) == 1 and images[0][:2] == (32, 16)
        actual = images[0][2]
        assert all(c is not None for c in actual), 'dithering introduced holes'
        assert 128 <= len(set(actual)) <= 256, len(set(actual))
        errors = [abs(a - e) for p, q in zip(actual, expected)
            for a, e in zip(p, q)]
        assert max(errors) <= 80, max(errors)
        assert sum(errors) / len(errors) <= 16, sum(errors) / len(errors)
        assert max(actual[0]) <= 30 and min(actual[-1]) >= 225, (
            'black/white extremes lost', actual[0], actual[-1])
        for _ in range(3):
            test.redraw()
            assert client.images() == images, \
                'palette or dithering changed on redraw'
PY
