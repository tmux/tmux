#!/bin/sh

# User100 and User1000 must remain distinct when bindings are exported and reloaded.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestLKUR$$ -f/dev/null"

TMP=$(mktemp) || exit 1
trap 'rm -f "$TMP"; $TMUX kill-server 2>/dev/null' 0 1 15

$TMUX new -d || exit 1
$TMUX bind-key -T repro User100 display-message hundred || exit 1
$TMUX bind-key -T repro User1000 display-message thousand || exit 1

$TMUX list-keys -T repro >"$TMP" || exit 1
$TMUX unbind-key -a -T repro || exit 1
$TMUX source-file "$TMP" || exit 1

actual=$($TMUX list-keys -T repro -F '#{key_string} #{key_command}') || exit 1
expected='User100 display-message hundred
User1000 display-message thousand'
if [ "$actual" != "$expected" ]; then
	printf 'Expected:\n%s\nGot:\n%s\n' "$expected" "$actual"
	exit 1
fi

exit 0
