#!/bin/sh

# Tests of session management command semantics, as implemented in
# cmd-new-session.c, cmd-rename-session.c, cmd-kill-session.c and
# cmd-has-session.c, plus grouped sessions (new-session -t).
#
# This exercises:
# - new-session naming: explicit -s, invalid and duplicate names, automatic
#   numeric names, -n naming the initial window and -A attaching to (here:
#   not duplicating) an existing session;
# - session_id/session_name/session_windows formats and has-session;
# - rename-session, including duplicate and invalid names, and that the
#   session keeps its id when renamed;
# - kill-session, kill-session -a (all but target), the "-f only valid with
#   -a" guard, and that killing the last session stops the server;
# - grouped sessions: new-session -t shares the window list (a window made
#   in one session appears in the other; killed windows disappear), current
#   windows are tracked independently and destroying one grouped session
#   leaves the windows in the other.
#
# session-group-resize.sh covers sizing of grouped sessions.

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
	if ! $TMUX "$@" </dev/null; then
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
	out=$($TMUX "$@" </dev/null 2>&1)
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

# check_sessions $expected
#
# Compare the session list (as "name name ...", sorted by name) with
# $expected.
check_sessions()
{
	out=$(echo $($TMUX list-sessions -F '#{session_name}' | LC_ALL=C sort))
	if [ "$out" != "$1" ]; then
		echo "Session list wrong."
		echo "Expected: '$1'"
		echo "But got:  '$out'"
		exit 1
	fi
}

# check_windows $session $expected
#
# Compare the window list of a session (as "index:name ...") with $expected.
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

# ---------------------------------------------------------------------------
# new-session and has-session.

check_ok new-session -d -s S1 -x 80 -y 24 -n first
check_fmt 'S1:' '#{session_name}:#{window_name}:#{session_windows}' \
	'S1:first:1'

# A duplicate name is an error. Only invalid UTF-8 is rejected as a name:
# colons, periods and even an empty string are allowed (such sessions can
# only be targeted by id).
check_fail 'duplicate session: S1' new-session -d -s S1
badname=$(printf 'a\377b')
check_fail "invalid session name: $badname" new-session -d -s "$badname"
oddid=$($TMUX new-session -d -s 'a:b.c' -x 80 -y 24 -P -F '#{session_id}')
check_fmt "$oddid" '#{session_name}' 'a:b.c'
check_ok kill-session -t "$oddid"
emptyid=$($TMUX new-session -d -s '' -x 80 -y 24 -P -F '#{session_id}')
check_fmt "$emptyid" '#{session_name}' ''
check_ok kill-session -t "$emptyid"

# Without -s, sessions get a numeric name matching their id counter.
autoname=$($TMUX new-session -d -x 80 -y 24 -P -F '#{session_name}')
autoid=$($TMUX display-message -p -t "=$autoname:" '#{session_id}')
if [ "\$$autoname" != "$autoid" ]; then
	echo "Automatic session name '$autoname' does not match id '$autoid'."
	exit 1
fi
check_ok has-session -t "=$autoname"
check_ok rename-session -t "=$autoname" S2
check_sessions 'S1 S2'

# -A creates the session only if it does not exist; if it does, -A means
# attach, which a detached client without a terminal cannot do.
check_ok new-session -d -A -s S3
check_sessions 'S1 S2 S3'
check_fail 'open terminal failed: not a terminal' new-session -d -A -s S3
check_sessions 'S1 S2 S3'
check_ok kill-session -t S3

# has-session fails for a missing session.
check_fail "can't find session: nosuch" has-session -t nosuch

# ---------------------------------------------------------------------------
# rename-session.

# The id survives a rename and the old name is gone.
id=$($TMUX display-message -p -t S2: '#{session_id}')
check_ok rename-session -t S2 newname
check_sessions 'S1 newname'
check_fmt "$id" '#{session_name}' 'newname'
check_fail "can't find session: S2" has-session -t S2

# Renaming to an existing or invalid name is an error.
check_fail 'duplicate session: S1' rename-session -t newname S1
check_fail "invalid session name: $badname" rename-session -t newname \
	"$badname"
check_ok rename-session -t newname S2

# ---------------------------------------------------------------------------
# grouped sessions (new-session -t).

check_ok new-session -d -s G1 -x 80 -y 24 -n shared
check_ok new-session -d -s G2 -t G1
check_fmt 'G1:' '#{session_grouped}:#{session_group_size}' '1:2'
check_fmt 'G2:' '#{session_grouped}:#{session_group_list}' '1:G1,G2'

# The window list is shared: windows created or killed in one session
# appear and disappear in the other.
check_ok new-window -d -t G2: -n added
check_windows G1 '0:shared 1:added'
check_windows G2 '0:shared 1:added'
check_ok kill-window -t G1:added
check_windows G2 '0:shared'

# The current window is tracked per session.
check_ok new-window -d -t G2:1 -n other
check_ok select-window -t G1:0
check_ok select-window -t G2:1
check_fmt 'G1:' '#{window_name}' 'shared'
check_fmt 'G2:' '#{window_name}' 'other'

# Killing one grouped session leaves the windows in the other (the group
# itself survives with a single member).
check_ok kill-session -t G2
check_windows G1 '0:shared 1:other'
check_fmt 'G1:' '#{session_grouped}:#{session_group_size}' '1:1'
check_ok kill-window -t G1:other

# ---------------------------------------------------------------------------
# kill-session.

check_sessions 'G1 S1 S2'
check_fail '-f only valid with -a' kill-session -f 'x' -t S1

# -C only clears alerts; the session survives.
check_ok kill-session -C -t S2
check_ok has-session -t S2

# -a kills every other session.
check_ok kill-session -a -t S1
check_sessions 'S1'

# Killing the last session stops the server.
check_ok kill-session -t S1
if $TMUX has-session -t S1 2>/dev/null; then
	echo "Server still up after killing the last session."
	exit 1
fi

exit 0
