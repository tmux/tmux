#!/bin/sh

# Tests of save-state -w and load-state -w, which save a window as JSON and
# create a copy of it with nothing running in its panes.
#
# This covers:
# - a window with a horizontal and a vertical split, a floating pane and an
#   active pane that is not the first, keeping its layout (apart from the pane
#   ids), name, pane titles, working directories and commands;
# - the working directory being the one the pane's process is in, not the one
#   it was started in;
# - names, titles, directories and arguments with quotes, backslashes, tabs,
#   non-ASCII text and format syntax, which is not expanded;
# - a zoomed window, which keeps its full layout and is zoomed again, also
#   when its active pane is a floating pane kept above the zoomed one;
# - save, load and save again giving the same JSON apart from the pane ids;
# - -d, -t with an index and a session, and an index already in use;
# - loading into a session with a different size;
# - respawn-pane running the saved command in the saved directory;
# - a hand-written file with unknown keys and without the optional ones;
# - a window with a manual size keeping it;
# - invalid files failing with a message and creating nothing, without even a
#   window-linked hook firing.

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

# Print a window's layout without the pane ids, which a copy does not keep.
layout()
{
	show "$1" '#{window_layout}' | sed 's/"I":"%[0-9]*"//g'
}

# Print a state file without the pane ids.
strip()
{
	sed 's/"I":"%[0-9]*"//g' "$1"
}

# Print each pane's title and command, and whether it is active.
panes()
{
	$TMUX list-panes -t "$1" -F \
	    '#{pane_index} #{pane_active} #{pane_title}|#{pane_start_command}'
}

# Wait for a format to have a value.
wait_for()
{
	n=0
	while [ "$(show "$1" "$2")" != "$3" ]; do
		n=$((n + 1))
		[ $n -lt 50 ] || fail "$1 $2 is $(show "$1" "$2"), not $3"
		sleep 0.1
	done
}

# Wait for a pane to show a line matching a pattern.
wait_text()
{
	n=0
	while ! $TMUX capture-pane -p -S - -t "$1" | grep -qx -- "$2"; do
		n=$((n + 1))
		[ $n -lt 50 ] || fail "$1 does not show $2"
		sleep 0.1
	done
}

# Load a file that should be rejected and check nothing was created.
check_fail()
{
	before=$($TMUX list-panes -a -F '#{pane_id}')
	printf '%s' "$1" >$TMP/bad.json
	if out=$($TMUX load-state -w -t S: $TMP/bad.json 2>&1); then
		fail "invalid file was accepted: $1"
	fi
	[ -n "$out" ] || fail "no message for invalid file: $1"
	case "$out" in
	*"$2"*) ;;
	*) fail "unexpected message for $1: $out" ;;
	esac
	[ "$($TMUX list-panes -a -F '#{pane_id}')" = "$before" ] ||
	    fail "invalid file created panes: $1"
}

TAB=$(printf '\t')
NAME=$(printf 'a \\b "c" \303\251')
TITLE=$(printf 't \\1 "2" \346\227\245')
D1="$TMP/$(printf 'q"\\ \303\251')"
D2="$TMP/$(printf 'tab\there')"
D3="$TMP/#{pane_id}"
mkdir "$D1" "$D2" "$D3" "$TMP/after" || fail "mkdir failed"

$TMUX new-session -d -s S -x 80 -y 24 -c "$D1" || fail "new-session failed"
$TMUX set -g remain-on-exit on || fail "set remain-on-exit failed"
$TMUX rename-window -t S:0 "$NAME" || fail "rename-window failed"
$TMUX split-window -h -t S:0 \
    sh -c 'cd "$1" && exec sleep 1000' sh "$TMP/after" ||
    fail "split-window -h failed"
$TMUX split-window -v -t S:0 -c "$TMP/##{pane_id}" \
    printf "[%s]\n" "x${TAB}y" '' 'z' ||
    fail "split-window -v failed"
$TMUX new-pane -d -t S:0.0 -x 20 -y 6 -X 5 -Y 3 -c "$D2" ||
    fail "new-pane failed"
