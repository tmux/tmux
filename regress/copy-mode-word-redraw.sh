#!/bin/sh

# Word selection can extend beyond the cursor's row. Compare what an attached
# client drew with a full copy-mode repaint after extending and shrinking it.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
INNER="$TEST_TMUX -Lword-inner-$$ -f/dev/null"
OUTER="$TEST_TMUX -Lword-outer-$$ -f/dev/null"
DIR=$(mktemp -d) || exit 1
exit_status=0

cleanup()
{
	$OUTER kill-server 2>/dev/null
	$INNER kill-server 2>/dev/null
	rm -rf "$DIR"
}
trap cleanup 0
trap 'exit 1' 1 2 3 15

fail()
{
	echo "$*" >&2
	exit 1
}

check_buffer()
{
	$INNER send-keys -X copy-selection-no-clear || exit 1
	$INNER save-buffer "$DIR/copied" || exit 1
	cmp -s "$DIR/expected" "$DIR/copied" || fail 'unexpected copied text'
}

check_redraw()
{
	label=$1
	sleep 0.2
	$OUTER capture-pane -ep -t outer:0.0 >"$DIR/before" || exit 1
	$INNER send-keys -X toggle-position || exit 1
	$INNER send-keys -X toggle-position || exit 1
	sleep 0.2
	$OUTER capture-pane -ep -t outer:0.0 >"$DIR/after" || exit 1
	if ! cmp -s "$DIR/before" "$DIR/after"; then
		diff -u "$DIR/before" "$DIR/after" >&2
		echo "$label: stale word-selection highlight" >&2
		exit_status=1
	fi
}

$INNER new-session -d -s inner -x20 -y8 'sleep 100' || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1
$INNER set-option -g mode-keys emacs || exit 1
$INNER set-option -g copy-mode-position-format '' || exit 1
$INNER set-option -g copy-mode-selection-style 'fg=white,bg=red' || exit 1
$OUTER new-session -d -s outer -x20 -y8 "$INNER attach -t inner" || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
sleep 0.5

# A wrapped word ending on the penultimate row. In emacs mode end-of-line
# moves onto the exclusive end and extends the selection into the blank row
# below; start-of-line then removes that row from the selection.
awk 'BEGIN { print "PREFIX"; for (i = 0; i < 115; i++) printf "a"; print "" }' \
    >"$DIR/text" || exit 1
$INNER new-window -t inner:1 "cat '$DIR/text'; exec sleep 100" || exit 1
$INNER resize-window -x20 -y8 || exit 1
sleep 0.2
$INNER copy-mode || exit 1
$INNER send-keys -X history-top || exit 1
$INNER send-keys -X cursor-down || exit 1
$INNER send-keys -X select-word || exit 1
$INNER send-keys -X end-of-line || exit 1
[ "$($INNER display-message -p '#{selection_end_y}')" = 7 ] ||
	fail 'selection did not extend into the blank row'
check_redraw 'extend onto blank row'
awk 'BEGIN { for (i = 0; i < 115; i++) printf "a"; print "" }' \
    >"$DIR/expected" || exit 1
check_buffer
$INNER send-keys -X start-of-line || exit 1
[ "$($INNER display-message -p '#{selection_end_y}')" = 6 ] ||
	fail 'selection did not shrink away from the blank row'
check_redraw 'shrink away from blank row'
awk 'BEGIN { for (i = 0; i < 115; i++) printf "a" }' \
    >"$DIR/expected" || exit 1
check_buffer

# The selected next word can itself wrap across several rows even though the
# cursor only moved within one row. All of those rows must be repainted.
awk 'BEGIN { print "PREFIX"; print "one";
    for (i = 0; i < 65; i++) printf "b"; print ""; print "END" }' \
    >"$DIR/text" || exit 1
$INNER new-window -t inner:2 "cat '$DIR/text'; exec sleep 100" || exit 1
$INNER resize-window -x20 -y8 || exit 1
sleep 0.2
$INNER copy-mode || exit 1
$INNER send-keys -X history-top || exit 1
$INNER send-keys -X cursor-down || exit 1
$INNER send-keys -X select-word || exit 1
$INNER send-keys -X end-of-line || exit 1
[ "$($INNER display-message -p '#{selection_end_y}')" = 5 ] ||
	fail 'selection did not extend over the wrapped next word'
check_redraw 'extend over a wrapped next word'
awk 'BEGIN { print "one"; for (i = 0; i < 65; i++) printf "b" }' \
    >"$DIR/expected" || exit 1
check_buffer
$INNER send-keys -X start-of-line || exit 1
check_redraw 'shrink away from a wrapped next word'
printf 'one' >"$DIR/expected" || exit 1
check_buffer

# Drag a word selection onto the bottom row and back while there is still
# history below the viewport. A redraw must not read the next backing row
# and paint it onto the last visible row.
awk 'BEGIN { for (i = 0; i < 30; i++) printf "line%02d abc def\n", i }' \
    >"$DIR/text" || exit 1
$INNER new-window -t inner:3 "cat '$DIR/text'; exec sleep 100" || exit 1
$INNER resize-window -x20 -y8 || exit 1
$INNER set-option -g mouse on || exit 1
$INNER unbind-key -Tcopy-mode MouseDragEnd1Pane || exit 1
sleep 0.2
$INNER copy-mode || exit 1
$INNER send-keys -X history-top || exit 1
$INNER send-keys -X cursor-down || exit 1
$INNER send-keys -X select-word || exit 1
$OUTER send-keys -l "$(printf '\033[<0;3;2M')" || exit 1
sleep 0.1
$OUTER send-keys -l "$(printf '\033[<32;8;7M')" || exit 1
sleep 0.1
$INNER send-keys -X selection-mode word || exit 1
$OUTER send-keys -l "$(printf '\033[<32;8;8M')" || exit 1
sleep 0.1
# Leave the edge to cancel the auto-scroll timer before comparing.
$OUTER send-keys -l "$(printf '\033[<32;8;7M')" || exit 1
sleep 0.1
$OUTER send-keys -l "$(printf '\033[<0;8;7m')" || exit 1
sleep 0.1
[ "$($INNER display-message -p '#{selection_mode}')" = word ] ||
	fail 'mouse drag did not retain word selection mode'
[ "$($INNER display-message -p '#{scroll_position}')" -gt 0 ] ||
	fail 'mouse drag reached the bottom of history'
check_redraw 'drag word selection at bottom of viewport'

exit "$exit_status"
