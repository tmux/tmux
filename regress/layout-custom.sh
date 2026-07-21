#!/bin/sh

# Tests of the custom layout dumper and parser in layout-custom.c. layout_dump
# is reached through the #{window_layout} and #{window_visible_layout} formats
# and layout_parse through "select-layout <layout>".
#
# Both formats are covered:
# - the current (v2) JSON format, which is what every client except an old
#   control client sees;
# - the legacy (v1) format, which is still produced for a control client that
#   has not asked for the "new-layouts" flag, and which is still accepted by
#   the parser (the version is sniffed from the first character).
#
# This exercises:
# - dumping a single pane, a split, the "a" (active) and "l" (last pane) keys
#   and the "z" key of a floating pane;
# - #{window_visible_layout} agreeing with #{window_layout};
# - a dump being parsed back to exactly the same layout (round trip), after
#   another layout has been applied in between;
# - parsing a hand-written v2 layout, including insignificant whitespace;
# - the same layouts with their fields in reversed and scrambled orders,
#   including "c" before "t" (children parsed before the cell type is known)
#   and "V" after "L";
# - parsing a v1 layout, with the checksum computed here independently of
#   layout_checksum(), and dumping the same layout as v1 to a control client;
# - a layout with more cells than the window has panes having the bottom right
#   cells dropped;
# - failures: a bad v1 header or checksum, a malformed v1 body, unterminated
#   and malformed JSON, a wrong version, a missing or duplicated root cell,
#   missing sizes, bad cell types and pane ids, leaf cells with children and
#   node cells without, too few cells for the panes and inconsistent sizes.

PATH=/bin:/usr/bin
TERM=screen
LANG=C.UTF-8
LC_ALL=C.UTF-8
export TERM LANG LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
$TMUX kill-server 2>/dev/null

fail()
{
	echo "$*" >&2
	$TMUX kill-server 2>/dev/null
	exit 1
}

# must_equal $what $got $expected
must_equal()
{
	if [ "$2" != "$3" ]; then
		echo "$1 wrong." >&2
		echo "Expected: '$3'" >&2
		echo "But got:  '$2'" >&2
		$TMUX kill-server 2>/dev/null
		exit 1
	fi
}

# must_differ $what $got $unwanted
must_differ()
{
	[ "$2" != "$3" ] || fail "$1 unchanged: '$2'"
}

# must_contain $what $got $wanted
must_contain()
{
	case "$2" in
	*"$3"*) ;;
	*) fail "$1: '$2' does not contain '$3'";;
	esac
}

# check_ok $cmd...
#
# Run a command and require that it succeeds.
check_ok()
{
	out=$($TMUX "$@" 2>&1) || fail "Command failed (expected success): $* ($out)"
}

# check_fail $expected_error $cmd...
#
# Run a command and require that it fails with the given error message.
check_fail()
{
	exp="$1"
	shift
	out=$($TMUX "$@" 2>&1) &&
		fail "Command succeeded (expected failure): $*"
	must_equal "Error for: $*" "$out" "$exp"
}

# layout $target
#
# The layout of a window with pane ids replaced by %N, so that the expected
# strings do not depend on which ids the server handed out.
layout()
{
	$TMUX display-message -p -t "$1" '#{window_layout}' |
		sed 's/%[0-9][0-9]*/%N/g'
}

# visible_layout $target
#
# As layout(), but the visible (zoomed) layout.
visible_layout()
{
	$TMUX display-message -p -t "$1" '#{window_visible_layout}' |
		sed 's/%[0-9][0-9]*/%N/g'
}

# raw_layout $target
#
# The layout of a window with the real pane ids left in place.
raw_layout()
{
	$TMUX display-message -p -t "$1" '#{window_layout}'
}

# v1 $body
#
# Prefix a legacy (v1) layout body with its checksum. This is a separate
# implementation of layout_checksum(): a 16 bit rotate right then add, so a
# mistake in either one shows up as a mismatch.
v1()
{
	awk -v s="$1" 'BEGIN {
		for (i = 32; i < 127; i++)
			ord[sprintf("%c", i)] = i
		csum = 0
		for (i = 1; i <= length(s); i++) {
			csum = int(csum / 2) + (csum % 2) * 32768
			csum = (csum + ord[substr(s, i, 1)]) % 65536
		}
		printf "%04x,%s\n", csum, s
	}'
}

