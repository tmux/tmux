#!/bin/sh

# Test refresh-client -q with a fixed kernel size and terminal replies.
# Cover resizing, reply parsing, timeouts and lock/unlock.
#
# Also covered: repeated -q must neither resend an outstanding query
# nor restart its deadline; a pixel reply split across reads after the
# character reply must still be held pending at a low escape-time; the
# two replies are independent, so out-of-order replies still apply and
# a duplicate reply cannot bypass validation; an implausible size must
# be rejected without resizing; and locking a client with a query
# outstanding must not wedge later queries.

PATH=/bin:/usr/bin:/usr/local/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=../tmux
command -v python3 >/dev/null || exit 101

python3 - "$TEST_TMUX" <<'PY'
import fcntl
import os
import select
import shutil
import struct
import subprocess
import sys
import tempfile
import termios
import time

tmux = os.path.abspath(sys.argv[1])
if os.path.isdir(tmux):
    tmux = os.path.join(tmux, "tmux")
tmpdir = tempfile.mkdtemp(prefix="tmux-refresh-size-")
server = [tmux, "", "-f/dev/null"]

def run(*args, check=True):
    p = subprocess.run(server + list(args), capture_output=True, cwd=tmpdir)
    if check and p.returncode != 0:
        raise RuntimeError("%r: %s" % (args, p.stderr.decode()))
    return p.stdout.decode().strip()

server[1] = "-LtestRefreshSize%d" % (os.getpid())

class Terminal:
    """Fake terminal on a pty that answers XTWINOPS size queries."""

    def __init__(self):
        self.buf = b""

    def attach(self, session):
        self.pid, self.fd = os.forkpty()
        if self.pid == 0:
            os.environ["TERM"] = "xterm-256color"
            fcntl.ioctl(0, termios.TIOCSWINSZ,
                        struct.pack("HHHH", 24, 80, 0, 0))
            os.execv(tmux, server + ["attach-session", "-t", session])
        # Fixed winsize: the size never changes in the kernel, so the
        # only way tmux can learn about a resize is the size query.

    def pump(self, seconds, rows, cols):
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            if select.select([self.fd], [], [], .05)[0]:
                data = os.read(self.fd, 65536)
                if not data:
                    break
                self.buf += data
            while b"\033[18t" in self.buf:
                self.buf = self.buf.replace(b"\033[18t", b"", 1)
                os.write(self.fd,
                       ("\033[8;%u;%ut" % (rows, cols)).encode())
            while b"\033[14t" in self.buf:
                self.buf = self.buf.replace(b"\033[14t", b"", 1)
                os.write(self.fd, b"\033[4;600;800t")

    def drain(self, seconds):
        """Consume output without answering any query."""
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            if select.select([self.fd], [], [], .05)[0]:
                data = os.read(self.fd, 65536)
                if not data:
                    break
                self.buf += data

    def close(self):
        try:
            os.close(self.fd)
        except OSError:
            pass
        try:
            os.kill(self.pid, 1)
            os.waitpid(self.pid, 0)
        except (OSError, ChildProcessError):
            pass

def window_size():
    return run("display-message", "-p",
              "#{window_width}x#{window_height}")

def refresh_q(client):
    run("refresh-client", "-q", "-t", client)

def pane_text():
    # Any reply that leaks into the key stream reaches the "cat" in the
    # session pane and is echoed back into the pane grid.
    return run("capture-pane", "-p", "-t", "refreshsize")

def check_leak(where, expected):
    pane = pane_text()
    if pane != expected:
        raise RuntimeError("reply leaked into pane (%s): %r"
                         % (where, pane))

