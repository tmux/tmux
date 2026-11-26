#!/bin/sh

# Retain Kitty uploads through window changes, occlusion and scrollback.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_test import image_test, kitty, sixel
from image_kitty import KittyTerminal

red = (255, 0, 0)
picture = kitty('a=T,i=7,p=1,f=32,s=1,v=1,c=4,r=3,C=1', bytes((*red, 255)))
full = {(x, y): red for y in range(48) for x in range(32)}

with image_test(sys.argv[1]) as test:
    client = test.attach(kitty_graphics=True)
    terminal = KittyTerminal()
    test.run('set-option', '-g', 'scroll-on-clear', 'on')
    test.emit(picture)
    terminal.feed(client.data)
    terminal.check(full)
    uploads = terminal.uploads

    # Completely cover the image with another pane in the same window.
    client.clear()
    floating = test.run('new-pane', '-d', '-P', '-F', '#{pane_id}',
        '-B', 'none', '-x', '4', '-y', '3', '-X', '0', '-Y', '0',
        'stty raw -echo; exec cat')
    client.read(0.2)
    terminal.feed(client.data)
    terminal.check({})
    assert terminal.images and terminal.uploads == uploads

    # Window changes remove placements, retaining the uploaded pixels.
    client.clear()
    test.run('new-window', 'stty raw -echo; exec cat')
    client.read(0.2)
    terminal.feed(client.data)
    terminal.check({})
    assert terminal.images and terminal.uploads == uploads
    client.clear()
    test.run('select-window', '-t', ':0')
    test.run('kill-pane', '-t', floating)
    client.read(0.2)
    terminal.feed(client.data)
    terminal.check(full)
    assert terminal.uploads == uploads

    # Moving into history and back must not require a new upload.
    test.emit(b'\033[2J')
    terminal.feed(client.data)
    terminal.check({})
    assert terminal.images
    client.clear()
    test.run('copy-mode')
    test.run('send-keys', '-X', 'history-top')
    client.read(0.2)
    terminal.feed(client.data)
    terminal.check(full)
    assert terminal.uploads == uploads
    client.clear()
    test.run('send-keys', '-X', 'cancel')
    client.read(0.2)
    terminal.feed(client.data)
    terminal.check({})

    # A Kitty application may still hold the source after history is cleared.
    client.clear()
    test.run('clear-history')
    test.run('refresh-client', '-t', client.name)
    client.read(0.2)
    terminal.feed(client.data)
    assert terminal.images
    test.emit(kitty('a=d,d=I,i=7'))
    terminal.feed(client.data)
    assert not terminal.images

    # Closing the last window holding the source releases the upload.
    test.emit(picture)
    terminal.feed(client.data)
    client.clear()
    test.run('select-window', '-t', ':1')
    test.run('kill-window', '-t', ':0')
    client.read(0.2)
    terminal.feed(client.data)
    terminal.check({})
    assert not terminal.images


# SIXEL sources have no application image ID: history owns their last reference.
with image_test(sys.argv[1]) as test:
    client = test.attach(kitty_graphics=True)
    terminal = KittyTerminal()
    test.run('set-option', '-g', 'scroll-on-clear', 'on')
    test.emit(sixel(16, 16, [(100, 0, 0)], [0] * 256))
    terminal.feed(client.data)
    test.emit(b'\033[2J')
    terminal.feed(client.data)
    terminal.check({})
    assert terminal.images
    client.clear()
    test.run('clear-history')
    test.run('refresh-client', '-t', client.name)
    client.read(0.2)
    terminal.feed(client.data)
    assert not terminal.images


# Repeated replacement must respect the existing cache count and size budgets.
with image_test(sys.argv[1]) as test:
    client = test.attach(kitty_graphics=True)
    terminal = KittyTerminal()
    test.run('set-option', '-s', 'image-total-count-limit', '4')
    test.emit(picture)
    terminal.feed(client.data)
    for _ in range(12):
        test.emit(b'\033[H' + kitty('a=p,i=7,p=1,c=4,r=3,C=1'))
        terminal.feed(client.data)
        terminal.check(full)
        assert len(terminal.images) <= 4
    client.clear()
    test.run('set-option', '-s', 'image-total-size-limit', '1')
    test.run('refresh-client', '-t', client.name)
    client.read(0.2)
    terminal.feed(client.data)
    terminal.check(full)
    assert len(terminal.images) == 1
PY
