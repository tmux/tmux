#!/bin/sh

# Tests of the custom layout dumper and evaluator in layout-custom.c, and of
# the JSON tokenizer and parser in json.c that the current layout format is
# built on.
#
# layout_dump is reached through the #{window_layout} and
# #{window_visible_layout} formats and layout_parse through
# "select-layout <layout>". json.c has no command of its own either:
# layout_construct sniffs the first non-blank character and hands anything
# starting with '{' to json_parse, so select-layout is the only way into it
# from the shell as well.
#
# Both layout formats are covered:
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
# - the JSON syntax itself: insignificant whitespace, backslash escapes inside
#   strings, the number and boolean forms, and one failure for each error
#   json.c can report;
# - a dump being parsed back to exactly the same layout (round trip), after
#   another layout has been applied in between;
# - parsing a hand-written v2 layout and the panes being assigned to its cells
#   in order;
# - the same layouts with their fields in reversed and scrambled orders,
#   including "c" before "t" (children parsed before the cell type is known)
#   and "V" after "L";
# - a layout with more cells than the window has panes having the bottom right
#   cells dropped, in both formats;
# - a layout naming no active or last pane leaving both as they were;
# - parsing a v1 layout and dumping it back as v1 through a control client,
#   with the checksum computed here independently of layout_checksum(), and a
#   v1 layout leaving the active pane and last pane stack untouched;
# - the %layout-change notification, in both formats at once: two control
#   clients watching one layout change, only one of which has asked for new
#   layouts, and the number of notifications a change produces in each format;
# - failures: a bad v1 header, checksum or body, a wrong version, a missing or
#   duplicated root cell, missing sizes, bad cell types and pane ids, leaf
#   cells with children and node cells without, too few cells for the panes and
#   inconsistent sizes.

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

# A pane cell is dumped as its geometry, then "a" if it is the active pane or
# "l" with its position on the last pane stack if it is on it, then "i" with
# its pane index, then "z" if it is floating, then "I" with its pane id.
ONE='{"V":2,"L":{"t":"p","w":80,"h":24,"x":0,"y":0,"a":true,"i":0,"I":"%N"}}'

# A single leaf cell filling the window, without the keys that only the dumper
# writes. Used by the JSON checks, which care about the syntax around it.
LEAF='{"t":"p","w":80,"h":24,"x":0,"y":0}'

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
# single pane filling the window: the cell that is left takes the space of the
# one that was closed.
check_ok select-layout -t L:one \
	'{"V":2,"L":{"t":"v","w":80,"h":24,"x":0,"y":0,"c":[{"t":"p","w":80,"h":11,"x":0,"y":0},{"t":"p","w":80,"h":12,"x":0,"y":12}]}}'
must_equal 'Trimmed layout' "$(layout L:one)" "$ONE"

# ---------------------------------------------------------------------------
# The JSON syntax.
#
# These run on the one pane window and are written so that what they prove
# depends on json.c rather than on the layout evaluation in layout-custom.c:
# an accepted layout is only required to leave the window as its single pane,
# and values that are not part of the layout format are carried on keys
# layout-custom.c never looks at ("n", "b" and so on), which it skips, so
# numbers, booleans and escapes can be exercised on their own.
#
# Objects nested in an array nested in an object are not checked here: every
# split layout below is one.
#
# Two of json.c's messages cannot be reached from the shell and so are not
# covered. "expected object" is unreachable because layout_construct() only
# calls json_parse() once the string already starts with '{', and "invalid
# boolean" is unreachable because json_parse_boolean() is only called after the
# value has already matched "true" or "false".

# check_json_ok $what $layout
#
# select-layout must parse $layout and leave the window as its single pane.
check_json_ok()
{
	check_ok select-layout -t L:one "$2"
	must_equal "Layout after '$1'" "$(layout L:one)" "$ONE"
}

# check_json_fail $what $reason $layout
#
# select-layout must reject $layout with an error beginning with $reason.
# json_error() appends up to ERROR_CTX_LEN characters of context from the point
# of failure and cmd-select-layout.c then appends the layout itself, so only
# the reason is matched.
check_json_fail()
{
	out=$($TMUX select-layout -t L:one "$3" 2>&1) &&
		fail "$1: select-layout succeeded (expected failure)"
	case "$out" in
	"$2"*) ;;
	*) fail "$1: expected '$2...' but got '$out'";;
	esac
}

