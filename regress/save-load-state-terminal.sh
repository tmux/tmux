#!/bin/sh

# Tests of the terminal state of a pane's screen saved by save-state -w and
# restored by load-state -w, beyond its contents.
#
# Each item is set with the escape sequence an application would use, then the
# window is saved and loaded and the copy compared with the original:
# - modes (DECCKM, mouse tracking and SGR mouse), with formats;
# - the scroll region (DECSTBM), with scroll_region_upper and lower;
# - tab stops (HTS after TBC), with pane_tabs;
# - the cursor style and colour (DECSCUSR, OSC 12), with cursor_shape,
#   cursor_blinking and cursor_colour;
# - the cursor and cell saved by DECSC, by sending DECRC to both and comparing
#   the cursor and what is written next, also after the pane has shrunk below
#   the saved position;
# - the title stack (OSC 2 and XTPUSHTITLE), by popping it on both;
# - the path (OSC 7), with pane_path;
# - OSC 133 marks, by moving to the previous prompt in copy mode;
# - the palette (OSC 4, 10 and 11), with pane_fg and pane_bg and the saved
#   file;
# - hyperlinks (OSC 8), with capture-pane -e;
# - the cell saved by the alternate screen, by leaving it on both, and the
#   cursor and cell it keeps after it has ended, by leaving it again;
# - save, load and save again giving the same JSON apart from the pane ids;
# - the scroll region and tab stops being reset rather than restored when the
#   size changes, as a resize does, both after loading into a smaller client
#   and for a screen saved at another size from its pane;
# - invalid items being rejected and creating nothing.

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

# Write to an empty pane; display-message -I returns once it is written.
put()
{
	printf "$2" | $TMUX display-message -I -t "$1" || fail "display -I failed"
}

