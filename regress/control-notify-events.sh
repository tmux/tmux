#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
LANG=C.UTF-8
export TERM LC_ALL LANG

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"

TMPDIR=$(mktemp -d)
IN="$TMPDIR/in"
OUT="$TMPDIR/out"
PID=

cleanup()
{
	[ -n "$PID" ] && kill "$PID" 2>/dev/null
	$TMUX kill-server 2>/dev/null
	rm -rf "$TMPDIR"
}
trap cleanup EXIT

wait_for()
{
	pattern=$1
	timeout=${2:-6}
	i=0

	while [ "$i" -lt "$timeout" ]; do
		if grep -F -- "$pattern" "$OUT" >/dev/null 2>&1; then
			return 0
		fi
		sleep 1
		i=$((i + 1))
	done
	echo "missing: $pattern"
	cat "$OUT"
	return 1
}

wait_for_count()
{
	pattern=$1
	expected=$2
	timeout=${3:-6}
	i=0

	while [ "$i" -lt "$timeout" ]; do
		count=$(grep -F -- "$pattern" "$OUT" 2>/dev/null | wc -l)
		if [ "$count" -ge "$expected" ]; then
			return 0
		fi
		sleep 1
		i=$((i + 1))
	done
	echo "missing count $expected for: $pattern"
	cat "$OUT"
	return 1
}

send()
{
	printf '%s\n' "$*" >&3
}

$TMUX kill-server 2>/dev/null
$TMUX new-session -d -s notify -x 80 -y 24 || exit 1
wid=$($TMUX display-message -p -t notify:0 '#{window_id}') || exit 1
pane=$($TMUX display-message -p -t notify:0.0 '#{pane_id}') || exit 1

mkfifo "$IN"
: >"$OUT"
$TMUX -C attach-session -t notify <"$IN" >"$OUT" 2>&1 &
PID=$!
exec 3>"$IN"

send 'display-message -p ready'
wait_for 'ready' 3 || exit 1

send 'rename-window notify-renamed'
wait_for "%window-renamed $wid notify-renamed" || exit 1

send 'split-window -h'
wait_for "%layout-change $wid " || exit 1

send 'select-pane -t:.0'
wait_for "%window-pane-changed $wid $pane" || exit 1

send 'copy-mode -t:.0'
wait_for "%pane-mode-changed $pane" || exit 1

sessions_changed=$(grep -F -- '%sessions-changed' "$OUT" 2>/dev/null | \
	wc -l)
send 'new-session -d -s notify-extra'
wait_for_count '%sessions-changed' $((sessions_changed + 1)) || exit 1

send 'kill-session -t notify-extra'
wait_for_count '%sessions-changed' $((sessions_changed + 2)) || exit 1

exit 0