ONE='{"V":2,"L":{"t":"p","w":80,"h":24,"x":0,"y":0,"a":true,"i":"%N"}}'

$TMUX new-session -d -s L -x 80 -y 24 -n one || exit 1

# ---------------------------------------------------------------------------
# Dumping a single pane.

# The root cell of a new window is the pane itself, and it is the active pane
# so it has "a" rather than "l".
must_equal 'Single pane layout' "$(layout L:one)" "$ONE"

# Nothing is zoomed, so the visible layout is the same.
must_equal 'Single pane visible layout' "$(visible_layout L:one)" "$ONE"

# ---------------------------------------------------------------------------
# More cells than panes.

# The bottom right cells are closed until as many are left as there are panes,
# so a two cell layout applied to a one pane window collapses back to the
# single pane filling the window.
check_ok select-layout -t L:one \
	'{"V":2,"L":{"t":"v","w":80,"h":24,"x":0,"y":0,"c":[{"t":"p","w":80,"h":11,"x":0,"y":0},{"t":"p","w":80,"h":12,"x":0,"y":12}]}}'
must_equal 'Trimmed layout' "$(layout L:one)" "$ONE"

# ---------------------------------------------------------------------------
# Dumping a split.

check_ok new-window -d -t L:2 -n two
q0=$($TMUX display-message -p -t L:two.0 '#{pane_id}')

# -l 12 gives the new (bottom) pane 12 lines, leaving 11 for the top pane and
# one for the border between them. With -d the top pane stays active.
check_ok split-window -d -v -l 12 -t L:two.0
q1=$($TMUX display-message -p -t L:two.1 '#{pane_id}')
must_equal 'Split layout' "$(layout L:two)" \
	'{"V":2,"L":{"t":"v","w":80,"h":24,"x":0,"y":0,"c":[{"t":"p","w":80,"h":11,"x":0,"y":0,"a":true,"i":"%N"},{"t":"p","w":80,"h":12,"x":0,"y":12,"i":"%N"}]}}'

# ---------------------------------------------------------------------------
# The active and last pane keys.

# Selecting the bottom pane makes it active and pushes the top pane onto the
# last pane stack, where it is at index 0.
check_ok select-pane -t "$q1"
must_equal 'Layout after select-pane' "$(layout L:two)" \
	'{"V":2,"L":{"t":"v","w":80,"h":24,"x":0,"y":0,"c":[{"t":"p","w":80,"h":11,"x":0,"y":0,"l":0,"i":"%N"},{"t":"p","w":80,"h":12,"x":0,"y":12,"a":true,"i":"%N"}]}}'

# Selecting the top pane again swaps the two keys over.
check_ok select-pane -t "$q0"
SPLIT='{"V":2,"L":{"t":"v","w":80,"h":24,"x":0,"y":0,"c":[{"t":"p","w":80,"h":11,"x":0,"y":0,"a":true,"i":"%N"},{"t":"p","w":80,"h":12,"x":0,"y":12,"l":0,"i":"%N"}]}}'
must_equal 'Layout after select-pane back' "$(layout L:two)" "$SPLIT"

# ---------------------------------------------------------------------------
# The visible layout.

# With nothing zoomed the two layout formats agree.
#
# The zoomed case is deliberately not covered here. While a pane is zoomed
# #{window_layout} dumps the saved (unzoomed) layout and
# #{window_visible_layout} the zoomed one, but that depends on how zooming
# stashes the layout root rather than on anything in layout-custom.c. Add it
# back once zooming has settled.
must_equal 'Visible layout' "$(visible_layout L:two)" "$SPLIT"

# ---------------------------------------------------------------------------
# Round trip.

