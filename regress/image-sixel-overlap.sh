#!/bin/sh

# A completely covered SIXEL image is discarded, while a partly covered one
# survives. Count images in an explicit repaint after startup negotiation,
# rather than comparing attach-time transmissions from different clients.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, sixel, check_image

blue = sixel(100, 6, [(0, 0, 100)], [0] * 600)
red = sixel(100, 6, [(100, 0, 0)], [0] * 600)
narrow = sixel(40, 6, [(100, 0, 0)], [0] * 240)

with image_test(sys.argv[1]) as test:
    client = test.attach()
    for name, wire, expected in [
            ('single', blue, [(100, (0, 0, 255))]),
            ('covered', blue + b'\0338' + red, [(100, (255, 0, 0))]),
            ('partly covered', blue + b'\0338' + narrow,
                [(60, (0, 0, 255)), (40, (255, 0, 0))])]:
        test.emit(b'\033[2J\033[H\0337' + wire)
        test.redraw()
        images = client.images()
        assert len(images) == len(expected), (name, [im[:2] for im in images])
        for image, (width, colour) in zip(images, expected):
            check_image(image, width, 6, [colour] * (width * 6))
PY