# Whitespace between tokens is skipped. A number is scanned up to the ',', ']',
# '}' or whitespace that ends it, so a space after a number is fine but one
# inside it is not.
check_json_ok 'Spaces between tokens' \
	'{ "V" : 2 , "L" : { "t" : "p" , "w" : 80 , "h" : 24 , "x" : 0 , "y" : 0 } }'

check_json_ok 'Newlines and tabs between tokens' "$(printf '{
\t"V": 2,
\t"L": {
\t\t"t": "p",
\t\t"w": 80,
\t\t"h": 24,
\t\t"x": 0,
\t\t"y": 0
\t}
}')"

check_json_ok 'Carriage returns between tokens' \
	"$(printf '{\r"V":2,\r"L":%s\r}' "$LEAF")"

# A backslash makes the tokenizer consume the next character whatever it is, so
# an escaped quote does not end the string. The key is not one that
# layout-custom.c looks at, so all that is being checked is that the string
# ended in the right place and the object still parsed.
check_json_ok 'Escaped quote in a string' \
	'{"V":2,"a\"b":0,"L":'"$LEAF"'}'

# An escaped backslash immediately before the closing quote: the escape has to
# be cleared again so that the quote after it does end the string.
check_json_ok 'Escaped backslash before the closing quote' \
	'{"V":2,"a\\":0,"L":'"$LEAF"'}'

# Numbers and booleans, again on keys layout-custom.c ignores, so only json.c
# decides whether they are accepted.
check_json_ok 'Zero' '{"V":2,"n":0,"L":'"$LEAF"'}'
check_json_ok 'Several digits' '{"V":2,"n":1234567,"L":'"$LEAF"'}'
check_json_ok 'Negative number' '{"V":2,"n":-42,"L":'"$LEAF"'}'
check_json_ok 'Booleans' '{"V":2,"b":true,"d":false,"L":'"$LEAF"'}'

# Tokenizer failures. A value that runs to the end of the input has no
# terminator, so it is the tokenizer rather than the parser that gives up. Both
# the number scan and the string scan have to notice this, and with the closing
# quote escaped there is no terminator left either.
check_json_fail 'Unterminated number' 'tokenization error' '{"V":2'
check_json_fail 'Unterminated string' 'tokenization error' '{"V":"x'
check_json_fail 'Escaped closing quote' 'tokenization error' \
	'{"V":2,"L":{"t":"p\"}}'

# Something that is not a quoted string where a key belongs.
check_json_fail 'Missing key' 'invalid key' '{"V":2,,"L":'"$LEAF"'}'

# A key not followed by ':'.
check_json_fail 'Missing colon' 'missing colon' '{"V","L":2}'

# A bare word that is neither "true", "false" nor a number. This is where
# "null" ends up.
check_json_fail 'Unknown literal' 'invalid value' '{"V":null,"L":'"$LEAF"'}'

# A ':' with no value after it, so the token where the value belongs is one the
# object parser has no case for.
check_json_fail 'Missing value' 'unsupported object token' '{"V":}'

# A ',' with nothing after it, and a value with no ',' before the next key.
check_json_fail 'Trailing comma in an object' 'invalid object' \
	'{"V":2,"L":'"$LEAF"',}'
check_json_fail 'Missing comma in an object' 'invalid object' \
	'{"V":2 "L":'"$LEAF"'}'

# Arrays hold objects and nothing else.
check_json_fail 'Non-object in an array' 'invalid array member' \
	'{"V":2,"L":{"t":"v","w":80,"h":24,"x":0,"y":0,"c":["x"]}}'
check_json_fail 'Trailing comma in an array' 'invalid array' \
	'{"V":2,"L":{"t":"v","w":80,"h":24,"x":0,"y":0,"c":[{"t":"p","w":80,"h":11,"x":0,"y":0},]}}'

# An empty string is two adjacent quotes with no value token between them,
# which the string parser does not accept.
check_json_fail 'Empty string' 'invalid string' '{"V":2,"L":""}'

# A number token that strtoll does not consume all of.
check_json_fail 'Number with trailing characters' 'invalid number' \
	'{"V":8a,"L":'"$LEAF"'}'

# Anything after the top level object.
check_json_fail 'Data after the top level object' 'unexpected trailing data' \
	'{"V":2,"L":'"$LEAF"'}{}'

