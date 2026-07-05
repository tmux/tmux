#!/bin/sh

# Tests of window management command semantics (not parsing), as implemented
# in cmd-new-window.c, cmd-move-window.c (move-window and link-window),
# cmd-unlink-window.c, cmd-swap-window.c, cmd-rotate-window.c,
# cmd-kill-window.c and cmd-select-window.c.
#
# This exercises:
# - new-window placement: next free index, explicit index, index in use with
#   and without -k, -a (after) and -b (before) insertion with shuffling, and
#   -S selecting an existing window by name instead of creating;
# - move-window to a free index, to an occupied index with and without -k,
#   -a insertion and -r renumbering (including base-index);
# - renumber-windows closing gaps;
# - link-window sharing a window between two sessions (window_linked and
#   window_linked_sessions), unlink-window removing one link and refusing to
#   unlink the last link without -k;
# - swap-window within and between sessions, -d keeping the active window,
#   and the grouped-sessions error;
# - rotate-window -U/-D rotating pane positions;
# - kill-window switching to the last (previously current) window, kill-window
#   -a killing all other windows and the "-f only valid with -a" guard.
#
# pane-ops.sh covers pane-level commands and buffers.sh covers paste buffers.

PATH=/bin:/usr/bin
TERM=screen
LANG=C.UTF-8
LC_ALL=C.UTF-8
export TERM LANG LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
$TMUX kill-server 2>/dev/null

# check_ok $cmd...
#
# Run a command and require that it succeeds.
check_ok()
{
	if ! $TMUX "$@"; then
		echo "Command failed (expected success): $*"
		exit 1
	fi
}

# check_fail $expected_error $cmd...
#
# Run a command and require that it fails with the given error message.
check_fail()
{
	exp="$1"
	shift
	out=$($TMUX "$@" 2>&1)
	if [ $? -eq 0 ]; then
		echo "Command succeeded (expected failure): $*"
		exit 1
	fi
	if [ "$out" != "$exp" ]; then
		echo "Wrong error for: $*"
		echo "Expected: '$exp'"
		echo "But got:  '$out'"
		exit 1
	fi
}

# check_windows $session $expected
#
# Compare the window list of a session (as "index:name index:name ...", in
# index order) with $expected.
check_windows()
{
	out=$(echo $($TMUX list-windows -t "$1" -F \
	    '#{window_index}:#{window_name}'))
	if [ "$out" != "$2" ]; then
		echo "Window list of '$1' wrong."
		echo "Expected: '$2'"
		echo "But got:  '$out'"
		exit 1
	fi
}

