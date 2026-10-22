#!/bin/sh

# An unanswered window size query must eventually stop extending escape-time,
# both at startup and after a resize.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 -B - "$TEST_TMUX" <<'PY'
import fcntl
import os
import select
import signal
import struct
import subprocess
import sys
import termios
import time

tmux = sys.argv[1]
server = [tmux, "-LtestWinsize%d" % os.getpid(), "-f/dev/null"]
pid = fd = None

def run(*args, check=True):
    p = subprocess.run(server + list(args), capture_output=True)
    if check and p.returncode != 0:
        raise RuntimeError("%r: %s" % (args, p.stderr.decode()))
    return p.stdout.decode().strip()

def pump(seconds):
    output = b""
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if select.select([fd], [], [], min(.01,
            max(0, deadline - time.monotonic())))[0]:
            output += os.read(fd, 65536)
    return output

def wait_query():
    output = b""
    deadline = time.monotonic() + 5
    while b"\033[18t\033[14t" not in output:
        assert time.monotonic() < deadline, "missing window size query"
        output += pump(.01)

def resize(rows, columns):
    fcntl.ioctl(fd, termios.TIOCSWINSZ,
        struct.pack("HHHH", rows, columns, 0, 0))
    os.kill(pid, signal.SIGWINCH)
    wait_query()

def check_escape(context):
    run("set-option", "-g", "@seen", "")
    os.write(fd, b"\033")
    pump(.15)
    os.write(fd, b"\033")
    pump(.15)
    os.write(fd, b"\r")
    pump(.1)
    actual = run("show-option", "-gv", "@seen")
    assert actual == "EER", (context, "expected EER", actual)

try:
    run("new-session", "-d", "-s", "winsize", "-x80", "-y24", "sleep 60")
    run("set-option", "-s", "escape-time", "0")
    run("set-option", "-g", "assume-paste-time", "0")
    for key, label in (("Escape", "E"), ("M-Escape", "M"),
        ("Enter", "R"), ("M-Enter", "N")):
        run("bind-key", "-n", key, "set-option", "-gF", "@seen",
            "#{@seen}" + label)
    pid, fd = os.forkpty()
    if pid == 0:
        os.environ["TERM"] = "xterm-256color"
        fcntl.ioctl(0, termios.TIOCSWINSZ,
            struct.pack("HHHH", 24, 80, 0, 0))
        os.execv(tmux, server + ["attach-session", "-t", "winsize"])
    wait_query()
    # Leave all startup queries unanswered and allow their five second
    # timeouts to expire. The window size query must time out too.
    pump(5.2)
    check_escape("unanswered startup query")

    # A resize starts a new query after the startup timer has already fired.
    resize(25, 81)
    pump(5.2)
    check_escape("unanswered resize query")

    # After a timeout another resize must still be able to query the size.
    # A successful reply must release the delay without waiting five seconds.
    resize(26, 82)
    os.write(fd, b"\033[8;26;82t\033[4;520;820t")
    pump(.1)
    assert run("list-clients", "-F",
        "#{client_cell_width}x#{client_cell_height}") == "10x20"
    check_escape("answered resize query")

    # Disconnect while a query is pending. Its timer must not outlive the
    # client, and the server must continue to run after the timeout.
    resize(27, 83)
    run("detach-client")
    os.close(fd)
    fd = None
    os.waitpid(pid, 0)
    pid = None
    time.sleep(5.2)
    run("has-session", "-t", "winsize")
finally:
    run("kill-server", check=False)
    if fd is not None:
        os.close(fd)
    if pid is not None:
        os.waitpid(pid, 0)
PY