# None of the rejections touched the layout.
must_equal 'Layout after rejected parses' "$(layout L:one)" "$ONE"

# ---------------------------------------------------------------------------
# Dumping a split.

check_ok new-window -d -t L:2 -n two
q0=$($TMUX display-message -p -t L:two.0 '#{pane_id}')

# -l 12 gives the new (bottom) pane 12 lines, leaving 11 for the top pane and
# one for the border between them. With -d the top pane stays active.
check_ok split-window -d -v -l 12 -t L:two.0
q1=$($TMUX display-message -p -t L:two.1 '#{pane_id}')

# Nothing has changed the active pane, so the last pane stack is still empty
# and the bottom pane has neither "a" nor "l".
must_equal 'Split layout' "$(layout L:two)" \
	'{"V":2,"L":{"t":"v","w":80,"h":24,"x":0,"y":0,"c":[{"t":"p","w":80,"h":11,"x":0,"y":0,"a":true,"i":0,"I":"%N"},{"t":"p","w":80,"h":12,"x":0,"y":12,"i":1,"I":"%N"}]}}'

# ---------------------------------------------------------------------------
# The active and last pane keys.

# Selecting the bottom pane makes it active and pushes the top pane onto the
# last pane stack, where it is at index 0.
check_ok select-pane -t "$q1"
must_equal 'Layout after select-pane' "$(layout L:two)" \
	'{"V":2,"L":{"t":"v","w":80,"h":24,"x":0,"y":0,"c":[{"t":"p","w":80,"h":11,"x":0,"y":0,"l":0,"i":0,"I":"%N"},{"t":"p","w":80,"h":12,"x":0,"y":12,"a":true,"i":1,"I":"%N"}]}}'

# Selecting the top pane again swaps the two keys over. "i" and "I" do not
# move: they are the pane's position in the window and its id.
check_ok select-pane -t "$q0"
SPLIT='{"V":2,"L":{"t":"v","w":80,"h":24,"x":0,"y":0,"c":[{"t":"p","w":80,"h":11,"x":0,"y":0,"a":true,"i":0,"I":"%N"},{"t":"p","w":80,"h":12,"x":0,"y":12,"l":0,"i":1,"I":"%N"}]}}'
must_equal 'Layout after select-pane back' "$(layout L:two)" "$SPLIT"

# ---------------------------------------------------------------------------
# The visible layout.

# With nothing zoomed the two layout formats agree.
#
# The zoomed case is deliberately not covered here. While a pane is zoomed
# #{window_layout} dumps the saved (unzoomed) layout and
# #{window_visible_layout} the zoomed one, but that depends on how zooming
# stashes the layout root rather than on anything in layout-custom.c.
must_equal 'Visible layout' "$(visible_layout L:two)" "$SPLIT"

# ---------------------------------------------------------------------------
# Round trip.

# Make the two panes obviously uneven so that the layout applied in between
# cannot be mistaken for the saved one. A resize shows up in the dump as the
# new cell sizes and offsets.
check_ok resize-pane -t "$q0" -y 5
saved=$(raw_layout L:two)
must_equal 'Resized layout' "$(layout L:two)" \
	'{"V":2,"L":{"t":"v","w":80,"h":24,"x":0,"y":0,"c":[{"t":"p","w":80,"h":5,"x":0,"y":0,"a":true,"i":0,"I":"%N"},{"t":"p","w":80,"h":18,"x":0,"y":6,"l":0,"i":1,"I":"%N"}]}}'

check_ok select-layout -t L:two even-vertical
must_differ 'Layout after even-vertical' "$(raw_layout L:two)" "$saved"

# Parsing a dump gives back exactly the same dump, pane ids included. The panes
# go back into the cells that named them: the cells are ordered by "i" and then
# given the window's panes in order, so a cell dumped with "i":k must come back
# the k'th.
check_ok select-layout -t L:two "$saved"
must_equal 'Round tripped layout' "$(raw_layout L:two)" "$saved"

# ---------------------------------------------------------------------------
# Parsing a hand-written layout.

