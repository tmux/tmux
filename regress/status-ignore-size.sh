#!/bin/sh

# A client attached with ignore-size that is too small for the status line or
# the side status line must have them turned off rather than kill the server.
# Each inner client runs in an outer pane of its size.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Lstatus-ignore-inner-$$ -f/dev/null"
TMUX2="$TEST_TMUX -Lstatus-ignore-outer-$$ -f/dev/null"

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

# Attach a normal client of size $1 and an ignore-size client of size $2
# (WxH) to the inner session.
attach()
{
	$TMUX2 kill-server >/dev/null 2>&1
	$TMUX2 new-session -d -s big -x "${1%x*}" -y "${1#*x}" 'sleep 100' ||
	    exit 1
	$TMUX2 set -g status off || exit 1
	$TMUX2 set -g window-size manual || exit 1
	$TMUX2 new-session -d -s small -x "${2%x*}" -y "${2#*x}" 'sleep 100' ||
	    exit 1
	$TMUX2 respawn-pane -k -t big:0.0 "$TMUX attach -t inner" || exit 1
	sleep 0.5
	$TMUX2 respawn-pane -k -t small:0.0 \
	    "$TMUX attach -f ignore-size -t inner" || exit 1
	sleep 1
}

$TMUX new-session -d -s inner -x 80 -y 24 'sleep 100' || exit 1

# A status line taller than the client.
$TMUX set -g status 5 || exit 1
attach 80x24 80x4
$TMUX has-session 2>/dev/null || fail "server exited: status line too tall"

# A side status line wider than the client.
$TMUX set -g status on || exit 1
$TMUX set -g side-status left || exit 1
attach 80x24 13x24
$TMUX has-session 2>/dev/null || fail "server exited: side status too wide"

exit 0
