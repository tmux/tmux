#!/bin/sh

# Test parsing and printing JSON with display-message -j.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
$TMUX kill-server 2>/dev/null

fail()
{
	echo "$1"
	$TMUX kill-server 2>/dev/null
	exit 1
}

check()
{
	actual=$($TMUX display-message -plj "$1" 2>&1) ||
	    fail "JSON was rejected: $1: $actual"
	[ "$actual" = "$2" ] ||
	    fail "JSON output mismatch: $1: expected $2, got $actual"
}

check_fail()
{
	if $TMUX display-message -plj "$1" >/dev/null 2>&1; then
		fail "invalid JSON was accepted: $1"
	fi
}

$TMUX new-session -d || exit 1

check '{}' '{}'
check ' { "z":true, "a":-9223372036854775808, "m":[{}, {"x":false}] } ' \
    '{"a":-9223372036854775808,"m":[{},{"x":false}],"z":true}'
check '{"max":9223372036854775807,"min":-9223372036854775808}' \
    '{"max":9223372036854775807,"min":-9223372036854775808}'
check '{" key":" value","{key":"[value"}' \
    '{" key":" value","{key":"[value"}'
check '{"brace":"{value","bracket":"[value","colon":":value",'\
'"comma":",value","space":" value"}' \
    '{"brace":"{value","bracket":"[value","colon":":value",'\
'"comma":",value","space":" value"}'
check '{"esc":"\"\\\/\b\f\n\r\t\u0041"}' \
    '{"esc":"\"\\\/\b\f\n\r\t\u0041"}'
check '{"unicode":"\u0123\uabcd\uABCD"}' \
    '{"unicode":"\u0123\uabcd\uABCD"}'

actual=$($TMUX display-message -palj '{}' 2>&1) ||
    fail "display-message -aj rejected JSON: $actual"
[ "$actual" = '{}' ] ||
    fail "display-message -aj did not ignore -a: got $actual"
actual=$($TMUX display-message -pIlj '{}' 2>&1) ||
    fail "display-message -Ij rejected JSON: $actual"
[ "$actual" = '{}' ] ||
    fail "display-message -Ij did not ignore -I: got $actual"

check_fail ''
check_fail '   '
check_fail '[]'
check_fail 'true'
check_fail '1'
check_fail '"string"'
check_fail '{}{}'

check_fail '{'
check_fail '{x:1}'
check_fail '{"":1}'
check_fail '{"x" 1}'
check_fail '{"x",1}'
check_fail '{"x":}'
check_fail '{"x":,}'
check_fail '{"x":1'
check_fail '{"x":1 "y":2}'
check_fail '{"x":1,,"y":2}'
check_fail '{"x":1,}'
check_fail '{"x":1,"x":2}'
check_fail '{"x":{}}{}'
check_fail '{"x":{}} trailing'
check_fail '{"x":{'
check_fail '{"x":{}'

check_fail '{"x":['
check_fail '{"x":[}'
check_fail '{"x":[{}'
check_fail '{"x":[{},}'
check_fail '{"x":[{},]}'
check_fail '{"x":[{}{}]}'
check_fail '{"x":[{},,{}]}'
check_fail '{"x":[{"y":}]}'
check_fail '{"x":""}'
check_fail '{"x":null}'
check_fail '{"x":[null]}'
check_fail '{"x":[true]}'
check_fail '{"x":["string"]}'
check_fail '{"x":[[]]}'
check_fail '{"x":1.0}'
check_fail '{"x":1e2}'
check_fail '{"x":+1}'
check_fail '{"x":-}'
check_fail '{"x":0x10}'
check_fail '{"x":01}'
check_fail '{"x":-01}'
check_fail '{"x":[1]}'
check_fail '{"x":9223372036854775808}'
check_fail '{"x":-9223372036854775809}'
check_fail '{"x":tru}'
check_fail '{"x":True}'
check_fail '{"x":falsee}'
check_fail '{"x":"bad\q"}'
check_fail '{"x":"bad\x20"}'
check_fail '{"x":"bad\u123"}'
check_fail '{"x":"bad\u12x4"}'
check_fail '{"x":"unterminated}'
check_fail '{"x":"escaped quote\"}'
check_fail '{"x":"line
break"}'

# The maximum object nesting depth is 200.
json='{}'
n=1
while [ "$n" -lt 200 ]; do
	json='{"x":'"$json"'}'
	n=$((n + 1))
done
check "$json" "$json"
check_fail '{"x":'"$json"'}'

if $TMUX display-message -pj >/dev/null 2>&1; then
	fail "display-message -j accepted a missing message"
fi

$TMUX kill-server 2>/dev/null
exit 0
