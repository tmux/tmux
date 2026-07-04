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

# wait_count $marker $n
#
# Wait (up to ~10s) until exactly $n rendered lines contain $marker.
wait_count()
{
	i=0
	while [ "$i" -lt 50 ]; do
		[ "$(capture | grep -c "$1")" -eq "$2" ] && return 0
		sleep 0.2
		i=$((i + 1))
	done
	fail "timed out waiting for $2 lines of '$1' (have $(capture | grep -c "$1"))"
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

# Session zzz is created first, so it sorts first by index, and has a
# two-pane window 0 and a single-pane window 1. Session aaa has one window
# with one pane and is where the tree is displayed. With everything expanded
# and no filter the tree is nine lines:
#
#	0 zzz  1 window 0  2 pane 0  3 pane 1  4 window 1  5 pane 0
#	6 aaa  7 window 0  8 pane 0
$TMUX new-session -d -s zzz -x 80 -y 24 || exit 1
$TMUX split-window -t zzz:0 || exit 1
$TMUX new-window -t zzz || exit 1
$TMUX new-session -d -s aaa -x 80 -y 24 || exit 1

$TMUX2 new-session -d -s out -x 80 -y 24 "$TMUX attach -t aaa" || exit 1
wait_clients 1 || fail "no client attached to test server"

# --- filter keeping only aaa ------------------------------------------------
#
# zzz must disappear entirely: its single-pane window 1 and - the GitHub 5326
# regression - its two-pane window 0. aaa contributes exactly three lines
# (session, window, pane).
$TMUX choose-tree -t aaa:0 -F 'F1' -f '#{==:#{session_name},aaa}' || exit 1
wait_count 'F1' 3
out=$(capture)
echo "$out" | grep -q 'aaa: F1' || fail "aaa missing when filter matches it"
echo "$out" | grep -q 'zzz: F1' && fail "zzz shown but no pane matches"
$TMUX send-keys -t aaa:0 q

# --- filter keeping only zzz ------------------------------------------------
#
# zzz contributes six lines (session, two windows, three panes); aaa must
# disappear.
$TMUX choose-tree -t aaa:0 -F 'F2' -f '#{==:#{session_name},zzz}' || exit 1
wait_count 'F2' 6
out=$(capture)
echo "$out" | grep -q 'zzz: F2' || fail "zzz missing when filter matches it"
echo "$out" | grep -q 'aaa: F2' && fail "aaa shown but no pane matches"
$TMUX send-keys -t aaa:0 q

# --- filter matching a single pane ------------------------------------------
#
# Only pane 1 of zzz:0 matches, so the tree is exactly session zzz, window 0
# and that pane; zzz:1 and all of aaa must disappear.
$TMUX choose-tree -t aaa:0 -F 'F3' -f '#{==:#{pane_index},1}' || exit 1
wait_count 'F3' 3
out=$(capture)
echo "$out" | grep -q 'zzz: F3' || fail "zzz missing when its pane matches"
echo "$out" | grep -q 'aaa: F3' && fail "aaa shown but no pane matches"
echo "$out" | grep -q '1: F3' || fail "matching pane missing"
$TMUX send-keys -t aaa:0 q

# --- filter matching nothing ------------------------------------------------
#
# Everything is shown and the filter indicator reports no matches.
$TMUX choose-tree -t aaa:0 -F 'F4' -f '#{==:#{session_name},nosuch}' || exit 1
wait_for 'F4'
out=$(capture)
echo "$out" | grep -q 'aaa: F4' || fail "aaa missing with no-match filter"
echo "$out" | grep -q 'zzz: F4' || fail "zzz missing with no-match filter"
echo "$out" | grep -q 'no matches' || fail "no matches indicator missing"
$TMUX send-keys -t aaa:0 q

# --- -h with the tree pane as the only match --------------------------------
#
# With -h the pane the tree is drawn in is hidden, but it still counts as a
# match, so session and window aaa stay listed: two lines, no pane line.
$TMUX choose-tree -h -t aaa:0 -F 'F5' -f '#{==:#{session_name},aaa}' || \
	exit 1
wait_count 'F5' 2
capture | grep -q 'aaa: F5' || fail "aaa missing with -h"
$TMUX send-keys -t aaa:0 q

# --- sort orders ------------------------------------------------------------
#
# By index zzz (created first) sorts first, by name aaa does, and -r reverses.
$TMUX choose-tree -t aaa:0 -F 'F6' -O index || exit 1
wait_for 'F6'
capture | grep 'F6' | head -1 | grep -q 'zzz: F6' || \
	fail "zzz not first with -O index"
$TMUX send-keys -t aaa:0 q

$TMUX choose-tree -t aaa:0 -F 'F7' -O name || exit 1
wait_for 'F7'
capture | grep 'F7' | head -1 | grep -q 'aaa: F7' || \
	fail "aaa not first with -O name"
$TMUX send-keys -t aaa:0 q

$TMUX choose-tree -t aaa:0 -F 'F8' -O name -r || exit 1
wait_for 'F8'
capture | grep 'F8' | head -1 | grep -q 'zzz: F8' || \
	fail "zzz not first with -O name -r"
$TMUX send-keys -t aaa:0 q

# --- collapse and expand with h and l -----------------------------------------
#
# g moves to the top (session zzz); h collapses it, hiding its five children;
# l expands it again.
$TMUX choose-tree -t aaa:0 -F 'G1' -O index || exit 1
wait_count 'G1' 9
$TMUX send-keys -t aaa:0 g h
wait_count 'G1' 4
$TMUX send-keys -t aaa:0 l
wait_count 'G1' 9
$TMUX send-keys -t aaa:0 q

# --- filter entered at the prompt with f, cleared with c ----------------------
$TMUX choose-tree -t aaa:0 -F 'G2' -O index || exit 1
wait_count 'G2' 9
$TMUX send-keys -t aaa:0 f
$TMUX send-keys -t aaa:0 -l '#{==:#{session_name},aaa}'
$TMUX send-keys -t aaa:0 Enter
wait_count 'G2' 3
out=$(capture)
echo "$out" | grep -q 'aaa: G2' || fail "aaa missing with prompt filter"
echo "$out" | grep -q 'zzz: G2' && fail "zzz shown with prompt filter"
$TMUX send-keys -t aaa:0 c
wait_count 'G2' 9
$TMUX send-keys -t aaa:0 q

# --- Enter runs the default command (switch-client) ----------------------------
#
# g selects session zzz and Enter switches the client to it.
$TMUX choose-tree -t aaa:0 -F 'G3' -O index || exit 1
wait_count 'G3' 9
$TMUX send-keys -t aaa:0 g Enter
i=0
while [ "$i" -lt 50 ]; do
	[ "$($TMUX list-clients -F '#{client_session}')" = "zzz" ] && break
	sleep 0.2
	i=$((i + 1))
done
[ "$i" -lt 50 ] || fail "client did not switch to zzz"
$TMUX switch-client -c "$($TMUX list-clients -F '#{client_name}')" -t aaa || \
	exit 1

# --- x kills the current item after confirmation -------------------------------
#
# g and four times j select window 1 of zzz; x asks for confirmation and y
# kills it, leaving zzz with one window and the tree with seven lines.
$TMUX choose-tree -t aaa:0 -F 'G4' -O index || exit 1
wait_count 'G4' 9
$TMUX send-keys -t aaa:0 g j j j j x
wait_for 'Kill window 1'
$TMUX send-keys -t aaa:0 y
wait_count 'G4' 7
[ "$($TMUX list-windows -t zzz -F x | grep -c x)" -eq 1 ] || \
	fail "window 1 of zzz not killed"
$TMUX send-keys -t aaa:0 q

cleanup
exit 0
