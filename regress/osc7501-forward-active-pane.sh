#!/bin/sh

# Test OSC 7501 program status on attached clients using private PTYs.

PATH=/bin:/usr/bin:/usr/local/bin
LC_CTYPE=C.UTF-8
export LC_CTYPE
[ -z "$TEST_TMUX" ] && TEST_TMUX=../tmux
command -v python3 >/dev/null || exit 101

python3 - "$TEST_TMUX" "$@" <<'PY'
import base64
import errno
import fcntl
import os
import re
import select
import signal
import struct
import subprocess
import sys
import tempfile
import termios
import time

tmux = os.path.abspath(sys.argv[1])
if os.path.isdir(tmux):
    tmux = os.path.join(tmux, "tmux")

prefix = b"\033]7501;"
reply = b"?:states=idle,working,done,blocked,error"
frame = re.compile(rb"\x1b\]7501;(.*?)(?:\x07|\x1b\\)", re.DOTALL)
pane_command = "stty -echo; printf 'PANE-READY\\n'; exec cat"


def until(description, get):
    end = time.monotonic() + 5
    while time.monotonic() < end:
        result = get()
        if result:
            return result
        time.sleep(.01)
    raise AssertionError(f"timed out waiting for {description}")


class Terminal:
    def __init__(self, command):
        self.output = bytearray()
        self.fd, slave = os.openpty()
        try:
            fcntl.ioctl(slave, termios.TIOCSWINSZ,
                        struct.pack("HHHH", 24, 80, 0, 0))
            self.process = subprocess.Popen(
                command, stdin=slave, stdout=slave, stderr=slave,
                env=dict(os.environ, TERM="xterm-256color"))
        except OSError:
            os.close(self.fd)
            raise
        finally:
            os.close(slave)

    def drain(self, seconds):
        end = time.monotonic() + seconds
        while select.select([self.fd], [], [],
                            max(0, end - time.monotonic()))[0]:
            try:
                data = os.read(self.fd, 65536)
            except OSError as error:
                if error.errno != errno.EIO:
                    raise
                break
            if not data:
                break
            self.output.extend(data)
            if time.monotonic() >= end:
                break
        return self.output

    def send(self, data):
        assert os.write(self.fd, data) == len(data)

    def expect(self, *expected):
        self.drain(.1)
        end = time.monotonic() + 5
        while (len(frame.findall(self.output)) < len(expected)
               and self.process.poll() is None and time.monotonic() < end):
            self.drain(.1)
        actual = frame.findall(self.output)
        assert actual == list(expected), f"expected {expected!r}, got {actual!r}"
        self.output.clear()

    def close(self):
        os.close(self.fd)
        self.process.kill()
        self.process.wait(timeout=5)


