#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen
LANG=C.UTF-8
LC_ALL=C.UTF-8
export TERM LANG LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"

TMPDIR=${TMPDIR:-/tmp}/tmux-local-pane.$$
mkdir -p "$TMPDIR" || exit 1
IN1="$TMPDIR/in1"
IN2="$TMPDIR/in2"
OUT1="$TMPDIR/out1"
OUT2="$TMPDIR/out2"

cleanup()
{
	$TMUX kill-server 2>/dev/null
	rm -rf "$TMPDIR"
}
trap cleanup 0 1 15

fail()
{
	echo "$1"
	exit 1
}

wait_for()
{
	file=$1
	pattern=$2
	i=0
	while [ "$i" -lt 20 ]; do
		if grep -q "$pattern" "$file"; then
			return 0
		fi
		sleep 1
		i=$((i + 1))
	done
	echo "missing '$pattern' in $file"
	cat "$file"
	return 1
}

send1()
{
	printf '%s\n' "$*" >&3
}

send2()
{
	printf '%s\n' "$*" >&4
}

check_fmt()
{
	out=$($TMUX display-message -p -t "$1" "$2" 2>&1)
	if [ "$out" != "$3" ]; then
		echo "Format '$2' for '$1' wrong."
		echo "Expected: '$3'"
		echo "But got:  '$out'"
		exit 1
	fi
}

$TMUX kill-server 2>/dev/null
$TMUX new-session -d -s active -x 80 -y 24 || exit 1
$TMUX split-window -d -h -t active:0 || exit 1
$TMUX split-window -d -v -t active:0.0 || exit 1
style=$($TMUX show-options -wgv pane-active-border-style) || exit 1
case "$style" in
*pane_local_active*themecyan*) ;;
*) fail "pane-active-border-style does not include local active colour" ;;
esac

p0=$($TMUX display-message -p -t active:0.0 '#{pane_id}') || exit 1
p1=$($TMUX display-message -p -t active:0.1 '#{pane_id}') || exit 1
p2=$($TMUX display-message -p -t active:0.2 '#{pane_id}') || exit 1
$TMUX select-pane -t "$p0" || exit 1
check_fmt active:0 '#{pane_id}' "$p0"

mkfifo "$IN1" "$IN2" || exit 1
: >"$OUT1"
: >"$OUT2"
$TMUX -C attach-session -t active <"$IN1" >"$OUT1" 2>&1 &
PID1=$!
exec 3>"$IN1"

send1 'display-message -p C1_READY'
wait_for "$OUT1" 'C1_READY' || exit 1

# One attached client cannot enter local mode.
send1 'select-pane -s'
send1 "select-pane -t $p1"
send1 'display-message -p "C1_SINGLE:#{pane_id}:#{pane_active}:#{pane_local_active}"'
wait_for "$OUT1" "C1_SINGLE:$p1:1:" || exit 1
check_fmt active:0 '#{pane_id}' "$p1"
send1 "select-pane -t $p0"
check_fmt active:0 '#{pane_id}' "$p0"

$TMUX -C attach-session -t active <"$IN2" >"$OUT2" 2>&1 &
PID2=$!
exec 4>"$IN2"

send2 'display-message -p C2_READY'
wait_for "$OUT2" 'C2_READY' || exit 1

# Both clients start in shared mode and see the window active pane.
send1 'display-message -p "C1_START:#{pane_id}:#{pane_active}:#{pane_local_active}"'
send2 'display-message -p "C2_START:#{pane_id}:#{pane_active}:#{pane_local_active}"'
wait_for "$OUT1" "C1_START:$p0:1:" || exit 1
wait_for "$OUT2" "C2_START:$p0:1:" || exit 1

# Client 1 enters local mode and selects a pane without changing shared state.
send1 'select-pane -s'
send1 "select-pane -t $p1"
send1 'display-message -p "C1_LOCAL:#{pane_id}:#{pane_active}:#{pane_local_active}:#{pane_local_mode}"'
send1 "display-message -p -t $p0 \"C1_LOCAL_INACTIVE:#{pane_id}:#{pane_active}:#{pane_local_active}\""
wait_for "$OUT1" "C1_LOCAL:$p1:1:1:1" || exit 1
wait_for "$OUT1" "C1_LOCAL_INACTIVE:$p0:0:0" || exit 1
check_fmt active:0 '#{pane_id}' "$p0"
send2 'display-message -p "C2_SHARED:#{pane_id}:#{pane_active}:#{pane_local_active}"'
wait_for "$OUT2" "C2_SHARED:$p0:1:" || exit 1

