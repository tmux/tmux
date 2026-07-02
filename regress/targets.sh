#!/bin/sh

# Tests of target (session and window) resolution in cmd-find.c.
#
# A target string like "session:window.pane" is parsed by cmd_find_target()
# and resolved to a concrete session/window/pane.  This exercises the session
# and window halves of that machinery:
#
#   - session and window ids ($n, @n) and names;
#   - exact (=name), prefix and fnmatch matching, and the ambiguous/missing
#     error paths for each;
#   - the combined "sess:", "sess:win", ":win" and "sess:win.pane" forms and
#     the empty (current) target;
#   - the offset and special window tokens (^ $ ! + - and their {start},
#     {end}, {last}, {next}, {previous} spellings), including +N/-N with
#     wrap-around;
#   - the special whole-target tokens {active}/@/{current} and {mouse}/=;
#   - the CMD_FIND_WINDOW_INDEX "can't specify pane here" guard; and
#   - -s versus -t resolution on link-window/move-window.
#
# Positive cases are asserted with display-message -p -t (which renders the
# resolved target); error cases with has-session -t, which resolves strictly
# and prints the cmd-find error text.
#
# Pane resolution (directional/positional tokens, marked pane) is covered by
# targets-panes.sh.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
$TMUX kill-server 2>/dev/null