class Test:
    def __init__(self, directory):
        self.directory = directory
        self.socket = os.path.join(directory, "socket")
        self.command = [tmux, "-S", self.socket, "-f/dev/null"]
        self.terminals = []
        self.pane = None
        self.server = subprocess.Popen(self.command + ["-D"],
                                       cwd=directory)

    def run(self, *args):
        result = subprocess.run(self.command + ["-N"] + list(args), cwd=self.directory,
                                capture_output=True, timeout=5)
        if result.returncode != 0:
            raise AssertionError(f"{args!r}: {result.stderr.decode().strip()}")
        return result.stdout.decode().rstrip("\n")

    def start(self):
        until("the server socket", lambda: os.path.exists(self.socket))
        self.pane = self.run("new-session", "-dP", "-F", "#{pane_id}",
                             "-s", "status", pane_command)
        self.run("set-option", "-g", "status", "off")
        self.run("set-option", "-s", "escape-time", "10")
        self.ready(self.pane)

    def close(self):
        try:
            for terminal in self.terminals:
                terminal.close()
            if self.pane is not None and self.server.poll() is None:
                self.stop_panes()
        finally:
            if self.server.poll() is None:
                self.server.terminate()
            try:
                self.server.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.server.kill()
                self.server.wait()

    def stop_panes(self):
        # Keep exited panes until the server reaps their children.
        self.run("set-option", "-wg", "remain-on-exit", "on")

        def live_pids():
            return self.run("list-panes", "-a", "-f", "#{==:#{pane_dead},0}",
                            "-F", "#{pane_pid}").splitlines()

        for pid in live_pids():
            try:
                os.kill(int(pid), signal.SIGTERM)
            except ProcessLookupError:
                pass
        until("the pane helpers to exit", lambda: not live_pids())

    def attach(self, session="status"):
        terminal = Terminal(self.command + ["attach-session", "-t", session])
        self.terminals.append(terminal)
        terminal.name = until("client attachment", lambda: dict(
            line.split(" ", 1) for line in self.run(
                "list-clients", "-F", "#{client_pid} #{client_name}"
            ).splitlines()).get(str(terminal.process.pid)))
        return terminal

    def capture(self, pane):
        return self.run("capture-pane", "-p", "-t", pane)

    def ready(self, pane):
        until("pane helper startup", lambda: "PANE-READY" in self.capture(pane))

    def pane_write(self, pane, data):
        path = self.run("display-message", "-pt", pane, "#{pane_tty}")
        fd = os.open(path, os.O_WRONLY | os.O_NOCTTY)
        try:
            assert os.write(fd, data) == len(data)
        finally:
            os.close(fd)

    def report(self, pane, *bodies):
        self.pane_write(pane, b"".join(prefix + body + b"\033\\"
                                       for body in bodies))

    def wait_status(self, pane, status):
        fmt = "#{pane_program_state}|#{pane_program_name}|#{pane_program_message}"
        until(f"{pane} status {status!r}",
              lambda: self.run("display-message", "-pt", pane, fmt) == status)

    def enable(self):
        self.run("set-option", "-s", "forward-program-status", "on")


def probe(test):
    first, second = test.attach(), test.attach()
    test.report(test.pane, b"state=working")
    test.wait_status(test.pane, "working||")

    # Let ordinary startup queries expire. Only the OSC 7501 query
    # can then explain holding the fragments past escape-time.
    first.drain(5.2)
    first.expect()
    second.expect()
    test.enable()
    for terminal in (first, second):
        terminal.expect(b"?")

    first.send(prefix[:4])
    time.sleep(.05)
    first.send(prefix[4:] + reply + b"\033")
    time.sleep(.05)
    first.send(b"\\")
    second.send(prefix + b"unsupported\007")

    # An unsupported reply is consumed, but cannot confirm support.
    # A valid reply after the probe deadline also cannot confirm it.
    first.drain(5.2)
    second.send(prefix + reply + b"\007" + b"AFTER-REPLY\r")
    until("the ordinary key after the replies",
          lambda: "AFTER-REPLY" in test.capture(test.pane))
    pane = test.capture(test.pane)
    assert pane.splitlines() == ["PANE-READY", "AFTER-REPLY"], pane
    assert b"?" not in frame.findall(first.output), first.output
    second.expect()
    return first, second


def publication(test):
    first, second = probe(test)
    first.expect(b"state=working")
    pane = test.pane

    # A shell prompt clears a working status on the outer terminal.
    test.pane_write(pane, b"\033]133;A\007")
    first.expect(b"state=clear")

    test.report(pane, b"state=working:app=build:msg=4piD:progress=50",
                b"state=done")
    first.expect(b"state=working:app=build:msg=4piD", b"state=done")
    second.expect()

    test.report(pane, b"state=working:app=bad/name:msg=SGk=")
    test.wait_status(pane, "working|bad/name|Hi")
    first.expect(b"state=working:msg=SGk=")

    # Tabs fit the incoming length limit and expand during storage.
    test.report(pane, b"state=working:app=build:msg=" +
                base64.b64encode(b"\t" * 1025))
    test.wait_status(pane, "working|build|" + r"\t" * 1025)
    first.expect(b"state=working:app=build")

    test.report(pane, b"state=idle:msg=woU=")
    test.wait_status(pane, "idle||\u0085")
    first.expect(b"state=idle")

    background = test.run("split-window", "-dP", "-F", "#{pane_id}",
                          "-t", pane, pane_command)
    test.ready(background)
    test.report(background, b"state=blocked")
    test.wait_status(background, "blocked||")
    first.expect()
    test.run("select-pane", "-t", background)
    first.expect(b"state=blocked")
    test.run("refresh-client", "-t", first.name)
    first.expect()

    # A retained pane clears its blocked status on exit.
    test.run("set-option", "-w", "-t", background, "remain-on-exit", "on")
    test.run("send-keys", "-t", background, "C-d")
    until("the retained pane to exit",
          lambda: test.run("display-message", "-pt", background,
                           "#{pane_dead}") == "1")
    first.expect(b"state=clear")
    second.expect()