# Laid out over several lines to keep it readable; that the whitespace is
# skipped at all is json.c's business, what matters here is that the cells come
# out of it in the right shape.
#
# "a" and "l" are given on the cells so that the active pane and the last pane
# stack are pinned by the layout rather than left to whatever a layout that
# names neither happens to produce.
check_ok select-layout -t L:two '{
	"V": 2,
	"L": {
		"t": "h",
		"w": 80,
		"h": 24,
		"x": 0,
		"y": 0,
		"c": [
			{"t": "p", "w": 30, "h": 24, "x": 0, "y": 0, "a": true},
			{"t": "p", "w": 49, "h": 24, "x": 31, "y": 0, "l": 0}
		]
	}
}'
must_equal 'Hand-written layout' "$(layout L:two)" \
	'{"V":2,"L":{"t":"h","w":80,"h":24,"x":0,"y":0,"c":[{"t":"p","w":30,"h":24,"x":0,"y":0,"a":true,"i":0,"I":"%N"},{"t":"p","w":49,"h":24,"x":31,"y":0,"l":0,"i":1,"I":"%N"}]}}'

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
check_ok select-layout -t L:two '{"L":{"c":[{"a":true,"y":0,"x":0,"h":8,"w":80,"t":"p"},{"l":0,"y":9,"x":0,"h":15,"w":80,"t":"p"}],"y":0,"x":0,"h":24,"w":80,"t":"v"},"V":2}'
must_equal 'Reversed field order' "$(layout L:two)" \
	'{"V":2,"L":{"t":"v","w":80,"h":24,"x":0,"y":0,"c":[{"t":"p","w":80,"h":8,"x":0,"y":0,"a":true,"i":0,"I":"%N"},{"t":"p","w":80,"h":15,"x":0,"y":9,"l":0,"i":1,"I":"%N"}]}}'

# Keys interleaved rather than simply reversed, with "c" in the middle. This
# time "a" is on the second cell, so the second pane becomes the active one:
# which pane is active comes from the layout, while "i" and "I" still come from
# the window. The first cell names neither "a" nor "l", so its pane is neither
# active nor on the last pane stack and the dump gives it neither key.
check_ok select-layout -t L:two '{"V":2,"L":{"h":24,"c":[{"w":40,"t":"p","y":0,"h":24,"x":0},{"a":true,"h":24,"w":39,"y":0,"t":"p","x":41}],"w":80,"y":0,"t":"h","x":0}}'
must_equal 'Scrambled field order' "$(layout L:two)" \
	'{"V":2,"L":{"t":"h","w":80,"h":24,"x":0,"y":0,"c":[{"t":"p","w":40,"h":24,"x":0,"y":0,"i":0,"I":"%N"},{"t":"p","w":39,"h":24,"x":41,"y":0,"a":true,"i":1,"I":"%N"}]}}'

# ---------------------------------------------------------------------------
# The legacy (v1) format.

# The layout just applied, in v1: a left/right cell is written with braces and
# a top/bottom cell with brackets, and each leaf carries its pane id without
# the leading %.
v1body="80x24,0,0{40x24,0,0,${q0#%},39x24,41,0,${q1#%}}"

# A control client that has not asked for new layouts is dumped v1. Its output
# is wrapped in %begin/%end guard lines.
got=$($TMUX -C display-message -p -t L:two '#{window_layout}' | grep -v '^%')
must_equal 'v1 dump' "$got" "$(v1 "$v1body")"

# With the new-layouts flag the same client is dumped v2 instead. The flag is
# set with "attach -f" rather than refresh-client because refresh-client needs
# a current client, which a control client that has not attached has not got.
got=$(printf "display-message -p -t L:two '#{window_layout}'\n" |
	$TMUX -C attach -f new-layouts -t L 2>&1 | grep -v '^%')
must_contain 'v2 dump for control client' "$got" '{"V":2,"L":'

# A v1 layout with a correct checksum is parsed, and dumping v1 again gives
# back the same string. That is the whole of what v1 carries: the cells take
# the sizes and offsets from the body, and the panes are assigned to them in
# order, which is what puts the same two ids back in the same two places. It is
# checked in v1 rather than against a v2 dump so that nothing v1 has no opinion
# on - the active pane, the last pane stack, the pane index - comes into it.
v1vsplit="80x24,0,0[80x11,0,0,${q0#%},80x12,0,12,${q1#%}]"
check_ok select-layout -t L:two "$(v1 "$v1vsplit")"
got=$($TMUX -C display-message -p -t L:two '#{window_layout}' | grep -v '^%')
must_equal 'v1 round trip' "$got" "$(v1 "$v1vsplit")"

