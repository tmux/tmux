#!/bin/sh

# Theme hooks and client_active_theme follow the selected tmux theme, while
# client_theme continues to describe only the terminal's DSR 996 reports.

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
server = [tmux, "-LtestTheme%d" % os.getpid(), "-f/dev/null"]

def run(*args, check=True):
    p = subprocess.run(server + list(args), capture_output=True, timeout=5)
    if check and p.returncode != 0:
        raise RuntimeError("%r: %s" % (args, p.stderr.decode()))
    return p.stdout.decode().strip()

def drain(timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if select.select([fd], [], [], .01)[0]:
            os.read(fd, 65536)

def expect(active, reported, hooks):
    expected = "%s:%s:%s" % (active, reported, hooks)
    deadline = time.monotonic() + 5
    while True:
        actual = run("list-clients", "-F",
            "#{client_active_theme}:#{client_theme}:#{@theme_hooks}")
        if actual == expected:
            return
        assert time.monotonic() < deadline, (expected, actual)
        drain(.02)

def report(data, active, reported, hooks):
    os.write(fd, data)
    # Also allow unchanged reports and any extra hook commands to be processed.
    drain(.1)
    expect(active, reported, hooks)

pid = fd = None
try:
    run("new-session", "-d", "-s", "theme", "-x", "80", "-y", "24",
        "sleep 100")
    run("set-option", "-g", "@theme_hooks", "")
    for theme, letter in (("light", "L"), ("dark", "D")):
        # Changing theme colour options inside a hook must not fire it again.
        run("set-hook", "-t", "theme", "client-%s-theme" % theme,
            "set -gF @theme_hooks '#{@theme_hooks}%s' ; "
            "set -s %s-theme-green colour2" % (letter, theme))
    # The target session's hooks must be used even if another session is more
    # active when the terminal opens, before the client has a session.
    run("new-session", "-d", "-s", "other", "sleep 100")
    pid, fd = os.forkpty()
    if pid == 0:
        os.environ["TERM"] = "xterm-256color"
        os.environ.pop("TMUX", None)
        fcntl.ioctl(0, termios.TIOCSWINSZ,
            struct.pack("HHHH", 24, 80, 0, 0))
        # Options may already have populated active_theme before attachment.
        os.execv(tmux, server + ["set-option", "-s", "dark-theme-green",
            "colour2", ";", "attach-session", "-t", "theme"])

    # The initial fallback is dark, even before a terminal reply arrives.
    expect("dark", "", "D")
    report(b"\033]11;rgb:ffff/ffff/ffff\033\\", "light", "", "DL")
    report(b"\033]11;rgb:eeee/eeee/eeee\007", "light", "", "DL")
    report(b"\033]11;rgb:0000/0000/0000\007", "dark", "", "DLD")
    report(b"\033]11;rgb:0000/0000/0000\007", "dark", "", "DLD")
    report(b"\033]10;rgb:ffff/ffff/ffff\007", "dark", "", "DLD")

    # Explicit terminal reports take precedence over the background. Repeated
    # reports and a different background must not fire duplicate hooks.
    report(b"\033[?997;2n", "light", "light", "DLDL")
    report(b"\033[?997;2n", "light", "light", "DLDL")
    report(b"\033]11;rgb:1111/1111/1111\007", "light", "light", "DLDL")
    run("set-option", "-s", "theme", "dark")
    expect("dark", "light", "DLDLD")
    report(b"\033[?997;1n", "dark", "dark", "DLDLD")
    run("set-option", "-s", "theme", "light")
    expect("light", "dark", "DLDLDL")

    # Terminal ANSI colours have no active light/dark theme. Incoming reports
    # still update client_theme, but cannot change the active theme or hooks.
    run("set-option", "-s", "theme", "terminal")
    expect("", "dark", "DLDLDL")
    report(b"\033[?997;2n", "", "light", "DLDLDL")
    report(b"\033]11;rgb:2222/2222/2222\007", "", "light", "DLDLDL")
    run("set-option", "-s", "theme", "detect")
    expect("light", "light", "DLDLDLL")
    run("set-option", "-s", "theme", "dark")
    expect("dark", "light", "DLDLDLLD")
    run("set-option", "-s", "theme", "detect")
    expect("light", "light", "DLDLDLLDL")
    run("set-option", "-s", "light-theme-green", "colour3")
    expect("light", "light", "DLDLDLLDL")
finally:
    run("kill-server", check=False)
    if fd is not None:
        os.close(fd)
    if pid is not None:
        os.waitpid(pid, 0)
PY
