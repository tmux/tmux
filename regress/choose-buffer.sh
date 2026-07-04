#!/bin/sh

# Tests of buffer mode (window-buffer.c) as driven by choose-buffer: that the
# -f filter removes buffers that do not match (by name and by content), that
# a filter matching nothing falls back to showing everything, that -O and -r
# change the sort order, that d deletes the selected buffer and C-t and D
# delete all tagged buffers, and that Enter runs the default command
# (paste-buffer) with the selected buffer.
#
# The list is drawn on a mode screen which capture-pane does not show, so a
# second server provides a client: an inner "tmux attach" runs inside a pane
# of the second server, and that pane is captured to read what the inner
# client rendered. Each choose-buffer call uses a distinct -F marker so a
# capture can be tied to the call it belongs to.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
TMUX2="$TEST_TMUX -Ltest2 -f/dev/null"

cleanup()
{
	$TMUX kill-server 2>/dev/null
	$TMUX2 kill-server 2>/dev/null
}
fail()
{
	echo "$1"
	cleanup
	exit 1
}

# capture the screen rendered by the inner client
capture()
{
	$TMUX2 capture-pane -p -t out:0 2>/dev/null
}

# wait_for $marker
#
# Wait (up to ~10s) until the rendered screen contains $marker, so the
# capture is known to show the mode instance under test.
wait_for()
{
	i=0
	while [ "$i" -lt 50 ]; do
		if capture | grep -q "$1"; then
			sleep 0.2
			return 0
		fi
		sleep 0.2
		i=$((i + 1))
	done
	fail "timed out waiting for '$1'"
}

# wait_buffers $n
#
# Wait (up to ~10s) until the test server has exactly $n paste buffers.
wait_buffers()
{
	i=0
	while [ "$i" -lt 50 ]; do
		c=$($TMUX list-buffers -F x 2>/dev/null | grep -c x)
		[ "$c" -eq "$1" ] && return 0
		sleep 0.2
		i=$((i + 1))
	done
	fail "expected $1 buffers, have $c"
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

$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

$TMUX new-session -d -s aaa -x 80 -y 24 || exit 1

$TMUX2 new-session -d -s out -x 80 -y 24 "$TMUX attach -t aaa" || exit 1
wait_clients 1 || fail "no client attached to test server"

# Two named buffers with distinct contents; bufa is created first.
$TMUX set-buffer -b bufa "hello buffer" || exit 1
$TMUX set-buffer -b bufz "other buffer" || exit 1

# --- filter by buffer name ---------------------------------------------------
$TMUX choose-buffer -t aaa:0 -F 'B1' -f '#{==:#{buffer_name},bufa}' || exit 1
wait_for 'B1'
out=$(capture)
echo "$out" | grep -q 'bufa: B1' || fail "bufa missing when it matches"
echo "$out" | grep -q 'bufz: B1' && fail "bufz shown but does not match"
[ "$(echo "$out" | grep -c ': B1')" -eq 1 ] || fail "expected 1 buffer"
$TMUX send-keys -t aaa:0 q

# --- filter by buffer content ------------------------------------------------
$TMUX choose-buffer -t aaa:0 -F 'B2' -f '#{m:*hello*,#{buffer_sample}}' || \
	exit 1
wait_for 'B2'
out=$(capture)
echo "$out" | grep -q 'bufa: B2' || fail "bufa missing when content matches"
echo "$out" | grep -q 'bufz: B2' && fail "bufz shown but content not matched"
$TMUX send-keys -t aaa:0 q

# --- no filter shows both buffers ---------------------------------------------
$TMUX choose-buffer -t aaa:0 -F 'B3' || exit 1
wait_for 'B3'
out=$(capture)
echo "$out" | grep -q 'bufa: B3' || fail "bufa missing with no filter"
echo "$out" | grep -q 'bufz: B3' || fail "bufz missing with no filter"
[ "$(echo "$out" | grep -c ': B3')" -eq 2 ] || fail "expected 2 buffers"
$TMUX send-keys -t aaa:0 q

# --- filter matching nothing ---------------------------------------------------
#
# Everything is shown and the filter indicator reports no matches.
$TMUX choose-buffer -t aaa:0 -F 'B4' -f '#{==:#{buffer_name},nosuch}' || \
	exit 1
wait_for 'B4'
out=$(capture)
echo "$out" | grep -q 'bufa: B4' || fail "bufa missing with no-match filter"
echo "$out" | grep -q 'bufz: B4' || fail "bufz missing with no-match filter"
echo "$out" | grep -q 'no matches' || fail "no matches indicator missing"
$TMUX send-keys -t aaa:0 q

# --- sort orders ---------------------------------------------------------------
#
# By name bufa sorts first and -r reverses.
$TMUX choose-buffer -t aaa:0 -F 'B5' -O name || exit 1
wait_for 'B5'
capture | grep ': B5' | head -1 | grep -q 'bufa: B5' || \
	fail "bufa not first with -O name"
$TMUX send-keys -t aaa:0 q

$TMUX choose-buffer -t aaa:0 -F 'B6' -O name -r || exit 1
wait_for 'B6'
capture | grep ': B6' | head -1 | grep -q 'bufz: B6' || \
	fail "bufz not first with -O name -r"
$TMUX send-keys -t aaa:0 q

# --- d deletes the selected buffer --------------------------------------------
#
# The filter leaves only bufz listed and selected; d deletes it.
$TMUX choose-buffer -t aaa:0 -F 'G1' -f '#{==:#{buffer_name},bufz}' || exit 1
wait_for 'bufz: G1'
$TMUX send-keys -t aaa:0 d
wait_buffers 1
$TMUX list-buffers -F '#{buffer_name}' | grep -q 'bufa' || \
	fail "wrong buffer deleted"
$TMUX send-keys -t aaa:0 q

# --- C-t tags all buffers and D deletes the tagged ------------------------------
$TMUX set-buffer -b bufz "other buffer" || exit 1
$TMUX set-buffer -b bufb "third buffer" || exit 1
$TMUX choose-buffer -t aaa:0 -F 'G2' || exit 1
wait_for ': G2'
$TMUX send-keys -t aaa:0 C-t D
wait_buffers 0
$TMUX send-keys -t aaa:0 q

# --- Enter runs the default command (paste-buffer) ------------------------------
#
# The only buffer is listed and selected; Enter leaves the mode and pastes it
# into the shell in the pane, where it appears on the screen.
$TMUX set-buffer -b bufa "hello buffer" || exit 1
$TMUX choose-buffer -t aaa:0 -F 'G3' || exit 1
wait_for 'bufa: G3'
$TMUX send-keys -t aaa:0 Enter
i=0
while [ "$i" -lt 50 ]; do
	[ "$($TMUX display -p -t aaa:0 '#{pane_in_mode}')" = "0" ] && break
	sleep 0.2
	i=$((i + 1))
done
[ "$i" -lt 50 ] || fail "mode did not exit after Enter"
i=0
while [ "$i" -lt 50 ]; do
	$TMUX capture-pane -p -t aaa:0 | grep -q 'hello buffer' && break
	sleep 0.2
	i=$((i + 1))
done
[ "$i" -lt 50 ] || fail "buffer not pasted into pane"

cleanup
exit 0
