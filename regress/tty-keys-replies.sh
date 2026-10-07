#!/bin/sh

# Test terminal reply state changes, including replies fragmented at every
# byte and replies preceded by a separate Escape. Use a PTY directly so that
# an outer tmux cannot answer startup queries before the test does.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 - "$TEST_TMUX" <<'PY'
import fcntl
import os
import select
import struct
import subprocess
import sys
import termios
import time

tmux = sys.argv[1]
server = [tmux, "", "-f/dev/null"]

def run(*args, check=True):
    p = subprocess.run(server + list(args), capture_output=True)
    if check and p.returncode != 0:
        raise RuntimeError("%r: %s" % (args, p.stderr.decode()))
    return p.stdout.decode().strip()

def state():
    return run("list-clients", "-F",
        "#{client_termfeatures}|#{client_termtype}|"
        "#{client_width}x#{client_height}|"
        "#{client_cell_width}x#{client_cell_height}")

def matches(name, value):
    features, kind, size, cells = value.split("|")
    features = set(features.split(","))
    if name == "DA":
        return {"sixel", "margins", "rectfill", "clipboard"} <= features
    if name == "DA2":
        return "ignorefkeys" in features
    if name == "XDA":
        return kind == "ReplyTest"
    if name == "sync":
        return "sync" in features
    if name == "characters":
        return size == "61x17"
    if name == "pixels":
        return cells == "10x20"

reports = [
    ("DA", b"\033[?65;4;21;28;52c"),
    ("DA2", b"\033[>85;1;0c"),
    ("XDA", b"\033P>|ReplyTest\033\\"),
    ("sync", b"\033[?2026;2$y"),
    ("characters", b"\033[8;17;61t"),
    ("pixels", b"\033[4;480;800t"),
]
cases = 0
for name, report in reports:
    for prefixed in (False, True):
        deliveries = ["whole", "split", "bytewise"]
        if name == "sync" and not prefixed:
            deliveries += ["invalid", "expired"]
        for delivery in deliveries:
            context = (name, prefixed, delivery)
            pid = fd = None
            # Each case needs fresh one-shot startup reply flags. A new
            # socket also avoids racing shutdown of the previous server.
            server[1] = "-LtestReplies%d-%d" % (os.getpid(), cases)
            try:
                run("new-session", "-d", "-s", "replies", "-x", "80",
                    "-y", "24", "stty raw -echo; exec cat -v")
                run("set-option", "-g", "status", "off")
                run("set-option", "-g", "assume-paste-time", "0")
                if delivery == "expired":
                    run("set-option", "-s", "escape-time", "100")
                else:
                    run("set-option", "-s", "escape-time", "5000")
                run("set-option", "-g", "@seen", "")
                run("bind-key", "-n", "Escape", "set-option", "-gF",
                    "@seen", "#{@seen}E")
                pid, fd = os.forkpty()
                if pid == 0:
                    os.environ["TERM"] = "xterm-256color"
                    fcntl.ioctl(0, termios.TIOCSWINSZ,
                        struct.pack("HHHH", 24, 80, 0, 0))
                    os.execv(tmux, server + ["attach-session", "-t", "replies"])
                output = b""
                deadline = time.monotonic() + 5
                while b"\033[?2026$p" not in output:
                    assert time.monotonic() < deadline, (context, "missing query")
                    if select.select([fd], [], [], .01)[0]:
                        output += os.read(fd, 65536)
                baseline = state()
                assert not matches(name, baseline), (context, baseline)
                wire = (b"\033" if prefixed else b"") + report
                if delivery == "invalid":
                    # A near match must fall back to ordinary key parsing.
                    wire = b"\033[?2026;5$y"
                if delivery in ("whole", "invalid"):
                    chunks = [wire]
                elif delivery in ("split", "expired"):
                    chunks = [wire[:-1], wire[-1:]]
                else:
                    chunks = [wire[i:i+1] for i in range(len(wire))]
                for part in chunks[:-1]:
                    os.write(fd, part)
                    time.sleep(.01)
                    actual = state()
                    assert actual == baseline, (context, "premature effect", actual)
                    assert run("capture-pane", "-p") == "", (context, "partial leak")
                expected = "Z"
                if delivery == "expired":
                    # Allow the partial reply's timer to expire. The pending
                    # query extends it to at least 500ms; wait for fallback
                    # rather than assuming a fixed expiration time.
                    expected = wire[:-1].replace(b"\033", b"^[").decode()
                    deadline = time.monotonic() + 5
                    while run("capture-pane", "-p") != expected:
                        assert time.monotonic() < deadline, (context, "timer stalled")
                        if select.select([fd], [], [], .01)[0]:
                            os.read(fd, 65536)
                if delivery in ("invalid", "expired"):
                    expected = wire.replace(b"\033", b"^[").decode() + "Z"
                # A trailing key confirms that the entire reply has been
                # processed, including cases with no extra Escape.
                os.write(fd, chunks[-1] + b"Z")
                deadline = time.monotonic() + 5
                while run("capture-pane", "-p") != expected:
                    assert time.monotonic() < deadline, (context,
                        "reply leaked or input stalled", run("capture-pane", "-p"))
                    if select.select([fd], [], [], .01)[0]:
                        os.read(fd, 65536)
                actual = state()
                if delivery in ("invalid", "expired"):
                    assert actual == baseline, (context, "unexpected effect", actual)
                else:
                    assert matches(name, actual), (context, "missing effect", actual)
                assert run("show-option", "-gv", "@seen") == (
                    "E" if prefixed else ""), (context, "Escape count")
                cases += 1
            finally:
                run("kill-server", check=False)
                if fd is not None:
                    os.close(fd)
                if pid is not None:
                    os.waitpid(pid, 0)
PY
