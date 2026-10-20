#!/bin/sh

# Check image quotas, last use order, copied grids and released image slots.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, sixel
from image_terminal import Terminal


def image(colour):
    """Build one solid image cell which advances the cursor by one row."""
    return sixel(8, 16, [colour], [0] * (8 * 16))


def snapshot(test, client, rows):
    """Check the image cells in the first column after a full redraw."""
    test.redraw()
    terminal = Terminal()
    terminal.feed(client.data)
    expected = {(x, y * 16 + py): colour for y, colour in rows.items()
        for py in range(16) for x in range(8)}
    terminal.check(expected)


red, green, blue = (255, 0, 0), (0, 255, 0), (0, 0, 255)
red_image = image((100, 0, 0))
green_image = image((0, 100, 0))
blue_image = image((0, 0, 100))
limit = 32

with image_test(sys.argv[1]) as test:
    test.run('set-option', '-s', 'image-total-count-limit', str(limit))
    client = test.attach()
    test.emit(red_image + green_image * (limit - 1))
    test.emit(blue_image)
    snapshot(test, client, dict.fromkeys(range(14), green) | {14: blue})
    # Images in scrollback count towards the quota; the oldest one is gone.
    test.run('copy-mode')
    test.run('send-keys', '-X', 'history-top')
    snapshot(test, client, dict.fromkeys(range(1, 16), green))

with image_test(sys.argv[1]) as test:
    test.run('set-option', '-s', 'image-total-count-limit', str(limit))
    client = test.attach()
    test.emit(red_image + green_image * (limit - 1))
    test.run('copy-mode')
    test.run('send-keys', '-X', 'history-top')
    snapshot(test, client, {0: red} | dict.fromkeys(range(1, 16), green))
    # The copied grid keeps these old images in use while a new window fills.
    test.run('new-window', '-n', 'quota', '/bin/sh', '-c',
        "stty raw -echo; printf '\033]2;QUOTA\007'; exec cat")
    test.wait(lambda: test.run('display-message', '-p',
        '#{pane_title}') == 'QUOTA')
    test.emit(blue_image)
    test.run('select-window', '-t', 'image:0')
    snapshot(test, client, {0: red} | dict.fromkeys(range(1, 16), green))
    # The first image below the redrawn rows was evicted from both grids.
    test.run('send-keys', '-X', 'scroll-down')
    snapshot(test, client, dict.fromkeys(range(15), green))
    test.run('send-keys', '-X', 'cancel')
    test.run('copy-mode')
    test.run('send-keys', '-X', 'history-top')
    test.run('send-keys', '-X', 'scroll-down')
    snapshot(test, client, dict.fromkeys(range(15), green))

with image_test(sys.argv[1]) as test:
    test.run('set-option', '-s', 'image-total-count-limit', str(limit))
    client = test.attach()
    # Replacing an image releases its slot rather than exhausting the quota.
    test.emit((b'\033[H' + green_image) * (limit + 1))
    test.emit(b'\033[3;1H' + blue_image)
    snapshot(test, client, {0: green, 2: blue})
PY
