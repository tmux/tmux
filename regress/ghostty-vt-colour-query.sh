#!/bin/sh

PATH=/bin:/usr/bin
TERM=xterm-256color

# The regular regress build does not include the Zig backend.
[ -z "$TEST_TMUX" ] && exit 0

python3 - "$TEST_TMUX" <<'PY'
import os
import select
import shlex
import signal
import subprocess
import sys
import tempfile
import time

tmux = sys.argv[1]
label = f"ghostty-colour-{os.getpid()}"
server = [tmux, "-L", label]
home = tempfile.TemporaryDirectory()
env = os.environ.copy()
env["HOME"] = home.name
env["TERM"] = "xterm-256color"
temporary_paths = []


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
    return result


def attach():
    pid, fd = os.forkpty()
    if pid == 0:
        os.execve(
            tmux,
            [tmux, "-L", label, "attach-session", "-t", "colour-query"],
            env,
        )
    os.set_blocking(fd, False)
    return pid, fd


def drain(fd):
    while True:
        ready, _, _ = select.select([fd], [], [], 0)
        if fd not in ready:
            return
        try:
            if not os.read(fd, 4096):
                return
        except BlockingIOError:
            return


def wait_client(timeout=5):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        result = run(
            "list-clients",
            "-F",
            "#{client_session}",
            check=False,
        )
        if b"colour-query" in result.stdout.splitlines():
            return
        time.sleep(0.05)
    raise RuntimeError("timed out waiting for attached client")


def read_until(fd, needle, timeout=5):
    end = time.monotonic() + timeout
    data = b""
    while time.monotonic() < end:
        ready, _, _ = select.select([fd], [], [], 0.05)
        if fd not in ready:
            continue
        try:
            chunk = os.read(fd, 4096)
        except BlockingIOError:
            continue
        if not chunk:
            continue
        data += chunk
        if needle in data:
            return data
    raise RuntimeError("did not see terminal request %r in %r" % (needle, data))


def wait_path(path, timeout=5):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        if os.path.exists(path):
            return
        time.sleep(0.05)
    raise RuntimeError("timed out waiting for %s" % path)


def temporary_path():
    fd, path = tempfile.mkstemp()
    os.close(fd)
    temporary_paths.append(path)
    os.unlink(path)
    return path


def cleanup(pid=None):
    if pid is not None:
        try:
            os.kill(pid, signal.SIGHUP)
        except ProcessLookupError:
            pass
    run("kill-server", check=False)
    for path in temporary_paths:
        try:
            os.unlink(path)
        except FileNotFoundError:
            pass
    home.cleanup()


run("kill-server", check=False)
window = run(
    "new-session",
    "-d",
    "-P",
    "-F",
    "#{window_id}",
    "-x",
    "80",
    "-y",
    "24",
    "-s",
    "colour-query",
    "sleep 60",
).stdout.decode().strip()
run("set-option", "-w", "-t", window, "ghostty-vt", "on")
run("set-option", "-w", "-t", window, "pane-colours[1]", "#010203")

pid, fd = attach()
try:
    wait_client()
    drain(fd)

    reply_path = temporary_path()
    extra_path = temporary_path()

    # Set the dynamic foreground in a separate write so both terminal models
    # have synchronized before querying it. Index 1 is known locally, index 2
    # is set earlier in the same OSC, and index 99 must be requested from the
    # attached client. Every later reply must remain ordered behind that
    # asynchronous request.
    command = (
        "stty raw -echo min 1 time 50; "
        "printf '\\033]10;rgb:55/66/77\\007\\033[5n'; "
        "dd bs=1 count=4 2>/dev/null >/dev/null; "
        "printf '\\033]4;99;?;260;?;1;?;2;rgb:22/33/44;2;?\\007"
        "\\033]10;?\\007\\033[5n'; "
        "dd bs=1 count=104 2>/dev/null | cat -v >%s; "
        "stty min 0 time 5; "
        "dd bs=128 count=1 2>/dev/null | cat -v >%s"
        % (shlex.quote(reply_path), shlex.quote(extra_path))
    )
    run("respawn-window", "-k", "-t", window, command)

    query = b"\033]4;99;?\033\\"
    read_until(fd, query)
    os.write(fd, b"\033]4;99;rgb:1111/2222/3333\033\\")

    wait_path(reply_path)
    wait_path(extra_path)
    with open(reply_path, "rb") as f:
        got = f.read()
    expected = (
        b"^[]4;99;rgb:1111/2222/3333^G"
        b"^[]4;1;rgb:0101/0202/0303^G"
        b"^[]4;2;rgb:2222/3333/4444^G"
        b"^[]10;rgb:5555/6666/7777^G"
        b"^[[0n"
    )
    if got != expected:
        raise AssertionError("expected %r, got %r" % (expected, got))

    with open(extra_path, "rb") as f:
        extra = f.read()
    if extra:
        raise AssertionError("unexpected duplicate response: %r" % extra)
finally:
    cleanup(pid)
PY
