#!/bin/sh

# The style at the end of a side status row carries over to the next row, as
# if the rows were one line: attributes removed from side-status-style stay
# removed and a pushed default stays pushed. The inner client runs in an
# outer pane which is captured with its attributes.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Lside-style-inner-$$ -f/dev/null"
TMUX2="$TEST_TMUX -Lside-style-outer-$$ -f/dev/null"

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

# Check the second side status row starts with $2.
row2()
{
	got=$($TMUX2 capture-pane -ep -t outer:0.0 | sed -n 2p)
	case "$got" in
	"$2"*) ;;
	*) fail "$1: row 2 is '$got'" ;;
	esac
}

$TMUX new-session -d -s inner -x 40 -y 10 'sleep 100' || exit 1
$TMUX set -g status off || exit 1
$TMUX set -g side-status left || exit 1
$TMUX2 new-session -d -s outer -x 40 -y 10 'sleep 100' || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 respawn-pane -k -t outer:0.0 "$TMUX attach -t inner" || exit 1
sleep 1

ESC=$(printf '\033')

$TMUX set -g side-status-style bold || exit 1
$TMUX set -g side-status-format '#[nobold]one#[nl]two' || exit 1
sleep 1
row2 "nobold" "two"

$TMUX set -g side-status-style default || exit 1
$TMUX set -g side-status-format \
    '#[fg=red]#[push-default]one#[nl]#[fg=blue]#[default]two' || exit 1
sleep 1
row2 "push-default" "$ESC[31mtwo"

exit 0
