#!/bin/sh

# Tests of the options engine scoping and inheritance, as described in the
# OPTIONS section of tmux(1) and implemented in options.c, cmd-set-option.c and
# cmd-show-options.c.
#
# This exercises: global vs session vs window vs pane precedence; -u to remove
# an option (revealing the inherited value); -gu to restore a global option to
# its compiled default; scope inference from the option name (-w/-p and the
# set-window-option alias); show -v (which does NOT walk parents) versus show -A
# (which does, marking inherited values with a trailing *); unknown/ambiguous
# option errors and -q suppression; and user options (@foo) at every scope.
#
# options-values.sh covers value validation and options-array.sh covers arrays.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
$TMUX kill-server 2>/dev/null

# check_value $args $expected
#
# Run show-option with $args and compare the single-line output with $expected.
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

assert_alive()
{
	if [ "$($TMUX display-message -p alive)" != "alive" ]; then
		echo "Server died: $1"
		exit 1
	fi
}

$TMUX new-session -d -s main -x 80 -y 24 || exit 1

# --- global vs session precedence -----------------------------------------
#
# status-left is a session option.  A value set at the session scope shadows
# the global one; show -v at each scope reports that scope's own value.
check_ok set -g status-left "GLOBAL"
check_ok set status-left "SESSION"
check_value "-v status-left" "SESSION"
check_value "-gv status-left" "GLOBAL"

# show -v does NOT inherit: -u removes the session entry, after which the
# session-scope show -v is empty even though the global value still exists.
check_ok set -u status-left
check_value "-v status-left" ""
check_value "-gv status-left" "GLOBAL"

# show -A walks the parent scopes and marks an inherited value with a "*".
out=$($TMUX show -A 2>/dev/null | grep '^status-left\*')
if [ "$out" != "status-left* GLOBAL" ]; then
	echo "show -A did not mark inherited status-left."
	echo "But got:  '$out'"
	exit 1
fi

# --- -gu restores the compiled default ------------------------------------
#
# Removing a global option with -u restores its built-in default rather than
# deleting it; status-left's default is the format "[#{session_name}] ".
check_ok set -g status-left "GLOBAL2"
check_value "-gv status-left" "GLOBAL2"
check_ok set -gu status-left
check_value "-gv status-left" "[#{session_name}] "

# --- scope inference from the option name ---------------------------------
#
# mode-keys is a window option, so a bare set-option infers the window scope;
# set-window-option (setw) is an explicit alias for the same thing, and -g w
# targets the global window options.
check_ok set mode-keys vi
check_value "-wv mode-keys" "vi"
check_ok setw mode-keys emacs
check_value "-wv mode-keys" "emacs"
check_ok set -gw mode-keys vi
check_value "-gwv mode-keys" "vi"

# cursor-colour is a window-and-pane option.  A pane-scope value overrides a
# window-scope one for that pane.
check_ok set -w cursor-colour blue
check_ok set -p cursor-colour red
check_value "-pv cursor-colour" "red"
out=$($TMUX show -Ap 2>/dev/null | grep '^cursor-colour ')
if [ "$out" != "cursor-colour red" ]; then
	echo "pane cursor-colour did not override window value."
	echo "But got:  '$out'"
	exit 1
fi

# --- -U unsets a window option and clears pane copies ----------------------
#
# When a window option also has per-pane copies, -u on the window scope leaves
# those pane copies in place; -U additionally removes the option from every
# pane in the window, so all panes fall back to inheritance.
$TMUX split-window -t main || exit 1
panes=$($TMUX list-panes -t main -F '#{pane_id}')
set -- $panes
pa=$1
pb=$2
check_ok set -p -t "$pa" cursor-colour red
check_ok set -p -t "$pb" cursor-colour blue
check_ok set -w -t main cursor-colour green
check_value "-pv -t $pa cursor-colour" "red"
check_value "-pv -t $pb cursor-colour" "blue"
check_value "-wv -t main cursor-colour" "green"
check_ok set -Uw -t main cursor-colour
check_value "-pv -t $pa cursor-colour" ""
check_value "-pv -t $pb cursor-colour" ""
check_value "-wv -t main cursor-colour" ""

# --- unknown, ambiguous and -q --------------------------------------------
check_fail "invalid option: no-such-option" set -g no-such-option x
check_fail "ambiguous option: status-l" set -g status-l x
# A unique prefix resolves to the full option name.
check_ok set -g status-inte 5
check_value "-gv status-interval" "5"
# -q suppresses the error and exits successfully.
check_ok set -gq no-such-option x
check_ok show -gqv no-such-option

# --- errors from unresolvable targets -------------------------------------
#
# A -t target that does not resolve produces a scope-specific error from
# options_scope_from_name()/options_scope_from_flags().
check_fail "no such session: nosuch" show -t nosuch status-left
check_fail "no such window: nosuch" show -w -t nosuch mode-keys
check_fail "no such pane: nosuch" set -p -t nosuch cursor-colour red

# --- show with no option name lists every option --------------------------
#
# show without a specific option walks the whole table (cmd_show_options_all).
# Hooks are hidden unless -H is given.
$TMUX set -g @listme "here" || exit 1
if ! $TMUX show -g | grep -q '^@listme here$'; then
	echo "show -g did not list @listme."
	exit 1
fi
# alert-bell is a hook: only shown with -H.
if $TMUX show -g | grep -q '^alert-bell'; then
	echo "show -g listed a hook without -H."
	exit 1
fi
if ! $TMUX show -gH | grep -q '^alert-bell'; then
	echo "show -gH did not list the alert-bell hook."
	exit 1
fi

# --- user options at every scope ------------------------------------------
#
# @-prefixed user options can be created freely at any scope and do not
# inherit type checking.
check_ok set -g @u "global-user"
check_ok set @u "session-user"
check_ok set -w @u "window-user"
check_ok set -p @u "pane-user"
check_value "-gv @u" "global-user"
check_value "-v @u" "session-user"
check_value "-wv @u" "window-user"
check_value "-pv @u" "pane-user"

assert_alive "after options-scope tests"

$TMUX kill-server 2>/dev/null
exit 0
