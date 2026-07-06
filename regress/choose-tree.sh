#!/bin/sh

# Tests of tree mode (window-tree.c) as driven by choose-tree.
#
# Filtering: the -f filter is applied per pane and removes panes, windows and
# sessions with no matching panes (a window with more than one pane and no
# matching panes must disappear - GitHub issue 5326); -h keeps a window
# listed when its only matching pane is the pane the tree is drawn in; a
# filter matching nothing falls back to showing everything.
#
# Sorting: -O and -r change the sort order.
#
# Keys: h and l collapse and expand; f prompts for a filter and c clears it;
# g goes to the top; Enter runs the default command (switch-client); x kills
# the current item after a confirmation prompt.
#
# The tree is drawn on a mode screen which capture-pane does not show, so - as
# in environ-update.sh - a second server provides a client: an inner "tmux
# attach" runs inside a pane of the second server, and that pane is captured
# to read what the inner client rendered. Each choose-tree call uses a
# distinct -F marker so a capture can be tied to the call it belongs to.

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

# wait_count $row-marker $n
#
# Wait (up to ~10s) until exactly $n rendered list rows contain $row-marker.
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
	$TMUX send-keys -t aaa:0 "$@" || fail "send-keys $* failed"
	wait_mode aaa:0 0
}

# Session zzz is created first, so it sorts first by index, and has a
# two-pane window 0 and a single-pane window 1. Session aaa has one window
# with one pane and is where the tree is displayed. With everything expanded
# and no filter the tree is nine lines:
#
#	0 zzz  1 window 0  2 pane 0  3 pane 1  4 window 1  5 pane 0
#	6 aaa  7 window 0  8 pane 0
$TMUX new-session -d -s zzz -x 80 -y 24 'cat' || exit 1
$TMUX split-window -t zzz:0 'cat' || exit 1
$TMUX new-window -t zzz 'cat' || exit 1
$TMUX new-session -d -s aaa -x 80 -y 24 'cat' || exit 1

$TMUX2 new-session -d -s out -x 80 -y 24 "$TMUX attach -t aaa" || exit 1
wait_clients 1 || fail "no client attached to test server"

# --- filter keeping only aaa ------------------------------------------------
#
# zzz must disappear entirely: its single-pane window 1 and - the GitHub 5326
# regression - its two-pane window 0. aaa contributes exactly three lines
# (session, window, pane).
$TMUX choose-tree -t aaa:0 -F 'F1' -f '#{==:#{session_name},aaa}' || exit 1
wait_count ': F1' 3
out=$CAPTURED
printf '%s\n' "$out" | grep -F -q 'aaa: F1' || \
	fail "aaa missing when filter matches it"
printf '%s\n' "$out" | grep -F -q 'zzz: F1' && \
	fail "zzz shown but no pane matches"
exit_mode q

# --- filter keeping only zzz ------------------------------------------------
#
# zzz contributes six lines (session, two windows, three panes); aaa must
# disappear.
$TMUX choose-tree -t aaa:0 -F 'F2' -f '#{==:#{session_name},zzz}' || exit 1
wait_count ': F2' 6
out=$CAPTURED
printf '%s\n' "$out" | grep -F -q 'zzz: F2' || \
	fail "zzz missing when filter matches it"
printf '%s\n' "$out" | grep -F -q 'aaa: F2' && \
	fail "aaa shown but no pane matches"
exit_mode q

# --- filter matching a single pane ------------------------------------------
#
# Only pane 1 of zzz:0 matches, so the tree is exactly session zzz, window 0
# and that pane; zzz:1 and all of aaa must disappear.
$TMUX choose-tree -t aaa:0 -F 'F3' -f '#{==:#{pane_index},1}' || exit 1
wait_count ': F3' 3
out=$CAPTURED
printf '%s\n' "$out" | grep -F -q 'zzz: F3' || \
	fail "zzz missing when its pane matches"
printf '%s\n' "$out" | grep -F -q 'aaa: F3' && \
	fail "aaa shown but no pane matches"
printf '%s\n' "$out" | grep -F -q '1: F3' || \
	fail "matching pane missing"
exit_mode q

# --- filter matching nothing ------------------------------------------------
#
# Everything is shown and the filter indicator reports no matches.
$TMUX choose-tree -t aaa:0 -F 'F4' -f '#{==:#{session_name},nosuch}' || exit 1
wait_count ': F4' 9
out=$CAPTURED
printf '%s\n' "$out" | grep -F -q 'aaa: F4' || \
	fail "aaa missing with no-match filter"
printf '%s\n' "$out" | grep -F -q 'zzz: F4' || \
	fail "zzz missing with no-match filter"
