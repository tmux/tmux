#!/bin/sh

# Width cache ranges must include their endpoint without overflowing wchar_t.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
SERVER=
WATCHDOG=

cleanup()
{
	if [ -n "$WATCHDOG" ]; then
		kill "$WATCHDOG" 2>/dev/null
		wait "$WATCHDOG" 2>/dev/null
	fi
	[ -n "$SERVER" ] && kill -9 "$SERVER" 2>/dev/null
}
trap cleanup 0
trap 'exit 1' 1 2 15

$TMUX new-session -d 'exec sleep 60' || exit 1
SERVER=$($TMUX display-message -p '#{pid}') || exit 1

# kill-server cannot stop a server stuck rebuilding the width cache.
(
	sleep 5
	kill -9 "$SERVER" 2>/dev/null
) &
WATCHDOG=$!

$TMUX set -g codepoint-widths 'U+1F600=2' || exit 1

# Cover both signed and unsigned 32-bit wchar_t. Values above WCHAR_MAX
# are ignored by the parser on platforms where they cannot be represented.
for value in U+7FFFFFFF=1 U+7FFFFFFE-U+7FFFFFFF=2 \
    U+FFFFFFFF=1 U+FFFFFFFE-U+FFFFFFFF=2; do
	$TMUX set -g codepoint-widths "$value" || exit 1
	[ "$($TMUX display-message -p alive)" = alive ] || exit 1
done

# Check that an ordinary range still includes both endpoints and stops there.
$TMUX set -g codepoint-widths 'U+03B1-U+03B3=2' || exit 1
$TMUX set -g @text 'αβγδ' || exit 1
[ "$($TMUX display-message -p '#{w:@text}')" = 7 ] || exit 1
$TMUX set -gu codepoint-widths || exit 1
[ "$($TMUX display-message -p '#{w:@text}')" = 4 ] || exit 1

$TMUX kill-server || exit 1
SERVER=
exit 0
