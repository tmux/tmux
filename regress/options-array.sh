#!/bin/sh

# Tests of array options in the options engine (options_array_* in options.c
# and the array handling in cmd-set-option.c / cmd-show-options.c).
#
# Array options are indexed by integer.  This exercises: setting a whole array
# from a separator-delimited string; per-index set with option[N]; -a append
# (which lands at the next free index); show ordering by ascending index and
# preservation of gaps; per-index unset with -u; show -v of a single index and
# of a missing index; and per-option separators (user-keys splits only on
# comma, update-environment on space or comma).
#
# update-environment (session), status-format (session), user-keys (server)
# and command-alias (server) are used as representative array options.
#
# options-scope.sh covers scoping/inheritance and options-values.sh covers
# value validation.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
$TMUX kill-server 2>/dev/null

check_value()
{
	out=$($TMUX show $1 2>&1)
	if [ "$out" != "$2" ]; then
		echo "show $1 failed."
		echo "Expected: '$2'"
		echo "But got:  '$out'"
		exit 1
	fi
}

# check_array $args $expected
#
# Compare the full (multi-line) show output for an array option with a
# newline-separated $expected string.
check_array()
{
	out=$($TMUX show $1 2>&1)
	if [ "$out" != "$(printf '%s' "$2")" ]; then
		echo "show $1 (array) failed."
		echo "Expected:"; printf '%s\n' "$2"
		echo "But got:"; printf '%s\n' "$out"
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

assert_alive()
{
	if [ "$($TMUX display-message -p alive)" != "alive" ]; then
		echo "Server died: $1"
		exit 1
	fi
}

$TMUX new-session -d -s main -x 80 -y 24 || exit 1

# --- whole-array assignment splits on the separator -----------------------
#
# update-environment has the default " ," separator, so a single string value
# is split into consecutive indices starting at 0.
check_ok set -g update-environment "AAA BBB,CCC"
check_array "-g update-environment" "update-environment[0] AAA
update-environment[1] BBB
update-environment[2] CCC"

# --- -a append goes to the next free index --------------------------------
check_ok set -ga update-environment "DDD"
check_array "-g update-environment" "update-environment[0] AAA
update-environment[1] BBB
update-environment[2] CCC
update-environment[3] DDD"

# --- per-index unset leaves a gap; show preserves order and gaps ----------
check_ok set -gu update-environment[1]
check_array "-g update-environment" "update-environment[0] AAA
update-environment[2] CCC
update-environment[3] DDD"
# show -v of an existing index returns its value; a missing index is empty.
check_value "-gv update-environment[0]" "AAA"
check_value "-gv update-environment[1]" ""

# --- explicit indexed set, including out-of-order and gaps ----------------
#
# status-format is a session array; assigning an empty string first clears its
# multi-index default, then set specific indices out of order and confirm show
# sorts by ascending index and keeps the gap at [1].
check_ok set -g status-format ""
check_array "-g status-format" "status-format"
check_ok set -g status-format[5] "five"
check_ok set -g status-format[0] "zero"
check_ok set -g status-format[2] "two"
check_array "-g status-format" "status-format[0] zero
status-format[2] two
status-format[5] five"

# --- comma-only separator (user-keys) -------------------------------------
#
# user-keys splits only on comma, so an embedded space stays within one entry
# (and show quotes a value containing a space).
check_ok set -g user-keys "One,Two Three"
check_array "-g user-keys" 'user-keys[0] One
user-keys[1] "Two Three"'

# --- command-type array (a hook) ------------------------------------------
#
# Hooks are command arrays: an indexed value is parsed as a command when set
# and re-printed from the parsed command list; a syntax error is reported.
check_ok set -g alert-bell[0] "display-message hi"
check_value "-gv alert-bell[0]" "display-message hi"
check_fail "syntax error" set -g alert-bell[0] "if -x {"

# --- colour-type array ----------------------------------------------------
#
# pane-colours is a colour array; an indexed value is validated as a colour.
check_ok set -w pane-colours[0] red
check_value "-wv pane-colours[0]" "red"
check_fail "bad colour: xxxyyy" set -w pane-colours[1] xxxyyy

# --- -o refuses to overwrite an already-set index -------------------------
check_ok set -g command-alias[9] "x=list-keys"
check_fail "already set: command-alias[9]" set -go command-alias[9] "y=list-keys"

# --- non-array option rejects index syntax --------------------------------
#
# status-left is a plain string; indexing it is an error.
check_fail "not an array: status-left[0]" set -g status-left[0] "x"

assert_alive "after options-array tests"

$TMUX kill-server 2>/dev/null
exit 0
