#!/bin/sh

# The side status line must refresh every status-interval seconds on its own,
# whether or not the horizontal status line is on, and whether it was on when
# the client attached or turned on afterwards. The client status timer drives
# both lines, so this checks it is armed in both cases with status off.
#
# The side status line is rendered by an inner tmux attached inside an outer
# tmux pane. Its format is a #() command which appends a byte to a file each
# time it runs, and a #() command is run again only when the format is
# expanded in a later second, so the file grows once per refresh. Nothing in
# the test asks for a redraw between the two counts, so growth means the
# timer refreshed the side status line by itself.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

COUNT1=$(mktemp)
COUNT2=$(mktemp)
trap "rm -f $COUNT1 $COUNT2; $TMUX kill-server 2>/dev/null; \
	$TMUX2 kill-server 2>/dev/null" 0 1 15

fail() {
	echo "$*" >&2
	exit 1
}

count() {
	wc -c <"$1" | tr -d ' '
}

# Attach the inner server in an outer pane, then wait for the first draw.
attach() {
	$TMUX new -d -x30 -y8 "$TMUX2 attach" || exit 1
	sleep 1
}

# Check the side status line refreshed at least twice in three seconds.
refreshes() {
	before=$(count "$1")
	sleep 3
	after=$(count "$1")
	[ $((after - before)) -ge 2 ] ||
	    fail "$2: side status ran $before then $after times, did not refresh"
}

$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

# Side status on before the client attaches, with status off.
$TMUX2 new -d -x30 -y8 "sh -c 'exec sleep 100'" || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 set -g status-interval 1 || exit 1
$TMUX2 set -g side-status left || exit 1
$TMUX2 set -g side-status-format "#(printf x >>$COUNT1)" || exit 1
attach
refreshes "$COUNT1" "on at attach"

# Side status turned on after the client attached, with status off. A new
# inner server so the client timer starts from nothing.
$TMUX2 kill-server || exit 1
sleep 1
$TMUX2 new -d -x30 -y8 "sh -c 'exec sleep 100'" || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 set -g status-interval 1 || exit 1
$TMUX2 set -g side-status-format "#(printf x >>$COUNT2)" || exit 1
attach
$TMUX2 set -g side-status left || exit 1
sleep 1
refreshes "$COUNT2" "on after attach"

exit 0
