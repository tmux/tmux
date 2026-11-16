#!/bin/sh

# Moving a pane within a window must preserve a modal pane's return target.

PATH=/bin:/usr/bin
TERM=screen
export TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"

cleanup()
{
	$TMUX kill-server >/dev/null 2>&1
}
trap cleanup EXIT

fail()
{
	echo "$*" >&2
	exit 1
}

check_ok()
{
	$TMUX "$@" || fail "command failed: $*"
}

check_active()
{
	active=$($TMUX display-message -p -t "$1" '#{pane_id}')
	[ "$active" = "$2" ] ||
		fail "active pane is $active, expected $2"
}

check_ok new-session -d -s modal -x 120 -y 40 'cat'
p0=$($TMUX display-message -p -t modal:0 '#{pane_id}')
p1=$($TMUX split-window -PF '#{pane_id}' -t "$p0" 'cat') ||
	fail "first split failed"
p2=$($TMUX split-window -PF '#{pane_id}' -t "$p1" 'cat') ||
	fail "second split failed"

# Both commands, with and without -d, must restore the moved pane after
# the modal closes. The modal must keep focus during the move.
for command in join-pane move-pane; do
	for detached in '' -d; do
		check_ok select-pane -t "$p2"
		modal=$($TMUX new-pane -OdPF '#{pane_id}' -t "$p0" \
		    -x 80 -y 10 'cat') || fail "new modal failed"
		check_active modal:0 "$modal"
		check_ok "$command" $detached -h -s "$p2" -t "$p0"
		check_active modal:0 "$modal"
		check_ok kill-pane -t "$modal"
		check_active modal:0 "$p0"
	done
done

# Moving a different pane also leaves the modal active until it closes.
modal=$($TMUX new-pane -OPF '#{pane_id}' -t "$p0" \
    -x 80 -y 10 'cat') || fail "new modal failed"
check_ok join-pane -h -s "$p1" -t "$p0"
check_active modal:0 "$modal"
check_ok kill-pane -t "$modal"
check_active modal:0 "$p0"

# Closing the modal selects the top pane remaining in its own window.
check_ok new-window -d -t modal:1 'cat'
modal=$($TMUX new-pane -OPF '#{pane_id}' -t "$p0" \
    -x 80 -y 10 'cat') || fail "new modal failed"
check_ok join-pane -d -h -s "$p2" -t modal:1
check_active modal:0 "$modal"
check_ok kill-pane -t "$modal"
active=$($TMUX display-message -p -t modal:0 '#{pane_id}')
case "$active" in
"$p0"|"$p1") ;;
*) fail "modal returned to a pane outside its window: $active" ;;
esac
window=$($TMUX display-message -p -t "$p2" '#{window_index}')
[ "$window" = 1 ] || fail "pane did not move to the other window"
