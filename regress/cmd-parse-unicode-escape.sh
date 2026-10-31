#!/bin/sh

# \u and \U escapes in configuration files give the UTF-8 encoding of the
# character, including characters outside the BMP and characters with no
# width.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestU$$ -f/dev/null"
CONF=$(mktemp)
trap '$TMUX kill-server 2>/dev/null; rm -f "$CONF"' 0
trap 'exit 1' 1 2 15
$TMUX new-session -d || exit 1

check()
{
	printf 'set -g @x "%s"\n' "$1" >"$CONF"
	$TMUX source-file "$CONF" || exit 1
	val=$($TMUX show-options -gv @x) || exit 1
	out=$(printf %s "$val" | od -An -tx1 | tr -d ' \n')
	if [ "$out" != "$2" ]; then
		echo "$1: expected $2, got $out"
		exit 1
	fi
}

check '\u00e9' c3a9
check '\u20ac' e282ac
check '\U0001f600' f09f9880
check '\u200d' e2808d
check 'a\U0001F600b' 61f09f988062
check '\ud7ff' ed9fbf
check '\ue000' ee8080
check '\U0010ffff' f48fbfbf

# A code point that is not a character, or NUL, is an error.
for e in '\U00110000' '\U7fffffff' '\ud800' '\udfff' '\u0000'; do
	printf 'set -g @x "%s"\n' "$e" >"$CONF"
	if $TMUX source-file "$CONF" 2>/dev/null; then
		echo "$e: accepted"
		exit 1
	fi
done

exit 0
