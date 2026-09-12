#!/bin/sh

# window-size manual-or-smallest uses the stored manual size unless an
# attached client is smaller, in which case the smallest client wins.
# Width and height are capped independently so a tall-and-narrow client
# shrinks only the dimension that is actually smaller.
#
# Also covers: two clients at once, a live refresh-client -C, using the
# creation size as the cap (no resize-window), and resize-window resetting
# the option back to manual.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
trap 'rm -f "$TMP"; $TMUX kill-server 2>/dev/null' 0 1 15

fail()
{
	echo "$@"
	exit 1
}

window_size()
{
	$TMUX display -t t -p '#{window_width}x#{window_height}'
}

manual_size()
{
	$TMUX display -t t -p '#{window_manual_width}x#{window_manual_height}'
}

# Wait until the window reports $1, or fail after a short poll.
wait_size()
{
	want=$1
	n=0
	while [ $n -lt 30 ]; do
		got=$(window_size 2>/dev/null) || got=
		[ "$got" = "$want" ] && return 0
		sleep 0.1
		n=$((n + 1))
	done
	fail "expected $want, got $got"
}

# Keep a control client attached at a given size in the background.
attach_sized()
{
	sx=$1
	sy=$2
	(echo "refresh-client -C ${sx},${sy}"; sleep 8) |
		$TMUX -f/dev/null -C attach -t t >>$TMP 2>&1 &
}

$TMUX -f/dev/null new -d -s t -x 100 -y 50 || exit 1
$TMUX resizew -t t -x 100 -y 50 || exit 1
$TMUX set -w -t t window-size manual-or-smallest || exit 1

# Detached: stay at the manual size and expose it via formats.
[ "$(window_size)" = "100x50" ] || fail "detached size $(window_size)"
[ "$(manual_size)" = "100x50" ] || fail "manual format $(manual_size)"

# A larger client must not grow the window past the manual size.
attach_sized 120 60
wait_size 100x50

# A smaller client shrinks the window.
attach_sized 40 10
wait_size 40x10

# Width and height are independent: a wide-but-short client only shrinks
# height, a tall-but-narrow client only shrinks width.
$TMUX kill-server 2>/dev/null
$TMUX -f/dev/null new -d -s t -x 100 -y 50 || exit 1
$TMUX resizew -t t -x 100 -y 50 || exit 1
$TMUX set -w -t t window-size manual-or-smallest || exit 1

attach_sized 120 20
wait_size 100x20

$TMUX kill-server 2>/dev/null
$TMUX -f/dev/null new -d -s t -x 100 -y 50 || exit 1
$TMUX resizew -t t -x 100 -y 50 || exit 1
$TMUX set -w -t t window-size manual-or-smallest || exit 1

attach_sized 30 80
wait_size 30x50

# After the small client goes away, grow back to the manual size.
$TMUX kill-server 2>/dev/null
$TMUX -f/dev/null new -d -s t -x 100 -y 50 || exit 1
$TMUX resizew -t t -x 100 -y 50 || exit 1
$TMUX set -w -t t window-size manual-or-smallest || exit 1

attach_sized 40 10
wait_size 40x10
$TMUX detach-client -s t || exit 1
wait_size 100x50

# Two clients at once: smallest wins, still never above the manual cap.
$TMUX kill-server 2>/dev/null
$TMUX -f/dev/null new -d -s t -x 100 -y 50 || exit 1
$TMUX resizew -t t -x 100 -y 50 || exit 1
$TMUX set -w -t t window-size manual-or-smallest || exit 1

attach_sized 120 60
wait_size 100x50
attach_sized 40 10
wait_size 40x10
[ "$(window_size)" = "40x10" ] || fail "two clients $(window_size)"

# Live refresh-client -C on an already-attached client.
$TMUX kill-server 2>/dev/null
$TMUX -f/dev/null new -d -s t -x 100 -y 50 || exit 1
$TMUX resizew -t t -x 100 -y 50 || exit 1
$TMUX set -w -t t window-size manual-or-smallest || exit 1

attach_sized 120 60
wait_size 100x50
client=
n=0
while [ $n -lt 30 ]; do
	client=$($TMUX lsc -F '#{client_name}' 2>/dev/null | head -1) || client=
	[ -n "$client" ] && break
	sleep 0.1
	n=$((n + 1))
done
[ -n "$client" ] || fail "no control client to refresh"
$TMUX refresh-client -t "$client" -C 40,10 || exit 1
wait_size 40x10

# Creation size is the cap when resize-window was never used.
$TMUX kill-server 2>/dev/null
$TMUX -f/dev/null new -d -s bootstrap || exit 1
$TMUX set -g window-size manual-or-smallest || exit 1
$TMUX new -d -s t -x 80 -y 24 || exit 1
$TMUX kill-session -t bootstrap || exit 1
[ "$(window_size)" = "80x24" ] || fail "created size $(window_size)"
[ "$(manual_size)" = "80x24" ] || fail "created manual $(manual_size)"
attach_sized 40 10
wait_size 40x10

# resize-window switches the option back to manual; re-set to continue.
$TMUX kill-server 2>/dev/null
$TMUX -f/dev/null new -d -s t -x 100 -y 50 || exit 1
$TMUX resizew -t t -x 100 -y 50 || exit 1
$TMUX set -w -t t window-size manual-or-smallest || exit 1
$TMUX resizew -t t -x 90 -y 40 || exit 1
opt=$($TMUX show -wv -t t window-size)
[ "$opt" = "manual" ] || fail "resizew left window-size=$opt"
[ "$(window_size)" = "90x40" ] || fail "after resizew $(window_size)"
$TMUX set -w -t t window-size manual-or-smallest || exit 1
attach_sized 40 10
wait_size 40x10

exit 0