# Check a format is the same in two panes.
same()
{
	[ "$(show "$2" "$3")" = "$(show "$1" "$3")" ] ||
	    fail "$3 is $(show "$2" "$3"), not $(show "$1" "$3")"
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
$TMUX new-window -d -t S:1 '' || fail "new-window failed"

put S:1 '\033]2;first\033\\\033[22;0t\033]2;second\033\\\033[22;0t'
put S:1 '\033]2;current\033\\\033]7;file://host/tmp/dir\033\\'
put S:1 '\033[3g\033[1;5H\033H\033[1;12H\033H\033[1;30H\033H'
put S:1 '\033[5 q\033]12;red\033\\'
put S:1 '\033[1;31m\033[3;4H\0337\033[m'
put S:1 '\033[?1h\033[?1000h\033[?1006h'
put S:1 '\033]4;1;#112233\033\\\033]10;#445566\033\\\033]11;#778899\033\\'
put S:1 '\033[H\033]133;A\033\\p1$ \033]133;B\033\\one\r\n'
put S:1 '\033]133;C\033\\out\r\n\033]133;D;3\033\\'
put S:1 '\033]133;A\033\\p2$ \033]133;B\033\\two\r\n'
put S:1 '\033]8;id=x1;http://example.com/a\033\\link\033]8;;\033\\ plain\r\n'
put S:1 '\033[2;8r'

$TMUX save-state -w -t S:1 $TMP/a.json || fail "save-state failed"
$TMUX load-state -w -d -t S:2 $TMP/a.json || fail "load-state failed"
for f in keypad_cursor_flag mouse_standard_flag mouse_sgr_flag cursor_flag \
    scroll_region_upper scroll_region_lower pane_tabs cursor_shape \
    cursor_blinking cursor_colour pane_path pane_fg pane_bg pane_title \
    cursor_x cursor_y; do
	same S:1 S:2 "#{$f}"
done
[ "$(show S:2 '#{pane_tabs}')" = 4,11,29 ] || fail "tab stops not as set"
[ "$(show S:2 '#{scroll_region_upper},#{scroll_region_lower}')" = 1,7 ] ||
    fail "scroll region not as set"
[ "$(show S:2 '#{cursor_shape} #{cursor_blinking}')" = "bar 1" ] ||
    fail "cursor style not as set"
grep -qF '"colours":[{"index":1,"colour":"#112233"}]' $TMP/a.json ||
    fail "palette entry not saved"
[ "$($TMUX capture-pane -ep -t S:2)" = "$($TMUX capture-pane -ep -t S:1)" ] ||
    fail "contents with hyperlinks differ"
$TMUX capture-pane -ep -t S:2 | grep -qF 'http://example.com/a' ||
    fail "hyperlink lost"
$TMUX save-state -w -t S:2 $TMP/b.json || fail "save-state of copy failed"
[ "$(strip $TMP/b.json)" = "$(strip $TMP/a.json)" ] ||
    fail "saving the copy gives different JSON"

# OSC 133 marks.
grep -qF '"m":{"p":0,"c":4}' $TMP/a.json || fail "prompt marks not saved"
for p in S:1 S:2; do
	$TMUX copy-mode -t $p || fail "copy-mode failed"
	$TMUX send -t $p -X bottom-line || fail "bottom-line failed"
	$TMUX send -t $p -X previous-prompt || fail "previous-prompt failed"
done
[ "$(show S:2 '#{copy_cursor_y}')" = 2 ] || fail "second prompt mark lost"
same S:1 S:2 '#{copy_cursor_x},#{copy_cursor_y}'
for p in S:1 S:2; do
	$TMUX send -t $p -X previous-prompt || fail "previous-prompt failed"
done
[ "$(show S:2 '#{copy_cursor_y}')" = 0 ] || fail "first prompt mark lost"
same S:1 S:2 '#{copy_cursor_x},#{copy_cursor_y}'
$TMUX send -t S:1 -X cancel
$TMUX send -t S:2 -X cancel

# DECSC and the title stack.
for p in S:1 S:2; do
	put $p '\033[r\0338X\033[23;0t'
done
same S:1 S:2 '#{cursor_x},#{cursor_y} #{pane_title}'
[ "$(show S:2 '#{cursor_x},#{cursor_y} #{pane_title}')" = "4,2 second" ] ||
    fail "DECRC or title pop: $(show S:2 '#{cursor_x},#{cursor_y} #{pane_title}')"
[ "$($TMUX capture-pane -ep -t S:2)" = "$($TMUX capture-pane -ep -t S:1)" ] ||
    fail "cell saved by DECSC differs"

# The cell saved by the alternate screen.
$TMUX new-window -d -t S:3 '' || fail "new-window failed"
put S:3 'abc\033[4;32m\033[?1049h\033[mxyz'
$TMUX save-state -w -t S:3 $TMP/c.json || fail "save-state failed"
$TMUX load-state -w -d -t S:4 $TMP/c.json || fail "load-state failed"
for p in S:3 S:4; do
	put $p '\033[?1049lZ'
done
[ "$($TMUX capture-pane -ep -t S:4)" = "$($TMUX capture-pane -ep -t S:3)" ] ||
    fail "cell saved by the alternate screen differs"

# The cursor and cell kept from the alternate screen after it has ended, which
# leaving it again restores.
$TMUX new-window -d -t S:8 '' || fail "new-window failed"
put S:8 '\033[3;7H\033[1;35m\033[?1049h\033[m\033[?1049l\033[m\033[8;20H'
$TMUX save-state -w -t S:8 $TMP/k.json || fail "save-state failed"
$TMUX load-state -w -d -t S:9 $TMP/k.json || fail "load-state failed"
for p in S:8 S:9; do
	put $p '\033[?1049lQ'
done
[ "$(show S:9 '#{cursor_x},#{cursor_y}')" = 7,2 ] ||
    fail "cursor kept from the alternate screen lost: $(show S:9 '#{cursor_x},#{cursor_y}')"
[ "$($TMUX capture-pane -ep -t S:9)" = "$($TMUX capture-pane -ep -t S:8)" ] ||
    fail "cell kept from the alternate screen differs"
$TMUX kill-window -t S:8
$TMUX kill-window -t S:9

# Another size resets the scroll region and tab stops.
$TMUX new-session -d -s N -x 30 -y 8 || fail "new-session N failed"
printf 'refresh-client -C 30,8\nload-state -w -t N: %s\n' $TMP/a.json |
    $TMUX -C attach -t N >/dev/null 2>&1
[ "$(show N:1 '#{window_width}x#{window_height}')" = 30x8 ] ||
    fail "window not resized"
[ "$(show N:1 '#{scroll_region_upper},#{scroll_region_lower}')" = 0,7 ] ||
    fail "scroll region kept at another height"
[ "$(show N:1 '#{pane_tabs}')" = 8,16,24 ] ||
    fail "tab stops kept at another width: $(show N:1 '#{pane_tabs}')"
$TMUX kill-session -t N

# DECSC keeps what it saved when the pane shrinks, and DECRC moves the cursor
# inside the screen.
$TMUX new-window -d -t S:6 '' || fail "new-window failed"
put S:6 '\033[9;30H\0337'
$TMUX split-window -v -d -t S:6 -l 6 '' || fail "split-window failed"
$TMUX save-state -w -t S:6 $TMP/d.json || fail "save-state failed"
grep -qF '"saved":{"x":29,"y":8}' $TMP/d.json || fail "DECSC not saved as it was"
$TMUX load-state -w -d -t S:7 $TMP/d.json || fail "load after shrink failed"
for p in S:6.0 S:7.0; do
	put $p '\0338'
done
same S:6.0 S:7.0 '#{cursor_x},#{cursor_y}'
$TMUX kill-window -t S:6
$TMUX kill-window -t S:7

# A screen of another size from its pane gets the scroll region and tab stops
# a resize would leave rather than ones that do not fit.
L1='{"V":2,"L":{"t":"p","w":10,"h":3,"x":0,"y":0,"a":true,"i":0}}'
W='{"version":1,"window":{"layout":'"$L1"',"panes":[{'
printf '%s"screen":{"sx":12,"sy":5,"region":{"upper":1,"lower":4},"tabs":[{"x":3},{"x":11}]}}]}}' \
    "$W" >$TMP/size.json
$TMUX load-state -w -d -t S:5 $TMP/size.json || fail "load-state failed"
[ "$(show S:5 '#{scroll_region_upper},#{scroll_region_lower}')" = 0,2 ] ||
    fail "scroll region does not fit: $(show S:5 '#{scroll_region_upper},#{scroll_region_lower}')"
[ "$(show S:5 '#{pane_tabs}')" = 8 ] ||
    fail "tab stops do not fit: $(show S:5 '#{pane_tabs}')"

# Invalid items.
S='"screen":{"sx":10,"sy":3'
check_fail "$W$S"',"modes":[{"name":"nosuch"}]}}]}}' 'unknown mode "nosuch"'
check_fail "$W$S"',"modes":[{"name":"sync"}]}}]}}' 'unknown mode "sync"'
check_fail "$W$S"',"cursor-style":"round"}}]}}' \
    'unknown cursor style "round"'
check_fail "$W$S"',"cursor-colour":"nope"}}]}}' 'invalid colour "nope"'
check_fail "$W$S"',"region":{"upper":2,"lower":1}}}]}}' \
    'invalid scroll region'
check_fail "$W$S"',"region":{"upper":0,"lower":3}}}]}}' \
    '"lower" is out of range'
check_fail "$W$S"',"tabs":[{"x":10}]}}]}}' '"x" is out of range'
check_fail "$W$S"',"titles":[{"title":"a\u0007"}]}}]}}' \
    'invalid title in stack'
check_fail "$W$S"',"path":"a\u001b"}}]}}' 'invalid path'
check_fail "$W$S"',"saved":{"x":0,"y":10001}}}]}}' '"y" is out of range'
check_fail "$W$S"',"alternate-cursor":{"x":10001,"y":0}}}]}}' \
    '"x" is out of range'
check_fail "$W$S"',"saved":{"x":0,"y":0,"origin":1}}}]}}' \
    '"origin" expected a boolean'
check_fail "$W$S"',"links":[{"id":0,"uri":"x"}]}}]}}' '"id" is out of range'
check_fail "$W$S"',"lines":[{"m":{"p":65536}}]}}]}}' '"p" is out of range'
check_fail "$W$S"',"lines":[{"m":{"x":256}}]}}]}}' '"x" is out of range'
check_fail "$W"'"palette":{"fg":"nope"}}]}}' 'invalid colour "nope"'
check_fail "$W"'"palette":{"colours":[{"index":256,"colour":"red"}]}}]}}' \
    '"index" is out of range'

exit 0