# check $target $expected [format]
#
# Resolve $target and compare the rendered value.  The default format is the
# window index; pass a third argument to override.
check()
{
	fmt=${3:-'#{window_index}'}
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

# check_fail $expected_error $target
#
# has-session resolves the target strictly and prints the cmd-find error.
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

# --- fixture --------------------------------------------------------------
#
# Session alpha with four named windows (0 editor, 1 editing, 2 shell,
# 3 logs); "editor"/"editing" share a prefix for the ambiguity tests.  Two
# grp* sessions share a prefix for the session ambiguity tests.
check_ok new-session -d -s alpha -x 80 -y 24
check_ok rename-window -t alpha:0 editor
check_ok new-window -d -t alpha: -n editing
check_ok new-window -d -t alpha: -n shell
check_ok new-window -d -t alpha: -n logs
check_ok new-session -d -s beta -x 80 -y 24
check_ok new-window -d -t beta: -n bw1
check_ok new-session -d -s grp1 -x 80 -y 24
check_ok new-session -d -s grp2 -x 80 -y 24

# Give alpha a last-window (2) with the current window left at 0.
check_ok select-window -t alpha:2
check_ok select-window -t alpha:0

# --- session ids and names ------------------------------------------------
sid=$($TMUX display-message -p -t alpha: '#{session_id}')
check "$sid:" "alpha" '#{session_name}'
check "=alpha:" "alpha" '#{session_name}'	# exact
check "alpha:" "alpha" '#{session_name}'	# full name
check "al:" "alpha" '#{session_name}'		# prefix
check "al*:" "alpha" '#{session_name}'		# fnmatch

# --- session error paths --------------------------------------------------
check_fail "can't find session: grp" "grp:"	# ambiguous prefix
check_fail "can't find session: grp*" "grp*:"	# ambiguous fnmatch
check_fail "can't find session: al" "=al:"	# exact-only, no such session
check_fail "can't find session: nosuch" "nosuch:"

# --- window ids and names -------------------------------------------------
wid=$($TMUX display-message -p -t alpha:editing '#{window_id}')
# A bare @id (no session) resolves both window and session.
check "$wid" "alpha:1" '#{session_name}:#{window_index}'
check "alpha:shell" "2"			# exact name
check "alpha:edito" "0"			# prefix
check "alpha:=editor" "0"		# exact match flag
check "alpha:sh*" "2"			# fnmatch
check "alpha:1" "1"			# index

# A window id qualified by a session resolves within that session; a window id
# belonging to a different session is rejected.
w2=$($TMUX display-message -p -t alpha:shell '#{window_id}')
check "alpha:$w2" "2"
bw=$($TMUX display-message -p -t beta: '#{window_id}')
check_fail "can't find window: $bw" "alpha:$bw"		# window id, wrong session

# --- window error paths ---------------------------------------------------
check_fail "can't find window: edit" "alpha:edit"	# ambiguous prefix
check_fail "can't find window: e*" "alpha:e*"		# ambiguous fnmatch
check_fail "can't find window: nope" "alpha:nope"	# missing
check_fail "can't find window: @999" "@999"		# missing window id

# --- offset and special window tokens -------------------------------------
#
# alpha's current window is 0; offsets wrap around the four windows.
check "alpha:^" "0"		# start
check "alpha:\$" "3"		# end
check "alpha:+" "1"		# next
check "alpha:-" "3"		# previous (wraps)
check "alpha:+2" "2"
check "alpha:-2" "2"		# wraps
check "alpha:{start}" "0"
check "alpha:{end}" "3"
check "alpha:{next}" "1"
check "alpha:{previous}" "3"
check "alpha:!" "2"		# last window
check "alpha:{last}" "2"

# --- combined and empty forms ---------------------------------------------
#
# ":win" uses the current session; confirm that is alpha first so the test is
# unambiguous, then resolve a window inside it with an empty session part.
check "" "alpha" '#{session_name}'		# empty target is current
check "" "alpha:0" '#{session_name}:#{window_index}'
check ":shell" "alpha:2" '#{session_name}:#{window_index}'
check "alpha:shell.0" "alpha:2" '#{session_name}:#{window_index}'
check "alpha:.0" "alpha:0" '#{session_name}:#{window_index}'	# empty window part

# --- bare-name fallbacks --------------------------------------------------
#
# A bare pane target that is not a pane falls back to a window, then to a
# session, using the current session (alpha).
check "editor" "0" '#{window_index}'			# bare window name
check "beta" "beta" '#{session_name}'			# bare session name

# --- whole-target special tokens ------------------------------------------
#
# {active}/@/{current} need a client with a session; with only a detached
# command client they must error cleanly (regression: this used to crash the
# server via a NULL session dereference).  {mouse}/= need a mouse event.
check_fail "no current client" "{active}"
check_fail "no current client" "@"
check_fail "no current client" "{current}"
check_fail "no mouse target" "{mouse}"
check_fail "no mouse target" "="
assert_alive "after whole-target special tokens"

# --- CMD_FIND_WINDOW_INDEX rejects a pane part ----------------------------
out=$($TMUX new-window -d -t 'alpha:1.%0' 2>&1)
[ $? -ne 0 ] || { echo "new-window with pane target succeeded"; exit 1; }
[ "$out" = "can't specify pane here" ] || \
	{ echo "wrong pane-here error: '$out'"; exit 1; }

# --- window index targets: offsets resolve to an index --------------------
#
# new-window's -t is a window index (CMD_FIND_WINDOW_INDEX); an offset from
# the current window (0) picks the numeric index rather than an existing
# window, so "+6" creates window 6.
check_ok select-window -t alpha:0
check_ok new-window -d -t 'alpha:+6' -n offwin
check "alpha:6" "offwin" '#{window_name}'
check_ok kill-window -t alpha:6

# --- -s versus -t resolution ----------------------------------------------
#
# link-window takes a source window (-s) and a destination index (-t); each
# side is resolved independently by cmd-find.
check_ok new-session -d -s src -x 80 -y 24
check_ok new-window -d -t src: -n payload
check_ok link-window -s src:payload -t alpha:9
check "alpha:9" "payload" '#{window_name}'
# move-window relocates it; the old index must be gone.
check_ok move-window -s alpha:9 -t alpha:5
check "alpha:5" "payload" '#{window_name}'
check_fail "can't find window: 9" "alpha:9"

# --- default state with no client -----------------------------------------
#
# run-shell with no target and no attached client has cmd-find build the
# current state from nothing (the best session).
check_ok run-shell 'true'

assert_alive "after target tests"

$TMUX kill-server 2>/dev/null
exit 0
