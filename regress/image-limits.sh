#!/bin/sh

# Image quotas evict uploads and placements across protocols and saved grids.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, kitty, sixel
from image_terminal import Terminal


def upload(image_id, colour):
    """Upload one solid source pixel without placing it."""
    return kitty('a=t,f=32,s=1,v=1,i=%d' % image_id, bytes((*colour, 255)))


def place(image_id, placement_id=0):
    """Place one image at the cursor without moving the cursor."""
    return kitty('a=p,i=%d,p=%d,c=1,r=1,C=1' % (image_id, placement_id))


def cell(colour, x=0, y=0):
    """Expand a solid image cell into positioned pixels."""
    return {(px, py): colour for py in range(y * 16, (y + 1) * 16)
        for px in range(x * 8, (x + 1) * 8)}


def snapshot(test, client, expected):
    """Check retained images after a full redraw."""
    test.redraw()
    terminal = Terminal()
    terminal.feed(client.data)
    terminal.check(expected)


red, green, blue = (255, 0, 0), (0, 255, 0), (0, 0, 255)
with image_test(sys.argv[1]) as test:
    client = test.attach()
    test.emit(upload(1, red) +
        b''.join(upload(i, green) for i in range(2, 1001)))
    test.emit(upload(1001, blue))
    # The oldest upload is evicted and its application ID no longer resolves.
    test.emit(place(1))
    snapshot(test, client, {})
    test.emit(place(1001))
    snapshot(test, client, cell(blue))
    test.emit(b'\033[3;1H' + place(2))
    snapshot(test, client, cell(blue))
    test.emit(place(1000))
    snapshot(test, client, cell(blue) | cell(green, y=2))

with image_test(sys.argv[1]) as test:
    client = test.attach()
    test.emit(upload(1, red) + place(1, 1))
    test.emit(upload(2, green))
    test.emit(kitty('a=d,d=i,i=1,p=1'))
    # Losing the last placement keeps the source's original last use order.
    test.emit(b''.join(upload(i, blue) for i in range(3, 1002)))
    test.emit(place(2))
    snapshot(test, client, cell(green))
    test.emit(b'\033[3;1H' + place(1))
    snapshot(test, client, cell(green))

with image_test(sys.argv[1]) as test:
    client = test.attach()
    test.emit(upload(1, red) + b'\033[7;1H' + place(1, 1) +
        b'\033[H' + place(1, 2) + b'\033[3;1H' + place(1, 3) +
        b'\033[5;1H' + b''.join(place(1, p) for p in range(4, 1000)))
    snapshot(test, client,
        cell(red) | cell(red, y=2) | cell(red, y=4) | cell(red, y=6))
    # Redraw order differs from creation order: the top view is oldest.
    test.emit(upload(2, green))
    snapshot(test, client, cell(red, y=2) | cell(red, y=4) | cell(red, y=6))

with image_test(sys.argv[1]) as test:
    client = test.attach()
    test.emit(upload(1, red) + b''.join(place(1, p) for p in range(1, 1000)))
    test.emit(upload(2, green) + place(2, 1))
    snapshot(test, client, cell(green))
    # The shared source survives; only its two oldest views were evicted.
    test.emit(kitty('a=d,d=I,i=2'))
    snapshot(test, client, cell(red))
    test.emit(b''.join(kitty('a=d,d=i,i=1,p=%d' % p)
        for p in range(3, 1000)))
    snapshot(test, client, {})

with image_test(sys.argv[1]) as test:
    client = test.attach()
    test.emit(sixel(8, 16, [(100, 0, 0)], [0] * (8 * 16)))
    # Kitty views share the same limit as the older SIXEL source.
    test.emit(b'\033[3;1H' + upload(1, green) +
        b''.join(place(1, p) for p in range(1, 1000)))
    snapshot(test, client, cell(green, y=2))

with image_test(sys.argv[1]) as test:
    client = test.attach()
    test.emit(upload(1, red) + place(1))
    test.emit(b'\033[3;1H' + upload(2, green) +
        b''.join(place(2, p) for p in range(1, 998)))
    snapshot(test, client, cell(red) | cell(green, y=2))
    # SIXEL input also evicts Kitty placements and their uploaded image IDs.
    test.emit(b'\033[5;1H' +
        sixel(8, 16, [(0, 0, 100)], [0] * (8 * 16)))
    snapshot(test, client, cell(green, y=2) | cell(blue, y=4))
    test.emit(b'\033[7;1H' +
        sixel(8, 16, [(0, 0, 100)], [0] * (8 * 16)))
    snapshot(test, client, cell(green, y=2) | cell(blue, y=4) | cell(blue, y=6))
    test.emit(b'\033[H' + place(1))
    snapshot(test, client, cell(green, y=2) | cell(blue, y=4) | cell(blue, y=6))

with image_test(sys.argv[1]) as test:
    client = test.attach()
    test.emit(upload(1, red) + place(1))
    test.emit(b'\033[?1049h\033[H' + upload(2, green) +
        b''.join(place(2, p) for p in range(1, 1000)))
    # Eviction reaches the saved primary grid and removes its Kitty ID.
    test.emit(b'\033[?1049l')
    snapshot(test, client, {})
    test.emit(place(1))
    snapshot(test, client, {})

with image_test(sys.argv[1]) as test:
    client = test.attach()
    test.emit(sixel(8, 16, [(100, 0, 0)], [0] * (8 * 16)))
    test.run('copy-mode')
    # Keep the copied grid alive in a hidden window while exceeding the quota.
    test.run('new-window', '-n', 'quota', '/bin/sh', '-c',
        "stty raw -echo; printf '\033]2;QUOTA\007'; exec cat")
    test.wait(lambda: test.run('display-message', '-p',
        '#{pane_title}') == 'QUOTA')
    test.emit(upload(1, green) + b''.join(place(1, p) for p in range(1, 1000)))
    test.run('select-window', '-t', 'image:0')
    snapshot(test, client, {})
    test.run('send-keys', '-X', 'cancel')
    snapshot(test, client, {})
PY