# Make the two panes obviously uneven so that the layout applied in between
# cannot be mistaken for the saved one.
check_ok resize-pane -t "$q0" -y 5
saved=$(raw_layout L:two)
must_equal 'Resized layout' "$(layout L:two)" \
	'{"V":2,"L":{"t":"v","w":80,"h":24,"x":0,"y":0,"c":[{"t":"p","w":80,"h":5,"x":0,"y":0,"a":true,"i":"%N"},{"t":"p","w":80,"h":18,"x":0,"y":6,"l":0,"i":"%N"}]}}'

check_ok select-layout -t L:two even-vertical
must_differ 'Layout after even-vertical' "$(raw_layout L:two)" "$saved"

# Parsing a dump gives back exactly the same dump, pane ids included.
check_ok select-layout -t L:two "$saved"
must_equal 'Round tripped layout' "$(raw_layout L:two)" "$saved"

# ---------------------------------------------------------------------------
# Parsing a hand-written layout.

# Whitespace between tokens is skipped. Note that a number is scanned up to the
# ',', ']' or '}' that ends it, so there is deliberately no space there.
check_ok select-layout -t L:two '{
	"V": 2,
	"L": {
		"t": "h",
		"w": 80,
		"h": 24,
		"x": 0,
		"y": 0,
		"c": [
			{"t": "p", "w": 30, "h": 24, "x": 0, "y": 0},
			{"t": "p", "w": 49, "h": 24, "x": 31, "y": 0}
		]
	}
}'
must_equal 'Hand-written layout' "$(layout L:two)" \
	'{"V":2,"L":{"t":"h","w":80,"h":24,"x":0,"y":0,"c":[{"t":"p","w":30,"h":24,"x":0,"y":0,"a":true,"i":"%N"},{"t":"p","w":49,"h":24,"x":31,"y":0,"l":0,"i":"%N"}]}}'

# The panes are assigned to the cells in order.
must_equal 'First pane width' \
	"$($TMUX display-message -p -t "$q0" '#{pane_width}')" '30'
must_equal 'Second pane width' \
	"$($TMUX display-message -p -t "$q1" '#{pane_width}')" '49'

# ---------------------------------------------------------------------------
# Field order.

# Fields are looked up by key, so any order must give the same layout. Here
# every object has its keys reversed: "c" comes before "t", so the children are
# evaluated while the cell type is still the default, and "V" comes after "L",
# so the version is only known once the layout has been built.
check_ok select-layout -t L:two '{"L":{"c":[{"y":0,"x":0,"h":8,"w":80,"t":"p"},{"y":9,"x":0,"h":15,"w":80,"t":"p"}],"y":0,"x":0,"h":24,"w":80,"t":"v"},"V":2}'
must_equal 'Reversed field order' "$(layout L:two)" \
	'{"V":2,"L":{"t":"v","w":80,"h":24,"x":0,"y":0,"c":[{"t":"p","w":80,"h":8,"x":0,"y":0,"a":true,"i":"%N"},{"t":"p","w":80,"h":15,"x":0,"y":9,"l":0,"i":"%N"}]}}'

# Keys interleaved rather than simply reversed, with "c" in the middle. The
# "a" here is on the second cell but is ignored on parsing, so the dump still
# marks the first cell active: the active pane comes from the window, not the
# layout.
check_ok select-layout -t L:two '{"V":2,"L":{"h":24,"c":[{"w":40,"t":"p","y":0,"h":24,"x":0},{"a":true,"h":24,"w":39,"y":0,"t":"p","x":41}],"w":80,"y":0,"t":"h","x":0}}'
must_equal 'Scrambled field order' "$(layout L:two)" \
	'{"V":2,"L":{"t":"h","w":80,"h":24,"x":0,"y":0,"c":[{"t":"p","w":40,"h":24,"x":0,"y":0,"a":true,"i":"%N"},{"t":"p","w":39,"h":24,"x":41,"y":0,"l":0,"i":"%N"}]}}'

