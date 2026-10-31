#!/bin/sh

# A bare assignment in a command sequence must not discard earlier commands.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
CONF=$(mktemp)
trap '$TMUX kill-server 2>/dev/null; rm -f "$CONF"' 0
trap 'exit 1' 1 2 15
$TMUX new-session -d || exit 1

check()
{
	name=$1
	expected=$2
	$TMUX set -g @trace '' || exit 1
	$TMUX set-environment -gu PARSE_ASSIGN || exit 1
	printf '%s\n' "$3" >"$CONF"
	$TMUX source-file "$CONF" || exit 1
	trace=$($TMUX show-options -gqv @trace) || exit 1
	assignment=$($TMUX show-environment -g PARSE_ASSIGN 2>/dev/null)
	out="$trace:$assignment"
	if [ "$out" != "$expected" ]; then
		echo "$name: expected '$expected', got '$out'"
		cat "$CONF"
		exit 1
	fi
}

check trailing 'ab:PARSE_ASSIGN=1' \
    'set -ag @trace a; set -ag @trace b; PARSE_ASSIGN=1'
check middle 'ab:PARSE_ASSIGN=1' \
    'set -ag @trace a; PARSE_ASSIGN=1; set -ag @trace b'
check leading 'ab:PARSE_ASSIGN=1' \
    'PARSE_ASSIGN=1; set -ag @trace a; set -ag @trace b'
check repeated 'ab:PARSE_ASSIGN=2' \
    'set -ag @trace a; PARSE_ASSIGN=1; set -ag @trace b; PARSE_ASSIGN=2'
check semicolon 'a:PARSE_ASSIGN=1' \
    'set -ag @trace a; PARSE_ASSIGN=1;'
check standalone ':PARSE_ASSIGN=1' 'PARSE_ASSIGN=1'
check braces 'ab:PARSE_ASSIGN=1' \
    'if-shell -F 1 { set -ag @trace a; set -ag @trace b; PARSE_ASSIGN=1 }'
check active 'ab:PARSE_ASSIGN=1' \
    '%if 1 set -ag @trace a; set -ag @trace b; PARSE_ASSIGN=1 %endif'
check inactive ':' \
    '%if 0 set -ag @trace a; set -ag @trace b; PARSE_ASSIGN=1 %endif'

# An assignment must not hide an invalid command earlier in the sequence.
$TMUX set -g @trace '' || exit 1
printf '%s\n' \
    'set -ag @trace a; nonexistent-command; PARSE_ASSIGN=1' >"$CONF"
if out=$($TMUX source-file "$CONF" 2>&1); then
	echo 'Invalid command before assignment was ignored'
	exit 1
fi
if [ "$out" != "$CONF:1: unknown command: nonexistent-command" ]; then
	echo "Unexpected parse error: '$out'"
	exit 1
fi
trace=$($TMUX show-options -gqv @trace) || exit 1
if [ -n "$trace" ]; then
	echo 'Commands executed despite a parse error'
	exit 1
fi

exit 0
