#!/bin/sh

# Tests of the screen contents saved by save-state -w and restored by
# load-state -w.
#
# This covers:
# - capture-pane -epJ of the whole history being the same after a load for
#   coloured, bold and RGB text, an underline colour, wrapped lines, a wide
#   character, a combining character (also in the last column, where a
#   character split into two cells would push the rest off the line), a tab
#   and history of more than one screen, and the cursor and history size
#   being the same;
# - a pane in the alternate screen, both screens being the same after a load
#   and after leaving the alternate screen;
# - save, load and save again giving the same JSON apart from the pane ids;
# - history beyond the history-limit of the target session keeping the newest
#   lines;
# - a pane loaded at another width being reflowed;
# - a pane made narrower in the alternate screen, which leaves the cursor past
#   the right edge, and a cursor past the edge of a normal screen being moved
#   in;
# - a character whose width has changed since it was saved, and a line with
#   more text than fits, neither corrupting the grid;
# - cells that are not where the widths before them put them, cells written
#   apart that would combine if written together, and Hangul jamo with a
#   width, all keeping their columns;
# - malformed screens, lines and runs being rejected and creating nothing.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"; $TMUX kill-server 2>/dev/null' 0 1 15

fail()
{
	echo "$1"
	exit 1
}

# Print a format for a target.
show()
{
	$TMUX display -p -t "$1" "$2"
}

# Print a state file without the pane ids.
strip()
{
	sed 's/"I":"%[0-9]*"//g' "$1"
}

# Print the whole of a pane with its styles.
capture()
{
	$TMUX capture-pane $2 -epJ -S - -E - -t "$1"
}

# Print the cursor, history and alternate screen state of a pane.
state()
{
	show "$1" '#{cursor_x},#{cursor_y} #{history_size} #{alternate_on} #{alternate_saved_x},#{alternate_saved_y}'
}

# Write to an empty pane; display-message -I returns once it is written.
put()
{
	printf "$2" | $TMUX display-message -I -t "$1" || fail "display -I failed"
}

# Load a file that should be rejected and check nothing was created.
check_fail()
{
	before=$($TMUX list-panes -a -F '#{pane_id}')
	printf '%s' "$1" >$TMP/bad.json
	if out=$($TMUX load-state -w -t S: $TMP/bad.json 2>&1); then
		fail "invalid file was accepted: $1"
	fi
	case "$out" in
	*"$2"*) ;;
	*) fail "unexpected message for $1: $out" ;;
	esac
	[ "$($TMUX list-panes -a -F '#{pane_id}')" = "$before" ] ||
	    fail "invalid file created panes: $1"
}

$TMUX new-session -d -s S -x 40 -y 10 || fail "new-session failed"
$TMUX set -g history-limit 1000 || fail "set history-limit failed"

# A pane with styles, wide and combining characters, a tab and history.
$TMUX new-window -d -t S:1 '' || fail "new-window failed"
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
	put S:1 "line $i \033[1;31mred\033[0m \033[38;2;10;200;30mrgb\033[m\r\n"
done
put S:1 '\033[4;58;5;3mul\033[m wide:\346\227\245 comb:e\314\201 tab:\tX\r\n'
put S:1 "$(printf '%038d' 0)e\314\201Z\r\n"
put S:1 "$(printf '%050d' 0)\r\nend"
[ "$(show S:1 '#{history_size}')" -gt 0 ] || fail "no history"

$TMUX save-state -w -t S:1 $TMP/a.json || fail "save-state failed"
$TMUX load-state -w -d -t S:2 $TMP/a.json || fail "load-state failed"
[ "$(capture S:2)" = "$(capture S:1)" ] || fail "contents differ"
[ "$(state S:2)" = "$(state S:1)" ] ||
    fail "state is $(state S:2), not $(state S:1)"
$TMUX save-state -w -t S:2 $TMP/b.json || fail "save-state of copy failed"
[ "$(strip $TMP/b.json)" = "$(strip $TMP/a.json)" ] ||
    fail "saving the copy gives different JSON"
grep -qF '{"t":"\t","s":2}' $TMP/a.json || fail "tab not saved with its span"
grep -qF '{"w":true,' $TMP/a.json || fail "wrapped line not saved"

# A pane in the alternate screen.
$TMUX new-window -d -t S:3 '' || fail "new-window failed"
for i in 1 2 3 4 5 6 7 8 9 10 11 12; do
	put S:3 "normal $i\r\n"
done
put S:3 'abc\033[?1049h\033[2;3H\033[7malt\033[m screen\033[5;1Hmore'
[ "$(show S:3 '#{alternate_on}')" = 1 ] || fail "not in alternate screen"
$TMUX save-state -w -t S:3 $TMP/c.json || fail "save-state failed"
$TMUX load-state -w -d -t S:4 $TMP/c.json || fail "load-state failed"
[ "$(state S:4)" = "$(state S:3)" ] ||
    fail "alternate state is $(state S:4), not $(state S:3)"
