#!/bin/sh

# Check replies are dropped once a pane has died: a clipboard request still
# waiting for the terminal when the pane exits with remain-on-exit, and a
# reply queued behind it, must not be written after the pane's buffer is
# freed, whether the request times out or the terminal answers late. Also
# check a clipboard query in view mode (which has no pane) is ignored.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 - "$TEST_TMUX" <<'PY'
import os
import select
import signal
import subprocess
import sys
import time

tmux = sys.argv[1]
label = "test%d" % os.getpid()
server = [tmux, "-L" + label, "-f/dev/null"]

def run(*args, check=True):
    return subprocess.run(server + list(args), check=check,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)

def attach():
    pid, fd = os.forkpty()
    if pid == 0:
        os.environ["TERM"] = "xterm-256color"
        os.execl(tmux, tmux, "-L" + label, "-f/dev/null", "attach-session",
            "-t", "dead")
    os.set_blocking(fd, False)
    return pid, fd

def read_until(fd, needle, timeout=5):
    end = time.time() + timeout
    data = b""
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.05)
        if fd in r:
            try:
                chunk = os.read(fd, 4096)
            except (BlockingIOError, OSError):
                chunk = b""
            data += chunk
            if needle in data:
                return data
    raise RuntimeError("did not see %r in %r" % (needle, data))

def drain(fd):
    while True:
        r, _, _ = select.select([fd], [], [], 0.05)
        if fd not in r:
            return
        try:
            if os.read(fd, 4096) == b"":
                return
        except (BlockingIOError, OSError):
            return

def wait_dead():
    end = time.monotonic() + 5
    while run("display-message", "-p", "-t", "dead:0",
            "#{pane_dead}").stdout.strip() != b"1":
        assert time.monotonic() < end, "pane did not die"
        time.sleep(.01)

def alive(context):
    out = run("display-message", "-p", "-t", "dead:0", "#{pane_dead}",
        check=False)
    assert out.returncode == 0 and out.stdout.strip() == b"1", (context,
        "server is gone", out.stderr)

def request(command):
    # The clipboard query waits for the terminal, so the device attributes
    # reply is queued behind it; then the pane exits.
    drain(fd)
    run("respawn-pane", "-k", "-t", "dead:0", command)
    read_until(fd, b"]52;")
    wait_dead()

run("kill-server", check=False)
run("new-session", "-d", "-x", "80", "-y", "24", "-s", "dead", "sleep 60")
pid, fd = attach()
try:
    time.sleep(0.5)
    run("set-option", "-g", "remain-on-exit", "on")
    run("set-option", "-s", "set-clipboard", "on")
    run("set-option", "-s", "get-clipboard", "request")
    query = "printf '\\033]52;c;?\\033\\\\\\033[c'"

    # The terminal never answers, so the request times out and the queued
    # reply is sent.
    request(query)
    time.sleep(1)
    alive("timeout")

    # The terminal answers after the pane has died. With get-clipboard both,
    # the answer is also added as a buffer, which shows the request was still
    # waiting; if it timed out before the answer arrived, try again.
    run("set-option", "-s", "get-clipboard", "both")
    for attempt in range(5):
        request(query)
        os.write(fd, b"\033]52;c;UmVwbHk=\033\\")
        end = time.monotonic() + 1
        while time.monotonic() < end:
            alive("late reply")
            if run("show-buffer", check=False).stdout == b"Reply":
                break
            time.sleep(.05)
        else:
            continue
        break
    else:
        raise RuntimeError("late reply was never handled")
    time.sleep(.5)
    alive("late reply")

    # View mode has no pane to reply to.
    run("set-option", "-s", "get-clipboard", "buffer")
    run("run-shell", "-t", "dead:0", "printf '\\033]52;c;?\\007'")
    alive("view mode")
finally:
    try:
        os.kill(pid, signal.SIGHUP)
    except ProcessLookupError:
        pass
    run("kill-server", check=False)
    os.waitpid(pid, 0)
    os.close(fd)
PY
