#!/bin/sh

# GitHub #5784: a tiled pane must not lose its border junction ownership
# when older panes mark the same cell. Capture an attached client's rendering
# through an outer tmux: capture-pane on the inner panes cannot see borders.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 - "$TEST_TMUX" <<'PY'
import re
import shlex
import subprocess
import sys
import tempfile
import time


def run(server, *args, check=True):
    result = subprocess.run(server + list(args), capture_output=True,
        text=True, timeout=10)
    if check and result.returncode:
        raise RuntimeError((args, result.stderr))
    return result.stdout.rstrip("\n")


def wait(predicate):
    deadline = time.monotonic() + 10
    while True:
        if predicate():
            return
        if time.monotonic() >= deadline:
            raise AssertionError("timed out waiting for client rendering")
        time.sleep(0.05)


def capture():
    # capture-pane -e starts each line with default attributes. Decode only
    # foreground and reverse, which are the attributes checked below.
    grid = {}
    output = run(outer, "capture-pane", "-ep")
    for y, line in enumerate(output.splitlines()):
        x, fg, reverse = 0, 39, False
        for token in re.findall(r"\x1b\[[0-9;]*m|[^\x1b]", line):
            if token.startswith("\x1b"):
                for code in map(int, token[2:-1].split(";")):
                    if code == 0:
                        fg, reverse = 39, False
                    elif 30 <= code <= 37 or code == 39:
                        fg = code
                    elif code in (7, 27):
                        reverse = code == 7
            else:
                grid[x, y] = token, fg, reverse
                x += 1
    return grid


def check_junctions(status, marked=False):
    run(inner, "set", "pane-border-status", status)
    panes = {}
    for line in run(inner, "list-panes", "-F",
            "#{pane_id} #{pane_left} #{pane_top} #{pane_width} "
            "#{pane_height}").splitlines():
        pane, *rect = line.split()
        x, y, sx, sy = map(int, rect)
        panes[pane] = x - 1, y - 1, x + sx, y + sy

    # A junction touches at least three pane rectangles. Derive this from
    # geometry, independently of the redraw scene's ownership slots.
    junctions = {}
    for y in range(24):
        for x in range(80):
            adjacent = {pane for pane, (l, t, r, b) in panes.items()
                if l <= x <= r and t <= y <= b and
                (x in (l, r) or y in (t, b))}
            if len(adjacent) >= 3:
                junctions[x, y] = adjacent
    assert junctions, (status, panes)

    for index, active in enumerate(panes):
        # Change the colour and selection together. A previous frame's green
        # junction must not satisfy the check before the new selection draws.
        active_fg = 32 + index % 2
        run(inner, "set", "pane-active-border-style",
            "fg=colour%d" % (active_fg - 30), ";",
            "select-pane", "-t", active)
        for mark in (panes if marked else [None]):
            run(inner, "select-pane", "-M")
            if mark is not None:
                def unmarked():
                    grid = capture()
                    return all(pos in grid and not grid[pos][2]
                        for pos in junctions)

                wait(unmarked)
                run(inner, "select-pane", "-m", "-t", mark)
            expected = {pos: (active_fg if active in adjacent else 31,
                mark in adjacent) for pos, adjacent in junctions.items()}

            def matches():
                grid = capture()
                for pos, style in expected.items():
                    cell = grid.get(pos)
                    if cell is None or cell[0] not in "┼├┤┬┴" or cell[1:] != style:
                        return False
                return True

            try:
                wait(matches)
            except AssertionError:
                grid = capture()
                actual = {pos: grid.get(pos) for pos in expected}
                raise AssertionError((status, active, mark, expected, actual))
    run(inner, "select-pane", "-M")


with tempfile.TemporaryDirectory(prefix="tmux-junctions-", dir="/tmp") as tmp:
    inner = [sys.argv[1], "-S", tmp + "/i", "-f/dev/null"]
    outer = [sys.argv[1], "-S", tmp + "/o", "-f/dev/null"]
    try:
        for server in (inner, outer):
            run(server, "new-session", "-d", "-x80", "-y24", "exec sleep 1000")
            run(server, "set", "-g", "status", "off")
            run(server, "set", "-g", "window-size", "manual")
        run(inner, "set", "-g", "pane-border-style", "fg=red")
        run(inner, "set", "-g", "pane-active-border-style", "fg=green")
        run(inner, "set", "-g", "pane-border-indicators", "off")
        run(inner, "set", "-g", "pane-border-format", "")
        run(outer, "set", "-g", "default-terminal", "tmux-256color")
        run(outer, "respawn-pane", "-k", shlex.join(inner + ["attach-session"]))
        wait(lambda: bool(run(inner, "list-clients")))

        # Make each quadrant the newest pane in turn. Check every active pane
        # with status lines off, above, and below, including reversed borders
        # for marked panes. Status lines turn the cross into two T-junctions.
        for row in (0, 1):
            for before in (False, True):
                run(inner, "new-window", "exec sleep 1000")
                run(inner, "split-window", "-v", "exec sleep 1000")
                run(inner, "select-pane", "-t", str(1 - row))
                run(inner, "split-window", "-h", "exec sleep 1000")
                run(inner, "select-pane", "-t", str(0 if row == 0 else 2))
                run(inner, "split-window", "-h", *(["-b"] if before else []),
                    "exec sleep 1000")
                for status in ("off", "top", "bottom"):
                    check_junctions(status)
                check_junctions("off", marked=True)

        # Multiple junctions, including ones not touching the active pane.
        run(inner, "new-window", "exec sleep 1000")
        for _ in range(8):
            run(inner, "split-window", "exec sleep 1000")
            run(inner, "select-layout", "tiled")
        for status in ("off", "top", "bottom"):
            check_junctions(status)
    finally:
        for server in (outer, inner):
            run(server, "kill-server", check=False)
PY
