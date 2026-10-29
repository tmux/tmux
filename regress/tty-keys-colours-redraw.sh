#!/bin/sh

# A delayed OSC 11 reply must redraw theme colours without a status timer,
# window rename, pane output or another command causing the redraw.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 -B - "$TEST_TMUX" <<'PY'
import fcntl
import os
import select
import struct
import subprocess
import sys
import termios
import time

tmux = sys.argv[1]
server = [tmux, "-LtestColours%d" % os.getpid(), "-f/dev/null"]

def run(*args, check=True):
    p = subprocess.run(server + list(args), capture_output=True)
    if check and p.returncode != 0:
        raise RuntimeError("%r: %s" % (args, p.stderr.decode()))

def read_output(timeout):
    output = b""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        remaining = max(0, deadline - time.monotonic())
        if select.select([fd], [], [], min(.05, remaining))[0]:
            output += os.read(fd, 65536)
    return output

def expect_redraw(colour, context):
    output = b""
    deadline = time.monotonic() + 3
    while colour not in output or b"THEME" not in output:
        assert time.monotonic() < deadline, (context, "missing redraw", output)
        output += read_output(.05)
    # Let the redraw finish before sending another report.
    read_output(.2)

pid = fd = None
try:
    run("new-session", "-d", "-s", "colours", "-x", "80", "-y", "24",
        "sleep 100")
    run("set-option", "-s", "theme", "detect")
    run("set-option", "-s", "dark-theme-dark-grey",
        "#{?#{==:#{client_active_theme},dark},red,blue}")
    run("set-option", "-s", "light-theme-dark-grey",
        "#{?#{==:#{client_active_theme},light},green,blue}")
    run("set-option", "-g", "automatic-rename", "off")
    run("set-option", "-g", "status-interval", "0")
    run("set-option", "-g", "status-style", "bg=themedarkgrey,fg=white")
    run("set-option", "-g", "status-format[0]", "THEME")
    pid, fd = os.forkpty()
    if pid == 0:
        os.environ["TERM"] = "xterm-256color"
        os.environ.pop("TMUX", None)
        fcntl.ioctl(0, termios.TIOCSWINSZ,
            struct.pack("HHHH", 24, 80, 0, 0))
        os.execv(tmux, server + ["attach-session", "-t", "colours"])

    expect_redraw(b"\033[41m", "initial dark theme")
    # No DSR 996 response is sent: OSC 11 alone must update the theme.
    os.write(fd, b"\033]11;rgb:ffff/ffff/ffff\033\\")
    expect_redraw(b"\033[42m", "delayed light background (ST)")
    os.write(fd, b"\033]11;rgb:0000/0000/0000\007")
    expect_redraw(b"\033[41m", "dark background (BEL)")

    # An unchanged background and a foreground reply need no redraw.
    for report in (b"\033]11;rgb:0000/0000/0000\007",
                   b"\033]10;rgb:ffff/ffff/ffff\007"):
        os.write(fd, report)
        output = read_output(.3)
        assert b"THEME" not in output, ("unnecessary redraw", report, output)
finally:
    run("kill-server", check=False)
    if fd is not None:
        os.close(fd)
    if pid is not None:
        os.waitpid(pid, 0)
PY
