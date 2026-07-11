#!/bin/sh

# Serialization-fidelity test for the control-mode resync REPAINT/REPLAY path.
#
# Uses tmux itself as the reference emulator (an "oracle" round-trip): a screen
# is constructed on an ORIGINAL server, a resync is induced on an attached -C
# client, the delivered repaint %output byte stream is extracted, and those raw
# bytes are fed into a fresh pane on a SECOND (ORACLE) server. Because the same
# tmux emulator parses the repaint, comparing oracle-vs-original is a true
# fidelity check with no reference-emulator gap: any divergence is a real
# screen_repaint / control.c serialiser bug.
#
# One numbered case per screen construction: SGR spectrum, UTF-8 wide+combining,
# custom tab stops, scroll region, alt screen, origin mode, cursor style/colour,
# hidden cursor, insert mode, application keypad/cursor-keys, bracketed paste,
# wrap off, mouse modes, empty and full screens, wrapped-line scrollback replay,
# scrollback ordering, width-change degrade and history-trim degrade.
#
# Silent on pass; on failure the diverging cases are printed to stderr and the
# script exits 1. The heavy lifting is a python3 driver (the four existing
# control-resync regress tests likewise embed python3 to drive a -C client).

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

command -v python3 >/dev/null 2>&1 || {
	echo "control-resync-replay: python3 required" >&2
	exit 1
}

OSOCK="resyncreplayo$$"
RSOCK="resyncreplayr$$"
WD=$(mktemp -d)

trap '$TEST_TMUX -L "$OSOCK" kill-server 2>/dev/null;
      $TEST_TMUX -L "$RSOCK" kill-server 2>/dev/null;
      rm -rf "$WD"' 0 1 15

python3 - "$TEST_TMUX" "$OSOCK" "$RSOCK" "$WD" <<'PYEOF' || exit 1
"""Dev driver for the tmux-as-oracle resync fidelity test.

Round-trip: build a screen on an ORIGINAL tmux server, induce a control-mode
resync, extract the client's repaint %output byte stream, feed it into a fresh
pane on a second (ORACLE) tmux server, and compare oracle-vs-original. Because
the same tmux emulator parses the repaint, there is no reference-emulator
fidelity gap: any divergence is a real serialiser bug.

argv: <tmux> <orig-sock> <orac-sock> <workdir>
"""
import glob
import os
import pty
import re
import select
import subprocess
import sys
import termios
import time

TMUX, OSOCK, RSOCK, WD = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
REPAINT_SIG = b"\x18\x1b\\"                    # CAN + ST begins every repaint
OCT = re.compile(rb"\\([0-7]{3})")
FAIL = []


def unescape(b):
    return OCT.sub(lambda m: bytes([int(m.group(1), 8)]), b)


def run(*a, **kw):
    return subprocess.run([str(x) for x in a], capture_output=True,
                          timeout=25, **kw)


def tmux(sock, *a):
    r = run(TMUX, "-L", sock, *a)
    return r.returncode, r.stdout.decode("utf-8", "replace"), r.stderr.decode()


def logcount(needle):
    g = sorted(glob.glob(os.path.join(WD, "tmux-server-*.log")))
    if not g:
        return 0
    r = run("grep", "-cF", needle, g[-1])
    try:
        return int(r.stdout.strip() or b"0")
    except ValueError:
        return 0


# --------------------------------------------------------------------------- victim
class Victim:
    """A real `tmux -C` client over a pty (small buffer => the repaint defers
    behind the pre-gap backlog until we resume, so it is rendered from the final
    grid, not an intermediate one)."""

    def __init__(self, sock, session):
        mfd, sfd = pty.openpty()
        a = termios.tcgetattr(sfd)
        a[3] &= ~(termios.ECHO | termios.ECHONL)
        termios.tcsetattr(sfd, termios.TCSANOW, a)
        self.proc = subprocess.Popen(
            [TMUX, "-L", sock, "-C", "attach", "-t", session],
            stdin=sfd, stdout=sfd, stderr=sfd, start_new_session=True, cwd=WD)
        os.close(sfd)
        self.fd = mfd
        os.set_blocking(self.fd, False)
        self.buf = bytearray()

    def drain(self, dur):
        end = time.time() + dur
        while time.time() < end:
            r, _, _ = select.select([self.fd], [], [], 0.1)
            if r:
                try:
                    d = os.read(self.fd, 65536)
                except OSError:
                    break
                if d:
                    self.buf.extend(d)

    def clear(self):
        self.buf = bytearray()

    def pane_output(self, pane):
        prefix = b"%output %" + pane.lstrip("%").encode() + b" "
        out = bytearray()
        for line in bytes(self.buf).split(b"\n"):
            if line.endswith(b"\r"):
                line = line[:-1]
            if line.startswith(prefix):
                out += unescape(line[len(prefix):])
        return bytes(out)

    def close(self):
        try:
            self.proc.terminate()
        except Exception:
            pass
        try:
            os.close(self.fd)
        except OSError:
            pass