$TMUX select-pane -t S:0.1 -T "$TITLE" || fail "select-pane -T failed"
$TMUX select-pane -t S:0.0 || fail "select-pane failed"
$TMUX select-pane -t S:0.1 || fail "select-pane failed"
wait_for S:0.1 '#{pane_current_path}' "$TMP/after"

# A window with splits, a floating pane and the second pane active.
$TMUX save-state -w -t S:0 $TMP/a.json || fail "save-state failed"
$TMUX load-state -w -d -t S: $TMP/a.json || fail "load-state failed"
[ "$(show S: '#{window_index}')" = 0 ] || fail "-d selected the new window"
[ "$(layout S:1)" = "$(layout S:0)" ] || fail "layout differs"
[ "$(show S:1 '#{window_name}')" = "$(show S:0 '#{window_name}')" ] ||
    fail "name differs: $(show S:1 '#{window_name}')"
[ "$(show S:1 '#{automatic-rename}')" = 0 ] ||
    fail "automatic-rename is on for a named window"
[ "$(panes S:1)" = "$(panes S:0)" ] || fail "panes differ: $(panes S:1)"
[ "$(show S:1.1 '#{pane_title}')" = "$(show S:0.1 '#{pane_title}')" ] ||
    fail "title differs"
for i in 0 1 2 3; do
	[ "$(show S:1.$i '#{pane_pid}')" = "" ] ||
	    fail "pane $i of the copy is running something"
done
[ "$(show S:1.0 '#{pane_start_path}')" = "$D1" ] || fail "path 0 differs"
[ "$(show S:1.1 '#{pane_start_path}')" = "$TMP/after" ] ||
    fail "path 1 is not the directory the process changed to"
[ "$(show S:1.2 '#{pane_start_path}')" = "$D3" ] ||
    fail "path 2 differs: $(show S:1.2 '#{pane_start_path}')"
[ "$(show S:1.3 '#{pane_start_path}')" = "$D2" ] || fail "path 3 differs"
$TMUX save-state -w -t S:1 $TMP/b.json || fail "save-state of copy failed"
[ "$(strip $TMP/b.json)" = "$(strip $TMP/a.json)" ] ||
    fail "saving the copy gives different JSON"

# The file holds names as they were given and directories as they are.
grep -qF '"name":"a \\b \"c\" ' $TMP/a.json || fail "name not as given"
grep -qF '"cwd":"'"$TMP"'/tab\there"' $TMP/a.json || fail "tab not escaped"

# respawn-pane runs the saved command in the saved directory.
$TMUX respawn-pane -t S:1.2 || fail "respawn-pane failed"
wait_text S:1.2 "\\[x${TAB}y\\]"
wait_text S:1.2 '\[\]'
wait_text S:1.2 '\[z\]'
$TMUX respawn-pane -t S:1.1 || fail "respawn-pane failed"
wait_for S:1.1 '#{pane_current_path}' "$TMP/after"
$TMUX respawn-pane -t S:1.0 'pwd; exec sleep 1000' ||
    fail "respawn-pane with a command failed"
wait_text S:1.0 '.*/q".*'
$TMUX kill-window -t S:1

# A zoomed window.
$TMUX resize-pane -Z -t S:0.1 || fail "resize-pane -Z failed"
$TMUX save-state -w -t S:0 $TMP/z.json || fail "save-state of zoom failed"
$TMUX load-state -w -t S:5 $TMP/z.json || fail "load-state of zoom failed"
[ "$(show S: '#{window_index}')" = 5 ] || fail "new window not selected"
[ "$(show S:5 '#{window_zoomed_flag}')" = 1 ] || fail "copy is not zoomed"
[ "$(show S:5 '#{pane_index}')" = 1 ] || fail "wrong pane zoomed"
[ "$(layout S:5)" = "$(layout S:0)" ] || fail "zoomed layout differs"
$TMUX save-state -w -t S:5 $TMP/z2.json || fail "save-state of zoom failed"
[ "$(strip $TMP/z2.json)" = "$(strip $TMP/z.json)" ] ||
    fail "saving the zoomed copy gives different JSON"
out=$($TMUX load-state -w -t S:5 $TMP/z.json 2>&1) &&
    fail "load-state into an index in use worked"
[ "$out" = "$TMP/z.json: create window failed: index 5 in use" ] ||
    fail "unexpected message: $out"