[ "$(capture S:4)" = "$(capture S:3)" ] || fail "alternate screen differs"
[ "$(capture S:4 -a)" = "$(capture S:3 -a)" ] || fail "normal screen differs"
put S:3 '\033[?1049l'
put S:4 '\033[?1049l'
[ "$(state S:4)" = "$(state S:3)" ] ||
    fail "after leaving, state is $(state S:4), not $(state S:3)"
[ "$(capture S:4)" = "$(capture S:3)" ] ||
    fail "after leaving, contents differ"

# The alternate screen is not reflowed, so narrowing a pane in it can leave the
# cursor past the right edge.
$TMUX new-window -d -t S:8 '' || fail "new-window failed"
put S:8 '\033[?1049h\033[1;36Hx'
$TMUX split-window -h -d -t S:8 -l 25 '' || fail "split-window failed"
[ "$(show S:8.0 '#{cursor_x}')" -gt "$(show S:8.0 '#{pane_width}')" ] ||
    fail "cursor is not past the edge"
$TMUX save-state -w -t S:8 $TMP/e.json || fail "save-state failed"
$TMUX load-state -w -d -t S:9 $TMP/e.json || fail "narrowed alternate failed"
[ "$(state S:9.0)" = "$(state S:8.0)" ] ||
    fail "narrowed state is $(state S:9.0), not $(state S:8.0)"
[ "$(capture S:9.0)" = "$(capture S:8.0)" ] || fail "narrowed contents differ"
$TMUX kill-window -t S:8
$TMUX kill-window -t S:9

# History beyond the target's history-limit keeps the newest lines.
$TMUX new-session -d -s H -x 40 -y 10 || fail "new-session H failed"
$TMUX set -t H history-limit 5 || fail "set history-limit failed"
$TMUX load-state -w -d -t H: $TMP/a.json || fail "load-state into H failed"
[ "$(show H:1 '#{history_size}')" = 5 ] ||
    fail "history is $(show H:1 '#{history_size}') lines, not 5"
[ "$(capture H:1)" = \
    "$($TMUX capture-pane -epJ -S -5 -E - -t S:1)" ] ||
    fail "history does not keep the newest lines"
$TMUX kill-session -t H

# Another width is reflowed.
$TMUX new-session -d -s N -x 25 -y 10 || fail "new-session N failed"
printf 'refresh-client -C 25,10\nload-state -w -t N: %s\n' $TMP/a.json |
    $TMUX -C attach -t N >/dev/null 2>&1
[ "$(show N:1 '#{window_width}')" = 25 ] || fail "window not resized"
[ "$($TMUX capture-pane -pJ -S - -E - -t N:1 | grep -c .)" = \
    "$($TMUX capture-pane -pJ -S - -E - -t S:1 | grep -c .)" ] ||
    fail "reflowed lines differ"
$TMUX capture-pane -pJ -S - -E - -t N:1 | grep -qx "$(printf '%050d' 0)" ||
    fail "wrapped line not joined after reflow"
$TMUX kill-session -t N

# A character whose width changed keeps its cell and moves the rest along.
$TMUX new-window -d -t S:5 '' || fail "new-window failed"
put S:5 'a\346\227\245b'
$TMUX save-state -w -t S:5 $TMP/w.json || fail "save-state failed"
$TMUX set -as codepoint-widths 'U+65E5=1' || fail "set codepoint-widths failed"
$TMUX load-state -w -d -t S:6 $TMP/w.json || fail "load with new width failed"
$TMUX set -su codepoint-widths
[ "$($TMUX capture-pane -p -t S:6 | head -1)" = "$(printf 'a\346\227\245b')" ] ||
    fail "changed width lost the text"
[ "$(show S:6 '#{cursor_x}')" = "$(show S:5 '#{cursor_x}')" ] ||
    fail "cursor moved"

# Cells that are not where the widths before them put them, as erasing half of
# a wide character or a tab leaves, and cells written apart that would combine
# if written together, keep their columns.
$TMUX new-window -d -t S:10 '' || fail "new-window failed"
put S:10 '\033[1;1H\344\270\255x|\033[1;2H\033[P'
put S:10 '\033[2;1Hab\344\270\255x|\033[2;3H\033[P'
put S:10 '\033[3;1Ha\tb|\033[3;4H\033[P'
put S:10 '\033[4;3HX|\033[4;1H\360\237\221\250\342\200\215'
put S:10 '\033[5;3H\360\237\217\275|\033[5;1H\360\237\221\215'
$TMUX save-state -w -t S:10 $TMP/p.json || fail "save-state failed"
$TMUX load-state -w -d -t S:11 $TMP/p.json || fail "load-state failed"
for p in S:10 S:11; do
	put $p '\033[1;8H#\033[2;8H#\033[3;15H#\033[4;6H#\033[5;6H#'