# --------------------------------------------------------------------------- oracle
_ORAC_SEQ = [0]


# A glyph the test content never emits, so any cell left showing it after the
# repaint is a cell the repaint failed to write (a write omission).
DIRTY = b"@"


def _dirty_fill(sx, sy):
    """Bytes that paint every visible cell with the sentinel, then home. Feeding
    this BEFORE the repaint models a real client repainting over its stale
    pre-gap screen: any cell the repaint never writes keeps the sentinel, so
    write OMISSION (e.g. cells a tab glides over) becomes visible, not just
    wrong writes."""
    out = bytearray(b"\x1b[m")
    for r in range(1, sy + 1):
        out += b"\x1b[%d;1H" % r
        out += DIRTY * sx
    out += b"\x1b[H"
    return bytes(out)


def oracle_feed(repaint, sx, sy, history_limit, dirty=False):
    """Feed `repaint` into a fresh oracle pane sized sx*sy; return pane id. When
    `dirty`, pre-fill every cell with the sentinel first so unwritten cells show.

    The pane blocks on a FIFO so we can resize BEFORE the bytes are parsed."""
    _ORAC_SEQ[0] += 1
    name = "o%d" % _ORAC_SEQ[0]
    fifo = os.path.join(WD, "fifo_%s" % name)
    try:
        os.remove(fifo)
    except OSError:
        pass
    os.mkfifo(fifo)
    tmux(RSOCK, "set-option", "-g", "history-limit", str(history_limit))
    tmux(RSOCK, "set-option", "-g", "window-size", "manual")
    tmux(RSOCK, "new-window", "-t", "orac:", "-n", name,
         "sh -c 'cat %s; exec sleep 3600'" % fifo)
    tmux(RSOCK, "resize-window", "-t", "orac:" + name, "-x", str(sx),
         "-y", str(sy))
    time.sleep(0.4)
    payload = (_dirty_fill(sx, sy) if dirty else b"") + repaint
    with open(fifo, "wb") as f:
        f.write(payload)
    time.sleep(1.0 + len(repaint) / 60000.0)      # large repaints parse slower
    _, out, _ = tmux(RSOCK, "display-message", "-p", "-t", "orac:" + name,
                     "#{pane_id}")
    return out.strip()


def cap_e(sock, pane):
    _, out, _ = tmux(sock, "capture-pane", "-p", "-e", "-C", "-t", pane)
    lines = out.split("\n")
    while lines and lines[-1] == "":
        lines.pop()
    return lines


def cap_plain(sock, pane):
    _, out, _ = tmux(sock, "capture-pane", "-p", "-C", "-t", pane)
    lines = [ln.rstrip() for ln in out.split("\n")]
    while lines and lines[-1] == "":
        lines.pop()
    return lines


def cap_join(sock, pane, n):
    _, out, _ = tmux(sock, "capture-pane", "-p", "-J", "-S", "-%d" % n,
                     "-t", pane)
    return [ln.rstrip() for ln in out.split("\n")]


def cursor(sock, pane):
    _, out, _ = tmux(sock, "display-message", "-p", "-t", pane,
                     "#{cursor_x},#{cursor_y},#{cursor_flag}")
    return out.strip()


def flag(sock, pane, name):
    _, out, _ = tmux(sock, "display-message", "-p", "-t", pane, "#{%s}" % name)
    return out.strip()


# --------------------------------------------------------------------------- flow
def new_pane(win):
    tmux(OSOCK, "new-window", "-t", "m:", "-n", win)
    _, out, _ = tmux(OSOCK, "display-message", "-p", "-t", "m:" + win,
                     "#{pane_id}")
    return "m:" + win, out.strip()


