#!/bin/sh

# Compress Kitty images only when the terminal reports support.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import os
import sys
from image_test import image_test, kitty
from image_kitty import KittyTerminal

query = b'\033_Gi=32,'
graphics_query = b'\033_Gi=31,'
responses = [None, b'OK', b'EINVAL:compression unsupported', b'OKAY']
cases = [
    ('configured', True, True, None),
    ('configured-no-XT', False, True, None),
    ('graphics-reply', True, None, b'\033_Gi=31;OK\033\\'),
    ('XDA-no-XT', False, None, b'\033P>|kitty(0.49.2)\033\\'),
    ('XDA-with-XT', True, None, b'\033P>|kitty(0.49.2)\033\\'),
]
for name, xt, configured, identification in cases:
    for response in (responses if xt else [None]):
        with image_test(sys.argv[1]) as test:
            if not xt:
                test.run('set-option', '-as', 'terminal-overrides',
                    ',xterm-256color:XT@')
            client = test.attach(kitty_graphics=configured, reply_da=False,
                clear=False)
            if xt:
                # Send both queries before support is learned from replies.
                test.wait(lambda: query in client.data)
                assert client.data.count(graphics_query) == 1, name
                assert client.data.count(query) == 1, name
                assert (client.data.index(graphics_query) <
                    client.data.index(query)), name
            else:
                assert graphics_query not in client.data, name
                assert query not in client.data, name
            if identification is not None:
                os.write(client.fd, identification)

            if response is not None:
                wire = b'\033_Gi=32;' + response + b'\033\\'
                # Exercise the response parser with a bytewise reply.
                for byte in wire:
                    os.write(client.fd, bytes([byte]))
                    client.read(0.01)

            # Primary DA completes the startup replies.
            os.write(client.fd, b'\033[?65cZ')
            test.wait(lambda: test.run('capture-pane', '-p') == 'Z')
            assert client.data.count(query) == (1 if xt else 0), name
            test.emit(b'\033[H' + kitty(
                'a=T,i=7,f=32,s=1,v=1,c=16,r=8,C=1', b'\xff\0\0\xff'))
            terminal = KittyTerminal()
            terminal.feed(client.data)
            terminal.check({(x, y): (255, 0, 0)
                for y in range(128) for x in range(128)})
            headers = [c for c in terminal.commands
                if c.get(b'a') == b't' and b'f' in c]
            assert len(headers) == 1, headers
            compressed = headers[0].get(b'o') == b'z'
            assert compressed == (response == b'OK'), (response, headers)
            if compressed:
                assert len(client.data) < 4096, len(client.data)
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
    os.write(client.fd, b'\033_Gi=32;OK\033\\\033_Gi=31;OK\033\\\033[?65cY')
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
PY
