#!/bin/sh

# Test feature overrides and keyboard push/pop across option changes and detach.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101

python3 -B - "$TEST_TMUX" <<'PY'
import errno
import fcntl
import os
import select
import struct
import subprocess
import sys
import termios
import time

tmux = sys.argv[1]


# Run a tmux command against the current private server.
def run(*args, check=True):
    p = subprocess.run(server + list(args), capture_output=True, timeout=10)
    if check and p.returncode != 0:
        raise RuntimeError((args, p.stderr.decode()))
    value = p.stdout.decode().strip()
    return value


# Read available terminal output while waiting for an observable condition.
def wait_for(predicate):
    deadline = time.monotonic() + 10
    while True:
        if select.select([fd], [], [], .01)[0]:
            output.extend(os.read(fd, 65536))
        if predicate():
            return
        assert time.monotonic() < deadline, (case, "timed out", bytes(output[-256:]))


# Read the negotiated mode for the sole attached client.
def mode():
    value = run("list-clients", "-F", "#{client_key_mode}")
    return value


# The sixel feature confirms that primary DA has been applied.
def primary_da_applied():
    features = run("list-clients", "-F", "#{client_termfeatures}")
    return "sixel" in features.split(",")


for case in ("detect", "enable-later", "disabled", "forced"):
    server = [tmux, "-Lkn%d-%s" % (os.getpid(), case), "-f/dev/null"]
    pid = fd = None
    try:
        run("new-session", "-d", "-s", "keyboard", "-x", "80", "-y", "24",
            "sleep 60")
        run("set-option", "-s", "extended-keys",
            "off" if case == "enable-later" else "on")
        run("set-option", "-as", "terminal-features", ",*:extkeys")
        if case in ("disabled", "forced"):
            feature = "kittykeys@" if case == "disabled" else "kittykeys"
            run("set-option", "-as", "terminal-features", ",*:" + feature)

        pid, fd = os.forkpty()
        if pid == 0:
            os.environ["TERM"] = "xterm-256color"
            fcntl.ioctl(0, termios.TIOCSWINSZ,
                struct.pack("HHHH", 24, 80, 0, 0))
            os.execv(tmux, server + ["attach-session", "-t", "keyboard"])
        output = bytearray()
        wait_for(lambda: b"\033[c" in output)
        if case == "enable-later":
            assert b"\033[?u" not in output, (case, "unexpected probe")
            os.write(fd, b"\033[?65;4c")
            wait_for(primary_da_applied)
            wait_for(lambda: mode() == "VT10x")
            output.clear()
            run("set-option", "-s", "extended-keys", "on")
            wait_for(lambda: b"\033[?u" in output)
        else:
            assert b"\033[?u" in output, (case, "missing probe")

        # The forced feature works even when no keyboard reply is received.
        if case == "forced":
            os.write(fd, b"\033[?65;4c")
        else:
            os.write(fd, b"\033[?8u\033[?65;4c")
        wait_for(primary_da_applied)
        wanted = "Ext" if case == "disabled" else "Kitty 1"
        wait_for(lambda: mode() == wanted)
        if case == "disabled":
            assert b"\033[>1u" not in output, (case, "unexpected push")
        else:
            wait_for(lambda: b"\033[>1u" in output)
            assert output.count(b"\033[>1u") == 1, (case, "extra push")

            # Turning the option off pops the terminal's saved mode once.
            output.clear()
            run("set-option", "-s", "extended-keys", "off")
            wait_for(lambda: mode() == "VT10x (Kitty)")
            wait_for(lambda: b"\033[<u" in output)
            assert output.count(b"\033[<u") == 1, (case, "extra pop")
            output.clear()
            run("set-option", "-s", "extended-keys", "on")
            wait_for(lambda: mode() == "Kitty 1")
            wait_for(lambda: b"\033[>1u" in output)
            assert b"\033[?u" not in output, (case, "repeated probe")

        # Detach restores the terminal's mode and leaves the session alive.
        output.clear()
        run("detach-client")
        while True:
            if not select.select([fd], [], [], 10)[0]:
                raise AssertionError((case, "client did not detach"))
            try:
                chunk = os.read(fd, 65536)
            except OSError as error:
                if error.errno != errno.EIO:
                    raise
                break
            if not chunk:
                break
            output.extend(chunk)
        expected = 0 if case == "disabled" else 1
        assert output.count(b"\033[<u") == expected, (case, bytes(output))
        run("has-session", "-t", "keyboard")
    finally:
        run("kill-server", check=False)
        if fd is not None:
            os.close(fd)
        if pid is not None:
            os.waitpid(pid, 0)
PY