# The same rejections apply whatever order the fields are in: a leaf with
# children when "c" is seen first, a node with no children when "t" is last, a
# bad cell type when "t" is last, and a bad pane id when "i" is first.
check_fail 'invalid layout: {"V":2,"L":{"c":[{"t":"p","w":80,"h":24,"x":0,"y":0}],"t":"p","w":80,"h":24,"x":0,"y":0}}' \
	select-layout -t L:two '{"V":2,"L":{"c":[{"t":"p","w":80,"h":24,"x":0,"y":0}],"t":"p","w":80,"h":24,"x":0,"y":0}}'
check_fail 'invalid layout: {"V":2,"L":{"w":80,"h":24,"x":0,"y":0,"t":"v"}}' \
	select-layout -t L:two '{"V":2,"L":{"w":80,"h":24,"x":0,"y":0,"t":"v"}}'
check_fail 'invalid layout: {"V":2,"L":{"w":80,"h":24,"x":0,"y":0,"t":"q"}}' \
	select-layout -t L:two '{"V":2,"L":{"w":80,"h":24,"x":0,"y":0,"t":"q"}}'
check_fail 'invalid layout: {"V":2,"L":{"i":"0","t":"p","w":80,"h":24,"x":0,"y":0}}' \
	select-layout -t L:two '{"V":2,"L":{"i":"0","t":"p","w":80,"h":24,"x":0,"y":0}}'

# A child that fails after some children have already been added, with "c"
# before "t" so the parent's type is still the default when it gives up. This
# is the case the cleanup at the end of layout_evaluate_layout exists for: the
# already-built children have to be freed even though the parent does not yet
# look like a node. The second child has no "y".
check_fail 'invalid layout: {"V":2,"L":{"c":[{"t":"p","w":80,"h":11,"x":0,"y":0},{"t":"p","w":80,"h":12,"x":0}],"t":"v","w":80,"h":24,"x":0,"y":0}}' \
	select-layout -t L:two '{"V":2,"L":{"c":[{"t":"p","w":80,"h":11,"x":0,"y":0},{"t":"p","w":80,"h":12,"x":0}],"t":"v","w":80,"h":24,"x":0,"y":0}}'

# A wrong version after a layout that is otherwise fine, so the built cells
# have to be thrown away once "V" is finally seen.
check_fail 'invalid layout: {"L":{"t":"p","w":80,"h":24,"x":0,"y":0},"V":1}' \
	select-layout -t L:two '{"L":{"t":"p","w":80,"h":24,"x":0,"y":0},"V":1}'

# ---------------------------------------------------------------------------
# The legacy (v1) format.

body="80x24,0,0[80x11,0,0,${q0#%},80x12,0,12,${q1#%}]"

# A v1 layout with a correct checksum is accepted and gives the same layout as
# the equivalent v2 one.
check_ok select-layout -t L:two "$(v1 "$body")"
must_equal 'Layout parsed from v1' "$(layout L:two)" "$SPLIT"

# A control client that has not asked for new layouts is dumped v1, which must
# be the string that was just parsed. Its output is wrapped in %begin/%end
# guard lines.
got=$($TMUX -C display-message -p -t L:two '#{window_layout}' | grep -v '^%')
must_equal 'v1 dump' "$got" "$(v1 "$body")"

# With the new-layouts flag the same client is dumped v2 instead. The flag is
# set with "attach -f" rather than refresh-client because refresh-client needs
# a current client, which a control client that has not attached has not got.
got=$(printf "display-message -p -t L:two '#{window_layout}'\n" |
	$TMUX -C attach -f new-layouts -t L 2>&1 | grep -v '^%')
must_contain 'v2 dump for control client' "$got" '{"V":2,"L":'

# ---------------------------------------------------------------------------
# Failures.

# check_layout_fail $cause $layout
#
# select-layout must reject $layout with "<cause>: <layout>".
check_layout_fail()
{
	check_fail "$1: $2" select-layout -t L:two "$2"
}

# Not JSON and not a checksum.
check_layout_fail 'malformed layout header' 'garbage'

# A v1 header with the checksum of a different body.
good=$(v1 '80x24,0,0')
check_layout_fail 'invalid layout checksum' "${good%%,*},80x24,0,1"

# A correct checksum over a body that is not a layout.
check_layout_fail 'invalid layout' "$(v1 '80x24')"
check_layout_fail 'invalid layout' "$(v1 '80x24,0,0[80x11,0,0,80x12,0,12}')"

