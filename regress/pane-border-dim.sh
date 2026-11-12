#!/bin/sh

# Borders and pane status lines must use the border dim, including when a
# focus change changes only dim and leaves the cached status grid unchanged.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import os
import re
import shlex
import subprocess
import sys
import time

tmux = sys.argv[1]
inner = [tmux, "-Ldim-inner-%d" % os.getpid(), "-f/dev/null"]
outer = [tmux, "-Ldim-outer-%d" % os.getpid(), "-f/dev/null"]


def run(server, *args, check=True):
    p = subprocess.run(server + list(args), capture_output=True,
        text=True, timeout=10)
    if check and p.returncode != 0:
        raise RuntimeError("%r: %s" % (args, p.stderr))
    return p.stdout


def colours(text):
    rows = [[]]
    fg = bg = None
    for match in re.finditer(r"\033\[([0-9;]*)m|([^\033])", text):
        if match[1] is not None:
            codes = [int(code or "0") for code in match[1].split(";")]
            while codes:
                code = codes.pop(0)
                if code == 0:
                    fg = bg = None
                elif code == 39:
                    fg = None
                elif code == 49:
                    bg = None
                elif code in (38, 48) and codes[:1] == [2]:
                    value = tuple(codes[1:4])
                    del codes[:4]
                    if code == 38:
                        fg = value
                    else:
                        bg = value
        elif match[2] == "\n":
            rows.append([])
        else:
            rows[-1].append((match[2], fg, bg))
    return rows


def wait_borders(active):
    x = int(run(inner, "display-message", "-p", "-t", "inner:0.0",
        "#{pane_width}"))
    deadline = time.monotonic() + 10
    while True:
        capture = run(outer, "capture-pane", "-pe", "-t", "outer:0.0")
        rows = colours(capture)
        matched = True
        # Colour indicators assign the top of this separator to the left
        # pane and the bottom to the right pane. Check both styles.
        for pane, y in ((0, 0), (1, 15)):
            fg, bg = (100, 50) if pane == active else (50, 25)
            expected = ("|", (fg,) * 3, (bg,) * 3)
            if len(rows[y]) <= x or rows[y][x] != expected:
                matched = False
                break
        if matched:
            return
        if time.monotonic() >= deadline:
            raise AssertionError(("borders", active, capture))
        time.sleep(0.05)


def wait_titles(position, active, dims=(75, 50), inline=False):
    deadline = time.monotonic() + 10
    while True:
        capture = run(outer, "capture-pane", "-pe", "-t", "outer:0.0")
        rows = colours(capture)
        cells = rows[0 if position == "top" else 15]
        text = "".join(cell[0] for cell in cells)
        matched = True
        for pane in (0, 1):
            label = "TITLE%d" % pane
            start = text.find(label)
            dim = dims[pane == active]
            fg, bg = ((160, 192, 224), (32, 64, 96)) if inline else (
                (200, 200, 200), (100, 100, 100))
            expected = tuple(tuple(c * (100 - dim) // 100 for c in rgb)
                for rgb in (fg, bg))
            if start == -1 or any(cell[1:] != expected
                for cell in cells[start:start + len(label)]):
                matched = False
                break
            # The remaining horizontal border uses the base style, even
            # when the title overrides its foreground and background.
            expected = ((200 * (100 - dim) // 100,) * 3,
                (100 * (100 - dim) // 100,) * 3)
            start += len(label)
            if any(cell != ("-", *expected)
                for cell in cells[start:start + 3]):
                matched = False
                break
        if matched:
            return
        if time.monotonic() >= deadline:
            raise AssertionError((position, active, dims, inline, capture))
        time.sleep(0.05)


try:
    run(inner, "new-session", "-d", "-s", "inner", "-x", "60", "-y", "16",
        "sleep 100")
    for option, value in (("status", "off"), ("window-size", "manual"),
        ("automatic-rename", "off"), ("status-interval", "0"),
        ("terminal-features", "*:RGB"), ("pane-border-lines", "simple"),
        ("pane-border-status", "off"), ("pane-border-indicators", "colour"),
        ("pane-border-format", "TITLE#{pane_index}"),
        ("pane-border-style", "fg=#c8c8c8,bg=#646464,dim=75%"),
        ("pane-active-border-style", "fg=#c8c8c8,bg=#646464,dim=50%")):
        run(inner, "set", "-g", option, value)
    run(inner, "split-window", "-h", "-t", "inner", "sleep 100")
    run(outer, "new-session", "-d", "-s", "outer", "-x", "60", "-y", "16",
        "sleep 100")
    run(outer, "set", "-g", "status", "off")
    run(outer, "set", "-g", "window-size", "manual")
    run(outer, "set", "-g", "default-terminal", "tmux-256color")
    run(outer, "respawn-pane", "-k", "-t", "outer:0.0",
        shlex.join(inner + ["attach-session", "-t", "inner"]))

    for active in (1, 0, 1):
        run(inner, "select-pane", "-t", "inner:0.%d" % active)
        wait_borders(active)

    for position in ("top", "bottom"):
        run(inner, "set", "-g", "pane-border-status", position)
        for active in (1, 0, 1):
            run(inner, "select-pane", "-t", "inner:0.%d" % active)
            wait_titles(position, active)

    run(inner, "set", "-g", "pane-border-format",
        "#[fg=#a0c0e0,bg=#204060]TITLE#{pane_index}")
    for active in (1, 0):
        run(inner, "select-pane", "-t", "inner:0.%d" % active)
        wait_titles("bottom", active, inline=True)

    for option in ("pane-border-style", "pane-active-border-style"):
        run(inner, "set", "-g", option, "fg=#c8c8c8,bg=#646464,dim=0%")
    wait_titles("bottom", 0, dims=(0, 0), inline=True)
finally:
    run(outer, "kill-server", check=False)
    run(inner, "kill-server", check=False)
PY