try:
    run("new-session", "-d", "-s", "refreshsize",
        "sh", "-c", "echo 'Hello World'; cat")
    run("set-option", "-g", "status", "off")

    term = Terminal()
    term.attach("refreshsize")
    time.sleep(.5)

    # Find the fake client's name so commands can target it.
    client = None
    for line in run("list-clients", "-F",
                    "#{client_pid} #{client_name}").splitlines():
        pid, _, cid = line.partition(" ")
        if pid == str(term.pid):
            client = cid
    if client is None:
        raise RuntimeError("fake client not found")

    # Phase 1: without -q nothing is applied.  Answer any queries that
    # are sent during attach (with the same size); nothing must resize.
    term.pump(1.0, 24, 80)
    size = window_size()
    if size != "80x24":
        raise RuntimeError("unexpected size without -q: %s" % size)
    expected_pane = pane_text()

    # Phase 2: refresh-client -q sends the size query.  The attached
    # terminal keeps its fixed kernel winsize (no SIGWINCH); only the
    # query reply reports the new size of 100x30, which tmux must
    # detect and apply.
    refresh_q(client)
    term.pump(1.0, 30, 100)
    size = window_size()
    if size != "100x30":
        raise RuntimeError("resize via refresh-client -q failed: "
                         "size is %s" % size)

    # Replies must not reach the pane.
    check_leak("plain replies", expected_pane)

    # Nothing runs between invocations: with no -q sent, no queries
    # must appear on the wire.
    term.buf = b""
    term.drain(1.0)
    if b"\033[18t" in term.buf or b"\033[14t" in term.buf:
        raise RuntimeError("size query sent without refresh-client -q")

    # A later query reporting a smaller size must shrink the window.
    refresh_q(client)
    term.pump(1.0, 24, 80)
    size = window_size()
    if size != "80x24":
        raise RuntimeError("shrink via refresh-client -q failed: "
                         "size is %s" % size)

    # Phase 5: a query that is never answered must be abandoned after
    # the query timeout (a few seconds) so pending-query flags do not
    # hold up escape sequences indefinitely.  A second -q while the
    # first query is outstanding must neither resend the query nor
    # restart its deadline, so the timeout still fires about five
    # seconds after the first query, and a reply arriving after that
    # point is stale and must not resize anything.  The timeout is the
    # shared start timer, also armed when the session attaches; wait
    # for that arming to have passed so this phase sees a fresh
    # deadline.
    time.sleep(1.0)
    term.buf = b""
    refresh_q(client)
    term.drain(2.0)
    refresh_q(client)
    term.drain(4.0)
    if term.buf.count(b"\033[18t") != 1:
        raise RuntimeError("refresh-client -q resent an outstanding "
                         "query (%d on the wire)"
                         % term.buf.count(b"\033[18t"))
    os.write(term.fd, b"\033[8;30;100t\033[4;600;800t")
    term.drain(0.5)
    size = window_size()
    if size != "80x24":
        raise RuntimeError("stale reply after abandoned query resized: "
                         "size is %s" % size)
    expected_pane = pane_text()

    # Phase 6: with a very low escape-time, a pixel reply split across
    # reads after the character reply must still be held pending (the
    # unanswered pixel query keeps the escape delay raised) and must
    # never leak into the pane.  Sizes and reply bytes are unique per
    # phase so earlier deliberate leaks cannot trip the checks; the
    # replies here are written by hand, so drain without the automatic
    # responder (the queries are already answered).
    run("set-option", "-s", "escape-time", "5")
    refresh_q(client)
    time.sleep(.2)
    os.write(term.fd, b"\033[8;26;86t")
    time.sleep(.2)
    os.write(term.fd, b"\033[4;520;6")
    time.sleep(.25)
    os.write(term.fd, b"88t")
    term.drain(1.0)
    size = window_size()
    if size != "86x26":
        raise RuntimeError("fragmented pixel reply not applied: "
                         "size is %s" % size)
    check_leak("fragmented pixel reply", expected_pane)
    run("set-option", "-s", "escape-time", "500")

    # Phase 7: the two replies are tracked independently, so replies
    # arriving out of order must both work: the pixel reply first must
    # not consume the character reply's pending state, and the
    # character reply after it must still resize.
    refresh_q(client)
    time.sleep(.2)
    os.write(term.fd, b"\033[4;700;900t")
    os.write(term.fd, b"\033[8;28;90t")
    term.drain(1.0)
    size = window_size()
    if size != "90x28":
        raise RuntimeError("out-of-order replies not applied: "
                         "size is %s" % size)
    check_leak("out-of-order replies", expected_pane)

    # Phase 8: an implausible size must be rejected without resizing
    # and without consuming the pixel reply's state; the valid pixel
    # reply closes the query normally.
    refresh_q(client)
    time.sleep(.2)
    os.write(term.fd, b"\033[8;24;10001t")
    os.write(term.fd, b"\033[4;560;720t")
    term.drain(1.0)
    size = window_size()
    if size != "90x28":
        raise RuntimeError("implausible size accepted: size is %s" % size)
    check_leak("implausible size", expected_pane)

    # Phase 9: a query outstanding when the client is locked has its
    # timer cancelled by tty_stop_tty(); the pending state must be
    # dropped with it, otherwise after unlocking every later -q is
    # rejected as "already outstanding" forever.  So: send -q and
    # leave it unanswered, lock and unlock, then -q again must produce
    # a fresh query on the wire.
    # Use a shell builtin so this does not depend on a system lock
    # program or require a password to unlock.
    run("set-option", "-g", "lock-command", "true")
    refresh_q(client)
    term.drain(0.5)
    run("lock-client", "-t", client)
    deadline = time.monotonic() + 5
    while "suspended" in run("display-message", "-p", "-c", client,
                           "#{client_flags}").split(","):
        if time.monotonic() >= deadline:
            raise RuntimeError("client did not unlock")
        term.drain(.05)
    term.buf = b""
    refresh_q(client)
    term.drain(1.0)
    if b"\033[18t" not in term.buf:
        raise RuntimeError("refresh-client -q wedged after lock/unlock")
    term.pump(0.5, 28, 90)
    size = window_size()
    if size != "90x28":
        raise RuntimeError("size changed across lock/unlock: "
                         "size is %s" % size)

    term.close()
finally:
    run("kill-server", check=False)
    shutil.rmtree(tmpdir, ignore_errors=True)
PY
