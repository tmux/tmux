#!/bin/sh

# Tests of the environment engine (environ.c) and its two commands,
# set-environment/setenv (cmd-set-environment.c) and show-environment/showenv
# (cmd-show-environment.c).
#
# The environment is a red-black tree of name/value entries held at two
# scopes: the global environment and each session's environment.  An entry
# may be marked hidden (ENVIRON_HIDDEN) or "cleared" (a NULL value, which
# masks an inherited variable rather than removing the entry).  This
# exercises: set and show at global and session scope; the shell (-s) output
# form and its escaping of $ ` " and \; hidden variables (-h) and their
# filtering; -r cleared entries printed as -NAME / "unset NAME;"; -u removal;
# -F expansion of the value at set time; the plain "NAME=value" and "%hidden"
# config-file assignment forms (environ_put); and the full set of argument and
# target errors from both commands.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
$TMUX kill-server 2>/dev/null

# check_value $args $expected
#
# Run show-environment with $args and compare the single-line output.
check_value()
{
	out=$($TMUX show-environment $1 2>&1)
	if [ "$out" != "$2" ]; then
		echo "show-environment $1 failed."
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

# check_fail $expected_error $cmd...
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

# --- set and show at session scope ----------------------------------------
check_ok set-environment FOO bar
check_value "FOO" "FOO=bar"
# setenv is an alias for set-environment; showenv for show-environment.
check_ok setenv FOO2 bar2
out=$($TMUX showenv FOO2 2>&1)
[ "$out" = "FOO2=bar2" ] || { echo "setenv/showenv alias failed: '$out'"; exit 1; }

# --- set and show at global scope -----------------------------------------
#
# The global scope is separate from the session scope: a session variable is
# not visible in the global environment.
check_ok set-environment -g GVAR gval
check_value "-g GVAR" "GVAR=gval"
check_fail "unknown variable: FOO" show-environment -g FOO

# --- overwrite replaces the value -----------------------------------------
check_ok set-environment FOO baz
check_value "FOO" "FOO=baz"

# --- shell (-s) output form and escaping ----------------------------------
#
# With -s the value is printed as a shell assignment with export, and the
# characters $ ` " and \ are backslash-escaped (POSIX double-quote rules).
check_ok set-environment ESC 'a$b`c"d\e'
check_value "-s ESC" 'ESC="a\$b\`c\"d\\e"; export ESC;'

# --- -F expands the value as a format at set time -------------------------
#
# With a resolvable target the value is expanded once when set; the stored
# value is the result, not the format.
check_ok set-environment -t main -F EXP '#{session_name}'
check_value "EXP" "EXP=main"

# --- hidden variables (-h) ------------------------------------------------
#
# set-environment -h marks a variable hidden.  show-environment hides it by
# default and only prints it when -h is given; conversely a normal variable is
# omitted when -h is given.
check_ok set-environment -h SECRET s3cr
check_value "SECRET" ""
check_value "-h SECRET" "SECRET=s3cr"
check_value "-h FOO" ""

# --- -r clears a variable (NULL value, masks inheritance) -----------------
#
# A cleared entry still exists but has no value: normal form prints "-NAME"
# and shell form prints "unset NAME;".
check_ok set-environment -r FOO
check_value "FOO" "-FOO"
check_value "-s FOO" "unset FOO;"

# --- -u removes a variable entirely ---------------------------------------
check_ok set-environment -u FOO
check_fail "unknown variable: FOO" show-environment FOO

# --- show with no name lists every (non-hidden) variable ------------------
check_ok set-environment -g LISTA 1
check_ok set-environment -g LISTB 2
check_ok set-environment -gh LISTHID 3
out=$($TMUX show-environment -g 2>&1)
echo "$out" | grep -q '^LISTA=1$' || { echo "list missing LISTA"; exit 1; }
echo "$out" | grep -q '^LISTB=2$' || { echo "list missing LISTB"; exit 1; }
# A hidden variable is not listed without -h.
echo "$out" | grep -q '^LISTHID' && { echo "list showed hidden var without -h"; exit 1; }
# With -h only hidden variables are listed.
$TMUX show-environment -gh 2>&1 | grep -q '^LISTHID=3$' || \
	{ echo "list -h missing LISTHID"; exit 1; }

# --- config-file assignment forms (environ_put) ---------------------------
#
# A bare NAME=value line in a config file sets a global variable; a "%hidden"
# NAME=value line sets a hidden one.  Start a second server from such a config
# and read the values back.
CONF=$(mktemp)
cat > "$CONF" <<EOF
CFGVAR=fromconfig
%hidden CFGHID=hiddencfg
EOF
CTMUX="$TEST_TMUX -Ltest2 -f$CONF"
$CTMUX kill-server 2>/dev/null
$CTMUX new-session -d -s c -x 80 -y 24 || { rm -f "$CONF"; exit 1; }
out=$($CTMUX show-environment -g CFGVAR 2>&1)
[ "$out" = "CFGVAR=fromconfig" ] || \
	{ echo "config assignment failed: '$out'"; $CTMUX kill-server; rm -f "$CONF"; exit 1; }
out=$($CTMUX show-environment -gh CFGHID 2>&1)
[ "$out" = "CFGHID=hiddencfg" ] || \
	{ echo "config %hidden failed: '$out'"; $CTMUX kill-server; rm -f "$CONF"; exit 1; }
# The %hidden variable is hidden from a plain show.
out=$($CTMUX show-environment -g CFGHID 2>&1)
[ "$out" = "" ] || \
	{ echo "config %hidden not hidden: '$out'"; $CTMUX kill-server; rm -f "$CONF"; exit 1; }
$CTMUX kill-server 2>/dev/null
rm -f "$CONF"

# --- set-environment argument errors --------------------------------------
check_fail "empty variable name" set-environment "" x
check_fail "variable name contains =" set-environment "A=B" x
check_fail "can't specify a value with -u" set-environment -u FOO val
check_fail "can't specify a value with -r" set-environment -r FOO val
check_fail "no value specified" set-environment NOVAL

# --- show-environment errors ----------------------------------------------
check_fail "unknown variable: MISSING" show-environment MISSING

# --- unresolvable target errors -------------------------------------------
check_fail "no such session: nosuch" show-environment -t nosuch FOO
check_fail "no such session: nosuch" set-environment -t nosuch FOO bar

assert_alive "after environ tests"

$TMUX kill-server 2>/dev/null
exit 0
