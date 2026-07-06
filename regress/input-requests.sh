#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 - "$TEST_TMUX" <<'PY'
import os
import select
import signal
import subprocess
import sys
import tempfile
import time

tmux = sys.argv[1]
label = "testA%d" % os.getpid()
server = [tmux, "-L" + label, "-f/dev/null"]

def run(*args, check=True):
    return subprocess.run(server + list(args), check=check,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)

def attach():
    pid, fd = os.forkpty()
    if pid == 0:
        os.environ["TERM"] = "xterm-256color"
        os.execl(tmux, tmux, "-L" + label, "-f/dev/null", "attach-session",
            "-t", "requests")
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
            except BlockingIOError:
                chunk = b""
            if chunk == b"":
                continue
            data += chunk
            if needle in data:
                return data
    raise RuntimeError("did not see terminal request %r in %r" %
        (needle, data))

def wait_file(path, timeout=5):
    end = time.time() + timeout
    while time.time() < end:
        try:
            with open(path, "rb") as f:
                data = f.read()
            if data:
                return data
        except FileNotFoundError:
            pass
        time.sleep(0.05)
    return b""

def respawn(command):
    run("respawn-window", "-k", "-t", "requests:0", command)
    time.sleep(0.2)

def cleanup(pid=None):
    if pid is not None:
        try:
            os.kill(pid, signal.SIGHUP)
        except ProcessLookupError:
            pass
    run("kill-server", check=False)

run("kill-server", check=False)
run("new-session", "-d", "-x", "80", "-y", "24", "-s", "requests",
    "sleep 60")

pid, fd = attach()
try:
    time.sleep(0.5)

    with tempfile.NamedTemporaryFile(delete=False) as f:
        palette_out = f.name
    respawn("stty raw -echo min 1 time 50; "
        "printf '\\033]4;99;?\\033\\\\'; "
        "dd bs=1 count=27 2>/dev/null | cat -v >%s; sleep 1" %
        palette_out)
    read_until(fd, b"\033]4;99;?\033\\")
    os.write(fd, b"\033]4;99;rgb:0101/0202/0303\033\\")
    got = wait_file(palette_out)
    expected = b"^[]4;99;rgb:0101/0202/0303^[\\"
    if got != expected:
        raise AssertionError("palette reply: expected %r got %r" %
            (expected, got))

    run("set-option", "-s", "set-clipboard", "on")
    run("set-option", "-s", "get-clipboard", "request")
    with tempfile.NamedTemporaryFile(delete=False) as f:
        clip_out = f.name
    respawn("stty raw -echo min 1 time 50; "
        "printf '\\033]52;c;?\\033\\\\'; "
        "dd bs=1 count=21 2>/dev/null | cat -v >%s; sleep 1" %
        clip_out)
    data = read_until(fd, b"]52;")
    if b"?" not in data:
        raise RuntimeError("clipboard request missing query in %r" % data)
    os.write(fd, b"\033]52;c;UmVxdWVzdA==\033\\")
    got = wait_file(clip_out)
    expected = b"^[]52;c;UmVxdWVzdA==^[\\"
    if got != expected:
        raise AssertionError("clipboard reply: expected %r got %r" %
            (expected, got))
finally:
    cleanup(pid)
PY