def send(target, s):
    tmux(OSOCK, "send-keys", "-t", target, s, "Enter")


def _await_resync(base, timeout=14):
    t0 = time.time()
    while time.time() - t0 < timeout:
        time.sleep(0.3)
        if logcount("resync %") > base:
            return True
    return False


# Resync trigger mode -- THE single migration switch (mirrors replay_oracle.py).
# "bytecap": legacy control-resync-size (a byte cap set above the socket so the
# repaint defers). "linelimit": the new trigger -- no control-resync-size; the
# resync fires on missed scrolled lines >= max(history-limit, ~2 screenfuls), so
# a small history-limit set before pane creation induces (and speeds up) the
# gap. history-limit then serves double duty (trigger threshold + replay depth);
# TRIGGER_HLIMIT is the deferral-safe floor. Values tuned against the new binary.
RESYNC_TRIGGER = "linelimit"
TRIGGER_HLIMIT = 250
LINELIMIT_FILL_LINE = "F" * 78     # wide (>=65 B) discrete filler row (deferral)


def arm_resync_trigger(cap, history_limit):
    if RESYNC_TRIGGER == "bytecap":
        tmux(OSOCK, "set-option", "-g", "control-resync-size", str(cap))
        tmux(OSOCK, "set-option", "-g", "history-limit", str(history_limit))
    else:                                               # linelimit
        hl = history_limit if history_limit < 100000 else TRIGGER_HLIMIT
        tmux(OSOCK, "set-option", "-g", "history-limit", str(hl))


def resync_redraw(victim, target, pane, draw, cap, resize, history_limit,
                  filler=90000):
    """Byte-cap resync, THEN draw the final screen while resyncing, resume,
    return (repaint_bytes, fired)."""
    arm_resync_trigger(cap, history_limit)
    victim.drain(0.8)
    victim.clear()
    base = logcount("resync %")
    send(target, "head -c %d /dev/zero | tr '\\0' Z; echo _F_" % filler)
    fired = _await_resync(base)
    time.sleep(0.6)
    if resize is not None:
        tmux(OSOCK, "refresh-client", "-C", "%dx%d" % resize)
        time.sleep(0.8)
    for d in draw:
        send(target, d)
        time.sleep(0.35)
    time.sleep(1.0)
    victim.clear()
    victim.drain(3.0)
    raw = victim.pane_output(pane)
    sig = raw.find(REPAINT_SIG)
    return (raw[sig:] if sig >= 0 else b""), fired


def resync_then_produce(victim, target, pane, produce_cmd, sentinel,
                        cap=40000, filler=80000, history_limit=100000):
    """A byte filler fires the resync with the socket full (so the repaint
    defers). Content is then produced during the resync; it scrolls into
    history within the post-resync replay window. Wait for `sentinel` to reach
    the pane, then resume. Return (repaint_bytes, fired)."""
    arm_resync_trigger(cap, history_limit)
    victim.drain(0.8)
    victim.clear()
    base = logcount("resync %")
    # Newline-terminated filler (discrete lines, NOT one giant wrapped line)
    # so the scrollback replay stays line-structured and the lines under test
    # are not absorbed into a single logical line.
    # Wide discrete filler rows so the line trigger fires only after the socket
    # is full (deferral); ZFILLER's ~8 B rows would trip it before saturation.
    send(target, "yes %s | head -c %d; echo _F_" % (LINELIMIT_FILL_LINE, filler))
    fired = _await_resync(base)
    time.sleep(0.6)
    send(target, "%s; echo %s" % (produce_cmd, sentinel))
    t0 = time.time()
    while time.time() - t0 < 10:
        _, out, _ = tmux(OSOCK, "capture-pane", "-p", "-t", pane)
        if sentinel in out:
            break
        time.sleep(0.3)
    time.sleep(0.6)
    victim.clear()
    victim.drain(3.0)
    raw = victim.pane_output(pane)
    sig = raw.find(REPAINT_SIG)
    return (raw[sig:] if sig >= 0 else b""), fired


def check(cond, case, msg):
    if not cond:
        FAIL.append("%s: %s" % (case, msg))
    return bool(cond)


def progress(name):
    if os.environ.get("REPLAY_DEBUG"):
        sys.stderr.write("[%6.1f] case %s\n" % (time.time() - T0, name))
        sys.stderr.flush()


