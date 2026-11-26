#!/bin/sh

# Test Kitty mode changes, stack operations and resets independently of input.

PATH=/bin:/usr/bin
TERM=screen

. ./regress-common.inc

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(test_tmpdir) || exit 1
OUT=$DIR/out
TMUX="$TEST_TMUX -LtestKstate$$ -f/dev/null"

# Stop the server and remove its captures.
cleanup()
{
	stop_server $TMUX
	rm -rf "$DIR"
}
trap cleanup 0
trap 'exit 1' 1 2 15

$TMUX new-session -d -x80 -y24 || exit 1
$TMUX set-option -s extended-keys on || exit 1

# Check the query replies after a sequence of keyboard mode changes.
check_reply()
{
	request=$1
	expected=$2
	count=$((${#expected} / 2))

	: >"$OUT"
	$TMUX respawn-pane -k \
	    "stty raw -echo; printf '$request'; dd bs=1 count=$count 2>/dev/null | od -An -v -t x1 >'$OUT'; sleep 60" || exit 1
	wait_until test -s "$OUT" || exit 1
	actual=$(tr -d ' \n' <"$OUT")
	if [ "$actual" != "$expected" ]; then
		echo "request $request: expected $expected, got $actual"
		exit 1
	fi
}

# Replace, enable and disable flags without changing saved entries.
check_reply '\033[=1u\033[=8;2u\033[?u\033[=1;3u\033[?u' \
    1b5b3f39751b5b3f3875
check_reply '\033[=31u\033[?u' 1b5b3f3975
check_reply '\033[=8u\033[=1;0u\033[?u' 1b5b3f3875
check_reply '\033[=8u\033[=1;4u\033[?u' 1b5b3f3875

# An explicit zero pops nothing; an omitted count pops one entry.
check_reply '\033[>1u\033[>8u\033[<0u\033[?u\033[<u\033[?u' \
    1b5b3f38751b5b3f3175
check_reply '\033[>1u\033[>8u\033[<2u\033[?u' 1b5b3f3075

# Main-screen flags and the saved entry survive an alternate-screen visit.
check_reply '\033[>1u\033[>8u\033[?1049h\033[>9u\033[?1049l\033[<u\033[?u' \
    1b5b3f3175

# RIS clears both the current flags and the saved entry on this screen.
check_reply '\033[>1u\033[>8u\033c\033[<u\033[?u' 1b5b3f3075

# Kitty takes precedence over modifyOtherKeys and popping restores that mode.
check_reply '\033[>4;2m\033[>1u\033[?u\033[<u\033[?u' \
    1b5b3f31751b5b3f3075
wait_for_output 'Ext 2' $TMUX display-message -p '#{pane_key_mode}' || exit 1

exit 0
