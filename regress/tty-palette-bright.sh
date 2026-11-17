#!/bin/sh

# Inspect an attached client's output through a second tmux. Bold ANSI
# foregrounds use the bright palette entry unless Nobr is present.

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
inner = [tmux, "-Lbright-inner-%d" % os.getpid(), "-f/dev/null"]
outer = [tmux, "-Lbright-outer-%d" % os.getpid(), "-f/dev/null"]
rgb_palette = [(32 + i, 64 + i, 96 + i) for i in range(8)]
rgb_palette += [(160 + i, 192 + i, 224 + i) for i in range(8)]
rgb_palette[3], rgb_palette[11] = (127, 128, 8), (253, 255, 28)


def run(server, *args, check=True):
    p = subprocess.run(server + list(args), capture_output=True,
        text=True, timeout=10)
    if check and p.returncode != 0:
        raise RuntimeError("%r: %s" % (args, p.stderr))
    return p.stdout.strip()


def cells(text):
    colours = [None, None, None]
    bold = False
    result = []
    for match in re.finditer(r"\033\[([0-9;:]*)m|([ABCDE])", text):
        if match[2] is not None:
            result.append((match[2], *colours, bold))
            continue
        args = iter(int(n or 0) for n in match[1].replace(":", ";").split(";"))
        for code in args:
            if code == 0:
                colours = [None, None, None]
                bold = False
            elif code in (1, 22):
                bold = code == 1
            elif code in (39, 49, 59):
                colours[(code - 39) // 10] = None
            elif 30 <= code <= 37 or 40 <= code <= 47:
                colours[code // 10 - 3] = code % 10
            elif 90 <= code <= 97 or 100 <= code <= 107:
                colours[code // 10 - 9] = code % 10 + 8
            elif code in (38, 48, 58):
                mode = next(args)
                colours[(code - 38) // 10] = (tuple(next(args) for _ in range(3))
                    if mode == 2 else next(args))
    return result


def wait_capture(expected, context):
    deadline = time.monotonic() + 10
    while True:
        capture = run(outer, "capture-pane", "-pe", "-t", "outer:0.0")
        actual = cells(capture)
        if actual == expected:
            return
        if time.monotonic() >= deadline:
            raise AssertionError((context, expected, actual, capture))
        time.sleep(0.05)


try:
    for nobr in (False, True):
        run(inner, "new-session", "-d", "-s", "inner", "-x", "20", "-y", "10",
            "sleep 100")
        for option, value in (("status", "off"), ("window-size", "manual"),
            ("terminal-features", "*:RGB:usstyle"),
            ("terminal-overrides", "*:Nobr=" if nobr else "*:Nobr@")):
            run(inner, "set", "-g", option, value)
        # Normal ANSI, bold ANSI, bold indexed, explicit bright, and RGB.
        wire = "\r\n".join(
            "\033[0;4;%d;%d;58;5;%dmA\033[1mB" % (30 + i, 40 + i, i) +
            "\033[38;5;%d;48;5;%dmC" % (i, i) +
            "\033[%d;%d;58;5;%dmD" % (90 + i, 100 + i, 8 + i) +
            "\033[38;2;20;40;60;48;2;20;40;60;58;2;20;40;60mE\033[0m"
            for i in range(8))
        run(inner, "respawn-pane", "-k",
            "printf %s " + shlex.quote(wire) + "; exec sleep 100")
        run(outer, "new-session", "-d", "-s", "outer", "-x", "20", "-y", "10",
            "sleep 100")
        run(outer, "set", "-g", "status", "off")
        run(outer, "set", "-g", "window-size", "manual")
        run(outer, "respawn-pane", "-k",
            shlex.join(inner + ["attach-session", "-t", "inner"]))

        # Include sparse palettes: a missing bright entry must leave the
        # original ANSI foreground for the terminal to brighten itself.
        for entries, dim in ((range(16), 0), (range(16), 50), (range(8), 0),
            (range(8, 16), 0), (range(0), 0)):
            palette = rgb_palette
            run(inner, "set", "-pu", "pane-colours")
            for i in entries:
                run(inner, "set", "-p", "pane-colours[%d]" % i,
                    "#%02x%02x%02x" % palette[i])
            run(inner, "set", "-g", "window-style", "dim=%d%%" % dim)
            run(inner, "set", "-g", "window-active-style", "dim=%d%%" % dim)
            expected = []
            for i in range(8):
                for char, fg, bg, us, bold in (
                    ("A", i, i, i, False),
                    ("B", i if nobr else i + 8, i, i, True),
                    ("C", i, i, i, True),
                    ("D", i + 8, i + 8, i + 8, True),
                    ("E", (20, 40, 60), (20, 40, 60), (20, 40, 60), True)):
                    fg = palette[fg] if fg in entries else (i if char == "B" else fg)
                    bg = palette[bg] if bg in entries else bg
                    us = palette[us] if us in entries else us
                    if dim:
                        fg = tuple(c * (100 - dim) // 100 for c in fg)
                        bg = tuple(c * (100 - dim) // 100 for c in bg)
                    expected.append((char, fg, bg, us, bold))
            context = (nobr, entries, dim)
            wait_capture(expected, context)
            run(inner, "refresh-client")
            wait_capture(expected, (context, "redraw"))

        # Preserve existing handling of themed defaults inside the pane
        # for this bold fix. Decorations use no pane palette or NOPALETTE.
        run(inner, "set", "-p", "pane-colours[1]", "#12ab34")
        run(inner, "set", "-p", "pane-colours[9]", "#34ab12")
        run(inner, "set", "-p", "pane-colours[4]", "#ab1234")
        for option in ("window-style", "window-active-style"):
            run(inner, "set", "-g", option, "fg=themered,bg=themeblue")
        for theme in ("terminal", "dark", "light"):
            if theme != "terminal":
                run(inner, "set", "-s", theme + "-theme-red", "red")
                run(inner, "set", "-s", theme + "-theme-blue", "blue")
            run(inner, "set", "-s", "theme", theme)
            run(inner, "respawn-pane", "-k",
                "printf '\\033[0mA\\033[1mB\\033[0m'; exec sleep 100")
            wait_capture([
                ("A", (18, 171, 52), (171, 18, 52), None, False),
                ("B", (18, 171, 52) if nobr else (52, 171, 18),
                    (171, 18, 52), None, True)], ("theme", theme, nobr))
        run(outer, "kill-server")
        run(inner, "kill-server")
finally:
    run(outer, "kill-server", check=False)
    run(inner, "kill-server", check=False)
PY
