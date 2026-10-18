#!/bin/sh

# Test refresh-client -q: resizing detected purely from an on-demand
# terminal size query, as happens on a serial line where SIGWINCH is
# never delivered.  The fake terminal answers both ESC[18t (size in
# characters) and ESC[14t (size in pixels); the pixel reply must be
# consumed as a reply and must never leak into the key stream (check
# the server log for single-character keys, which nothing here types).
#
# Also covered: repeated -q must neither resend an outstanding query
# nor restart its deadline; a pixel reply split across reads after the
# character reply must still be held pending at a low escape-time; the
# two replies are independent, so out-of-order replies still apply and
# a duplicate reply cannot bypass validation; an implausible size must
# be rejected without resizing; and locking a client with a query
# outstanding must not wedge later queries.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
# Ensure the binary is actually called, not the directory
if [ -d "$TEST_TMUX" ]; then
    TEST_TMUX="$TEST_TMUX/tmux"
fi

python3 - "$TEST_TMUX" <<'PY'
import fcntl
import glob
import os
import select
import shutil
import struct
import subprocess
import sys
import tempfile
import termios
import time

tmux = sys.argv[1]
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

def server_log():
    files = glob.glob(os.path.join(tmpdir, "tmux-server-*.log"))
    return "".join(open(f, errors="replace").read() for f in files)

def window_size():
    return run("display-message", "-p",
              "#{window_width}x#{window_height}")

def refresh_q(client):
    run("refresh-client", "-q", "-t", client)

def pane_text():
    # Any reply that leaks into the key stream reaches the "cat" in the
    # session pane and is echoed back into the pane grid.
    return run("capture-pane", "-p", "-t", "refreshsize")

def check_leak(where, *needles, **kw):
    # The log check is cumulative, so it is only valid before phase 5
    # deliberately delivers stale replies that must leak as keys.
    pane = pane_text()
    for junk in needles:
        if junk in pane:
            raise RuntimeError("reply leaked into pane (%s): %s"
                             % (where, junk))
    if not kw.get("log", True):
        return
    log = server_log()
    for junk in ("complete key \\033[ ", "complete key 4 ",
                 "complete key 8 ", "complete key ; "):
        if junk in log:
            raise RuntimeError("reply leaked into key stream (%s): %s"
                             % (where, junk))

try:
    # Note: -v on the command that spawns the server makes it log (the
    # cwd of that first client is where the log is written).
    run("-v", "new-session", "-d", "-s", "refreshsize",
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

    # The pixel-size reply must have been consumed as a reply, not
    # dispatched as keys: the pane and the server log must show no
    # trace of the reply bytes, since nothing in this test ever types.
    check_leak("plain replies", "8;30;100", "4;600", ";800t")

    # Nothing runs between invocations: with no -q sent, no queries
    # must appear on the wire.
    term.buf = b""
    term.pump(1.0, 30, 100)
    if b"\033[18t" in term.buf:
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
    if "already outstanding" not in server_log():
        raise RuntimeError("second refresh-client -q was not skipped")
    if "size query timed out" not in server_log():
        raise RuntimeError("unanswered size query was not abandoned")
    term.pump(0.5, 30, 100)
    size = window_size()
    if size != "80x24":
        raise RuntimeError("stale reply after abandoned query resized: "
                         "size is %s" % size)

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
    check_leak("fragmented pixel reply",
               "8;26;86", "4;520", ";688t", log=False)
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
    check_leak("out-of-order replies",
               "8;28;90", "4;700", ";900t", log=False)

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
    if "ignoring reported size" not in server_log():
        raise RuntimeError("implausible size was not logged as ignored")

    # Phase 9: a query outstanding when the client is locked has its
    # timer cancelled by tty_stop_tty(); the pending state must be
    # dropped with it, otherwise after unlocking every later -q is
    # rejected as "already outstanding" forever.  So: send -q and
    # leave it unanswered, lock and unlock, then -q again must produce
    # a fresh query on the wire.
    refresh_q(client)
    term.drain(0.5)
    run("lock-client", "-t", client)
    time.sleep(.3)
    os.write(term.fd, b"\n")	# any key exits lock-command -> MSG_UNLOCK
    time.sleep(.5)
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
