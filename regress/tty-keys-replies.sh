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
import re
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
    if name in ("XDA-kitty", "XDA-kitty-nosync"):
        required = {
            "256", "RGB", "bpaste", "clipboard", "mouse",
            "strikethrough", "title", "ccolour", "cstyle", "extkeys",
            "focus", "hyperlinks", "osc7", "usstyle", "kittygraphics"}
        if name == "XDA-kitty":
            required.add("sync")
        elif "sync" in features:
            return False
        return kind == "kitty(0.43.0)" and required <= features
    if name == "XDA-ghostty":
        return kind == "ghostty 1.2.0" and {
            "kittygraphics", "margins", "sync", "usstyle", "progressbar"} <= features
    if name == "XDA-WezTerm":
        return kind == "WezTerm 20240203-110809-5046fc22" and {
            "sixel", "margins", "usstyle"} <= features
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
    ("XDA-kitty", b"\033P>|kitty(0.43.0)\033\\"),
    ("XDA-kitty-nosync", b"\033P>|kitty(0.43.0)\033\\"),
    ("XDA-ghostty", b"\033P>|ghostty 1.2.0\033\\"),
    ("XDA-WezTerm", b"\033P>|WezTerm 20240203-110809-5046fc22\033\\"),
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
                if name == "pixels":
                    # Draw the window cell size so it can be read from the
                    # client's output without running another command, since
                    # each command's client recalculates the window sizes.
                    run("set-option", "-g", "status", "on")
                    run("set-option", "-g", "status-format[0]",
                        "W=#{window_cell_width}x#{window_cell_height}")
                run("set-option", "-g", "assume-paste-time", "0")
                if delivery == "expired":
                    run("set-option", "-s", "escape-time", "100")
                else:
                    run("set-option", "-s", "escape-time", "60000")
                run("set-option", "-g", "@seen", "")
                if name == "XDA-kitty-nosync":
                    run("set-option", "-as", "terminal-features", ",*:sync@")
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
                output = b""
                for part in chunks[:-1]:
                    os.write(fd, part)
                    time.sleep(.01)
                    if delivery != "bytewise":
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
                if name == "pixels":
                    # With no command run since the reply, the window must
                    # have followed the client's new cell size by itself.
                    time.sleep(.3)
                    while select.select([fd], [], [], .2)[0]:
                        output += os.read(fd, 65536)
                    drawn = re.findall(rb"W=(\d+x\d+)", output)
                    assert drawn and drawn[-1] == b"10x20", (
                        context, "window cell size", drawn)
                deadline = time.monotonic() + 5
                while run("capture-pane", "-p") != expected:
                    assert time.monotonic() < deadline, (context,
                        "reply leaked or input stalled", run("capture-pane", "-p"))
                    if select.select([fd], [], [], .01)[0]:
                        output += os.read(fd, 65536)
                actual = state()
                if delivery in ("invalid", "expired"):
                    assert actual == baseline, (context, "unexpected effect", actual)
                else:
                    assert matches(name, actual), (context, "missing effect", actual)
                    if name in ("XDA-kitty", "XDA-kitty-nosync", "XDA-ghostty"):
                        assert run("display-message", "-p", "#{I/f:kittygraphics}") == "1", (
                            context, "kittygraphics feature lookup failed")
                    if name == "XDA-kitty-nosync":
                        assert run("display-message", "-p", "#{I/f:sync}") == "0", (
                            context, "kitty enabled synchronized updates")
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