printf '%s\n' "$out" | grep -F -q 'no matches' || \
	fail "no matches indicator missing"
exit_mode q

# --- -h with the tree pane as the only match --------------------------------
#
# With -h the pane the tree is drawn in is hidden, but it still counts as a
# match, so session and window aaa stay listed: two lines, no pane line.
$TMUX choose-tree -h -t aaa:0 -F 'F5' -f '#{==:#{session_name},aaa}' || \
	exit 1
wait_count ': F5' 2
printf '%s\n' "$CAPTURED" | grep -F -q 'aaa: F5' || \
	fail "aaa missing with -h"
exit_mode q

# --- sort orders ------------------------------------------------------------
#
# By index zzz (created first) sorts first, by name aaa does, and -r reverses.
$TMUX choose-tree -t aaa:0 -F 'F6' -O index || exit 1
wait_count ': F6' 9
printf '%s\n' "$CAPTURED" | grep -F ': F6' | head -1 | \
	grep -F -q 'zzz: F6' || \
	fail "zzz not first with -O index"
exit_mode q

$TMUX choose-tree -t aaa:0 -F 'F7' -O name || exit 1
wait_count ': F7' 9
printf '%s\n' "$CAPTURED" | grep -F ': F7' | head -1 | \
	grep -F -q 'aaa: F7' || \
	fail "aaa not first with -O name"
exit_mode q

$TMUX choose-tree -t aaa:0 -F 'F8' -O name -r || exit 1
wait_count ': F8' 9
printf '%s\n' "$CAPTURED" | grep -F ': F8' | head -1 | \
	grep -F -q 'zzz: F8' || \
	fail "zzz not first with -O name -r"
exit_mode q

# --- collapse and expand with h and l -----------------------------------------
#
# g moves to the top (session zzz); h collapses it, hiding its five children;
# l expands it again.
$TMUX choose-tree -t aaa:0 -F 'G1' -O index || exit 1
wait_count ': G1' 9
$TMUX send-keys -t aaa:0 g h || fail "send-keys collapse failed"
wait_count ': G1' 4
$TMUX send-keys -t aaa:0 l || fail "send-keys expand failed"
wait_count ': G1' 9
exit_mode q

# --- filter entered at the prompt with f, cleared with c ----------------------
$TMUX choose-tree -t aaa:0 -F 'G2' -O index || exit 1
wait_count ': G2' 9
$TMUX send-keys -t aaa:0 f || fail "send-keys f failed"
$TMUX send-keys -t aaa:0 -l '#{==:#{session_name},aaa}' || \
	fail "send-keys filter failed"
$TMUX send-keys -t aaa:0 Enter || fail "send-keys Enter failed"
wait_count ': G2' 3
out=$CAPTURED
printf '%s\n' "$out" | grep -F -q 'aaa: G2' || \
	fail "aaa missing with prompt filter"
printf '%s\n' "$out" | grep -F -q 'zzz: G2' && \
	fail "zzz shown with prompt filter"
$TMUX send-keys -t aaa:0 c || fail "send-keys c failed"
wait_count ': G2' 9
exit_mode q

# --- Enter runs the default command (switch-client) ----------------------------
#
# g selects session zzz and Enter switches the client to it.
$TMUX choose-tree -t aaa:0 -F 'G3' -O index || exit 1
wait_count ': G3' 9
$TMUX send-keys -t aaa:0 g Enter || fail "send-keys Enter failed"
i=0
while [ "$i" -lt 50 ]; do
	[ "$($TMUX list-clients -F '#{client_session}')" = "zzz" ] && break
	sleep 0.2
	i=$((i + 1))
done
[ "$i" -lt 50 ] || fail "client did not switch to zzz"
$TMUX switch-client -c "$($TMUX list-clients -F '#{client_name}')" -t aaa || \
	exit 1
wait_mode aaa:0 0

# --- x kills the current item after confirmation -------------------------------
#
# g and four times j select window 1 of zzz; x asks for confirmation and y
# kills it, leaving zzz with one window and the tree with seven lines.
$TMUX choose-tree -t aaa:0 -F 'G4' -O index || exit 1
wait_count ': G4' 9
$TMUX send-keys -t aaa:0 g j j j j x || fail "send-keys x failed"
wait_for 'Kill window 1'
$TMUX send-keys -t aaa:0 y || fail "send-keys y failed"
wait_count ': G4' 7
[ "$($TMUX list-windows -t zzz -F x | grep -c x)" -eq 1 ] || \
	fail "window 1 of zzz not killed"
exit_mode q

exit 0
