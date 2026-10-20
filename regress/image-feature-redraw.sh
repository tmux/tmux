#!/bin/sh

# Terminal replies which change no features must not replay our images.
# A reply which adds a feature must still cause a redraw.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 -B - "$TEST_TMUX" <<'PY'
import os
import sys
from image_test import image_test, sixel, check_image

with image_test(sys.argv[1]) as test:
    client = test.attach()
    test.emit(sixel(8, 16, [(100, 0, 0)], [0] * (8 * 16)))
    assert len(client.images()) == 1
    client.clear()
    # The primary DA was answered during attach. This secondary DA adds
    # nothing, but still calls tty_update_features().
    os.write(client.fd, b'\033[>0;1;0c')
    client.read(0.5)
    assert not client.images(), 'unchanged features replayed the image'
    features = test.run('display-message', '-p', '#{client_termfeatures}')
    assert 'sync' not in features.split(','), features
    os.write(client.fd, b'\033[?2026;1$y')
    test.wait(lambda: 'sync' in test.run('display-message', '-p',
        '#{client_termfeatures}').split(','))
    client.read(0.2)
    assert len(client.images()) == 1, 'new terminal feature did not redraw'
    check_image(client.images()[0], 8, 16, [(255, 0, 0)] * (8 * 16))
PY
