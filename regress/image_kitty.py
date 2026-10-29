"""Decode native Kitty uploads and placements independently of tmux's image code."""

import base64
from image_terminal import Terminal


class KittyTerminal(Terminal):
    """Keep images separate from text, as required by the Kitty protocol."""

    def __init__(self, *args, scroll_images=False, **kwargs):
        super().__init__(*args, **kwargs)
        self.images = {}
        self.placements = {}
        self.upload = None
        self.loading = bytearray()
        self.uploads = 0
        self.commands = []
        self.scroll_images = scroll_images

    def apc(self, data):
        """Apply a graphics command, including independently encoded chunks."""
        assert data.startswith(b'G'), data
        control, sep, payload = data[1:].partition(b';')
        fields = dict(item.split(b'=', 1) for item in control.split(b','))
        action = fields.get(b'a', b't')
        if action == b'q':
            return
        self.commands.append(fields)
        image = int(fields.get(b'i', 0))
        placement = int(fields.get(b'p', 0))
        if action == b't':
            assert sep and len(payload) <= 4096, (fields, len(payload))
            if self.upload is None:
                assert image and fields[b'f'] == b'32', fields
                assert fields[b'q'] == b'2', fields
                self.upload = fields
                self.loading.clear()
            self.loading.extend(base64.b64decode(payload, validate=True))
            if fields.get(b'm', b'0') == b'0':
                width, height = int(self.upload[b's']), int(self.upload[b'v'])
                assert len(self.loading) == width * height * 4
                image = int(self.upload[b'i'])
                assert image not in self.images, image
                self.images[image] = width, height, bytes(self.loading)
                self.uploads += 1
                self.upload = None
        elif action == b'p':
            assert fields[b'q'] == b'2' and fields[b'C'] == b'1', fields
            assert placement and image in self.images, fields
            assert int(fields.get(b'z', 0)) == 0, fields
            width, height, _ = self.images[image]
            x, y, w, h = [int(fields[key]) for key in (b'x', b'y', b'w', b'h')]
            assert 0 <= x < x + w <= width and 0 <= y < y + h <= height
            c, r = int(fields[b'c']), int(fields[b'r'])
            # Equal aspect ratios avoid Kitty's letterboxing rule.
            assert w * r * self.ch == h * c * self.cw, fields
            self.placements[image, placement] = dict(fields,
                dst_x=self.x, dst_y=self.y)
        elif action == b'd':
            assert fields[b'q'] == b'2' and fields[b'd'] in (b'i', b'I'), fields
            self.placements = {key: value for key, value in self.placements.items()
                if key[0] != image or placement and key[1] != placement}
            if fields[b'd'] == b'I':
                self.images.pop(image, None)
        else:
            raise AssertionError(fields)

    def scroll(self, top, bottom, n):
        """Move graphics with the scroll region when imagescroll is enabled."""
        super().scroll(top, bottom, n)
        if self.scroll_images:
            for fields in self.placements.values():
                if top <= fields['dst_y'] <= bottom:
                    fields['dst_y'] -= n

    def render(self):
        """Blend visible placements in terminal image ID order."""
        pixels = {}
        for key in sorted(self.placements):
            fields = self.placements[key]
            width, height, data = self.images[key[0]]
            sx, sy, w, h = [int(fields[k]) for k in (b'x', b'y', b'w', b'h')]
            dw, dh = int(fields[b'c']) * self.cw, int(fields[b'r']) * self.ch
            dx, dy = fields['dst_x'] * self.cw, fields['dst_y'] * self.ch
            for y in range(dh):
                for x in range(dw):
                    point = dx + x, dy + y
                    if not (0 <= point[0] < self.sx * self.cw and
                            0 <= point[1] < self.sy * self.ch):
                        continue
                    pos = ((sy + y * h // dh) * width + sx + x * w // dw) * 4
                    colour = data[pos:pos + 4]
                    alpha = colour[3]
                    if alpha:
                        under = pixels.get(point, (0, 0, 0))
                        pixels[point] = tuple((v * alpha + u * (255 - alpha))
                            // 255 for v, u in zip(colour[:3], under))
        return pixels

    def check(self, expected):
        """Compare the composed graphics with expected pixel positions and colours."""
        self.pixels = self.render()
        super().check(expected)