# check_fmt $target $format $expected
#
# Expand a format in a target's context and compare with $expected.
check_fmt()
{
	out=$($TMUX display-message -p -t "$1" "$2" 2>&1)
	if [ "$out" != "$3" ]; then
		echo "Format '$2' for '$1' wrong."
		echo "Expected: '$3'"
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

# ---------------------------------------------------------------------------
# new-window placement.

check_ok new-session -d -s W -x 80 -y 24 -n w0

# Next free index.
check_ok new-window -d -t W: -n w1
check_ok new-window -d -t W: -n w2
check_windows W '0:w0 1:w1 2:w2'

# Explicit index, then the next new window fills the first free index, not
# one past the highest.
check_ok new-window -d -t W:9 -n w9
check_ok new-window -d -t W: -n w3
check_windows W '0:w0 1:w1 2:w2 3:w3 9:w9'

# Occupied index fails without -k and replaces with -k.
check_fail 'create window failed: index 9 in use' \
	new-window -d -t W:9 -n dup
check_ok new-window -d -k -t W:9 -n w9k
check_windows W '0:w0 1:w1 2:w2 3:w3 9:w9k'

# -a inserts after the target, shuffling the following windows up.
check_ok new-window -d -a -t W:1 -n wA
check_windows W '0:w0 1:w1 2:wA 3:w2 4:w3 9:w9k'

# -b inserts before the target, shuffling the target and followers up.
check_ok new-window -d -b -t W:0 -n wB
check_windows W '0:wB 1:w0 2:w1 3:wA 4:w2 5:w3 9:w9k'

# -S selects an existing window with the same name instead of creating (with
# -d it would not switch, so no -d here).
check_ok select-window -t W:0
check_ok new-window -S -t W: -n w3
check_windows W '0:wB 1:w0 2:w1 3:wA 4:w2 5:w3 9:w9k'
check_fmt 'W:' '#{window_index}:#{window_name}' '5:w3'

# Clean up to a known arrangement.
check_ok kill-window -t W:wB
check_ok kill-window -t W:wA
check_ok kill-window -t W:w9k
check_ok move-window -r -t W:
check_windows W '0:w0 1:w1 2:w2 3:w3'

# ---------------------------------------------------------------------------
# move-window.

# To a free index.
check_ok move-window -d -s W:2 -t W:7
check_windows W '0:w0 1:w1 3:w3 7:w2'

# To an occupied index, without and with -k.
check_fail 'index in use: 7' move-window -d -s W:3 -t W:7
check_ok move-window -d -k -s W:3 -t W:7
check_windows W '0:w0 1:w1 7:w3'

# -a inserts after the target and shuffles.
check_ok move-window -d -a -s W:7 -t W:0
check_windows W '0:w0 1:w3 2:w1'

# -r renumbers in order, respecting base-index.
check_ok move-window -d -s W:2 -t W:8
check_ok set-option -t W base-index 5
check_ok move-window -r -t W:
check_windows W '5:w0 6:w3 7:w1'
check_ok set-option -t W base-index 0
check_ok move-window -r -t W:
check_windows W '0:w0 1:w3 2:w1'

# With the renumber-windows option on, killing a window renumbers the rest
# automatically.
check_ok set-option -t W renumber-windows on
check_ok new-window -d -t W:9 -n wtmp
check_windows W '0:w0 1:w3 2:w1 9:wtmp'
check_ok kill-window -t W:1
check_windows W '0:w0 1:w1 2:wtmp'
check_ok kill-window -t W:2
check_ok set-option -t W renumber-windows off
check_ok new-window -d -t W:2 -n w3
check_windows W '0:w0 1:w1 2:w3'

# Without -s, the current window of the client/session moves.
check_ok select-window -t W:2
check_ok move-window -d -t W:6
check_windows W '0:w0 1:w1 6:w3'
check_ok move-window -r -t W:
check_windows W '0:w0 1:w1 2:w3'

# ---------------------------------------------------------------------------
# link-window and unlink-window.

check_ok new-session -d -s L -x 80 -y 24 -n l0

# Link a window from W into L and check it is shared.
check_ok link-window -d -s W:w1 -t L:5
check_windows L '0:l0 5:w1'
check_fmt 'W:w1' '#{window_linked}' '1'
check_fmt 'L:5' '#{window_linked_sessions}' '2'

# The linked window is the same window: renaming in one session shows in the
# other.
check_ok rename-window -t L:5 shared
check_windows W '0:w0 1:shared 2:w3'
check_ok rename-window -t W:1 w1

# Linking again to an occupied index fails without -k.
check_fail 'index in use: 0' link-window -d -s W:w3 -t L:0

# Unlink removes one link; the window survives in the other session.
check_ok unlink-window -t L:5
check_windows L '0:l0'
check_windows W '0:w0 1:w1 2:w3'
check_fmt 'W:w1' '#{window_linked}' '0'

# Unlinking a window linked to only one session needs -k.
check_fail 'window only linked to one session' unlink-window -t W:w3
check_ok unlink-window -k -t W:w3
check_windows W '0:w0 1:w1'

# ---------------------------------------------------------------------------
# swap-window.

check_ok new-window -d -t W:2 -n w2
check_ok new-window -d -t W:3 -n w3

# Swap within a session: indices are exchanged.
check_ok swap-window -d -s W:0 -t W:3
check_windows W '0:w3 1:w1 2:w2 3:w0'
check_ok swap-window -d -s W:0 -t W:3
check_windows W '0:w0 1:w1 2:w2 3:w3'

# Without -d the current index does not change, so the window that arrives
# there becomes current; with -d the swapped windows are selected, so the
# source window stays current at its new index.
check_ok select-window -t W:0
check_ok swap-window -s W:0 -t W:3
check_fmt 'W:' '#{window_index}:#{window_name}' '0:w3'
check_ok swap-window -s W:3 -t W:0
check_fmt 'W:' '#{window_index}:#{window_name}' '0:w0'
check_ok swap-window -d -s W:0 -t W:3
check_fmt 'W:' '#{window_index}:#{window_name}' '3:w0'
check_ok swap-window -d -s W:3 -t W:0
check_fmt 'W:' '#{window_index}:#{window_name}' '0:w0'

# Swap between two different sessions.
check_ok swap-window -d -s W:w2 -t L:l0
check_windows W '0:w0 1:w1 2:l0 3:w3'
check_windows L '0:w2'
check_ok swap-window -d -s W:2 -t L:0
check_windows W '0:w0 1:w1 2:w2 3:w3'
check_windows L '0:l0'

# Swapping between two sessions in the same group is an error.
check_ok new-session -d -s WG -t W
check_fail "can't move window, sessions are grouped" \
	swap-window -d -s W:0 -t WG:1
check_ok kill-session -t WG
check_windows W '0:w0 1:w1 2:w2 3:w3'

# ---------------------------------------------------------------------------
# rotate-window.

check_ok new-session -d -s R -x 80 -y 24
check_ok split-window -d -t R:0
check_ok split-window -d -t R:0
p0=$($TMUX display-message -p -t R:0.0 '#{pane_id}')
p1=$($TMUX display-message -p -t R:0.1 '#{pane_id}')
p2=$($TMUX display-message -p -t R:0.2 '#{pane_id}')

# check_panes $target $expected
#
# Compare the pane list of a window (as "index:id ...") with $expected.
check_panes()
{
	out=$(echo $($TMUX list-panes -t "$1" -F \
	    '#{pane_index}:#{pane_id}'))
	if [ "$out" != "$2" ]; then
		echo "Pane list of '$1' wrong."
		echo "Expected: '$2'"
		echo "But got:  '$out'"
		exit 1
	fi
}

check_panes R:0 "0:$p0 1:$p1 2:$p2"

# -U rotates panes up (each pane moves to the previous position); -D rotates
# down. -U then -D restores the original order.
check_ok rotate-window -U -t R:0
check_panes R:0 "0:$p1 1:$p2 2:$p0"
check_ok rotate-window -D -t R:0
check_panes R:0 "0:$p0 1:$p1 2:$p2"
check_ok rotate-window -D -t R:0
check_panes R:0 "0:$p2 1:$p0 2:$p1"
check_ok rotate-window -U -t R:0

# The active position is preserved across rotation: the pane that arrives at
# the active position becomes the active pane.
check_ok select-pane -t R:0.0
check_ok rotate-window -U -t R:0
check_fmt 'R:0' '#{pane_index}:#{pane_id}' "0:$p1"
check_ok rotate-window -D -t R:0
check_fmt 'R:0' '#{pane_index}:#{pane_id}' "0:$p0"

# ---------------------------------------------------------------------------
# kill-window.

# Killing the current window switches to the last (previously current)
# window.
check_ok select-window -t W:1
check_ok select-window -t W:3
check_fmt 'W:' '#{window_index}' '3'
check_ok kill-window -t W:3
check_fmt 'W:' '#{window_index}' '1'
check_windows W '0:w0 1:w1 2:w2'

# -f is only valid with -a.
check_fail '-f only valid with -a' kill-window -f 'x' -t W:0

# -a kills every window except the target.
check_ok kill-window -a -t W:w1
check_windows W '1:w1'

# ---------------------------------------------------------------------------
# select-window, next-window, previous-window.

# -P prints where the new window went; an invalid (non-UTF-8) name is an
# error.
out=$($TMUX new-window -d -t W:0 -n wa -P -F '#{window_index}:#{window_name}')
if [ "$out" != "0:wa" ]; then
	echo "new-window -P output wrong: '$out'"
	exit 1
fi
check_fail "invalid window name: $(printf 'a\377b')" \
	new-window -d -t W: -n "$(printf 'a\377b')"
check_ok new-window -d -t W:2 -n wc
check_windows W '0:wa 1:w1 2:wc'

# -n and -p select the next and previous window, wrapping at the ends.
check_ok select-window -t W:0
check_ok select-window -n -t W:
check_fmt 'W:' '#{window_index}' '1'
check_ok select-window -n -t W:
check_fmt 'W:' '#{window_index}' '2'
check_ok select-window -n -t W:
check_fmt 'W:' '#{window_index}' '0'
check_ok select-window -p -t W:
check_fmt 'W:' '#{window_index}' '2'

# next-window and previous-window are the same code.
check_ok next-window -t W:
check_fmt 'W:' '#{window_index}' '0'
check_ok previous-window -t W:
check_fmt 'W:' '#{window_index}' '2'

# -l selects the previously current window and select-window -T on the
# already-current window does the same.
check_ok select-window -t W:1
check_ok select-window -l -t W:
check_fmt 'W:' '#{window_index}' '2'
check_ok select-window -T -t W:2
check_fmt 'W:' '#{window_index}' '1'
check_ok select-window -T -t W:2
check_fmt 'W:' '#{window_index}' '2'

# With -a, next-window looks for a window with an alert and fails if there
# is none; a fresh session has no last window.
check_fail 'no next window' next-window -a -t W:
check_fail 'no previous window' previous-window -a -t W:
check_ok new-session -d -s F -x 80 -y 24
check_fail 'no last window' select-window -l -t F:
check_fail 'no last window' select-window -T -t F:0
check_ok kill-session -t F

# ---------------------------------------------------------------------------
# more kill-window -a: no-op, filters and multiply-linked windows.

# -a with a single window in the session does nothing.
check_ok kill-window -a -t L:0
check_windows L '0:l0'

# -a -f only kills other windows matching the filter.
check_ok kill-window -a -f '#{==:#{window_name},wc}' -t W:0
check_windows W '0:wa 1:w1'

# If the current window is linked into the session more than once, -a kills
# it too - taking the whole session with it here.
check_ok new-session -d -s D -x 80 -y 24 -n d0
check_ok link-window -d -s D:0 -t D:5
check_ok new-window -d -t D:1 -n dx
check_ok select-window -t D:0
check_ok kill-window -a -t D:0
if $TMUX has-session -t D 2>/dev/null; then
	echo "Session D survived kill-window -a on multiply-linked window."
	exit 1
fi

check_fmt 'R:0' '#{window_panes}' '3'
assert_alive

$TMUX kill-server 2>/dev/null
exit 0