$TMUX resize-pane -Z -t S:0.1 || fail "resize-pane -Z failed"
$TMUX kill-window -t S:5
$TMUX select-window -t S:0

# A zoomed window whose active pane is a floating pane kept above the zoom.
$TMUX new-window -d -t S:6 || fail "new-window failed"
$TMUX split-window -h -t S:6 || fail "split-window failed"
$TMUX select-pane -t S:6.0 || fail "select-pane failed"
$TMUX resize-pane -Z -t S:6.0 || fail "resize-pane -Z failed"
$TMUX new-pane -A -t S:6.0 -x 20 -y 5 || fail "new-pane -A failed"
F='#{pane_index} #{pane_active} #{window_zoomed_flag} #{pane_floating_flag}'
F="$F #{pane_width}x#{pane_height}"
$TMUX save-state -w -t S:6 $TMP/f.json || fail "save-state failed"
$TMUX load-state -w -d -t S:7 $TMP/f.json || fail "load-state failed"
[ "$($TMUX list-panes -t S:7 -F "$F")" = "$($TMUX list-panes -t S:6 -F "$F")" ] ||
    fail "float over zoom differs: $($TMUX list-panes -t S:7 -F "$F")"
$TMUX kill-window -t S:6
$TMUX kill-window -t S:7

# Through a pipe, as from stdout to stdin.
$TMUX save-state -w -t S:0 - | $TMUX load-state -w -d -t S:3 - ||
    fail "save-state | load-state failed"
[ "$(layout S:3)" = "$(layout S:0)" ] || fail "layout through pipe differs"
$TMUX kill-window -t S:3

# Another size. An unattached session leaves the window at the saved size,
# as select-layout does; a client resizes it.
$TMUX new-session -d -s T -x 50 -y 15 || fail "new-session T failed"
$TMUX load-state -w -d -t T: $TMP/a.json || fail "load-state into T failed"
[ "$(show T:1 '#{window_width}x#{window_height}')" = 80x24 ] ||
    fail "unattached size is $(show T:1 '#{window_width}x#{window_height}')"
[ "$(layout T:1)" = "$(layout S:0)" ] || fail "layout in T differs"
printf 'refresh-client -C 50,15\nselect-window -t T:1\n' |
    $TMUX -C attach -t T >/dev/null 2>&1
[ "$(show T:1 '#{window_width}x#{window_height}')" = 50x15 ] ||
    fail "attached size is $(show T:1 '#{window_width}x#{window_height}')"
[ "$($TMUX list-panes -t T:1 | wc -l)" -eq 4 ] || fail "panes lost in T"
$TMUX kill-session -t T

# A hand-written file: unknown keys are ignored, missing ones are empty.
L1='{"V":2,"L":{"t":"p","w":80,"h":24,"x":0,"y":0,"a":true,"i":0}}'
cat >$TMP/h.json <<EOF
{"version":1,"later":[{}],"window":{"layout":$L1,"panes":[{"x":1}],"y":true}}
EOF
$TMUX load-state -w -d -t S:7 $TMP/h.json || fail "hand-written file failed"
[ "$(show S:7 '#{window_name}|#{pane_title}|#{pane_start_command}')" = "||" ] ||
    fail "missing keys not empty: $(show S:7 '#{window_name}|#{pane_title}')"
$TMUX kill-window -t S:7

# Names are given as they would be to rename-window.
cat >$TMP/n.json <<EOF
{"version":1,"window":{"name":"x\\\\y \\u00e9","layout":$L1,"panes":[{}]}}
EOF
$TMUX load-state -w -d -t S:7 $TMP/n.json || fail "name file failed"
$TMUX new-window -d -t S:8 || fail "new-window failed"
$TMUX rename-window -t S:8 "$(printf 'x\\y \303\251')"
[ "$(show S:7 '#{window_name}')" = "$(show S:8 '#{window_name}')" ] ||
    fail "name not as rename-window: $(show S:7 '#{window_name}')"
$TMUX kill-window -t S:7
$TMUX kill-window -t S:8

