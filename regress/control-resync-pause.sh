#!/bin/sh

# pause-after policy under resync. A pause-after control client that stalls is
# handled per the control-resync option: 'on' preempts it with a resync and does
# NOT pause it, while 'keep-pause' keeps it on the %pause path.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

MARK="control_resync_pause_producer_$$"
WDA=$(mktemp -d)
WDB=$(mktemp -d)
trap 'pkill -f "$MARK" 2>/dev/null; $TMUX kill-server 2>/dev/null; rm -rf "$WDA" "$WDB"' 0 1 15

CMD="i=0; while :; do i=\$((i+1)); dd if=/dev/zero bs=262144 count=1 2>/dev/null | tr '\\0' x; done # $MARK"

# A pause-after control client that stalls: set pause-after=1s, read ~1s, stop
# reading ~8s (past the watchdog timeout), then resume ~4s.
run_client() {
	python3 - "$TEST_TMUX" test <<'PYEOF'
import sys, os, select, subprocess, time
tmux, label = sys.argv[1], sys.argv[2]
p = subprocess.Popen([tmux, "-L", label, "-C", "attach"],
                     stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                     stderr=subprocess.DEVNULL)
fd = p.stdout.fileno()
try:
    p.stdin.write(b"refresh-client -f pause-after=1\n")
    p.stdin.flush()
    def drain(dur):
        end = time.time() + dur
        while time.time() < end:
            r, _, _ = select.select([fd], [], [], 0.1)
            if r and not os.read(fd, 65536):
                break
    drain(1.0)
    time.sleep(8.0)
    drain(4.0)
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
}

# (a) control-resync on: the pause-after client is preempted with a resync, so
# the log shows the resync and no %pause is ever written for the pane.
cd "$WDA" || exit 1
$TMUX -vv -f/dev/null new -d -x80 -y24 "$CMD" || exit 1
$TMUX set -g control-resync on || exit 1
sleep 1
run_client || exit 1
$TMUX kill-server 2>/dev/null
pkill -f "$MARK" 2>/dev/null
sleep 1
grep -aq "control_pane_resync:" "$WDA"/tmux-server-*.log || exit 1
grep -aq "%pause %" "$WDA"/tmux-server-*.log && exit 1

# (b) control-resync keep-pause: the same client stays on the %pause path, so a
# %pause is written for the pane.
cd "$WDB" || exit 1
$TMUX kill-server 2>/dev/null
sleep 1
$TMUX -vv -f/dev/null new -d -x80 -y24 "$CMD" || exit 1
$TMUX set -g control-resync keep-pause || exit 1
sleep 1
run_client || exit 1
$TMUX kill-server 2>/dev/null
pkill -f "$MARK" 2>/dev/null
sleep 1
grep -aq "%pause %" "$WDB"/tmux-server-*.log || exit 1

exit 0
