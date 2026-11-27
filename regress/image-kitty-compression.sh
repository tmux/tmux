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

responses = [None, b'OK', b'EINVAL:compression unsupported', b'OKAY']
for response in responses:
    with image_test(sys.argv[1]) as test:
        client = test.attach(kitty_graphics=True, reply_da=False)
        if response is not None:
            wire = b'\033_Gi=32;' + response + b'\033\\'
            # Exercise the response parser with a bytewise reply.
            for byte in wire:
                os.write(client.fd, bytes([byte]))
                client.read(0.01)
        os.write(client.fd, b'\033[?65cZ')
        test.wait(lambda: test.run('capture-pane', '-p') == 'Z')
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
            terminal.check({(i % 64, i // 64): tuple(pixels[i * 4:i * 4 + 3])
                for i in range(64 * 64)})
            assert any(c.get(b'o') == b'z' for c in terminal.commands)
            assert any(c.get(b'm') == b'1' for c in terminal.commands)
PY
