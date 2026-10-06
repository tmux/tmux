#!/bin/sh

# Tests that the scroll position of a mode tree list (mode-tree.c) is kept
# when the list is rebuilt, using choose-buffer. Buffer mode rebuilds its list
# every time the status line is redrawn, which used to move the view so the
# selected line was on the bottom row (or the list jumped back to the top)
# even if the selected line was already visible. Also checks that when the
# list shrinks the view does not leave empty lines at the bottom.
#
# The list is drawn on a mode screen which capture-pane does not show, so - as
# in choose-buffer.sh - a second server provides a client: an inner "tmux
# attach" runs inside a pane of the second server, and that pane is captured
# to read what the inner client rendered.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMP=$(mktemp -d) || exit 1
TMUX_TMPDIR="$TMP"
export TMUX_TMPDIR
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

cleanup()
{
	$TMUX kill-server 2>/dev/null
	$TMUX2 kill-server 2>/dev/null
	rm -rf "$TMP"
}
trap cleanup EXIT

fail()
{
	echo "$1" >&2
	exit 1
}

# capture the screen rendered by the inner client
capture()
{
	$TMUX2 capture-pane -p -t out:0 2>/dev/null
}

# wait_for $marker
#
# Wait (up to ~10s) until the rendered screen contains $marker. The matching
# capture is left in CAPTURED.
wait_for()
{
	i=0
	while [ "$i" -lt 50 ]; do
		CAPTURED=$(capture)
		if printf '%s\n' "$CAPTURED" | grep -F -q "$1"; then
			return 0
		fi
		sleep 0.2
		i=$((i + 1))
	done
	fail "timed out waiting for '$1'"
}

# wait_gone $marker
#
# Wait (up to ~10s) until the rendered screen no longer contains $marker. The
# matching capture is left in CAPTURED.
wait_gone()
{
	i=0
	while [ "$i" -lt 50 ]; do
		CAPTURED=$(capture)
		if ! printf '%s\n' "$CAPTURED" | grep -F -q "$1"; then
			return 0
		fi
		sleep 0.2
		i=$((i + 1))
	done
	fail "timed out waiting for '$1' to disappear"
}

# wait_clients $n
#
# Wait (up to ~10s) until the test server has exactly $n clients attached.
wait_clients()
{
	i=0
	while [ "$i" -lt 10 ]; do
		c=$($TMUX list-clients -F x 2>/dev/null | grep -c x)
		[ "$c" -eq "$1" ] && return 0
		sleep 1
		i=$((i + 1))
	done
	return 1
}

# first_line
#
# Print the name of the first buffer shown in CAPTURED.
first_line()
{
	printf '%s\n' "$CAPTURED" | grep -F ': S1' | head -1 | \
	    sed 's/.*\(buf[0-9a-z]*\): S1.*/\1/'
}

$TMUX new-session -d -s aaa -x 80 -y 24 'cat' || exit 1

$TMUX2 new-session -d -s out -x 80 -y 24 "$TMUX attach -t aaa" || exit 1
wait_clients 1 || fail "no client attached to test server"

# Forty buffers, more than fit on the screen.
n=0
while [ "$n" -lt 40 ]; do
	$TMUX set-buffer -b "$(printf 'buf%02d' "$n")" "buffer $n" || exit 1
	n=$((n + 1))
done

# Without a preview the list has the whole pane (23 lines with the status
# line), so buf00 to buf22 are visible.
$TMUX choose-buffer -t aaa:0 -N -O name -F 'S1' || exit 1
wait_for 'buf00: S1'

# --- scrolling down moves the view -------------------------------------------
#
# Selecting buf30 scrolls so it is on the bottom row: buf08 is at the top.
$TMUX send-keys -t aaa:0 -N 30 Down || exit 1
wait_for 'buf30: S1'
top=$(first_line)
[ "$top" = "buf08" ] || fail "top line after scrolling is $top, not buf08"

# Moving up to buf20 stays inside the visible lines so the view does not move.
$TMUX send-keys -t aaa:0 -N 10 Up || exit 1

# --- a rebuild keeps the view where it is ------------------------------------
#
# Add a buffer that sorts into the visible part of the list and redraw the
# status line so the list is rebuilt. Once buf15a appears the rebuild has
# happened; buf08 must still be the top line. It used to jump back to buf00.
$TMUX set-buffer -b buf15a "new buffer" || exit 1
$TMUX refresh-client -S || exit 1
wait_for 'buf15a: S1'
top=$(first_line)
[ "$top" = "buf08" ] || fail "top line after rebuild is $top, not buf08"

# --- a shrinking list does not leave empty lines -----------------------------
#
# Go to the end of the list (buf39 is on the bottom row) and delete all but
# the last ten buffers. They now all fit, so the view goes to the top.
$TMUX send-keys -t aaa:0 End || exit 1
wait_for 'buf39: S1'
for b in $($TMUX list-buffers -F '#{buffer_name}'); do
	case "$b" in
	buf3?)
		;;
	*)
		$TMUX delete-buffer -b "$b" || exit 1
		;;
	esac
done
$TMUX refresh-client -S || exit 1
wait_gone 'buf29: S1'
top=$(first_line)
[ "$top" = "buf30" ] || fail "top line after shrinking is $top, not buf30"
[ "$(printf '%s\n' "$CAPTURED" | grep -F -c ': S1')" -eq 10 ] || \
	fail "expected 10 buffers after shrinking"

$TMUX send-keys -t aaa:0 q

exit 0
