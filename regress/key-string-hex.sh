#!/bin/sh

# Hexadecimal key names are Unicode characters: a code point which is not a
# character is not a key.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestH$$ -f/dev/null"
trap '$TMUX kill-server 2>/dev/null' 0
trap 'exit 1' 1 2 15
$TMUX new-session -d || exit 1

check()
{
	$TMUX unbind -a -T hex 2>/dev/null
	$TMUX bind -T hex "$1" list-keys || exit 1
	out=$($TMUX list-keys -T hex | awk '{ print $4 }' | od -An -tx1 |
	    tr -d ' \n')
	if [ "$out" != "$2" ]; then
		echo "$1: expected $2, got $out"
		exit 1
	fi
}

check 0xe9 c3a90a
check 0x1f600 f09f98800a
check 0x10ffff f48fbfbf0a

for k in 0x110000 0x200000 0x7fffffff 0xd800; do
	if $TMUX bind -T hex "$k" list-keys 2>/dev/null; then
		echo "$k: accepted"
		exit 1
	fi
done

exit 0
