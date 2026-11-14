#!/bin/sh

# Built-in switcher formats use mode-style's dim attribute on selected rows.
# Custom formats, unselected rows and search matches keep their styles (#5854).

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

. ./regress-common.inc
DIR=$(test_tmpdir) || exit 1
TMUX_TMPDIR=$DIR
export TMUX_TMPDIR
trap 'rm -rf "$DIR"' 0

python3 -B - "$TEST_TMUX" <<'PY'
import os
import re
import shlex
import subprocess
import sys
import time

tmux = sys.argv[1]
inner = [tmux, "-Lswitch-inner-%d" % os.getpid(), "-f/dev/null"]
outer = [tmux, "-Lswitch-outer-%d" % os.getpid(), "-f/dev/null"]


# Run a command on one of the isolated servers.
def run(server, *args, check=True):
    p = subprocess.run(server + list(args), capture_output=True,
        text=True, timeout=10)
    if check and p.returncode != 0:
        raise RuntimeError("%r: %s" % (args, p.stderr))
    output = p.stdout.strip()
    return output


# Decode capture-pane's SGR sequences into a style for each character.
def cells(line):
    text, styles = "", []
    attrs, colours = set(), [None, None]
    for match in re.finditer(r"\033\[([0-9;:]*)m|([^\033])", line):
        if match[2] is not None:
            text += match[2]
            styles.append((frozenset(attrs), *colours))
            continue
        args = iter(int(n or 0) for n in match[1].replace(":", ";").split(";"))
        for code in args:
            if code == 0:
                attrs, colours = set(), [None, None]
            elif code in (1, 2, 3, 4):
                attrs.add(code)
            elif code == 22:
                attrs.difference_update((1, 2))
            elif code in (23, 24):
                attrs.discard(code - 20)
            elif code in (39, 49):
                colours[(code - 39) // 10] = None
            elif 30 <= code <= 37 or 40 <= code <= 47:
                colours[code // 10 - 3] = code % 10
            elif code in (38, 48):
                mode = next(args)
                colours[(code - 38) // 10] = (tuple(next(args) for _ in range(3))
                    if mode == 2 else next(args))
    text = text.rstrip()
    return text, styles[:len(text)]


# Wait for both rows and the expected selection to reach the attached client.
def expect(selected, prefix="", selection_dim=False, custom=False, match=False):
    deadline = time.monotonic() + 5
    while True:
        capture = run(outer, "capture-pane", "-pe", "-t", "outer:0.0")
        rows = {}
        for line in capture.splitlines():
            text, styles = cells(line)
            for name in ("alpha", "bravo"):
                if text.startswith(prefix + name):
                    rows[name] = (text, styles)
        try:
            assert len(rows) == 2, rows
            for name, (text, styles) in rows.items():
                if match and not custom:
                    assert styles[0] == (frozenset({1, 2}), 6, 5), styles
                    styles = styles[1:]
                if custom:
                    # Inline dim and nodim override the starting style;
                    # other attributes and colours also survive selection.
                    for index in range(3):
                        attrs, fg, bg = styles[index]
                        dim = index == 1
                        if index == 0 and name == selected:
                            dim = selection_dim
                        assert (2 in attrs) == dim, (name, index, styles)
                        assert attrs - {2} == {1, 3, 4}, (name, index, styles)
                        assert (fg, bg) == (1, 4), (name, index, styles)
                    if match:
                        attrs, fg, bg = styles[text.index("a", 4)]
                        assert (attrs, fg, bg) == (frozenset({1, 2}), 6, 5), styles
                    else:
                        dim = selection_dim and name == selected
                        assert all((2 in s[0]) == dim for s in styles[4:]), styles
                elif name == selected:
                    assert all((2 in s[0]) == selection_dim for s in styles), styles
                    assert all(s[1:] == styles[0][1:] for s in styles), styles
                else:
                    assert 2 not in styles[0][0], styles
                    assert any(2 in s[0] for s in styles), styles
            return
        except AssertionError:
            if time.monotonic() >= deadline:
                raise
        time.sleep(.05)


# Close the mode before opening the next case.
def cancel():
    run(inner, "send-keys", "-t", "alpha:0", "Escape")


try:
    for name in ("alpha", "bravo"):
        run(inner, "new-session", "-d", "-s", name, "-n", name,
            "-x", "60", "-y", "12", "exec sleep 100")
    run(inner, "set", "-g", "status", "off")
    run(outer, "new-session", "-d", "-s", "outer", "-x", "60", "-y", "12",
        shlex.join(inner + ["attach-session", "-t", "alpha"]))
    run(outer, "set", "-g", "status", "off")

    # Both built-in formats retain secondary dimming only off the selection.
    for theme in ("dark", "light"):
        run(inner, "set", "-s", "theme", theme)
        for mode in ("-s", "-w"):
            run(inner, "switch-mode", mode, "-t", "alpha:0")
            expect("alpha")
            run(inner, "send-keys", "-t", "alpha:0", "Down")
            expect("bravo")
            cancel()

    # Explicit selection dimming is honoured, including after #[default].
    run(inner, "set", "-g", "mode-style", "dim,fg=black,bg=yellow")
    run(inner, "switch-mode", "-s", "-t", "alpha:0")
    expect("alpha", selection_dim=True)
    cancel()

    format = ("#[fg=red,bg=blue,bold,italics,underscore]X#[dim]Y#[nodim]Z"
        "#[default] #{session_name}")
    input_path = os.path.join(os.environ["TMUX_TMPDIR"], "switch-input")
    with open(input_path, "w") as output:
        for name in ("alpha", "bravo"):
            output.write(format.replace("#{session_name}", name) + "\n")

    # Explicit session/window formats and file input keep their own dimming.
    for dim in (False, True):
        style = "fg=black,bg=yellow"
        if dim:
            style += ",dim"
        run(inner, "set", "-g", "mode-style", style)
        for args in (("-s", "-F", format), ("-w", "-F", format),
            ("-f", input_path)):
            run(inner, "switch-mode", "-t", "alpha:0", *args)
            expect("alpha", "XYZ ", dim, custom=True)
            run(inner, "send-keys", "-t", "alpha:0", "Down")
            expect("bravo", "XYZ ", dim, custom=True)
            cancel()

    # Match styling is applied after selection styling, including explicit dim.
    run(inner, "set", "-g", "mode-style", "fg=black,bg=yellow")
    run(inner, "set", "-g", "switch-mode-match-style", "bold,dim,fg=cyan,bg=magenta")
    run(inner, "switch-mode", "-s", "-t", "alpha:0", "-F", format)
    expect("alpha", "XYZ ", custom=True)
    run(inner, "send-keys", "-t", "alpha:0", "-l", "a")
    expect("alpha", "XYZ ", custom=True, match=True)
    cancel()

    # Built-in normal and selected formats share their text and match columns.
    for mode in ("-s", "-w"):
        run(inner, "switch-mode", mode, "-t", "alpha:0")
        expect("alpha")
        run(inner, "send-keys", "-t", "alpha:0", "-l", "^a | ^b")
        expect("alpha", match=True)
        run(inner, "send-keys", "-t", "alpha:0", "Down")
        expect("bravo", match=True)
        cancel()
finally:
    run(outer, "kill-server", check=False)
    run(inner, "kill-server", check=False)
PY
