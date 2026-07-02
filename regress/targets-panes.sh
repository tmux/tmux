#!/bin/sh

# Tests of pane target resolution in cmd-find.c.
#
# Building on targets.sh (session/window resolution), this exercises the pane
# half of cmd_find_target() in a known 2x2 split:
#
#   - pane ids (%n), pane indices, and the +/- offset and ! last-pane tokens;
#   - positional tokens {top-left}/{top-right}/{bottom-left}/{bottom-right}
#     and {top}/{bottom}/{left}/{right};
#   - directional tokens {up-of}/{down-of}/{left-of}/{right-of} relative to
#     the active pane;
#   - the ".pane" and "sess:win.pane" combined forms;
#   - the marked pane, reached with ~ / {marked} and cleared with -M;
#   - and the error paths: a pane id in the wrong window, a directional token
#     with no neighbour, and an unset marked pane.
#
# The 2x2 split is created in a fixed order so pane ids are deterministic:
#
#     +--------+--------+
#     |  %0    |  %1    |   top-left  = %0   top-right    = %1
#     +--------+--------+   bottom-left = %2 bottom-right = %3
#     |  %2    |  %3    |
#     +--------+--------+

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
$TMUX kill-server 2>/dev/null

# check $target $expected [format]
#
# The default format is the pane id.
check()
{
	fmt=${3:-'#{pane_id}'}
	out=$($TMUX display-message -p -t "$1" "$fmt" 2>&1)
	if [ "$out" != "$2" ]; then
		echo "target '$1' resolved wrong."
		echo "Expected: '$2'"
		echo "But got:  '$out'"
		exit 1
	fi
}

check_ok()
{
	if ! $TMUX "$@"; then
		echo "Command failed (expected success): $*"
		exit 1
	fi
}

check_fail()
{
	out=$($TMUX has-session -t "$2" 2>&1)
	if [ $? -eq 0 ]; then
		echo "target '$2' resolved (expected failure)."
		exit 1
	fi
	if [ "$out" != "$1" ]; then
		echo "Wrong error for target '$2'."
		echo "Expected: '$1'"
		echo "But got:  '$out'"
		exit 1
	fi
}

assert_alive()
{
	if [ "$($TMUX display-message -p alive 2>&1)" != "alive" ]; then
		echo "Server died: $1"
		exit 1
	fi
}

# --- fixture: a 2x2 split plus a single-pane window -----------------------
check_ok new-session -d -s p -x 80 -y 24
check_ok split-window -h -t p:0		# %0 left,  %1 right
check_ok split-window -v -t p:0.%0	# split left:  %0 top,  %2 bottom
check_ok split-window -v -t p:0.%1	# split right: %1 top,  %3 bottom
check_ok new-window -d -t p: -n solo	# a second, single-pane window

# --- pane ids, index, offsets ---------------------------------------------
check "p:0.%3" "%3"			# exact pane id
check "p:0.3" "%3"			# pane by index
check "p:0.%1" "%1"			# sess:win.pane form
check ".%1" "%1"			# .pane form (current window)
# "sess:.pane" (empty window part) resolves the pane in the session's current
# window.  Make window 0 current first.
check_ok select-window -t p:0
check "p:.%1" "%1"
check "p:.{top-left}" "%0"
# Offsets are relative to the active pane; make %0 active first.
check_ok select-pane -t p:0.%0
check "p:0.+" "1" '#{pane_index}'	# next pane
check "p:0.-" "3" '#{pane_index}'	# previous pane (wraps)

# --- last pane (!) --------------------------------------------------------
check_ok select-pane -t p:0.%2
check_ok select-pane -t p:0.%0		# now the last pane is %2
check "p:0.!" "%2"

# --- positional tokens (absolute geometry) --------------------------------
check "p:0.{top-left}" "%0"
check "p:0.{top-right}" "%1"
check "p:0.{bottom-left}" "%2"
check "p:0.{bottom-right}" "%3"
check "p:0.{top}" "%0"			# leftmost of the top row
check "p:0.{bottom}" "%2"		# leftmost of the bottom row
check "p:0.{left}" "%0"			# top of the left column
check "p:0.{right}" "%1"		# top of the right column

# --- directional tokens (relative to the active pane) ---------------------
#
# From the top-left pane the real neighbours are below and to the right.
check_ok select-pane -t p:0.%0
check "p:0.{down-of}" "%2"
check "p:0.{right-of}" "%1"
# From the bottom-right pane the real neighbours are above and to the left.
check_ok select-pane -t p:0.%3
check "p:0.{up-of}" "%1"
check "p:0.{left-of}" "%2"

# --- pane error paths -----------------------------------------------------
check_fail "can't find pane: %0" "p:solo.%0"		# pane id, wrong window
check_fail "can't find pane: {up-of}" "p:solo.{up-of}"	# no neighbour
check_fail "can't find pane: 9" "p:0.9"			# no such index

# --- marked pane ----------------------------------------------------------
#
# ~ / {marked} resolve to the marked pane from anywhere; -M clears it.
check_fail "no marked target" "~"		# nothing marked yet
check_ok select-pane -m -t p:0.%1
check "~" "%1"
check "{marked}" "%1"
# The mark is global: it resolves even with a different current window.
check_ok select-window -t p:solo
check "~" "%1"
check_ok select-window -t p:0
check_ok select-pane -M				# clear the mark
check_fail "no marked target" "~"
check_fail "no marked target" "{marked}"

assert_alive "after pane target tests"

$TMUX kill-server 2>/dev/null
exit 0
