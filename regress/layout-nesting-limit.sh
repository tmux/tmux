#!/bin/sh

# Version 1 layouts must reject excessive nesting without losing the session or
# changing its layout. Check both split types and the depth limit boundary.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(mktemp -d) || exit 1
TMUX="$TEST_TMUX -S$DIR/socket -f/dev/null"

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
trap cleanup 0
trap 'exit 1' 1 2 15

# Generate a checksum-valid v1 layout with one child in every nested group.
layout()
{
	awk -v depth="$1" -v open="$2" 'BEGIN {
		closing = (open == "{" ? "}" : "]")
		body = ""
		for (i = 0; i < depth; i++)
			body = body "1x1,0,0" open
		body = body "1x1,0,0"
		for (i = 0; i < depth; i++)
			body = body closing

		ord["0"] = 48; ord["1"] = 49; ord["x"] = 120
		ord[","] = 44; ord["{"] = 123; ord["}"] = 125
		ord["["] = 91; ord["]"] = 93
		csum = 0
		for (i = 1; i <= length(body); i++) {
			c = substr(body, i, 1)
			bit = csum % 2
			csum = int(csum / 2) + bit * 32768
			csum = (csum + ord[c]) % 65536
		}
		printf "%04x,%s", csum, body
	}'
}

$TMUX new-session -d -s deep -x80 -y24 'exec sleep 100' || exit 1

for open in '{' '['; do
	# A valid checksum and nesting up to the limit must still be accepted.
	value=$(layout 1000 "$open") || fail "could not generate layout"
	$TMUX select-layout -t deep "$value" ||
		fail "layout at the depth limit was rejected ($open)"
	before=$($TMUX display-message -p -t deep \
	    '#{pane_id} #{pane_width} #{pane_height} #{window_layout}') || exit 1

	for depth in 1001 1500; do
		value=$(layout "$depth" "$open") || fail "could not generate layout"
		$TMUX select-layout -t deep "$value" >/dev/null 2>&1 &&
			fail "excessive nesting was accepted ($open, $depth)"
		$TMUX has-session -t deep ||
			fail "server died on excessive nesting ($open, $depth)"
		after=$($TMUX display-message -p -t deep \
		    '#{pane_id} #{pane_width} #{pane_height} #{window_layout}') || exit 1
		[ "$before" = "$after" ] ||
			fail "pane or layout changed after rejection ($open, $depth)"
	done
done

exit 0