def connect(test, session="status"):
    test.enable()
    terminal = test.attach(session)
    terminal.expect(b"?")
    terminal.send(prefix + reply + b"\007")
    terminal.expect(b"state=clear")
    return terminal


def multiple_clients(test):
    other = test.run("new-window", "-dP", "-F", "#{pane_id}", "-n", "other",
                     "-t", "status:", pane_command)
    test.ready(other)
    # Grouped sessions share windows but select them independently.
    test.run("new-session", "-d", "-s", "peer", "-t", "status")
    first, second = connect(test), connect(test, "peer")

    test.report(test.pane, b"state=done")
    for terminal in (first, second):
        terminal.expect(b"state=done")

    test.run("select-window", "-t", "peer:other")
    second.expect(b"state=clear")

    test.report(test.pane, b"state=blocked")
    test.report(other, b"state=error")
    first.expect(b"state=blocked")
    second.expect(b"state=error")
    test.run("select-window", "-t", "peer:0")
    second.expect(b"state=blocked")
    first.expect()


def output_lifecycle(test):
    terminal = connect(test)
    pane = test.pane
    test.report(pane, b"state=working:app=build")
    terminal.expect(b"state=working:app=build")

    def count(name):
        return int(test.run("display-message", "-pc", terminal.name,
                            "#{client_" + name + "}"))

    discarded = count("discarded")
    # Stop consuming output while the screen scrolls.
    flood = b"0123456789abcdefghijklmnopqrstuvwxyz" * 2 + b"\r\n"
    for _ in range(24):
        test.pane_write(pane, flood * 64)
        if count("discarded") > discarded:
            break

    def output_discarded():
        # The write callback discards the backlog once the fd is writable.
        terminal.drain(.02)
        return count("discarded") > discarded

    until("tty output discard", output_discarded)
    terminal.expect(b"state=working:app=build")

    # Fill the kernel PTY without making the tmux backlog large
    # enough to discard. The marker follows that output.
    slave = os.open(terminal.name, os.O_WRONLY | os.O_NOCTTY)
    try:
        for _ in range(24):
            if not select.select([], [slave], [], 0)[1]:
                break
            test.pane_write(pane, flood * 16)
        else:
            raise AssertionError("tty did not become backlogged")
    finally:
        os.close(slave)
    discarded = count("discarded")
    written = count("written")
    marker = b"BEFORE-DISABLE"
    test.pane_write(pane, marker + b"\r\n")
    until("the marker to be queued", lambda: count("written") > written)
    test.run("set-option", "-s", "forward-program-status", "off")
    found = until("the queued clear", lambda: frame.search(terminal.drain(.05)))
    assert marker in terminal.output, "queued output was lost"
    assert terminal.output.index(marker) < found.start(), "clear overtook output"
    terminal.expect(b"state=clear")
    assert count("discarded") == discarded, "disable discarded tty output"

    test.enable()
    terminal.expect(b"state=working:app=build")
    test.run("detach-client", "-t", terminal.name)
    terminal.process.wait(timeout=5)
    terminal.expect(b"state=clear")


cases = {"probe": probe, "publication": publication, "clients": multiple_clients,
         "output": output_lifecycle}
for name in sys.argv[2:] or ["publication", "clients", "output"]:
    with tempfile.TemporaryDirectory(prefix="tmux-osc7501-") as directory:
        test = Test(directory)
        try:
            test.start()
            cases[name](test)
        finally:
            test.close()
PY