done
[ "$($TMUX capture-pane -pN -t S:11)" = "$($TMUX capture-pane -pN -t S:10)" ] ||
    fail "cells moved: $($TMUX capture-pane -pN -t S:11 | head -5)"
$TMUX kill-window -t S:10
$TMUX kill-window -t S:11

# Hangul jamo that have a width are still composed as when they were written.
$TMUX set -s codepoint-widths 'U+1161=1,U+11AB=1' ||
    fail "set codepoint-widths failed"
$TMUX new-window -d -t S:10 '' || fail "new-window failed"
put S:10 'a\341\204\222\341\205\241\341\206\253b|'
$TMUX save-state -w -t S:10 $TMP/j.json || fail "save-state failed"
$TMUX load-state -w -d -t S:11 $TMP/j.json || fail "load-state failed"
for p in S:10 S:11; do
	put $p '\033[1;9H#'
done
[ "$($TMUX capture-pane -pN -t S:11)" = "$($TMUX capture-pane -pN -t S:10)" ] ||
    fail "jamo split: $($TMUX capture-pane -p -t S:11 | head -1)"
$TMUX set -su codepoint-widths
$TMUX kill-window -t S:10
$TMUX kill-window -t S:11

# A line with more than fits is cut at the edge.
L1='{"V":2,"L":{"t":"p","w":10,"h":3,"x":0,"y":0,"a":true,"i":0}}'
W='{"version":1,"window":{"layout":'"$L1"',"panes":[{"screen":'
printf '%s{"sx":10,"sy":3,"cursor":{"x":50,"y":1},"lines":[{"c":[{"t":"0123456789abc"},{"t":"x"}]},{"c":[{"t":"012345678\346\227\245"}]}]}}]}}' "$W" >$TMP/long.json
$TMUX load-state -w -d -t S:7 $TMP/long.json || fail "long lines failed"
[ "$($TMUX capture-pane -p -t S:7 | head -2)" = "$(printf '0123456789\n012345678')" ] ||
    fail "long lines not cut: $($TMUX capture-pane -p -t S:7 | head -2)"
[ "$(show S:7 '#{cursor_x},#{cursor_y}')" = 10,1 ] ||
    fail "cursor past the edge not moved in: $(show S:7 '#{cursor_x},#{cursor_y}')"

# Malformed screens, lines and runs.
S='"sx":10,"sy":3'
check_fail "$W"'5}]}}' 'screen is not an object'
check_fail "$W"'{"sy":3}}]}}' '"sx" not found'
check_fail "$W"'{"sx":0,"sy":3}}]}}' '"sx" is out of range'
check_fail "$W"'{"sx":10,"sy":10001}}]}}' '"sy" is out of range'
check_fail "$W"'{'"$S"',"cursor":{"x":10001,"y":0}}}]}}' '"x" is out of range'
check_fail "$W"'{'"$S"',"cursor":{"x":0,"y":3}}}]}}' '"y" is out of range'
check_fail "$W"'{'"$S"',"lines":[{},{},{},{}]}}]}}' \
    'screen has 4 lines but height 3'
check_fail "$W"'{'"$S"',"lines":{}}}]}}' '"lines" expected an array'
check_fail "$W"'{'"$S"',"history":[{"c":{}}]}}]}}' '"c" expected an array'
check_fail "$W"'{'"$S"',"lines":[{"w":1}]}}]}}' '"w" expected a boolean'
check_fail "$W"'{'"$S"',"lines":[{"c":[{}]}]}}]}}' 'run has no text'
check_fail "$W"'{'"$S"',"lines":[{"c":[{"t":"a","f":"nope"}]}]}}]}}' \
    'invalid colour "nope"'
check_fail "$W"'{'"$S"',"lines":[{"c":[{"t":"a","a":"loud"}]}]}}]}}' \
    'invalid attributes "loud"'
check_fail "$W"'{'"$S"',"lines":[{"c":[{"t":"a\u001bb"}]}]}}]}}' \
    'run has invalid text'
check_fail "$W"'{'"$S"',"lines":[{"c":[{"t":"a\tb"}]}]}}]}}' \
    'run has invalid text'
check_fail "$W"'{'"$S"',"lines":[{"c":[{"t":"a'"$(printf '\377')"'"}]}]}}]}}' \
    'run has invalid text'
check_fail "$W"'{'"$S"',"lines":[{"c":[{"t":"a","x":10}]}]}}]}}' \
    '"x" is out of range'
check_fail "$W"'{'"$S"',"lines":[{"c":[{"t":"a","s":2}]}]}}]}}' \
    'run with "s" is not a tab'
check_fail "$W"'{'"$S"',"lines":[{"c":[{"t":"\t","s":33}]}]}}]}}' \
    '"s" is out of range'
check_fail "$W"'{'"$S"',"alternate":{"sy":3}}}]}}' '"sx" not found'

exit 0