# A window with a manual size keeps it.
$TMUX set -g window-size manual || fail "set window-size failed"
$TMUX new-window -d -t S:6 '' || fail "new-window failed"
$TMUX split-window -d -h -t S:6 '' || fail "split-window failed"
$TMUX resize-window -t S:6 -x 50 -y 15 || fail "resize-window failed"
$TMUX save-state -w -t S:6 $TMP/m.json || fail "save-state failed"
$TMUX new-session -d -s M -x 100 -y 30 || fail "new-session M failed"
$TMUX load-state -w -d -t M: $TMP/m.json || fail "load-state into M failed"
[ "$(show M:1 '#{window_width}x#{window_height}')" = 50x15 ] ||
    fail "manual size lost: $(show M:1 '#{window_width}x#{window_height}')"
$TMUX kill-session -t M
$TMUX kill-window -t S:6
$TMUX set -gu window-size

# Invalid files, which create nothing, not even for a moment.
$TMUX set-hook -g window-linked 'set -g @linked 1' || fail "set-hook failed"
L2='{"V":2,"L":{"t":"h","w":80,"h":24,"x":0,"y":0,"c":[{"t":"p","w":40,"h":24,"x":0,"y":0,"i":0},{"t":"p","w":39,"h":24,"x":41,"y":0,"i":1}]}}'
L3='{"V":2,"L":{"t":"h","w":80,"h":24,"x":0,"y":0,"c":[{"t":"p","w":40,"h":23,"x":0,"y":0,"i":0},{"t":"p","w":39,"h":24,"x":41,"y":0,"i":1}]}}'
W='{"version":1,"window":'
check_fail '' 'empty input'
check_fail '{"version":1,' 'invalid JSON'
check_fail '[]' 'invalid JSON'
check_fail '{"version":2,"window":{}}' 'unsupported version 2'
check_fail '{"window":{}}' '"version" not found'
check_fail '{"version":1}' '"window" not found'
check_fail "$W"'{"panes":[{}]}}' '"layout" not found'
check_fail "$W"'{"layout":'"$L1"'}}' '"panes" not found'
check_fail "$W"'{"layout":'"$L1"',"panes":[]}}' 'window has no panes'
check_fail "$W"'{"layout":'"$L2"',"panes":[{}]}}' 'have 1 panes but need 2'
check_fail "$W"'{"layout":'"$L1"',"panes":[{},{}]}}' 'have 2 panes but need 1'
check_fail "$W"'{"name":5,"layout":'"$L1"',"panes":[{}]}}' \
    '"name" expected a string'
check_fail "$W"'{"name":"a\u0000b","layout":'"$L1"',"panes":[{}]}}' \
    '"name" has an invalid string'
check_fail "$W"'{"name":"a\ud800b","layout":'"$L1"',"panes":[{}]}}' \
    '"name" has an invalid string'
check_fail "$W"'{"name":"a\tb","layout":'"$L1"',"panes":[{}]}}' \
    'invalid window name'
check_fail "$W"'{"layout":'"$L1"',"panes":[{"title":"a\u001bb"}]}}' \
    'invalid pane title'
check_fail "$W"'{"layout":'"$L1"',"panes":[{"command":{}}]}}' \
    '"command" expected an array'
check_fail "$W"'{"layout":'"$L1"',"panes":[{"command":[{"arg":1}]}]}}' \
    '"arg" expected a string'
check_fail "$W"'{"zoomed":1,"layout":'"$L1"',"panes":[{}]}}' \
    '"zoomed" expected a boolean'
check_fail "$W"'{"layout":'"$L3"',"panes":[{},{}]}}' \
    'size mismatch after applying layout'
out=$($TMUX save-state -t S:0 $TMP/x.json 2>&1) && fail "save-state without -w"
[ "$out" = "-w must be given" ] || fail "unexpected message: $out"
out=$($TMUX load-state $TMP/a.json 2>&1) && fail "load-state without -w"
[ "$out" = "-w must be given" ] || fail "unexpected message: $out"
[ ! -e $TMP/x.json ] || fail "save-state without -w wrote a file"
[ "$($TMUX list-windows -t S -F '#{window_index}')" = 0 ] ||
    fail "windows left behind: $($TMUX list-windows -t S)"
[ "$($TMUX show -gv @linked 2>/dev/null)" = "" ] ||
    fail "an invalid file linked a window"

exit 0