T0 = time.time()


# =========================================================================== cases
ONLY = set(x for x in os.environ.get("REPLAY_ONLY", "").split(",") if x)


def visible_case(victim, name, draw, cap=40000, sx=80, sy=24, resize=None,
                 history_limit=100000, greps=(), nogreps=(),
                 oracle_flags=(), check_cursor=True, check_visible=True,
                 dirty=True):
    if ONLY and name not in ONLY:
        return
    progress(name)
    target, pane = new_pane(name)
    time.sleep(0.4)
    repaint, fired = resync_redraw(victim, target, pane, draw, cap, resize,
                                   history_limit)
    ok = check(fired, name, "resync did not fire")
    ok = check(bool(repaint), name, "no repaint extracted") and ok
    if not ok:
        tmux(OSOCK, "kill-window", "-t", target)
        return
    osx, osy = (resize if resize else (sx, sy))
    opane = oracle_feed(repaint, osx, osy, history_limit, dirty=dirty)
    if check_visible:
        # Content (plain text) and cursor are HARD: a surviving dirty-fill
        # sentinel or wrong glyph shows up here. The -e SGR representation is
        # HARD on a clean oracle (attribute fidelity) but SOFT under a
        # dirty-fill, where background-on-blank (BCE) cells legitimately differ
        # in representation while the plain text still matches -- distinguishing
        # "sentinel survived / content wrong" from "SGR representation differs".
        o_txt, r_txt = cap_plain(OSOCK, pane), cap_plain(RSOCK, opane)
        check(o_txt == r_txt, name,
              "content mismatch\n      orig=%r\n      orac=%r"
              % (o_txt[:8], r_txt[:8]))
        o_e, r_e = cap_e(OSOCK, pane), cap_e(RSOCK, opane)
        if o_e != r_e and o_txt == r_txt:
            if dirty:
                if os.environ.get("REPLAY_DEBUG"):
                    sys.stderr.write("  %s: attributes-only diff (soft)\n"
                                     % name)
            else:
                check(False, name,
                      "attribute mismatch\n      orig=%r\n      orac=%r"
                      % (o_e[:8], r_e[:8]))
    if check_cursor:
        oc, rc = cursor(OSOCK, pane), cursor(RSOCK, opane)
        check(oc == rc, name, "cursor mismatch orig=%s orac=%s" % (oc, rc))
    for g in greps:
        check(g in repaint, name, "repaint missing %r" % g)
    for g in nogreps:
        check(g not in repaint, name, "repaint unexpectedly has %r" % g)
    for fname, want in oracle_flags:
        got = flag(RSOCK, opane, fname)
        check(got == want, name,
              "oracle #{%s}=%s want %s" % (fname, got, want))
    tmux(OSOCK, "kill-window", "-t", target)


def scrollback_case(victim, name, produce_cmd, prefix, sentinel, cap=24000,
                    filler=55000, history_limit=100000, want_intact=None):
    if ONLY and name not in ONLY:
        return
    progress(name)
    target, pane = new_pane(name)
    time.sleep(0.4)
    repaint, fired = resync_then_produce(victim, target, pane, produce_cmd,
                                         sentinel, cap=cap, filler=filler,
                                         history_limit=history_limit)
    ok = check(fired, name, "resync did not fire")
    ok = check(bool(repaint), name, "no repaint extracted") and ok
    if not ok:
        tmux(OSOCK, "kill-window", "-t", target)
        return
    opane = oracle_feed(repaint, 80, 24, history_limit)
    orig = [ln for ln in cap_join(OSOCK, pane, 100000) if prefix in ln]
    orac = [ln for ln in cap_join(RSOCK, opane, 100000) if prefix in ln]
    if os.environ.get("DEBUG"):
        with open(os.path.join(WD, "last_repaint_%s.bin" % name), "wb") as f:
            f.write(repaint)
        ridx = repaint.find(prefix.encode() if isinstance(prefix, str)
                            else prefix)
        sys.stderr.write("DEBUG %s repaint@%d len=%d\n" % (name, ridx,
                                                           len(repaint)))
        sys.stderr.write("  repaint slice: %r\n" % repaint[max(0, ridx-40):ridx+340])
        sys.stderr.write("  orig(%d): %r\n" % (len(orig), [len(x) for x in orig]))
        sys.stderr.write("  orac(%d): %r\n" % (len(orac), [len(x) for x in orac]))
    # ordered subsequence, no duplicates
    it = iter(orig)
    subseq = all(any(x == y for y in it) for x in orac)
    seen, dup = set(), None
    for ln in orac:
        if ln in seen:
            dup = ln
            break
        seen.add(ln)
    check(bool(orac), name, "oracle replayed 0 %r lines" % prefix)
    check(subseq, name, "oracle not an ordered subsequence of original")
    check(dup is None, name, "oracle has duplicate line %r" % dup)
    if want_intact is not None:
        check(any(want_intact in ln for ln in orac), name,
              "wrapped logical line not intact in replay")
    tmux(OSOCK, "kill-window", "-t", target)


