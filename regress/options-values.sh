#!/bin/sh

# Tests of options engine value validation, as implemented by
# options_from_string() and friends in options.c.
#
# Each option table entry has a type (string, number, key, colour, flag,
# choice, command) with type-specific parsing and validation.  This exercises:
# number range limits; choice options rejecting unknown values; flag options
# toggling with no value and rejecting garbage; colour and key options
# rejecting invalid input; string append with -a; -F expansion at set time;
# and -o refusing to overwrite an option that is already set.
#
# options-scope.sh covers scoping/inheritance and options-array.sh covers
# arrays.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
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

# --- number options -------------------------------------------------------
#
# display-time is a number with a minimum of 0; a negative value and a
# non-numeric value are both rejected via strtonum(3).
check_ok set -g display-time 4000
check_value "-gv display-time" "4000"
check_fail "value is too small: -5" set -g display-time -5
check_fail "value is invalid: abc" set -g display-time abc
# A missing value is rejected for a non-flag, non-choice option.
check_fail "empty value" set -g display-time

# --- choice options -------------------------------------------------------
#
# status-keys accepts only its listed choices (vi/emacs); anything else is an
# "unknown value" error and the option keeps its previous value.
check_ok set -g status-keys vi
check_value "-gv status-keys" "vi"
check_fail "unknown value: bogus" set -g status-keys bogus
check_value "-gv status-keys" "vi"

# --- flag options ---------------------------------------------------------
#
# focus-events is an on/off flag.  Setting with no value toggles it; explicit
# on/off/yes/no/1/0 are accepted (case-insensitively); anything else fails.
check_ok set -g focus-events off
check_value "-gv focus-events" "off"
check_ok set -g focus-events        # toggle
check_value "-gv focus-events" "on"
check_ok set -g focus-events        # toggle back
check_value "-gv focus-events" "off"
check_ok set -g focus-events yes
check_value "-gv focus-events" "on"
check_ok set -g focus-events NO
check_value "-gv focus-events" "off"
check_fail "bad value: maybe" set -g focus-events maybe

# --- colour options -------------------------------------------------------
#
# status-bg is a colour; named colours, numbers and #rrggbb are accepted,
# garbage is rejected.
check_ok set -g status-bg red
check_value "-gv status-bg" "red"
check_ok set -g status-bg colour123
check_value "-gv status-bg" "colour123"
check_ok set -g status-bg "#00ff00"
check_value "-gv status-bg" "#00ff00"
check_fail "bad colour: xxxyyy" set -g status-bg xxxyyy

# --- style options --------------------------------------------------------
#
# status-style is a style string, validated when set; a bogus style keyword is
# rejected and the old value is retained.
check_ok set -g status-style "fg=red,bg=black"
check_value "-gv status-style" "fg=red,bg=black"
check_fail "invalid style: bg=xxxyyy" set -g status-style "bg=xxxyyy"
check_value "-gv status-style" "fg=red,bg=black"

# --- key options ----------------------------------------------------------
#
# prefix is a key; a valid key name is stored in canonical form, a bad one is
# rejected.
check_ok set -g prefix C-a
check_value "-gv prefix" "C-a"
check_fail "bad key: boguskey" set -g prefix boguskey

# --- string options with extra validation ---------------------------------
#
# default-shell is a string but is checked to be an executable shell; a bogus
# path is rejected and the old value kept.
old=$($TMUX show -gv default-shell)
check_fail "not a suitable shell: /not/a/shell" set -g default-shell /not/a/shell
check_value "-gv default-shell" "$old"

# --- user options require a value ------------------------------------------
#
# A user option set with no value at all is an error.
check_fail "empty value" set -g @novalue

# --- command options ------------------------------------------------------
#
# default-client-command is a command option: the value is parsed as a tmux
# command when set and re-printed from the parsed command list.  A syntax
# error is reported and the option is left unchanged.
check_ok set -g default-client-command "new-window"
check_value "-gv default-client-command" "new-window"
check_fail "syntax error" set -g default-client-command "if -x {"
check_value "-gv default-client-command" "new-window"

# --- renamed option aliases -----------------------------------------------
#
# Historical option names are mapped to their current spelling, so setting
# cursor-color updates cursor-colour.
check_ok set -w cursor-color red
check_value "-wv cursor-colour" "red"

# --- string append (-a) ---------------------------------------------------
#
# -a appends to the current string value rather than replacing it.
check_ok set -g @str "foo"
check_ok set -ga @str "bar"
check_value "-gv @str" "foobar"

# --- -F expands at set time -----------------------------------------------
#
# With -F the value is expanded as a format once, at set time; without -F it is
# stored literally.
check_ok set -gF @expanded "#{session_name}"
check_value "-gv @expanded" "main"
check_ok set -g @literal "#{session_name}"
check_value "-gv @literal" "#{session_name}"

# --- -o refuses to overwrite ----------------------------------------------
#
# -o makes set-option fail if the option is already set, leaving it unchanged;
# it succeeds for an option that is not yet set.
check_ok set -g @once "first"
check_fail "already set: @once" set -go @once "second"
check_value "-gv @once" "first"
check_ok set -go @fresh "value"
check_value "-gv @fresh" "value"

assert_alive "after options-values tests"

$TMUX kill-server 2>/dev/null
exit 0