# The value of "V" runs to the end of the string without a terminator, so
# tokenizing fails.
check_layout_fail 'invalid layout characters' '{"V":2'

# Structurally invalid JSON: an unclosed object, a trailing comma, a second
# object after the layout, a number that is not one, and an array member that
# is not an object.
check_layout_fail 'invalid layout json' '{"V":2,"L":{"t":"p"'
check_layout_fail 'invalid layout json' \
	'{"V":2,"L":{"t":"p","w":80,"h":24,"x":0,"y":0},}'
check_layout_fail 'invalid layout json' \
	'{"V":2,"L":{"t":"p","w":80,"h":24,"x":0,"y":0}}{}'
check_layout_fail 'invalid layout json' \
	'{"V":2,"L":{"t":"p","w":8a,"h":24,"x":0,"y":0}}'
check_layout_fail 'invalid layout json' \
	'{"V":2,"L":{"t":"p","w":80,"h":24,"x":0,"y":0,"c":["x"]}}'

# Valid JSON that is not a valid layout: the wrong version, no root cell, two
# root cells, a missing "y", an unknown cell type and a pane id without its %.
check_layout_fail 'invalid layout' \
	'{"V":1,"L":{"t":"p","w":80,"h":24,"x":0,"y":0}}'
check_layout_fail 'invalid layout' '{"V":2}'
check_layout_fail 'invalid layout' \
	'{"V":2,"L":{"t":"p","w":80,"h":24,"x":0,"y":0},"L":{"t":"p","w":80,"h":24,"x":0,"y":0}}'
check_layout_fail 'invalid layout' '{"V":2,"L":{"t":"p","w":80,"h":24,"x":0}}'
check_layout_fail 'invalid layout' \
	'{"V":2,"L":{"t":"q","w":80,"h":24,"x":0,"y":0}}'
check_layout_fail 'invalid layout' \
	'{"V":2,"L":{"t":"p","w":80,"h":24,"x":0,"y":0,"i":"0"}}'

# A node cell must have children and a leaf cell must not.
check_layout_fail 'invalid layout' '{"V":2,"L":{"t":"v","w":80,"h":24,"x":0,"y":0}}'
check_layout_fail 'invalid layout' \
	'{"V":2,"L":{"t":"v","w":80,"h":24,"x":0,"y":0,"c":[]}}'
check_layout_fail 'invalid layout' \
	'{"V":2,"L":{"t":"p","w":80,"h":24,"x":0,"y":0,"c":[{"t":"p","w":80,"h":24,"x":0,"y":0}]}}'

# Fewer cells than the window has panes; unlike the other way around this
# cannot be fixed up.
check_layout_fail 'have 2 panes but need 1' \
	'{"V":2,"L":{"t":"p","w":80,"h":24,"x":0,"y":0}}'

# The children of a top to bottom cell must all be the width of their parent.
check_layout_fail 'size mismatch after applying layout' \
	'{"V":2,"L":{"t":"v","w":80,"h":24,"x":0,"y":0,"c":[{"t":"p","w":80,"h":11,"x":0,"y":0},{"t":"p","w":40,"h":12,"x":0,"y":12}]}}'

# None of that touched the layout.
must_equal 'Layout after failures' "$(layout L:two)" "$SPLIT"

# ---------------------------------------------------------------------------
# Floating panes.

check_ok new-window -d -t L:3 -n float
check_ok select-window -t L:float
check_ok new-pane -d -x 20 -y 6 -X 8 -Y 3 'sleep 100'

# A floating cell is dumped with its z-index, which is what marks it as
# floating when the layout is parsed back.
must_contain 'Floating layout' "$(layout L:float)" '"z":'
check_ok select-layout -t L:float "$(raw_layout L:float)"
must_contain 'Floating layout after round trip' "$(layout L:float)" '"z":'

if [ "$($TMUX display-message -p alive 2>&1)" != "alive" ]; then
	echo "Server died." >&2
	exit 1
fi

$TMUX kill-server 2>/dev/null
exit 0
