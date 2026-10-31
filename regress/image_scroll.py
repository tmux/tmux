"""PTY checks adapted from the PR's image scroll regressions."""

import re
from image_test import image_test, sixel, check_image
from image_terminal import Terminal


MARKER = b'SCROLLMARKER'
WIRE = sixel(16, 16, [(100, 0, 0), (0, 100, 0)],
    [x >= 8 for y in range(16) for x in range(16)])


def attach(test, scrolls, margins=None, term='xterm-256color'):
    """Choose scroll features before attaching a client."""
    features = [term, 'imagescroll' if scrolls else 'imagescroll@']
    if margins is not None:
        features.append('margins' if margins else 'margins@')
    test.run('set-option', '-as', 'terminal-features', ':'.join(features))
    client = test.attach(term=term)
    prefix = str(client.pid) + ':'
    actual = next(line[len(prefix):] for line in test.run('list-clients',
        '-F', '#{client_pid}:#{client_termfeatures}').splitlines()
        if line.startswith(prefix)).split(',')
    assert ('imagescroll' in actual) == scrolls, actual
    if margins is not None:
        assert ('margins' in actual) == margins, actual
    return client


def place(test):
    """Place marker text and a two-colour image away from the scroll edges."""
    test.emit(b'\033[7;1H' + MARKER + b'\033[9;3H' + WIRE + b'\033[16;1H')
    for client in test.clients:
        images = client.images()
        assert len(images) == 1, 'initial image was not sent'
        check_image(images[0], 16, 16,
            [(255, 0, 0) if x < 8 else (0, 255, 0)
                for y in range(16) for x in range(16)])


def check_output(client, replay):
    """Check whether scroll damage replayed the image and surrounding text."""
    data = bytes(client.data)
    if replay:
        assert client.images(), 'image was not replayed after a manual repaint'
        assert MARKER in data, 'scroll damage did not repaint surrounding text'
    else:
        assert not client.images(), 'native scroll replayed the image'
        assert MARKER not in data, 'native scroll repainted existing text'
    return data


def check_position(test, mixed=False, down=False):
    """A later full redraw must show the scrolled image at its new position."""
    test.redraw()
    row = 9 if down else 7
    expected = {(x, y): (255, 0, 0) if x < 24 else (0, 255, 0)
        for y in range(row * 16, (row + 1) * 16) for x in range(16, 32)
        if not mixed or x >= 24}
    for client in test.clients:
        terminal = Terminal()
        terminal.feed(client.data)
        terminal.check(expected)


def check_scroll(tmux, scrolls, margins=None, no_erase=False, mixed=False,
    command='linefeed'):
    """Check one client's native scroll or fallback repaint."""
    with image_test(tmux) as test:
        test.run('set-option', '-w', 'automatic-rename', 'off')
        if margins is not None:
            test.run('set-option', '-w', 'pane-scrollbars', 'on')
        client = attach(test, scrolls, margins)
        place(test)
        if no_erase:
            test.redraw()
            terminal = Terminal()
            terminal.feed(client.data)
        operation = {'linefeed': b'\n', 'up': b'\033[S', 'down': b'\033[T',
            'reverse': b'\033[H\033M'}[command]
        if mixed:
            # An ordinary overwrite must prevent skipping merged scroll damage.
            operation += b'\033[8;3HX'
        test.emit(operation)
        replay = not scrolls or margins is False or mixed
        data = check_output(client, replay)
        if no_erase:
            # Erasing the newly exposed bottom row is valid. Check that
            # none of the moved image pixels were erased or drawn over.
            terminal.feed(data)
            terminal.check({(x, y): (255, 0, 0) if x < 24 else (0, 255, 0)
                for y in range(7 * 16, 8 * 16) for x in range(16, 32)})
        if scrolls and margins:
            assert re.search(rb'\033\[\d+;\d+s', data), (
                'no horizontal scrolling region was set', data)
        check_position(test, mixed, command in ('down', 'reverse'))


def check_clients(tmux, narrow=False):
    """Check different client capabilities and shared fallback damage."""
    with image_test(tmux) as test:
        test.run('set-option', '-w', 'automatic-rename', 'off')
        if narrow:
            test.run('set-option', '-w', 'pane-scrollbars', 'on')
            native = attach(test, True, True)
            repaint = attach(test, True, False, term='xterm')
        else:
            native = attach(test, True)
            repaint = attach(test, False, term='xterm')
        place(test)
        test.emit(b'\n')
        # A large fallback repaint adds ordinary window damage, which must
        # be drawn on both clients even if the first scrolled natively.
        check_output(native, narrow)
        check_output(repaint, True)
        check_position(test)


def check_capability(tmux, enabled):
    """Iscrl grants image scrolling, and Iscrl@ overrides an enabled feature."""
    with image_test(tmux) as test:
        test.run('set-option', '-w', 'automatic-rename', 'off')
        capability = 'Iscrl' if enabled else 'Iscrl@'
        test.run('set-option', '-as', 'terminal-overrides',
            'xterm-256color:' + capability)
        if not enabled:
            test.run('set-option', '-as', 'terminal-features',
                'xterm-256color:imagescroll')
        client = test.attach()
        place(test)
        test.emit(b'\n')
        check_output(client, not enabled)
        check_position(test)