def wrapped_case(victim, name):
    """A logical line far wider than the pane, scrolled into history, must be
    replayed WITHOUT artificial hard breaks: the scrollback-replay joins the
    wrapped rows into one continuous run (SGR resets between the 80-col row
    boundaries, but no CR/LF), so the client re-wraps it as one logical line.
    Asserted directly on the repaint bytes -- the precise invariant -- since a
    hard break would insert \\r\\n between the row segments."""
    if ONLY and name not in ONLY:
        return
    progress(name)
    target, pane = new_pane(name)
    time.sleep(0.4)
    repaint, fired = resync_then_produce(
        victim, target, pane,
        "printf 'W%.0s' $(seq 1 300); printf '\\n'; "
        "for i in $(seq 1 30); do echo post-$i; done",
        "WDONE_SENTINEL", cap=24000, filler=55000)
    ok = check(fired, name, "resync did not fire")
    ok = check(bool(repaint), name, "no repaint extracted") and ok
    if not ok:
        tmux(OSOCK, "kill-window", "-t", target)
        return
    wi = repaint.find(b"W" * 10)
    ok = check(wi >= 0, name, "wide line not present in replay")
    if ok:
        # The wide line occupies a contiguous region of the replay. Strip the
        # per-row SGR resets; the W run must stay whole (>= 240 of the 300),
        # proving the 80-col rows were joined with no \r\n hard break. A hard
        # break would cap the run near 80.
        region = repaint[wi:wi + 700]
        end = region.find(b"\r")            # first CR ends the logical line
        wline = region[:end] if end >= 0 else region
        stripped = wline.replace(b"\x1b[m", b"")
        run = 0
        best = 0
        for ch in stripped:
            if ch == ord("W"):
                run += 1
                best = max(best, run)
            else:
                run = 0
        check(best >= 240, name,
              "wide line hard-broken in replay (max contiguous W run %d, "
              "expected ~300)" % best)
        check(b"\r" not in wline and b"\n" not in wline, name,
              "artificial line break inside the wrapped logical line")
    tmux(OSOCK, "kill-window", "-t", target)


def _noscroll(k):
    # k rows of "<79 X>\r": overwrites one physical line (scrolls nothing) while
    # streaming ~80*k bytes, used to fill the socket / trip the byte backstop.
    return "yes %s | head -n %d | tr '\\n' '\\r'" % ("X" * 79, k)