# Client 2 can keep an independent local pane in the same window.
send2 'select-pane -s'
send2 "select-pane -t $p2"
send2 'display-message -p "C2_LOCAL:#{pane_id}:#{pane_active}:#{pane_local_active}"'
wait_for "$OUT2" "C2_LOCAL:$p2:1:1" || exit 1
send1 'display-message -p "C1_STILL:#{pane_id}:#{pane_active}:#{pane_local_active}"'
wait_for "$OUT1" "C1_STILL:$p1:1:1" || exit 1

# Local last-pane history is independent of the shared window history.
send1 "select-pane -t $p2"
send1 'last-pane'
send1 'display-message -p "C1_LAST:#{pane_id}:#{pane_last}"'
wait_for "$OUT1" "C1_LAST:$p1:0" || exit 1
check_fmt active:0 '#{pane_id}' "$p0"

# Switching back to shared mode immediately follows window->active.
send1 'select-pane -S'
send1 'display-message -p "C1_SHARED_AGAIN:#{pane_id}:#{pane_active}"'
wait_for "$OUT1" "C1_SHARED_AGAIN:$p0:1" || exit 1
send1 "select-pane -t $p1"
check_fmt active:0 '#{pane_id}' "$p1"
send2 'display-message -p "C2_STILL_LOCAL:#{pane_id}:#{pane_active}"'
wait_for "$OUT2" "C2_STILL_LOCAL:$p2:1" || exit 1

# Local selection survives switching away and back to the window.
send2 'new-window -n other'
send2 'previous-window'
send2 'display-message -p "C2_AFTER_WINDOW_SWITCH:#{pane_id}:#{pane_active}"'
wait_for "$OUT2" "C2_AFTER_WINDOW_SWITCH:$p2:1" || exit 1

# Removing a locally active pane recovers to the shared active pane.
$TMUX kill-pane -t "$p2" || exit 1
send2 'display-message -p "C2_AFTER_KILL:#{pane_id}:#{pane_active}"'
wait_for "$OUT2" "C2_AFTER_KILL:$p1:1" || exit 1

# Local selection of a floating pane does not raise it unless requested.
f1=$($TMUX new-pane -dPF '#{pane_id}' -x 20 -y 6 -X 8 -Y 3 \
    -t active:0 'sleep 100') || exit 1
f2=$($TMUX new-pane -dPF '#{pane_id}' -x 20 -y 6 -X 12 -Y 5 \
    -t active:0 'sleep 100') || exit 1
check_fmt "$f1" '#{pane_z}' 1
check_fmt "$f2" '#{pane_z}' 0
send1 'select-pane -s'
send1 "select-pane -t $f1"
send1 "display-message -p -t $f1 \"C1_FLOAT_LOCAL:#{pane_id}:#{pane_active}:#{pane_z}\""
wait_for "$OUT1" "C1_FLOAT_LOCAL:$f1:1:1" || exit 1
check_fmt "$f1" '#{pane_z}' 1
send1 "select-pane -M -t $f1"
send1 "display-message -p -t $f1 \"C1_FLOAT_BORDER:#{pane_id}:#{pane_active}:#{pane_z}\""
wait_for "$OUT1" "C1_FLOAT_BORDER:$f1:1:1" || exit 1
check_fmt "$f1" '#{pane_z}' 1
send1 "move-pane -P front -t $f1"
send1 "display-message -p -t $f1 \"C1_FLOAT_RAISE:#{pane_z}\""
wait_for "$OUT1" "C1_FLOAT_RAISE:0" || exit 1
check_fmt "$f1" '#{pane_z}' 0
$TMUX kill-pane -t "$f1" || exit 1
$TMUX kill-pane -t "$f2" || exit 1

# When only one client remains, local mode is cleared.
send1 'select-pane -s'
send1 "select-pane -t $p0"
send1 'display-message -p "C1_LOCAL_BEFORE_DETACH:#{pane_id}:#{pane_local_active}"'
wait_for "$OUT1" "C1_LOCAL_BEFORE_DETACH:$p0:1" || exit 1
kill "$PID2" 2>/dev/null
wait "$PID2" 2>/dev/null
send1 'display-message -p "C1_AFTER_DETACH:#{pane_id}:#{pane_local_active}"'
wait_for "$OUT1" "C1_AFTER_DETACH:$p1:" || exit 1

kill "$PID1" 2>/dev/null
wait "$PID1" 2>/dev/null
$TMUX display-message -p alive >/dev/null 2>&1 || fail "server died"

exit 0
