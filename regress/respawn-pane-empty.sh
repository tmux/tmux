#!/bin/sh

# respawn-pane -k -E leaves an empty pane, which must not be treated as exited
# when the process it replaced is reaped

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
$TMUX kill-server 2>/dev/null
sleep 1

# The only pane of the only session: the server must not exit.
$TMUX new -d 'sleep 1000' || exit 1
$TMUX respawn-pane -k -E || exit 1
sleep 1
$TMUX has-session -t0 2>/dev/null || exit 1
$TMUX kill-server 2>/dev/null
sleep 1

# With remain-on-exit the pane must not be dead or keep the old tty.
$TMUX new -d 'sleep 1000' \; set -g remain-on-exit on || exit 1
$TMUX respawn-pane -k -E || exit 1
sleep 1
[ "$($TMUX display -p '#{pane_dead}:#{pane_tty}')" = "0:" ] || exit 1
$TMUX kill-server 2>/dev/null

exit 0