def frontier_case(victim, name, prefix, produce, N, L, sentinel, mode):
    """Lossless-frontier of the line trigger. The missed-line counter only
    accrues once the client socket is full, so a non-scrolling prefill saturates
    it first; then exactly N numbered lines scroll (all missed). Asserted on the
    REPAINT's replayed numbers -- what the client receives -- since the oracle's
    kept history depth is a separate emulator property.
      complete (N<L): the repaint replays 1..N contiguous, no loss, no dup.
      degrade (N>>L): a bounded recent contiguous window, oldest dropped (min>1,
      a gap not a duplicate), reaching the latest, no dup."""
    if ONLY and name not in ONLY:
        return
    progress(name)
    target, pane = new_pane(name)
    time.sleep(0.4)
    tmux(OSOCK, "set-option", "-g", "history-limit", str(L))
    victim.drain(0.8)
    victim.clear()
    base = logcount("resync %")
    send(target, produce + "; echo " + sentinel)
    fired = _await_resync(base, timeout=18)
    t0 = time.time()
    while time.time() - t0 < 12:
        _, out, _ = tmux(OSOCK, "capture-pane", "-p", "-t", pane)
        if sentinel in out:
            break
        time.sleep(0.3)
    time.sleep(0.6)
    victim.clear()
    victim.drain(3.0)
    raw = victim.pane_output(pane)
    sig = raw.find(REPAINT_SIG)
    repaint = raw[sig:] if sig >= 0 else b""
    ok = check(fired, name, "resync did not fire")
    ok = check(bool(repaint), name, "no repaint extracted") and ok
    if not ok:
        tmux(OSOCK, "kill-window", "-t", target)
        return
    rep = [int(m) for m in re.findall(prefix.encode() + rb"(\d+)", repaint)]
    contiguous = bool(rep) and (max(rep) - min(rep) + 1 == len(set(rep)))
    nodup = len(rep) == len(set(rep))
    info = ("count=%d min=%s max=%s contig=%s dup=%s"
            % (len(rep), rep[0] if rep else None, max(rep) if rep else None,
               contiguous, not nodup))
    if mode == "complete":
        check(bool(rep) and min(rep) == 1 and max(rep) == N and len(rep) == N
              and contiguous and nodup, name,
              "N<L replay not lossless (%s, want 1..%d)" % (info, N))
    else:
        check(bool(rep) and min(rep) > 1 and max(rep) == N and len(rep) < N
              and len(rep) <= 3 * L and contiguous and nodup
              and rep == sorted(rep), name,
              "N>>L replay not a graceful bounded gap (%s)" % info)
    tmux(OSOCK, "kill-window", "-t", target)


