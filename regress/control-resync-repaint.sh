#!/bin/sh

# A resynced pane is repainted. A control client reads briefly, stalls (stops
# reading long enough to trigger the watchdog), then resumes. The delivered
# repaint arrives as a %output line beginning a DEC 2026 synchronized update,
# and the server log records entering and completing the resync.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

MARK="control_resync_repaint_producer_$$"
WD=$(mktemp -d)
OUT=$(mktemp)
trap 'pkill -f "$MARK" 2>/dev/null; $TMUX kill-server 2>/dev/null; rm -rf "$WD"; rm -f "$OUT"' 0 1 15

CMD="i=0; while :; do i=\$((i+1)); dd if=/dev/zero bs=262144 count=1 2>/dev/null | tr '\\0' x; done # $MARK"

# Run the server with logging from its own directory so tmux-server-*.log is
# isolated and trivial to clean.
cd "$WD" || exit 1
# A small history-limit makes the pane's history the resync frontier: the
# producer scrolls far more than this almost at once, so once the client stops
# reading it trips the missed-lines trigger quickly (the byte backstop and the
# no-progress watchdog would catch it too). Set it in the config so the pane's
# grid is created with it.
echo "set -g history-limit 50" >conf || exit 1
$TMUX -vv -fconf new -d -x80 -y24 "$CMD" || exit 1
$TMUX set -g control-resync on || exit 1
sleep 1

# Read ~1s, stop reading ~8s (forcing a stall and hence a resync), then resume
# ~4s to collect the delivered repaint. python3 gives a deterministic
# read/stop/resume that plain shell redirection cannot.
python3 - "$TEST_TMUX" test "$OUT" <<'PYEOF' || exit 1
import sys, os, select, subprocess, time
tmux, label, outf = sys.argv[1], sys.argv[2], sys.argv[3]
p = subprocess.Popen([tmux, "-L", label, "-C", "attach"],
                     stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                     stderr=subprocess.DEVNULL)
fd = p.stdout.fileno()
buf = bytearray()
def drain(dur):
    end = time.time() + dur
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.1)
        if r:
            d = os.read(fd, 65536)
            if not d:
                break
            buf.extend(d)
try:
    drain(1.0)
    time.sleep(8.0)
    drain(4.0)
    with open(outf, "wb") as f:
        f.write(buf)
finally:
    try: p.stdin.close()
    except Exception: pass
    try: p.terminate()
    except Exception: pass
    try: p.wait(timeout=3)
    except Exception:
        try: p.kill()
        except Exception: pass
PYEOF

# The repaint begins a DEC 2026 synchronized update, so the raw %output stream
# contains the literal 2026h.
grep -aq "2026h" "$OUT" || exit 1
# The server log records the pane entering resync and the repaint being
# delivered.
grep -aq "control_pane_resync:" "$WD"/tmux-server-*.log || exit 1
grep -aq "resync of %.* delivered" "$WD"/tmux-server-*.log || exit 1

pkill -f "$MARK" 2>/dev/null
$TMUX kill-server 2>/dev/null

exit 0
