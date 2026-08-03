#!/bin/sh

PATH=/bin:/usr/bin
TERM=xterm-256color

# The regular regress build does not include the Zig backend.
[ -z "$TEST_TMUX" ] && exit 0

python3 - "$TEST_TMUX" <<'PY'
import os
import re
import subprocess
import sys
import tempfile
import time

tmux = sys.argv[1]
label = f"ghostty-behaviour-{os.getpid()}"
server = [tmux, "-L", label, "-f", "/dev/null"]
home = tempfile.TemporaryDirectory()
env = os.environ.copy()
env["HOME"] = home.name
env["TERM"] = "xterm-256color"


def run(*args, check=True):
    result = subprocess.run(
        server + list(args),
        check=False,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if check and result.returncode != 0:
        error = result.stderr.decode(errors="replace").strip()
        raise RuntimeError(f"{result.args!r} failed: {error}")
    return result.stdout.decode(errors="replace")


def value(target, fmt):
    return run("display-message", "-p", "-t", target, fmt).strip()


def wait_value(target, fmt, expected, timeout=5):
    end = time.monotonic() + timeout
    actual = None
    while time.monotonic() < end:
        actual = value(target, fmt)
        if actual == expected:
            return
        time.sleep(0.02)
    raise AssertionError(f"expected {fmt}={expected!r}, got {actual!r}")


def respawn(target, command):
    run("respawn-pane", "-k", "-t", target, command)


try:
    pane = run(
        "new-session",
        "-d",
        "-P",
        "-F",
        "#{pane_id}",
        "-x",
        "20",
        "-y",
        "8",
        "-s",
        "behaviour",
        "stty -echo; exec cat",
    ).strip()

    # Changing the option cannot hand a live byte stream to another parser.
    run("send-keys", "-t", pane, "-l", "before")
    run("send-keys", "-t", pane, "Enter")
    time.sleep(0.1)
    run("set-option", "-p", "-t", pane, "ghostty-vt", "on")
    run("send-keys", "-t", pane, "-l", "after")
    run("send-keys", "-t", pane, "Enter")
    time.sleep(0.1)
    capture = run("capture-pane", "-p", "-t", pane)
    if "before" not in capture or "after" not in capture:
        raise AssertionError(f"option change erased pane content: {capture!r}")

    run("set-option", "-g", "@prompt", "0")
    run("set-option", "-g", "@started", "0")
    run("set-option", "-g", "@finished", "0")
    run("set-hook", "-g", "pane-shell-prompt", "set -g @prompt 1")
    run("set-hook", "-g", "pane-command-started", "set -g @started 1")
    run(
        "set-hook",
        "-g",
        "pane-command-finished",
        'set -gF @finished "#{hook_command_status}"',
    )
    respawn(
        pane,
        "printf 'xx\\033]133;A\\007p>\\033]133;B\\007cmd\\n'"
        "'zz\\033]133;C\\007out\\033]133;D;7\\007\\n'; sleep 30",
    )
    wait_value(pane, "#{@prompt}", "1")
    wait_value(pane, "#{@started}", "1")
    wait_value(pane, "#{@finished}", "7")
    raw = run("capture-pane", "-pR", "-t", pane)
    expected = (
        r"L 0 .*flags=START_PROMPT,START_COMMAND.*osc133=2,4,0,0,0.*"
        r"L 1 .*flags=START_OUTPUT,END_OUTPUT.*osc133=0,0,2,5,7"
    )
    if re.search(expected, raw, re.DOTALL) is None:
        raise AssertionError(f"OSC 133 metadata did not match tmux: {raw!r}")

    respawn(
        pane,
        "printf 'x\\033'; sleep 0.05; printf ']133;A'; sleep 0.05; "
        "printf '\\007p'; sleep 30",
    )
    time.sleep(0.3)
    raw = run("capture-pane", "-pR", "-t", pane)
    if re.search(r"L 0 .*flags=START_PROMPT.*osc133=1,0,0,0,0", raw) is None:
        raise AssertionError(f"fragmented OSC 133 was not preserved: {raw!r}")

    run("set-option", "-g", "@prompt", "0")
    respawn(
        pane,
        "printf 'x\\033'; sleep 0.05; printf ']junk\\033'; sleep 0.05; "
        "printf ']133;A\\007p'; sleep 30",
    )
    wait_value(pane, "#{@prompt}", "1")
    capture = run("capture-pane", "-p", "-t", pane).rstrip()
    raw = run("capture-pane", "-pR", "-t", pane)
    if capture != "xp" or re.search(
        r"L 0 .*flags=START_PROMPT.*osc133=1,0,0,0,0", raw
    ) is None:
        raise AssertionError(f"replacement OSC was not reprocessed natively: {raw!r}")

    run("set-option", "-g", "@prompt", "0")
    respawn(
        pane,
        "printf 'x\\033]133;A'; sleep 5.5; printf 'p'; sleep 30",
    )
    time.sleep(6)
    capture = run("capture-pane", "-p", "-t", pane).rstrip()
    if capture != "xp" or value(pane, "#{@prompt}") != "0":
        raise AssertionError(f"unterminated OSC did not time out: {capture!r}")

    run("resize-window", "-t", "behaviour", "-x", "20", "-y", "5")
    respawn(
        pane,
        "printf '\\033[?1049htop\\r\\nm\\033]133;A\\007"
        "\\r\\n2\\r\\n3\\r\\n4\\r\\n5'; sleep 30",
    )
    time.sleep(0.3)
    raw = run("capture-pane", "-pR", "-t", pane)
    if re.search(r"L 0 .*flags=START_PROMPT.*osc133=1,0,0,0,0", raw) is None:
        raise AssertionError(f"visible alternate-screen marker did not move: {raw!r}")

    respawn(
        pane,
        "printf '\\033[?1049hm\\033]133;A\\007"
        "\\r\\n1\\r\\n2\\r\\n3\\r\\n4\\r\\n5'; sleep 30",
    )
    time.sleep(0.3)
    raw = run("capture-pane", "-pR", "-t", pane)
    if "START_PROMPT" in raw:
        raise AssertionError(f"scrolled alternate-screen marker remained: {raw!r}")

    run("resize-window", "-t", "behaviour", "-x", "20", "-y", "5")
    respawn(
        pane,
        "printf '123456789012345\\033]133;A\\007\\r\\n'; sleep 30",
    )
    time.sleep(0.2)
    run("resize-window", "-t", "behaviour", "-x", "10", "-y", "5")
    time.sleep(0.2)
    raw = run("capture-pane", "-pR", "-t", pane)
    if re.search(r"L 0 .*flags=.*START_PROMPT.*osc133=15,0,0,0,0", raw) is None:
        raise AssertionError(f"resize moved OSC 133 line metadata: {raw!r}")
    run("resize-window", "-t", "behaviour", "-x", "20", "-y", "8")

    run("set-option", "-g", "history-limit", "3")
    run("set-option", "-g", "ghostty-vt", "on")
    history_pane = run(
        "new-window",
        "-d",
        "-P",
        "-F",
        "#{pane_id}",
        "-t",
        "behaviour:",
        "printf 'm\\033]133;A\\007\\r\\n1\\r\\n2\\r\\n3\\r\\n4'"
        "'\\r\\n5\\r\\n6\\r\\n7\\r\\n8\\r\\n9\\r\\n10\\r\\n11'"
        "'\\r\\n12\\r\\n13\\r\\n14\\r\\n15'; sleep 30",
    ).strip()
    time.sleep(0.3)
    raw = run("capture-pane", "-pR", "-t", history_pane)
    if "START_PROMPT" in raw:
        raise AssertionError(f"pruned history marker was reattached: {raw!r}")
    run("kill-pane", "-t", history_pane)
    run("set-option", "-g", "history-limit", "2000")
    run("set-option", "-g", "ghostty-vt", "off")

    run("set-option", "-g", "@title", "0")
    run(
        "set-hook",
        "-g",
        "pane-title-changed",
        'set -gF @title "#{hook_new_title}"',
    )
    run("set-option", "-p", "-t", pane, "allow-set-title", "off")
    respawn(pane, "printf '\\033]2;blocked\\007'; sleep 30")
    time.sleep(0.2)
    if value(pane, "#{@title}") != "0" or value(pane, "#{pane_title}") == "blocked":
        raise AssertionError("allow-set-title was bypassed")

    run("set-option", "-p", "-t", pane, "allow-set-title", "on")
    respawn(pane, "printf '\\033]2;allowed\\007'; sleep 30")
    wait_value(pane, "#{@title}", "allowed")
    wait_value(pane, "#{pane_title}", "allowed")

    run("set-option", "-g", "@title", "0")
    run(
        "set-hook",
        "-g",
        "pane-title-changed",
        'set -agF @title "|#{hook_new_title}"',
    )
    respawn(
        pane,
        "printf '\\033]2;stackbase\\007\\033[22;0t'"
        "'\\033]2;stacktemp\\007\\033[23;0t'; sleep 30",
    )
    wait_value(pane, "#{@title}", "0|stackbase|stacktemp|stackbase")

    run("set-option", "-g", "@bell", "0")
    run("set-hook", "-g", "pane-bell", "set -g @bell 1")
    respawn(pane, "printf '\\007'; sleep 30")
    wait_value(pane, "#{@bell}", "1")

    respawn(
        pane,
        "printf '\\033[?2026h'; sleep 0.5; "
        "printf '\\033[?2026l'; sleep 30",
    )
    wait_value(pane, "#{synchronized_output_flag}", "1")
    wait_value(pane, "#{synchronized_output_flag}", "0")

    run("set-option", "-p", "-t", pane, "cursor-colour", "#ff0000")
    respawn(
        pane,
        "printf '\\033]12;rgb:00/00/ff\\007'; sleep 1.5; "
        "printf '\\033]112\\007'; sleep 30",
    )
    wait_value(pane, "#{cursor_colour}", "#0000ff")
    run("set-option", "-p", "-t", pane, "cursor-colour", "#00ff00")
    wait_value(pane, "#{cursor_colour}", "#0000ff")
    wait_value(pane, "#{cursor_colour}", "#00ff00")
finally:
    run("kill-server", check=False)
    home.cleanup()
PY
