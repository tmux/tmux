"""Observe cursor positions, colours and erases in tmux's terminal output."""

import re
import unicodedata
from image_test import decode_sixel


class Terminal:
    """Track image pixels and text cells for the controls used by these tests."""

    def __init__(self, sx=40, sy=16, cw=8, ch=16):
        self.sx, self.sy, self.cw, self.ch = sx, sy, cw, ch
        self.x = self.y = 0
        self.top, self.bottom = 0, sy - 1
        self.fg = self.bg = None
        self.bold = False
        self.pixels = {}
        self.cells = {}
        self.pending = b''

    def erase(self, x, y, w, h):
        """Remove image pixels and text in a rectangle of cells."""
        for px, py in list(self.pixels):
            if x * self.cw <= px < (x + w) * self.cw and \
                y * self.ch <= py < (y + h) * self.ch:
                del self.pixels[px, py]
        for cy in range(y, y + h):
            for cx in range(x, x + w):
                self.cells[cx, cy] = (' ', self.fg, self.bg, self.bold)

    def sgr(self, args):
        """Apply text colours and bold attributes."""
        i = 0
        while i < len(args):
            value = args[i]
            i += 1
            if value == 0:
                self.fg = self.bg = None
                self.bold = False
            elif value in (1, 22):
                self.bold = value == 1
            elif value in (39, 49):
                setattr(self, 'fg' if value == 39 else 'bg', None)
            elif 30 <= value <= 37 or 40 <= value <= 47:
                setattr(self, 'fg' if value < 40 else 'bg', value % 10)
            elif 90 <= value <= 97 or 100 <= value <= 107:
                setattr(self, 'fg' if value < 100 else 'bg', value % 10 + 8)
            elif value in (38, 48):
                mode = args[i]
                i += 1
                assert mode in (2, 5), args
                n = 3 if mode == 2 else 1
                colour = tuple(args[i:i + n]) if n == 3 else args[i]
                setattr(self, 'fg' if value == 38 else 'bg', colour)
                i += n

    def scroll(self, top, bottom, n):
        """Move pixels and text within a vertical scroll region."""
        moved = {}
        for (x, y), colour in list(self.pixels.items()):
            if top * self.ch <= y < (bottom + 1) * self.ch:
                del self.pixels[x, y]
                if top * self.ch <= y - n * self.ch < (bottom + 1) * self.ch:
                    moved[x, y - n * self.ch] = colour
        cells = {(x, y - n): cell for (x, y), cell in self.cells.items()
            if top <= y <= bottom and top <= y - n <= bottom}
        self.erase(0, top, self.sx, bottom - top + 1)
        self.pixels.update(moved)
        self.cells.update(cells)

    def shift(self, x, y, n):
        """Move pixels and text when characters are inserted or deleted."""
        moved = {}
        for (px, py), colour in list(self.pixels.items()):
            if x * self.cw <= px < self.sx * self.cw and \
                y * self.ch <= py < (y + 1) * self.ch:
                if x * self.cw <= px - n * self.cw < self.sx * self.cw:
                    moved[px - n * self.cw, py] = colour
        cells = {(cx - n, cy): cell for (cx, cy), cell in self.cells.items()
            if cy == y and x <= cx < self.sx and x <= cx - n < self.sx}
        self.erase(x, y, self.sx - x, 1)
        self.pixels.update(moved)
        self.cells.update(cells)

    def csi(self, params, command):
        """Apply cursor movement and erases; ignore terminal mode controls."""
        if params.startswith((b'?', b'>', b'=')) or b' ' in params:
            return
        args = [int(p or b'0') for p in params.split(b';')]
        n = args[0] or 1
        if command in b'Hf':
            self.y = n - 1
            self.x = (args[1] or 1) - 1 if len(args) > 1 else 0
        elif command == ord('G'):
            self.x = n - 1
        elif command == ord('d'):
            self.y = n - 1
        elif command in b'ABCD':
            dx, dy = {65: (0, -n), 66: (0, n),
                67: (n, 0), 68: (-n, 0)}[command]
            self.x = max(0, self.x + dx)
            self.y = max(0, self.y + dy)
        elif command == ord('X'):
            self.erase(self.x, self.y, n, 1)
        elif command in b'@P':
            self.shift(self.x, self.y, n if command == ord('P') else -n)
        elif command == ord('K'):
            start = self.x if args[0] == 0 else 0
            end = self.x + 1 if args[0] == 1 else self.sx
            self.erase(start, self.y, end - start, 1)
        elif command == ord('J'):
            if args[0] in (2, 3):
                self.erase(0, 0, self.sx, self.sy)
            elif args[0] == 0:
                self.erase(self.x, self.y, self.sx - self.x, 1)
                self.erase(0, self.y + 1, self.sx, self.sy - self.y - 1)
            elif args[0] == 1:
                self.erase(0, 0, self.sx, self.y)
                self.erase(0, self.y, self.x + 1, 1)
        elif command == ord('m'):
            self.sgr(args)
        elif command == ord('r'):
            self.top = n - 1
            self.bottom = (args[1] or self.sy) - 1 if len(args) > 1 else self.sy - 1
            self.x = self.y = 0
        elif command in b'STLM':
            top = self.y if command in b'LM' else self.top
            self.scroll(top, self.bottom, n if command in b'SM' else -n)
        else:
            assert command in b'hlcqnt', (params, chr(command))

    def apc(self, data):
        """Ignore application controls unless a subclass handles them."""

    def feed(self, data):
        """Apply complete output sequences, keeping any unfinished sequence."""
        data = self.pending + bytes(data)
        pos = 0
        while pos < len(data):
            rest = data[pos:]
            if rest.startswith((b'\033P', b'\033]', b'\033_')):
                match = re.match(rb'\033([P\]_])(.*?)(?:\007|\033\\)',
                    rest, re.S)
                if match is None:
                    break
                if match[1] == b'P':
                    image = re.match(rb'([0-9;]*)q(.*)', match[2], re.S)
                    if image is not None:
                        params = image[1].split(b';')
                        p2 = int(params[1] or b'0') if len(params) > 1 else 0
                        w, h, pixels = decode_sixel(image[2], p2)
                        for i, colour in enumerate(pixels):
                            if colour is not None:
                                self.pixels[self.x * self.cw + i % w,
                                    self.y * self.ch + i // w] = colour
                if match[1] == b'_':
                    self.apc(match[2])
                pos += match.end()
            elif rest.startswith(b'\033['):
                match = re.match(rb'\033\[([0-?]*[ -/]*)([@-~])', rest)
                if match is None:
                    break
                self.csi(match[1], match[2][0])
                pos += match.end()
            elif rest[0] == 27:
                match = re.match(rb'\033(?:[()][0-~]|.)', rest, re.S)
                if match is None:
                    break
                if match[0] == b'\033M':
                    if self.y == self.top:
                        self.scroll(self.top, self.bottom, -1)
                    else:
                        self.y = max(0, self.y - 1)
                else:
                    assert match[0] in (b'\033(B', b'\033(0', b'\033)B',
                        b'\033=', b'\033>', b'\033\\'), match[0]
                pos += match.end()
            elif rest[0] < 32 or rest[0] == 127:
                if rest[0] == 13:
                    self.x = 0
                elif rest[0] == 10:
                    if self.y == self.bottom:
                        self.scroll(self.top, self.bottom, 1)
                    else:
                        self.y += 1
                elif rest[0] == 8:
                    self.x = max(0, self.x - 1)
                pos += 1
            else:
                if rest[0] < 128:
                    n = 1
                elif rest[0] < 224:
                    n = 2
                elif rest[0] < 240:
                    n = 3
                else:
                    n = 4
                if len(rest) < n:
                    break
                char = rest[:n].decode()
                width = 2 if unicodedata.east_asian_width(char) in 'WF' else 1
                if self.x >= self.sx:
                    self.x = 0
                    self.y += 1
                self.erase(self.x, self.y, width, 1)
                self.cells[self.x, self.y] = (char, self.fg, self.bg, self.bold)
                self.x += width
                pos += n
        self.pending = data[pos:]

    def check(self, expected):
        """Compare all visible image pixels, including their positions."""
        assert self.pixels.keys() == expected.keys(), (
            'pixel positions differ', len(self.pixels), len(expected),
            list(self.pixels.keys() - expected.keys())[:5],
            list(expected.keys() - self.pixels.keys())[:5])
        for point, colour in expected.items():
            assert max(abs(a - b) for a, b in zip(self.pixels[point], colour)) \
                <= 3, (point, self.pixels[point], colour)
