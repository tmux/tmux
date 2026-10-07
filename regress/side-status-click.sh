#!/bin/sh

# Clicking a window on the side status line, including the current window,
# dispatches the Status binding with that window.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Lside-click-inner-$$ -f/dev/null"
TMUX2="$TEST_TMUX -Lside-click-outer-$$ -f/dev/null"

cleanup()
{
	$TMUX2 kill-server >/dev/null 2>&1
	$TMUX kill-server >/dev/null 2>&1
}
trap cleanup 0 1 15

fail()
{
	echo "$*" >&2
	exit 1
}

# Click row $1 of the side status line and check the binding got $2.
click()
{
	$TMUX set -g @click none || exit 1
	seq=$(printf '\033[<0;3;%sM\033[<0;3;%sm' "$1" "$1")
	$TMUX2 send-keys -t outer:0.0 -l "$seq" || fail "send mouse failed"
	sleep 1
	got=$($TMUX show -gv @click)
	[ "$got" = "$2" ] || fail "row $1: got '$got', expected '$2'"
}

$TMUX new-session -d -s inner -n zero -x 60 -y 20 'sleep 100' || exit 1
$TMUX new-window -d -n one 'sleep 100' || exit 1
$TMUX new-window -d -n two 'sleep 100' || exit 1
$TMUX set -g mouse on || exit 1
$TMUX set -g status off || exit 1
$TMUX set -g side-status left || exit 1
$TMUX bind -n MouseDown1Status \
    set -gF @click '#{mouse_status_range} #{window_name}' || exit 1
$TMUX bind -n MouseDown1StatusDefault set -g @click default || exit 1
$TMUX2 new-session -d -s outer -x 60 -y 20 'sleep 100' || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 respawn-pane -k -t outer:0.0 "$TMUX attach -t inner" || exit 1
sleep 1

# The current window is first, then in the middle of the list.
click 1 'window zero'
click 2 'window one'
click 4 default
$TMUX select-window -t inner:1 || exit 1
sleep 0.5
click 1 'window zero'
click 2 'window one'
click 3 'window two'

exit 0
