#!/bin/sh

# Valid, fragmented and rejected SIXEL input, including the PR's stray
# character case and limits reached while growing the image.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, sixel, check_image

with image_test(sys.argv[1]) as test:
    client = test.attach()
    red = (255, 0, 0)
    valid = sixel(8, 16, [(100, 0, 0)], [0] * (8 * 16))
    # Check a clean image and the same one with stray bytes between bands.
    for wire in (valid, valid.replace(b'-', b' \t\177\200-', 1)):
        test.emit(b'\033[2J\033[H' + wire)
        images = client.images()
        assert len(images) == 1, len(images)
        check_image(images[0], 8, 16, [red] * (8 * 16))

    # HLS uses blue at 0, red at 120 and green at 240 degrees.
    for hue, rgb in [(0, (0, 0, 255)), (120, red), (240, (0, 255, 0))]:
        wire = ('\033P9;1q"1;1;8;16#0;1;%d;50;100'
            '#0!8~-!8~-!8N\033\\' % hue).encode()
        test.emit(b'\033[2J\033[H' + wire)
        assert len(client.images()) == 1
        check_image(client.images()[0], 8, 16, [rgb] * (8 * 16))

    # Undefined registers are black in the shared model and in retained output.
    test.emit(b'\033[2J\033[H\033P9;1q"1;1;8;16'
        b'#5!8~-!8~-!8N\033\\')
    assert len(client.images()) == 1
    check_image(client.images()[0], 8, 16, [(0, 0, 0)] * (8 * 16))
    test.redraw()
    check_image(client.images()[0], 8, 16, [(0, 0, 0)] * (8 * 16))

    # Deliver the terminator in two pieces. No image is placed before it.
    test.emit(b'\033[2J\033[H')
    for byte in valid[:-1]:
        test.send(bytes([byte]))
    client.read()
    assert not client.images(), 'image placed before the DCS terminator'
    assert test.run('display-message', '-p', '#{cursor_y}') == '0'
    test.emit(valid[-1:])
    assert len(client.images()) == 1
    check_image(client.images()[0], 8, 16, [red] * (8 * 16))

    rejected = [
        b'q', b'q#0;2;100', b'q!0~', b'q!10001~', b'q#1025~',
        b'q#0;2;101;0;0~', b'q#0;1;361;50;100~',
        b'q"1;1;10001;1', b'q"1;1;1;10001',
        b'q"1;1;4096;4097',
        b'q"1;1;18446744073709551616;1',
        b'q"1;1;10000;1#0;2;100;0;0@' + b'-' * 300 + b'@',
        b'q"1;1;1;10000#0;2;100;0;0!2000@',
    ]
    for body in rejected:
        test.emit(b'\033[2J\033[H\033P9;1' + body + b'\033\\')
        assert not client.images(), ('invalid image accepted', body[:80])
        assert test.run('display-message', '-p', '#{cursor_x},#{cursor_y}') == '0,0'
        test.redraw()
        assert not client.images(), ('invalid image retained', body[:80])
PY
