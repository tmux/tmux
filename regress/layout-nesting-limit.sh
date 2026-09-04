#!/bin/sh

# A layout string with enough nested { or [ groups used to drive unbounded
# recursion in layout_construct and exhaust the stack, crashing the server.
# Check that a deeply nested layout is now rejected cleanly instead, and
# that the server and an existing session survive the attempt.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(mktemp -d) || exit 1
TMUX_TMPDIR=$DIR
export TMUX_TMPDIR
TMUX="$TEST_TMUX -Ltest$$ -f/dev/null"

fail()
{
	echo "$*" >&2
	exit 1
}

cleanup()
{
	$TMUX kill-server 2>/dev/null
	rm -rf "$DIR"
}
trap cleanup 0 1 15

$TMUX new-session -d -s deep -x80 -y24 'exec sleep 100' || exit 1

# Build a layout string nested one thousand levels deep, with the checksum
# tmux's own format expects, so this fails on depth rather than on the
# checksum check. The body only ever uses digits and { } , x, so the
# checksum can be computed from a small fixed character map instead of a
# full byte table.
layout=$(awk -v depth=1500 'BEGIN {
	body = ""
	for (i = 0; i < depth; i++)
		body = body "1x1,0,0{"
	body = body "1x1,0,0"
	for (i = 0; i < depth; i++)
		body = body "}"

	csum = 0
	n = length(body)
	for (i = 1; i <= n; i++) {
		c = substr(body, i, 1)
		if (c == "{")
			ord = 123
		else if (c == "}")
			ord = 125
		else if (c == ",")
			ord = 44
		else if (c == "x")
			ord = 120
		else if (c == "0")
			ord = 48
		else if (c == "1")
			ord = 49
		bit = csum % 2
		csum = int(csum / 2) + bit * 32768
		csum = (csum + ord) % 65536
	}
	printf "%04x,%s", csum, body
}')

$TMUX select-layout -t deep "$layout" >/dev/null 2>&1 &&
	fail "an excessively nested layout string was accepted"

$TMUX has-session -t deep || fail "server died on a deeply nested layout string"
[ "$($TMUX list-panes -t deep | wc -l)" -eq 1 ] ||
	fail "the original pane did not survive the rejected layout"

exit 0