# v1 names no active pane, last pane or z-index and must disturb none of them.
# Applying the v1 form of the layout the window already has therefore leaves
# even the v2 dump the same byte for byte, last pane stack included.
check_ok select-pane -t "$q1"
check_ok select-pane -t "$q0"
before=$(raw_layout L:two)
check_ok select-layout -t L:two "$(v1 "$v1vsplit")"
must_equal 'v1 leaves the active and last panes alone' \
	"$(raw_layout L:two)" "$before"

# A v1 layout with more cells than the window has panes is trimmed like any
# other: the bottom right cell is closed and the cell above it takes its eight
# rows and the border between them, leaving 16. Pane ids in a v1 body are not
# used to place panes, so the third cell can carry any id.
v1three="80x24,0,0[80x7,0,0,${q0#%},80x7,0,8,${q1#%},80x8,0,16,999]"
check_ok select-layout -t L:two "$(v1 "$v1three")"
got=$($TMUX -C display-message -p -t L:two '#{window_layout}' | grep -v '^%')
must_equal 'v1 layout trimmed' "$got" \
	"$(v1 "80x24,0,0[80x7,0,0,${q0#%},80x16,0,8,${q1#%}]")"

# ---------------------------------------------------------------------------
# Cells that name no active or last pane.

# "a" and "l" are the only things that decide which pane is active and what is
# on the last pane stack, so a layout naming neither leaves the active pane
# where it was and puts nothing on the stack. Here the top pane is active and
# the bottom one is at index 0 of the stack beforehand; afterwards the top pane
# is still active and the bottom pane is on no stack, so it has no "l".
check_ok select-pane -t "$q1"
check_ok select-pane -t "$q0"
check_ok select-layout -t L:two \
	'{"V":2,"L":{"t":"v","w":80,"h":24,"x":0,"y":0,"c":[{"t":"p","w":80,"h":9,"x":0,"y":0},{"t":"p","w":80,"h":14,"x":0,"y":10}]}}'
must_equal 'Layout naming no active pane' "$(layout L:two)" \
	'{"V":2,"L":{"t":"v","w":80,"h":24,"x":0,"y":0,"c":[{"t":"p","w":80,"h":9,"x":0,"y":0,"a":true,"i":0,"I":"%N"},{"t":"p","w":80,"h":14,"x":0,"y":10,"i":1,"I":"%N"}]}}'

# ---------------------------------------------------------------------------
# Failures with a message.

# check_layout_fail $cause $layout
#
# select-layout must reject $layout with "<cause>: <layout>".
check_layout_fail()
{
	check_fail "$1: $2" select-layout -t L:two "$2"
}

# A rejected layout must leave the window alone, whatever it was.
unchanged=$(raw_layout L:two)

# Not JSON and not a checksum.
check_layout_fail 'malformed layout header' 'garbage'

# A v1 header with the checksum of a different body.
good=$(v1 '80x24,0,0')
check_layout_fail 'invalid layout checksum' "${good%%,*},80x24,0,1"

# A correct checksum over a body that is not a layout: a cell with no offsets,
# and a top to bottom cell closed with '}' instead of ']'. layout_construct_v1
# returns NULL for both and layout_construct() reports it.
check_layout_fail 'invalid layout' "$(v1 '80x24')"
check_layout_fail 'invalid layout' "$(v1 '80x24,0,0[80x11,0,0,80x12,0,12}')"

# Fewer cells than the window has panes; unlike the other way around this
# cannot be fixed up.
check_layout_fail 'have 2 panes but need 1' \
	'{"V":2,"L":{"t":"p","w":80,"h":24,"x":0,"y":0}}'

# The children of a top to bottom cell must all be the width of their parent.
check_layout_fail 'size mismatch after applying layout' \
	'{"V":2,"L":{"t":"v","w":80,"h":24,"x":0,"y":0,"c":[{"t":"p","w":80,"h":11,"x":0,"y":0},{"t":"p","w":40,"h":12,"x":0,"y":12}]}}'

# The rest are valid JSON, so it is layout_parse_json() and
# layout_parse_json_layout() doing the rejecting rather than json.c, and their
# own cause reaches the client.

# Two root cells.
check_layout_fail 'duplicate layout' \
	'{"V":2,"L":{"t":"p","w":80,"h":24,"x":0,"y":0},"L":{"t":"p","w":80,"h":24,"x":0,"y":0}}'

# A missing "y". A cell needs all four of "w", "h", "x" and "y".
check_layout_fail 'cell geometry must be fully specified' \
	'{"V":2,"L":{"t":"p","w":80,"h":24,"x":0}}'

# An unknown cell type: only "h", "v" and "p" exist.
check_layout_fail 'invalid cell type q' \
	'{"V":2,"L":{"t":"q","w":80,"h":24,"x":0,"y":0}}'

# A pane id without its %, and one with trailing rubbish after the number. Note
# it is "I" that carries the pane id and requires the %; "i" is the pane index
# and takes a plain number.
check_layout_fail "pane id must be prefixed by '%'" \
	'{"V":2,"L":{"t":"p","w":80,"h":24,"x":0,"y":0,"I":"0"}}'
check_layout_fail 'invalid pane id: %1x' \
	'{"V":2,"L":{"t":"p","w":80,"h":24,"x":0,"y":0,"I":"%1x"}}'

# A node cell must have children and a leaf cell must not.
check_layout_fail 'non-pane cells must have children' \
	'{"V":2,"L":{"t":"v","w":80,"h":24,"x":0,"y":0}}'
check_layout_fail 'non-pane cells must have children' \
	'{"V":2,"L":{"t":"v","w":80,"h":24,"x":0,"y":0,"c":[]}}'
check_layout_fail 'pane cells cannot have children' \
	'{"V":2,"L":{"t":"p","w":80,"h":24,"x":0,"y":0,"c":[{"t":"p","w":80,"h":24,"x":0,"y":0}]}}'

# The same rejections apply whatever order the fields are in: a leaf with
# children when "c" is seen first, a node with no children when "t" is last, a
# bad cell type when "t" is last, and a bad pane id when "I" is first.
check_layout_fail 'pane cells cannot have children' \
	'{"V":2,"L":{"c":[{"t":"p","w":80,"h":24,"x":0,"y":0}],"t":"p","w":80,"h":24,"x":0,"y":0}}'
check_layout_fail 'non-pane cells must have children' \
	'{"V":2,"L":{"w":80,"h":24,"x":0,"y":0,"t":"v"}}'
check_layout_fail 'invalid cell type q' '{"V":2,"L":{"w":80,"h":24,"x":0,"y":0,"t":"q"}}'
check_layout_fail "pane id must be prefixed by '%'" \
	'{"V":2,"L":{"I":"0","t":"p","w":80,"h":24,"x":0,"y":0}}'

# A child that fails after some children have already been added, with "c"
# before "t" so the parent's type is still the default when it gives up. This
# is the case the cleanup at the end of layout_parse_json_layout exists for:
# the already-built children have to be freed even though the parent does not
# yet look like a node. The second child has no "y", and its cause is the one
# that comes back.
check_layout_fail 'cell geometry must be fully specified' \
	'{"V":2,"L":{"c":[{"t":"p","w":80,"h":11,"x":0,"y":0},{"t":"p","w":80,"h":12,"x":0}],"t":"v","w":80,"h":24,"x":0,"y":0}}'

# No root cell at all. Every other rejection above comes from a cell that
# failed to parse; this one is the check after the loop, reached when no "L"
# was seen at all.
check_layout_fail 'missing layout' '{"V":2}'

# The wrong version, and the wrong version after a layout that is otherwise
# fine so that the built cells have to be thrown away once "V" is finally seen.
check_layout_fail 'version mismatch.' \
	'{"V":1,"L":{"t":"p","w":80,"h":24,"x":0,"y":0}}'
check_layout_fail 'version mismatch.' \
	'{"L":{"t":"p","w":80,"h":24,"x":0,"y":0},"V":1}'

# None of that touched the layout.
must_equal 'Layout after failures' "$(raw_layout L:two)" "$unchanged"

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

# ---------------------------------------------------------------------------
# Control mode notifications.
#
# %layout-change is what a control client actually reads a layout from, and it
# carries both #{window_layout} and #{window_visible_layout}. Its template is
# expanded once per client (control-notify.c), so two clients watching the same
# window must be told about the same change in different formats: v1 for the
# one that has not asked for new layouts, v2 for the one that has.
#
# The dumps above go through "-C display-message", which only ever reaches the
# format callbacks for the client asking. This needs clients that stay
# attached while something else changes the layout, so they go on the end of
# fifos and the change is made from outside.

DIR=$(mktemp -d) || exit 1
OLDIN="$DIR/old-in"
OLDOUT="$DIR/old-out"
NEWIN="$DIR/new-in"
NEWOUT="$DIR/new-out"
OLDPID=
NEWPID=

cleanup()
{
	[ -n "$OLDPID" ] && kill "$OLDPID" 2>/dev/null
	[ -n "$NEWPID" ] && kill "$NEWPID" 2>/dev/null
	rm -rf "$DIR"
}
trap cleanup EXIT

# wait_for $file $text
#
# Wait for $text to appear in a control client's output.
wait_for()
{
	i=0
	while [ "$i" -lt 6 ]; do
		grep -F -- "$2" "$1" >/dev/null 2>&1 && return 0
		sleep 1
		i=$((i + 1))
	done
	echo "missing from $1: $2" >&2
	cat "$1" >&2
	return 1
}

mkfifo "$OLDIN" "$NEWIN" || exit 1
: >"$OLDOUT"
: >"$NEWOUT"

$TMUX -C attach -t L <"$OLDIN" >"$OLDOUT" 2>&1 &
OLDPID=$!
exec 4>"$OLDIN"
$TMUX -C attach -f new-layouts -t L <"$NEWIN" >"$NEWOUT" 2>&1 &
NEWPID=$!
exec 5>"$NEWIN"

# Both clients have to be attached before the layout changes, or they miss the
# notification entirely.
printf 'display-message -p ready\n' >&4
printf 'display-message -p ready\n' >&5
wait_for "$OLDOUT" ready || fail 'Control client without new-layouts did not attach'
wait_for "$NEWOUT" ready || fail 'Control client with new-layouts did not attach'

wid=$($TMUX display-message -p -t L:two '#{window_id}')

# One layout change, made by a third client so that neither of the two is the
# one running the command. 8 lines for the top pane leaves 15 for the bottom
# and one for the border.
check_ok resize-pane -t "$q0" -y 8

# Nothing is zoomed, so both fields of the notification carry the same layout.
# The v2 one is compared against the dump rather than a literal so that it is
# the two formats being checked and not the geometry again.
v2now=$(raw_layout L:two)
v1now=$(v1 "80x24,0,0[80x8,0,0,${q0#%},80x15,0,9,${q1#%}]")
wait_for "$NEWOUT" "%layout-change $wid $v2now $v2now " ||
	fail 'No v2 %layout-change for the client with new-layouts'
wait_for "$OLDOUT" "%layout-change $wid $v1now $v1now " ||
	fail 'No v1 %layout-change for the client without new-layouts'

# How many notifications one layout change produces, which differs by format
# on purpose. cmd_select_layout_exec() fires window-layout-changed for any
# layout it applies, and layout_parse() fires it again for a v1 one, so v1
# arrives twice - which is what master does for every layout, and what control
# clients written against it expect. v2 is new and has no such clients, so it
# gets the single notification. Counting the delta rather than the total, with
# a settle in between, keeps this independent of what has already been sent.
n1=$(grep -c "%layout-change $wid " "$OLDOUT")
check_ok select-layout -t L:two \
	'{"V":2,"L":{"t":"v","w":80,"h":24,"x":0,"y":0,"c":[{"t":"p","w":80,"h":9,"x":0,"y":0},{"t":"p","w":80,"h":14,"x":0,"y":10}]}}'
sleep 2
n2=$(grep -c "%layout-change $wid " "$OLDOUT")
must_equal 'Notifications for a v2 layout' "$((n2 - n1))" '1'

check_ok select-layout -t L:two "$(v1 "$v1vsplit")"
sleep 2
n3=$(grep -c "%layout-change $wid " "$OLDOUT")
must_equal 'Notifications for a v1 layout' "$((n3 - n2))" '2'

# And the client that did not ask for new layouts must never have been sent
# one, in that notification or any other.
grep -F '{"V":2,' "$OLDOUT" >/dev/null 2>&1 &&
	fail 'Control client without new-layouts was sent a v2 layout'

if [ "$($TMUX display-message -p alive 2>&1)" != "alive" ]; then
	echo "Server died." >&2
	exit 1
fi

$TMUX kill-server 2>/dev/null
exit 0