def main():
    tmux(OSOCK, "kill-server")
    tmux(RSOCK, "kill-server")
    subprocess.run([TMUX, "-vv", "-L", OSOCK, "-f/dev/null", "new-session",
                    "-d", "-s", "m", "-x", "80", "-y", "24"], cwd=WD, check=True)
    subprocess.run([TMUX, "-L", RSOCK, "-f/dev/null", "new-session",
                    "-d", "-s", "orac", "-x", "80", "-y", "24"], cwd=WD,
                   check=True)
    time.sleep(0.5)
    tmux(OSOCK, "set-option", "-g", "history-limit", "100000")
    tmux(OSOCK, "set-option", "-g", "control-resync", "on")
    obs = subprocess.Popen([TMUX, "-L", OSOCK, "-C", "attach", "-t", "m"],
                           stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                           stderr=subprocess.DEVNULL, cwd=WD)
    obs.stdin.write(b"refresh-client -f no-output\n")
    obs.stdin.flush()
    time.sleep(0.5)
    victim = Victim(OSOCK, "m")
    victim.drain(1.0)

    try:
        # 1. SGR spectrum -- kept on a CLEAN oracle (dirty=False) so the -e
        #    attribute representation is checked strictly (this is the one case
        #    whose whole point is SGR fidelity; a dirty-fill would soften it).
        visible_case(victim, "sgr", [
            "printf '\\033c\\033[1mB\\033[0m\\033[3mI\\033[0m\\033[4mU\\033[0m"
            "\\033[9mS\\033[0m\\033[7mR\\033[0m \\033[4:3mCUL\\033[0m "
            "\\033[31mr\\033[32mg\\033[34mb\\033[0m \\033[38;5;208m256\\033[0m "
            "\\033[48;5;22mBG\\033[0m "
            "\\033[38;2;10;20;30m\\033[48;2;200;100;50mRGB\\033[0m\\n"
            "\\033[5;10H'; sleep 30"], dirty=False)

        # 2. UTF-8 wide + combining + wide char straddling the right margin
        visible_case(victim, "utf8", [
            "printf '\\033c\\346\\227\\245\\346\\234\\254 "        # CJK wide
            "cafe\\314\\201 e\\314\\201x "                          # combining
            "\\033[1;80H\\344\\270\\200TAIL\\n\\033[4;1H'; sleep 30"])

        # 4. custom tab stops (clear all, set via HTS). The repaint must restore
        #    the custom stops (clear-all DECST + HTS at each) AND leave the cells
        #    a TAB jumps over blank: screen_repaint serialises tab-gap cells as a
        #    literal TAB, so it must clear each line before painting or stale
        #    content survives under the gap. Checked over a dirty-filled oracle
        #    so a gap the repaint fails to clear shows the sentinel.
        visible_case(victim, "tabs", [
            "printf '\\033c\\033[3g\\033[1;4H\\033H\\033[1;20H\\033H"
            "\\033[1;40H\\033H\\033[2;1Ha\\tb\\tc\\td\\033[6;1H'; sleep 30"],
            greps=[b"\x1b[3g", b"\x1b[1;4H\x1bH", b"\x1b[1;20H\x1bH",
                   b"\x1b[1;40H\x1bH"],
            dirty=True)

        # 5. scroll region (DECSTBM) with cursor inside it
        visible_case(victim, "scroll_region", [
            "printf '\\033c\\033[5;15r\\033[8;1Hinside-region line\\n"
            "second\\033[10;3H'; sleep 30"],
            greps=[b"\x1b[5;15r"])

        # 6. alt screen active at gap time -> repaint-only, lands in alt
        visible_case(victim, "altscreen", [
            "printf '\\033c\\033[?1049h\\033[2J\\033[HALT-CONTENT here"
            "\\033[3;5H'; sleep 30"],
            greps=[b"\x1b[?1049h"])

        # 7. origin mode (DECOM): the cursor address the repaint emits is taken
        #    relative to the scroll region, so an absolute row would land the
        #    cursor in the wrong place. Check the exact cursor here.
        visible_case(victim, "origin", [
            "printf '\\033c\\033[5;15r\\033[?6h\\033[2;1Horigin-inside"
            "\\033[3;1H'; sleep 30"],
            greps=[b"\x1b[?6h"], oracle_flags=[("origin_flag", "1")],
            check_visible=False)

        # 8. cursor styles (DECSCUSR) -- no capture format, grep the repaint
        visible_case(victim, "cursor_style", [
            "printf '\\033c\\033[4 qsteady-underline-cursor\\033[2;1H'; "
            "sleep 30"], greps=[b"\x1b[4 q"])

        # 9. cursor colour (OSC 12)
        visible_case(victim, "cursor_colour", [
            "printf '\\033c\\033]12;#ff8800\\033\\\\colour-cursor"
            "\\033[2;1H'; sleep 30"], greps=[b"\x1b]12;"])

        # 10. hidden cursor
        visible_case(victim, "hidden_cursor", [
            "printf '\\033chidden\\033[?25l\\033[2;1H'; sleep 30"],
            oracle_flags=[("cursor_flag", "0")], check_cursor=False)

        # 11. insert mode
        visible_case(victim, "insert", [
            "printf '\\033cinsert-mode\\033[4h\\033[3;1H'; sleep 30"],
            greps=[b"\x1b[4h"], oracle_flags=[("insert_flag", "1")])

        # 12. application keypad
        visible_case(victim, "app_keypad", [
            "printf '\\033ckeypad-app\\033=\\033[2;1H'; sleep 30"],
            greps=[b"\x1b="], oracle_flags=[("keypad_flag", "1")])

        # 13. application cursor keys
        visible_case(victim, "app_cursor_keys", [
            "printf '\\033ccursor-keys\\033[?1h\\033[2;1H'; sleep 30"],
            greps=[b"\x1b[?1h"], oracle_flags=[("keypad_cursor_flag", "1")])

        # 14. bracketed paste (no capture format)
        visible_case(victim, "bracketed_paste", [
            "printf '\\033cbracket\\033[?2004h\\033[2;1H'; sleep 30"],
            greps=[b"\x1b[?2004h"])

        # 15a. autowrap OFF
        visible_case(victim, "wrap_off", [
            "printf '\\033cnowrap\\033[?7l\\033[2;1H'; sleep 30"],
            greps=[b"\x1b[?7l"], oracle_flags=[("wrap_flag", "0")])

        # 15b. mouse modes (button tracking + SGR extended)
        visible_case(victim, "mouse", [
            "printf '\\033cmouse\\033[?1000h\\033[?1006h\\033[2;1H'; sleep 30"],
            greps=[b"\x1b[?1000h", b"\x1b[?1006h"],
            oracle_flags=[("mouse_standard_flag", "1")])

        # 19. empty screen
        visible_case(victim, "empty", ["printf '\\033c\\033[H'; sleep 30"])

        # 20. full screen exactly at pane size (24 rows filled)
        visible_case(victim, "full_screen", [
            "printf '\\033c'; for i in $(seq 1 24); do "
            "printf 'row-%02d-###############################################"
            "###############\\n' $i; done; printf '\\033[12;40H'; sleep 30"])

        # 3. wrapped logical line wider than the pane -> the scrollback replay
        #    must join it back into one logical line (no artificial hard break).
        #    The W line is produced first, then enough pad lines to scroll it
        #    into history and saturate the socket (so the repaint defers).
        # A byte filler fires the resync (repaint deferred behind the full
        # socket); the W line + pads are then produced during the resync, so
        # they scroll into history within the post-resync replay window. The
        # replay must join the wrapped rows back into one logical line.
        wrapped_case(victim, "wrapped")

        # 16. scrollback replay: numbered lines, ordered subsequence, no dups
        scrollback_case(victim, "scrollback",
                        "for i in $(seq 1 300); do echo SBLINE-$i; done",
                        prefix="SBLINE-", sentinel="SBDONE_SENTINEL")

        # 17. width change during gap -> repaint-only at the new width
        visible_case(victim, "width_change", [
            "printf '\\033cwidth-change-line\\033[3;50Hmark\\033[5;1H'; "
            "sleep 30"], resize=(100, 30))

        # 18. history trimmed (tiny limit) -> degrade to repaint-only, no crash
        visible_case(victim, "history_trim", [
            "printf '\\033ctrimmed-history\\033[2;1H'; sleep 30"],
            history_limit=2)
        alive = flag(OSOCK, "m:", "pid")
        check(bool(alive), "history_trim", "server not alive after degrade")

        # 23. tab-gap stale cells over pre-existing content. Custom tab stops
        #     with a<TAB>b<TAB>c<TAB>d leave gaps grid_string_cells serialises
        #     as literal TABs, which advance the cursor without clearing the
        #     cells under them, so the repaint must erase each line before
        #     painting. A width change skips the scrollback replay, so the
        #     dirty-filled oracle is the only content under the gaps and the
        #     check is deterministic: pre-fix the sentinel survives there.
        visible_case(victim, "tab_gap", [
            "printf '\\033c\\033[3g\\033[1;4H\\033H\\033[1;20H\\033H"
            "\\033[1;40H\\033H\\033[2;1Ha\\tb\\tc\\td\\033[6;1H'; sleep 30"],
            greps=[b"\x1b[3g"], resize=(100, 30), dirty=True)

        # 24. resize + scroll region + origin mode (deterministic seed-40 case).
        #     The pane is resized during the gap with a scroll region and origin
        #     mode active; the repaint re-enables origin mode before addressing
        #     the cursor, so an absolute row lands it one row off. The address
        #     must be region-relative. Checks the exact restored cursor.
        visible_case(victim, "resize_scroll_region", [
            "printf '\\033c\\033[9;12r\\033[?6h\\033[3;5Hregion-origin"
            "\\033[2;4H'; sleep 30"],
            resize=(100, 30), greps=[b"\x1b[9;12r", b"\x1b[?6h"],
            check_visible=False)

        # 25a. Lossless frontier -- COMPLETE: fewer scrolled lines than the
        #      history limit (N<L). Prefill the socket (non-scroll), scroll N,
        #      then a non-scroll blast to trip the byte backstop. The repaint
        #      must replay all N missed lines (1..N), no loss, no off-by-one.
        frontier_case(victim, "frontier_complete", "FL-",
                      "%s; for i in $(seq 1 40); do echo FL-$i; done; %s"
                      % (_noscroll(2500), _noscroll(1200)),
                      40, 80, "FCDONE", "complete")
        # 25b. Lossless frontier -- DEGRADE: far more scrolled lines than the
        #      history limit (N>>L). The line trigger fires; the replay is a
        #      bounded recent window with the oldest lines dropped (graceful
        #      gap, never a duplicate), still reaching the latest line.
        frontier_case(victim, "frontier_degrade", "DL-",
                      "%s; for i in $(seq 1 300); do echo DL-$i; done"
                      % _noscroll(2500),
                      300, 80, "FDDONE", "degrade")
    finally:
        victim.close()
        obs.terminate()

    if FAIL:
        sys.stderr.write("REPLAY FIDELITY FAILURES (%d):\n" % len(FAIL))
        for f in FAIL:
            sys.stderr.write("  - %s\n" % f)
        sys.exit(1)
    if os.environ.get("REPLAY_DEBUG"):
        sys.stderr.write("all replay cases passed\n")


if __name__ == "__main__":
    main()
PYEOF

exit 0
