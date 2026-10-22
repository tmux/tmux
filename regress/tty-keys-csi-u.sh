#!/bin/sh

# Omitted CSI-u modifiers default to 1. Write terminal input to a private PTY
# so the bytes pass through tty-keys, not directly to the pane.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 -B - "$TEST_TMUX" <<'PY'
import fcntl
import os
import select
import struct
import subprocess
import sys
import tempfile
import termios
import time

with tempfile.TemporaryDirectory(prefix="tmux-csi-u-", dir="/tmp") as directory:
    tmux = [sys.argv[1], "-S", directory + "/socket", "-f/dev/null"]
    env = {k: v for k, v in os.environ.items() if k not in ("TMUX", "TMUX_PANE")}

    def run(*args, check=True):
        p = subprocess.run(tmux + list(args), env=env, capture_output=True,
            check=check, timeout=10)
        return p.stdout.decode().strip()

    master, slave = os.openpty()
    client = None
    try:
        fcntl.ioctl(slave, termios.TIOCSWINSZ,
            struct.pack("HHHH", 24, 80, 0, 0))
        run("new-session", "-d", "-s", "keys", "sleep 60",
            ";", "set-option", "-g", "status", "off",
            ";", "set-option", "-s", "escape-time", "30",
            ";", "set-option", "-s", "extended-keys", "on",
            ";", "set-option", "-s", "extended-keys-format", "csi-u",
            ";", "unbind-key", "-a", "-T", "root")
        client = subprocess.Popen(tmux + ["attach-session", "-t", "keys"],
            env=env, stdin=slave, stdout=slave, stderr=slave,
            start_new_session=True)

        def wait_for(expected, *args):
            deadline = time.monotonic() + 10
            while True:
                actual = run(*args)
                if actual == expected:
                    return
                assert time.monotonic() < deadline, (args, expected, actual)
                if select.select([master], [], [], .01)[0]:
                    os.read(master, 65536)

        wait_for("1", "list-clients", "-F", "1")
        cases = [
            (b"\033[27u", "^["),
            (b"\033[27;1u", "^["),
            (b"\033", "^["),
            (b"\033[13;2u", "^[[13;2u"),
            (b"\r", "^M"),
            (b"\033[13u", "^M"),
            (b"\033[97u", "a"),
            (b"\033[27;2;13~", "^[[13;2u"),
        ]
        # Rejected input falls back to M-[ followed by literal bytes. The
        # first overflow would wrap to Escape if parsed as an unsigned int.
        for suffix in ("u", "27;u", "27;;1u", "27xu", "27:1u",
                "4294967323u", "18446744073709551616u"):
            cases.append((b"\033[" + suffix.encode(), "^[[91;3u" + suffix))
        for index, (wire, expected) in enumerate(cases):
            title = "ready-%d" % index
            run("respawn-pane", "-k", "stty raw -echo; "
                "printf '\\033[>4;2m\\033]2;%s\\007'; exec cat -v" % title)
            wait_for(title + ":Ext 2", "display-message", "-p",
                "#{pane_title}:#{pane_key_mode}")
            # A trailing printable key confirms that all input was decoded.
            # Let a lone Escape expire first so it does not become M-Z.
            if wire == b"\033":
                os.write(master, wire)
                wait_for(expected, "capture-pane", "-p")
                os.write(master, b"Z")
            else:
                os.write(master, wire + b"Z")
            wait_for(expected + "Z", "capture-pane", "-p")
            print("PASS %r" % (wire,))
    finally:
        run("kill-server", check=False)
        os.close(slave)
        os.close(master)
        if client is not None:
            client.wait(timeout=10)
PY
