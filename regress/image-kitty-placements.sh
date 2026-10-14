#!/bin/sh

# Kitty ordinary placement geometry, cursor movement and replacement.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import base64
import sys
from image_test import image_test
from image_terminal import Terminal


def graphics(control, data=b''):
    """Build a Kitty graphics APC with replies suppressed."""
    payload = b';' + base64.b64encode(data) if data else b''
    return b'\033_Gq=2,' + control.encode() + payload + b'\033\\'


colours = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255)]
raw = bytes(value for y in range(32) for x in range(16)
    for value in (*colours[2 * (y >= 16) + (x >= 8)], 255))
upload = graphics('a=t,i=7,f=32,s=16,v=32', raw)
put = 'a=p,i=7,C=1'


def pattern(w=16, h=32, xoff=0, yoff=0):
    """Expected positions and colours for a scaled four-quadrant image."""
    return {(x + xoff, y + yoff): colours[2 * (y >= h // 2) + (x >= w // 2)]
        for y in range(h) for x in range(w)}


def solid(w, h, colour, xoff=0, yoff=0):
    """Expected positions for a solid rectangle."""
    return {(x + xoff, y + yoff): colour for y in range(h) for x in range(w)}


with image_test(sys.argv[1]) as test:
    client = test.attach()
    fallback = test.attach(graphical=False)

    def check(command, expected, cursor='0,0'):
        """Check the displayed image, its redraw and the pane cursor."""
        test.emit(b'\033[2J\033[H' + command)
        actual = test.run('display-message', '-p', '#{cursor_x},#{cursor_y}')
        assert actual == cursor, ('cursor', actual, cursor)
        term = Terminal()
        term.feed(client.data)
        term.check(expected)
        test.redraw()
        term = Terminal()
        term.feed(client.data)
        term.check(expected)
        assert not fallback.images(), 'graphics sent to fallback client'

    # Uploading alone draws nothing; placement reuses the stored pixels.
    check(upload, {})
    check(graphics(put), pattern())
    term = Terminal()
    term.feed(fallback.data)
    assert term.cells[0, 0][1:3] == (196, 196), term.cells[0, 0]
    check(graphics('a=T,i=7,f=32,s=16,v=32', raw), pattern(), cursor='2,1')
    check(graphics(put + ',c=4'), pattern(32, 64))
    check(graphics(put + ',r=4'), pattern(32, 64))
    check(graphics(put + ',c=4,r=1'), pattern(32, 16))

    # Crop first, then scale; oversized crop dimensions stop at the source edge.
    check(graphics(put + ',y=16,w=8,h=16,c=2,r=1'), solid(16, 16, colours[2]))
    check(graphics(put + ',x=8,y=16,w=999,h=999,c=1,r=1'),
        solid(8, 16, colours[3]))

    # Natural and explicit sizes both honour offsets within the first cell.
    check(graphics(put + ',X=4,Y=8'), pattern(xoff=4, yoff=8))
    check(graphics(put + ',c=3,r=3,X=4,Y=8'), pattern(20, 40, 4, 8))
    check(graphics(put + ',c=1,r=1,X=4'), pattern(4, 16, 4))
    check(graphics(put + ',X=4294967295'), pattern(xoff=7))
    check(b'\033[1;40H' + graphics(put),
        {(312 + x, y): colours[2 * (y >= 16)]
            for y in range(32) for x in range(8)}, cursor='39,0')

    # Stationary images clip at the bottom; moving images scroll by one row.
    check(b'\033[16;1H' + graphics(put),
        {(x, 240 + y): colours[x >= 8]
            for y in range(16) for x in range(16)}, cursor='0,15')
    check(b'\033[16;1H' + graphics('a=p,i=7'), pattern(yoff=224),
        cursor='2,15')

    # Named placement replacement removes the earlier display.
    first = graphics(put + ',p=9')
    second = b'\033[4;5H' + graphics(put + ',p=9,c=1,r=1')
    check(first + second, pattern(8, 16, 32, 48), cursor='4,3')

    # Placement zero creates independent displays; IDs are local to an image.
    check(graphics(put) + b'\033[4;5H' + graphics(put),
        pattern() | pattern(xoff=32, yoff=48), cursor='4,3')
    blue = graphics('a=t,i=8,f=32,s=1,v=1', b'\0\0\xff\xff')
    other = b'\033[4;5H' + graphics('a=p,i=8,p=9,C=1,c=1,r=1')
    check(first + blue + other, pattern() | solid(8, 16, colours[2], 32, 48),
        cursor='4,3')

    # Failed placement validation preserves the old source and display.
    bad = graphics('a=T,i=7,p=9,f=32,s=1,v=1,x=1,C=1', b'\0\xff\0\xff')
    check(first + bad + b'\033[4;5H' + graphics(put + ',c=1,r=1'),
        pattern() | pattern(8, 16, 32, 48), cursor='4,3')
    check(first + graphics(put + ',p=9,x=16'), pattern())

    # Successful source replacement removes all its old placements.
    green = graphics('a=t,i=7,f=32,s=1,v=1', b'\0\xff\0\xff')
    check(first + b'\033[4;5H' + graphics(put) + green, {}, cursor='4,3')
    check(graphics(put + ',c=1,r=1'), solid(8, 16, colours[1]))

    # Replacement also removes displays saved by the alternate screen.
    check(upload + first + b'\033[?1049h' + green + b'\033[?1049l', {})
    check(upload + first + b'\033[?1049h' + second + b'\033[?1049l', {})

    # Anonymous upload-and-place does not require a stored source mapping.
    check(graphics('a=T,f=32,s=1,v=1,p=9,C=1,c=1,r=1', b'\xff' * 4),
        solid(8, 16, colours[3]))

    # A placement keeps shared pixels after its anonymous source is released.
    check(graphics('a=T,f=32,s=16,v=32,X=4,Y=8,C=1', raw),
        pattern(xoff=4, yoff=8))

# Clients sample the original crop independently, including partial spans.
with image_test(sys.argv[1]) as test:
    client = test.attach()
    source_width, source_height = 32, 32
    source = [(255, 0, 0) if (x + y) % 2 else (0, 0, 255)
        for y in range(source_height) for x in range(source_width)]
    pixels = bytes(value for colour in source for value in (*colour, 255))
    # Shrink horizontally and enlarge vertically with transparent offsets.
    command = graphics('a=T,f=32,s=32,v=32,x=4,y=4,w=24,h=24,'
        'c=2,r=4,X=4,Y=8,C=1', pixels)
    test.emit(command)
    large = test.attach(cw=24, ch=24)

    def expected(cw, ch):
        """Map output coordinates through the full placement to its crop."""
        return {(x, y): source[(4 + (y * 16 - 8 * ch) * 24 // (ch * 56)) * 32 +
            4 + (x * 8 - 4 * cw) * 24 // (cw * 12)]
            for y in range(ch // 2, ch * 4) for x in range(cw // 2, cw * 2)}

    def check_clients(clipped=False):
        """Check shared-source scaling and alignment on both clients."""
        test.redraw()
        terminals = []
        for attached, cw, ch in [(client, 8, 16), (large, 24, 24)]:
            term = Terminal(cw=cw, ch=ch)
            term.feed(attached.data)
            wanted = expected(cw, ch)
            if clipped:
                wanted = {point: colour for point, colour in wanted.items()
                    if point[0] < cw}
            term.check(wanted)
            terminals.append((attached, cw, ch, term))
        return terminals

    terminals = check_clients()
    test.emit(b'\033[2;1H\033[X')
    for attached, cw, ch, term in terminals:
        term.feed(attached.data)
        term.check(expected(cw, ch))
    check_clients()
    # Clip the right-hand span without changing either client's cell pixels.
    test.run('resize-window', '-x', '1')
    check_clients(clipped=True)
    # Changing cell pixels must retain the same partial-span source origin.
    test.resize(client, 1, 16, cw=24, ch=24)
    test.redraw()
    term = Terminal(cw=24, ch=24)
    term.feed(client.data)
    wanted = {point: colour for point, colour in expected(24, 24).items()
        if point[0] < 24}
    term.check(wanted)

# Offsets and source crops contribute fractional coverage to text fallback.
with image_test(sys.argv[1]) as test:
    test.run('set-option', '-as', 'terminal-features', 'screen-256color:RGB')
    client = test.attach(graphical=False)
    pixels = b'\xff\0\0\xff' * 16 + b'\0\0\xff\xff' * 16
    test.emit(graphics('a=T,f=32,s=8,v=4,x=1,w=6,c=1,r=1,X=4,Y=4,C=1',
        pixels))
    test.redraw()
    term = Terminal()
    term.feed(client.data)
    char, fg, bg, bold = term.cells[0, 0]
    assert char == '\u2584' and not bold, (char, bold)
    assert fg == (31, 0, 95) and bg == (63, 0, 0), (fg, bg)
PY
