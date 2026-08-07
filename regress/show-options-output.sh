#!/bin/sh

# Tests of the default output from show-options and show-hooks. This is
# intended to guard the output compatibility when the implementation is changed
# to use formats internally, so it deliberately does not use show -F.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
$TMUX kill-server 2>/dev/null

check_value()
{
	out=$($TMUX show $1 2>&1)
	if [ "$out" != "$(printf '%s' "$2")" ]; then
		echo "show $1 failed."
		echo "Expected:"; printf '%s\n' "$2"
		echo "But got:"; printf '%s\n' "$out"
		exit 1
	fi
}

check_hooks()
{
	out=$($TMUX show-hooks $1 2>&1)
	if [ "$out" != "$(printf '%s' "$2")" ]; then
		echo "show-hooks $1 failed."
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

assert_alive()
{
	if [ "$($TMUX display-message -p alive)" != "alive" ]; then
		echo "Server died: $1"
		exit 1
	fi
}

$TMUX new-session -d -s main -x 80 -y 24 || exit 1

# Scalar options: strings are escaped, non-strings are not, and -v suppresses
# the option name.
check_ok set -g @words "two words"
check_value "-g @words" '@words "two words"'
check_value "-gv @words" 'two words'
check_ok set -g display-time 1234
check_value "-g display-time" 'display-time 1234'
check_value "-gv display-time" '1234'

# Inherited options shown with -A are marked with an asterisk.
check_ok set -g status-left "GLOBAL"
check_ok set -u status-left
out=$($TMUX show -A 2>/dev/null | grep '^status-left\*')
if [ "$out" != "status-left* GLOBAL" ]; then
	echo "show -A did not mark inherited status-left."
	echo "But got:"; printf '%s\n' "$out"
	exit 1
fi

# Arrays: an empty array prints just the name unless -v is used; populated
# arrays include every key, including key 0.
check_ok set -g status-format ""
check_value "-g status-format" 'status-format'
check_value "-gv status-format" ''
out=$($TMUX show -A 2>/dev/null | grep '^status-format\*')
if [ "$out" != "status-format*" ]; then
	echo "show -A did not mark inherited empty status-format."
	echo "But got:"; printf '%s\n' "$out"
	exit 1
fi
check_value "-A status-format" 'status-format'
check_ok set -g update-environment "AAA BBB,CCC"
check_value "-g update-environment" 'update-environment[0] AAA
update-environment[1] BBB
update-environment[2] CCC'
check_value "-gv update-environment[0]" 'AAA'

# Hooks use the same array output style, and monitor hooks have their own
# default output form.
check_ok set-hook -g window-renamed[first] "display-message renamed"
check_hooks "-g window-renamed" 'window-renamed[first] display-message renamed'
check_ok set-hook -g -B '@monitor:%*:#{pane_width}'
check_hooks "-g -B @monitor" '@monitor:%*:#{pane_width}'
check_ok set-hook -g @user-hook "display-message user"
check_hooks "-g @user-hook" '@user-hook "display-message user"'
out=$($TMUX show -g 2>&1)
echo "$out" | grep -q "^@monitor ''$" && {
	echo "show -g showed monitor hook without -H."
	echo "But got:"; printf '%s\n' "$out"
	exit 1
}
echo "$out" | grep -q '^@user-hook "display-message user"$' && {
	echo "show -g showed user hook without -H."
	echo "But got:"; printf '%s\n' "$out"
	exit 1
}
out=$($TMUX show -gH 2>&1)
echo "$out" | grep -q "^@monitor ''$" || {
	echo "show -gH did not show monitor hook."
	echo "But got:"; printf '%s\n' "$out"
	exit 1
}
echo "$out" | grep -q '^@user-hook "display-message user"$' || {
	echo "show -gH did not show user hook."
	echo "But got:"; printf '%s\n' "$out"
	exit 1
}
out=$($TMUX show-hooks -g 2>&1)
echo "$out" | grep -q "^@monitor ''$" || {
	echo "show-hooks -g did not show monitor hook."
	echo "But got:"; printf '%s\n' "$out"
	exit 1
}
echo "$out" | grep -q '^@user-hook "display-message user"$' || {
	echo "show-hooks -g did not show user hook."
	echo "But got:"; printf '%s\n' "$out"
	exit 1
}

assert_alive "after show-options output tests"

$TMUX kill-server 2>/dev/null
exit 0
