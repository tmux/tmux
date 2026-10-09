#!/bin/sh

# Removing the active pane must not move a queued layout hook to another
# session. Check both the event formats and a command's implicit target.
PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
OUT=$(mktemp -d)
TMUX_TMPDIR="$OUT"
export TMUX_TMPDIR
TMUX="$TEST_TMUX -Ltest$$ -f/dev/null"

cleanup()
{
	$TMUX kill-server 2>/dev/null || true
	rm -rf "$OUT"
}
trap cleanup EXIT

fail()
{
	echo "$*" >&2
	exit 1
}

window=$($TMUX new -d -s main -P -F '#{window_id}' /bin/cat) ||
	fail "new-session main failed"
pane=$($TMUX splitw -t main:0 -P -F '#{pane_id}' /bin/cat) ||
	fail "split-window failed"
$TMUX new -d -s other /bin/cat || fail "new-session other failed"
$TMUX set-hook -g window-layout-changed \
	'set -w @layout-ran 1 ; set -gF @layout-context "#{hook_window}:#{window_id}"' ||
	fail "set-hook failed"
$TMUX kill-pane -t "$pane" || fail "kill-pane failed"

i=0
while [ $i -lt 30 ]; do
	context=$($TMUX show -gqv @layout-context)
	[ -n "$context" ] && break
	i=$((i + 1))
	sleep 0.1
done
[ "$context" = "$window:$window" ] ||
	fail "expected layout context $window:$window, got $context"
ran=$($TMUX show -wqv -t main:0 @layout-ran)
[ "$ran" = 1 ] || fail "layout hook did not target main window"
ran=$($TMUX show -wqv -t other:0 @layout-ran)
[ -z "$ran" ] || fail "layout hook modified unrelated window"
