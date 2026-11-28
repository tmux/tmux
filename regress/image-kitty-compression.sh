#!/bin/sh

# Detect Kitty graphics with a compressed query and compress useful uploads.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PYTEST'
import os
import random
import sys
from image_test import image_test, kitty
from image_kitty import KittyTerminal

query = (b'\033_Gi=31,s=1,v=1,a=q,t=d,f=24,o=z;'
    b'eJxjYGAAAAADAAE=\033\\')
responses = [None, b'OK', b'EINVAL:compression unsupported', b'OKAY']
kitty_xda = b'\033P>|kitty(0.49.2)\033\\'
ghostty_xda = b'\033P>|ghostty 1.2.0\033\\'
cases = [
    ('detected', True, None, None, False),
    ('configured', True, True, None, True),
    ('configured-no-XT', False, True, None, True),
    ('disabled', True, False, None, False),
    ('XDA-kitty-no-XT', False, None, kitty_xda, True),
    ('XDA-kitty-with-XT', True, None, kitty_xda, True),
    ('XDA-ghostty-no-XT', False, None, ghostty_xda, True),
    ('XDA-ghostty-with-XT', True, None, ghostty_xda, True),
]
for name, xt, configured, identification, known in cases:
    for response in (responses if xt else [None]):
        with image_test(sys.argv[1]) as test:
            if not xt:
                test.run('set-option', '-as', 'terminal-overrides',
                    ',xterm-256color:XT@')
            client = test.attach(kitty_graphics=configured, reply_da=False,
                clear=False)
            if xt:
                test.wait(lambda: query in client.data)
            else:
                assert query not in client.data, name
            if identification is not None:
                os.write(client.fd, identification)
            if response is not None:
                wire = b'\033_Gi=31;' + response + b'\033\\'
                for byte in wire:
                    os.write(client.fd, bytes([byte]))
                    client.read(0.01)

            # Primary DA completes the startup replies.
            os.write(client.fd, b'\033[?65cZ')
            test.wait(lambda: test.run('capture-pane', '-p') == 'Z')
            assert client.data.count(b'\033_G') == (1 if xt else 0), name
            supported = known
            if response == b'OK' and configured is not False:
                supported = True
            features = test.run('list-clients', '-F',
                '#{client_termfeatures}').split(',')
            assert ('kittygraphics' in features) == supported, (name, features)
            test.emit(b'\033[H' + kitty(
                'a=T,i=7,f=32,s=1,v=1,c=16,r=8,C=1', b'\xff\0\0\xff'))
            terminal = KittyTerminal()
            terminal.feed(client.data)
            headers = [c for c in terminal.commands
                if c.get(b'a') == b't' and b'f' in c]
            if not supported:
                assert not headers, (name, headers)
                assert client.images(), name
                continue
            terminal.check({(x, y): (255, 0, 0)
                for y in range(128) for x in range(128)})
            assert len(headers) == 1 and headers[0].get(b'o') == b'z', headers
            assert len(client.data) < 4096, len(client.data)

            # Less repetitive data still uses compressed chunks.
            test.emit(kitty('a=d,d=A'))
            terminal.feed(client.data)
            terminal.commands.clear()
            pixels = bytes(v for y in range(64) for x in range(64)
                for v in ((x * 3 + y * 5) % 256, (x * 7 + y * 11) % 256,
                    (x * 13 + y * 17) % 256, 255))
            test.emit(kitty('a=T,i=8,f=32,s=64,v=64,C=1', pixels))
            terminal.feed(client.data)
            expected = {(i % 64, i // 64):
                tuple(pixels[i * 4:i * 4 + 3]) for i in range(64 * 64)}
            terminal.check(expected)
            assert any(c.get(b'o') == b'z' for c in terminal.commands)
            assert any(c.get(b'm') == b'1' for c in terminal.commands)


# Retained shared uploads count decoded pixels rather than compressed bytes.
with image_test(sys.argv[1]) as test:
    client = test.attach(kitty_graphics=None, reply_da=False, clear=False)
    test.wait(lambda: query in client.data)
    os.write(client.fd, b'\033_Gi=31;OK\033\\\033[?65cY')
    test.wait(lambda: test.run('capture-pane', '-p') == 'Y')
    terminal = KittyTerminal()
    test.emit(b'\033[H' + kitty(
        'a=T,i=7,p=1,f=32,s=1,v=1,c=4,r=3,C=1', b'\xff\0\0\xff') +
        b'\033[1;9H' + kitty('a=p,i=7,p=2,c=4,r=3,C=1'))
    terminal.feed(client.data)
    terminal.check({(x + dx, y): (255, 0, 0)
        for dx in (0, 64) for y in range(48) for x in range(32)})
    headers = [c for c in terminal.commands
        if c.get(b'a') == b't' and b'f' in c]
    assert len(headers) == 1 and headers[0].get(b'o') == b'z', headers
    size = sum(w * h * 4 for w, h, _ in terminal.images.values())

    def cache_state():
        """Read output and report cached uploads and visible placements."""
        terminal.feed(client.data)
        client.data.clear()
        return len(terminal.images), len(terminal.placements)

    client.clear()
    test.run('set-option', '-s', 'image-total-size-limit', str(size))
    test.run('new-window', 'stty raw -echo; exec cat')
    test.wait(lambda: cache_state() == (1, 0))
    assert terminal.uploads == 1
    test.run('set-option', '-s', 'image-total-size-limit', str(size - 1))
    test.run('refresh-client', '-t', client.name)
    test.wait(lambda: cache_state() == (0, 0))

    # Send raw pixels when compression would increase their size.
    rng = random.Random(0)
    pixels = bytes(rng.randrange(256) for _ in range(16 * 16 * 4))
    terminal.commands.clear()
    test.emit(b'\033[H' + kitty('a=T,i=8,f=32,s=16,v=16,C=1', pixels))
    terminal.feed(client.data)
    expected = {}
    for i in range(16 * 16):
        pos = i * 4
        alpha = pixels[pos + 3]
        if alpha:
            expected[i % 16, i // 16] = tuple(v * alpha // 255
                for v in pixels[pos:pos + 3])
    terminal.check(expected)
    headers = [c for c in terminal.commands
        if c.get(b'a') == b't' and b'f' in c]
    assert len(headers) == 1 and b'o' not in headers[0], headers
PYTEST
