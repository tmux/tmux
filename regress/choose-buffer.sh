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
# Wait (up to ~10s) until the rendered screen contains $marker, so the
# capture is known to show the mode instance under test.
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
# Wait (up to ~10s) until the rendered screen no longer contains $marker.
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

# wait_count $marker $n
#
# Wait (up to ~10s) until exactly $n rendered lines contain $marker. The
# matching capture is left in CAPTURED.
wait_count()
{
	i=0
	while [ "$i" -lt 50 ]; do
		CAPTURED=$(capture)
		c=$(printf '%s\n' "$CAPTURED" | grep -F -c "$1")
		[ "$c" -eq "$2" ] && return 0
		sleep 0.2
		i=$((i + 1))
	done
	fail "timed out waiting for $2 lines of '$1' (have $c)"
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
		sleep 0.5
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

# wait_mode $target $state
#
# Wait (up to ~10s) until a pane enters or leaves mode.
wait_mode()
{
	t=$1
	want=$2

	i=0
	while [ "$i" -lt 50 ]; do
		got=$($TMUX display-message -p -t "$t" '#{pane_in_mode}' \
		    2>/dev/null)
		[ "$got" = "$want" ] && return 0
		sleep 0.2
		i=$((i + 1))
	done
	fail "pane $t mode state is $got, expected $want"
}

exit_mode()
{
	marker=$1
	shift

	$TMUX send-keys -t aaa:0 "$@" || fail "send-keys $* failed"
	wait_mode aaa:0 0
	wait_gone "$marker"
}

$TMUX new-session -d -s aaa -x 80 -y 24 'cat' || exit 1

$TMUX2 new-session -d -s out -x 80 -y 24 "$TMUX attach -t aaa" || exit 1
wait_clients 1 || fail "no client attached to test server"

# Two named buffers with distinct contents; bufa is created first.
$TMUX set-buffer -b bufa "hello buffer" || exit 1
$TMUX set-buffer -b bufz "other buffer" || exit 1

# --- filter by buffer name ---------------------------------------------------
$TMUX choose-buffer -t aaa:0 -F 'B1' -f '#{==:#{buffer_name},bufa}' || exit 1
wait_count ': B1' 1
out=$CAPTURED
printf '%s\n' "$out" | grep -F -q 'bufa: B1' || \
	fail "bufa missing when it matches"
printf '%s\n' "$out" | grep -F -q 'bufz: B1' && \
	fail "bufz shown but does not match"
[ "$(printf '%s\n' "$out" | grep -F -c ': B1')" -eq 1 ] || \
	fail "expected 1 buffer"
exit_mode ': B1' q

# --- filter by buffer content ------------------------------------------------
$TMUX choose-buffer -t aaa:0 -F 'B2' -f '#{m:*hello*,#{buffer_sample}}' || \
	exit 1
wait_count ': B2' 1
out=$CAPTURED
printf '%s\n' "$out" | grep -F -q 'bufa: B2' || \
	fail "bufa missing when content matches"
printf '%s\n' "$out" | grep -F -q 'bufz: B2' && \
	fail "bufz shown but content not matched"
exit_mode ': B2' q

# --- no filter shows both buffers ---------------------------------------------
$TMUX choose-buffer -t aaa:0 -F 'B3' || exit 1
wait_count ': B3' 2
out=$CAPTURED
printf '%s\n' "$out" | grep -F -q 'bufa: B3' || \
	fail "bufa missing with no filter"
printf '%s\n' "$out" | grep -F -q 'bufz: B3' || \
	fail "bufz missing with no filter"
[ "$(printf '%s\n' "$out" | grep -F -c ': B3')" -eq 2 ] || \
	fail "expected 2 buffers"
exit_mode ': B3' q

# --- filter matching nothing ---------------------------------------------------
#
# Everything is shown and the filter indicator reports no matches.
$TMUX choose-buffer -t aaa:0 -F 'B4' -f '#{==:#{buffer_name},nosuch}' || \
	exit 1
wait_count ': B4' 2
out=$CAPTURED
printf '%s\n' "$out" | grep -F -q 'bufa: B4' || \
	fail "bufa missing with no-match filter"
printf '%s\n' "$out" | grep -F -q 'bufz: B4' || \
	fail "bufz missing with no-match filter"
printf '%s\n' "$out" | grep -F -q 'no matches' || \
	fail "no matches indicator missing"
exit_mode ': B4' q

# --- sort orders ---------------------------------------------------------------
#
# By name bufa sorts first and -r reverses.
$TMUX choose-buffer -t aaa:0 -F 'B5' -O name || exit 1
wait_count ': B5' 2
printf '%s\n' "$CAPTURED" | grep -F ': B5' | head -1 | \
	grep -F -q 'bufa: B5' || \
	fail "bufa not first with -O name"
exit_mode ': B5' q

$TMUX choose-buffer -t aaa:0 -F 'B6' -O name -r || exit 1
wait_count ': B6' 2
printf '%s\n' "$CAPTURED" | grep -F ': B6' | head -1 | \
	grep -F -q 'bufz: B6' || \
	fail "bufz not first with -O name -r"
exit_mode ': B6' q

# --- d deletes the selected buffer --------------------------------------------
#
# The filter leaves only bufz listed and selected; d deletes it.
$TMUX choose-buffer -t aaa:0 -F 'G1' -f '#{==:#{buffer_name},bufz}' || exit 1
wait_for 'bufz: G1'
$TMUX send-keys -t aaa:0 d
wait_buffers 1
$TMUX list-buffers -F '#{buffer_name}' | grep -F -q 'bufa' || \
	fail "wrong buffer deleted"
exit_mode ': G1' q

# --- C-t tags all buffers and D deletes the tagged ------------------------------
$TMUX set-buffer -b bufz "other buffer" || exit 1
$TMUX set-buffer -b bufb "third buffer" || exit 1
$TMUX choose-buffer -t aaa:0 -F 'G2' || exit 1
wait_count ': G2' 3
$TMUX send-keys -t aaa:0 C-t D
wait_buffers 0
wait_mode aaa:0 0
wait_gone ': G2'

# --- Enter runs the default command (paste-buffer) ------------------------------
#
# The only buffer is listed and selected; Enter leaves the mode and pastes it
# into the shell in the pane, where it appears on the screen.
$TMUX set-buffer -b bufa "hello buffer" || exit 1
$TMUX choose-buffer -t aaa:0 -F 'G3' || exit 1
wait_count ': G3' 1
exit_mode ': G3' Enter
i=0
while [ "$i" -lt 50 ]; do
	$TMUX capture-pane -p -t aaa:0 | grep -F -q 'hello buffer' && \
		break
	sleep 0.2
	i=$((i + 1))
done
[ "$i" -lt 50 ] || fail "buffer not pasted into pane"

exit 0
