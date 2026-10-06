#!/bin/sh

# Verify palette and clipboard payload delivery with both OSC terminators,
# fragmented replies and a separate Escape before the reply. Invalid payloads
# must leave the request pending for a subsequent valid reply. Check that
# foreground and background reports update the colours queried by a pane.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

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

def wait_file(path, length, timeout=5):
    end = time.time() + timeout
    while time.time() < end:
        try:
            with open(path, "rb") as f:
                data = f.read()
            if len(data) >= length:
                return data
        except FileNotFoundError:
            pass
        time.sleep(0.05)
    return b""

def respawn(command):
    run("respawn-window", "-k", "-t", "requests:0", command)
    time.sleep(0.2)

def wait_pane(expected, context):
    end = time.monotonic() + 5
    while run("capture-pane", "-p").stdout.strip() != expected:
        assert time.monotonic() < end, (context, "unexpected pane bytes",
            run("capture-pane", "-p").stdout.strip())
        time.sleep(.01)

def cleanup(pid=None):
    if pid is not None:
        try:
            os.kill(pid, signal.SIGHUP)
        except ProcessLookupError:
            pass
    run("kill-server", check=False)
    if pid is not None:
        os.waitpid(pid, 0)

run("kill-server", check=False)
run("new-session", "-d", "-x", "80", "-y", "24", "-s", "requests",
    "sleep 60")

pid, fd = attach()
try:
    time.sleep(0.5)

    run("set-option", "-g", "assume-paste-time", "0")
    run("set-option", "-s", "escape-time", "1000")
    run("set-option", "-g", "@seen", "")
    run("bind-key", "-n", "Escape", "set-option", "-gF", "@seen",
        "#{@seen}E")
    run("set-option", "-s", "set-clipboard", "on")
    run("set-option", "-s", "get-clipboard", "request")
    expected_escapes = b""
    replies = [
        ("palette", "\\033]4;99;?\\033\\\\", b"\033]4;99;?\033\\",
            b"\033]4;99;rgb:0101/0202/0303", b"\033]4;99;invalid"),
        ("clipboard", "\\033]52;c;?\\033\\\\", b"]52;",
            b"\033]52;c;UmVxdWVzdA==", b"\033]52;c;!!!!"),
    ]
    with tempfile.TemporaryDirectory() as directory:
        output = os.path.join(directory, "reply")
        for name, query, needle, payload, invalid in replies:
            for prefixed in (False, True):
                for split in (False, True):
                    for terminator in (b"\007", b"\033\\"):
                        context = (name, prefixed, split, terminator)
                        # Replies to the pane use its request's ST terminator,
                        # regardless of the terminal reply's terminator.
                        expected = payload + b"\033\\"
                        respawn("stty raw -echo min 1 time 50; "
                            "printf '%s'; dd bs=1 count=%d 2>/dev/null >%s; "
                            "exec cat -v" %
                            (query, len(expected), shlex.quote(output)))
                        data = read_until(fd, needle)
                        assert b"?" in data, (context, "query missing", data)

                        os.write(fd, invalid + terminator)
                        time.sleep(.05)
                        with open(output, "rb") as f:
                            assert f.read() == b"", (context, "invalid payload delivered")

                        wire = (b"\033" if prefixed else b"") + payload + terminator
                        if split:
                            os.write(fd, wire[:-1])
                            time.sleep(.05)
                            with open(output, "rb") as f:
                                assert f.read() == b"", (context, "partial payload delivered")
                            os.write(fd, wire[-1:])
                        else:
                            os.write(fd, wire)
                        got = wait_file(output, len(expected))
                        assert got == expected, (context, expected, got)
                        if prefixed:
                            expected_escapes += b"E"
                        seen = run("show-option", "-gv", "@seen").stdout.strip()
                        assert seen == expected_escapes, (context, "Escape count", seen)

                        # Check that decoding did not deliver the reply twice
                        # or leak raw bytes after delivering the payload.
                        os.write(fd, b"Z")
                        wait_pane(b"Z", context)

        colour_value = 1
        for colour in (10, 11):
            for prefixed in (False, True):
                for terminator in (b"\007", b"\033\\"):
                    context = ("colour", colour, prefixed, terminator)
                    # A different colour each time prevents an earlier
                    # successful report from masking a later ignored one.
                    payload = b"\033]%d;rgb:%04x/0202/0303" % (
                        colour, colour_value * 257)
                    colour_value += 1
                    wire = (b"\033" if prefixed else b"") + payload + terminator
                    if prefixed:
                        os.write(fd, wire[:-1])
                        time.sleep(.05)
                        assert run("capture-pane", "-p").stdout.strip() == b"Z", (
                            context, "partial reply leaked")
                        os.write(fd, wire[-1:] + b"X")
                        expected_escapes += b"E"
                    else:
                        os.write(fd, wire + b"X")
                    wait_pane(b"ZX", context)
                    seen = run("show-option", "-gv", "@seen").stdout.strip()
                    assert seen == expected_escapes, (context, "Escape count", seen)

                    expected = payload + b"\033\\"
                    respawn("stty raw -echo min 1 time 50; "
                        "printf '\\033]%d;?\\033\\\\'; "
                        "dd bs=1 count=%d 2>/dev/null >%s; exec cat -v" %
                        (colour, len(expected), shlex.quote(output)))
                    got = wait_file(output, len(expected))
                    assert got == expected, (context, "colour not applied", got)
                    os.write(fd, b"Z")
                    wait_pane(b"Z", context)
finally:
    cleanup(pid)
    os.close(fd)
PY
