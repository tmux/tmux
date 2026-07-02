#!/bin/sh

# Tests of the mouse format variables (mouse_x, mouse_y, mouse_word,
# mouse_line, ...).  These are only populated while a mouse key binding is being
# dispatched, so the test drives a real mouse event:
#
#   - an inner client is attached inside a pane of a second ("outer") tmux
#     server, giving the inner server a genuine terminal;
#   - mouse mode is on and a MouseDown1Pane binding records the mouse format
#     variables into an option;
#   - an SGR mouse sequence is written to the outer pane, so the inner client
#     receives it as a real mouse click.
#
# This exercises the mouse callbacks and the grid word/line lookup code that
# display-message cannot otherwise reach.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
TMUX2="$TEST_TMUX -Ltest2 -f/dev/null"

cleanup()
{
	$TMUX kill-server >/dev/null 2>&1
	$TMUX2 kill-server >/dev/null 2>&1
}
fail()
{
	echo "$1"
	cleanup
	exit 1
}

# click COL ROW
#
# Write an SGR mouse press then release (button 0) at 1-based COL/ROW to the
# outer pane holding the inner client.
click()
{
	col="$1"
	row="$2"
	seq=$(printf '\033[<0;%s;%sM\033[<0;%s;%sm' "$col" "$row" "$col" "$row")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 1
}

cleanup

# Inner session with a single pane running cat, so its content is exactly what
# we send it.
$TMUX new-session -d -s cov -x 80 -y 24 'cat' || exit 1
$TMUX set -g mouse on
sleep 1
$TMUX send-keys -t cov:0.0 'alpha beta gamma' Enter
sleep 1

# Record every pane mouse variable when the pane is clicked.
$TMUX bind -n MouseDown1Pane run-shell \
    "$TMUX set -g @m 'x=#{mouse_x} y=#{mouse_y} word=#{mouse_word} line=#{mouse_line} pane=#{mouse_pane} hl=[#{mouse_hyperlink}]'"

# Attach a real client inside an outer tmux pane.  Clicks all target the first
# row, which lines up with the inner client regardless of the outer status line.
$TMUX2 new-session -d -x 80 -y 24 "$TMUX attach -t cov" || exit 1
sleep 1
OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
[ -n "$OUTER" ] || fail "No outer pane."

# Click column 3, row 1: over the first word ("alpha") of the first line.
click 3 1

M=$($TMUX show -gv @m 2>/dev/null)
[ -n "$M" ] || fail "Mouse binding did not fire (no @m)."

# mouse_x is 0-based column (SGR column 3 -> x 2); mouse_y is 0-based row 0.
case "$M" in
*"x=2 "*) ;;
*) fail "Unexpected mouse_x in: $M" ;;
esac
case "$M" in
*"y=0 "*) ;;
*) fail "Unexpected mouse_y in: $M" ;;
esac
# mouse_word is the word under the cursor, mouse_line the whole line.
case "$M" in
*"word=alpha "*) ;;
*) fail "Unexpected mouse_word in: $M" ;;
esac
case "$M" in
*"line=alpha beta gamma "*) ;;
*) fail "Unexpected mouse_line in: $M" ;;
esac

# A click in a different column selects a different word.
click 8 1
M=$($TMUX show -gv @m 2>/dev/null)
case "$M" in
*"word=beta "*) ;;
*) fail "Unexpected mouse_word for second click in: $M" ;;
esac

# The same variables have a separate path when the pane is in a mode (the word
# and line come from the mode, not the live grid).  A binding in the copy-mode
# key table fires while copy mode is active.
$TMUX bind -T copy-mode MouseDown1Pane run-shell \
    "$TMUX set -g @cm 'x=#{mouse_x} word=#{mouse_word} line=#{mouse_line}'"
$TMUX copy-mode -t cov:0.0
sleep 1
click 8 1
CM=$($TMUX show -gv @cm 2>/dev/null)
case "$CM" in
*"word=beta"*) ;;
*) fail "Unexpected copy-mode mouse_word in: $CM" ;;
esac
$TMUX send-keys -t cov:0.0 -X cancel
sleep 1

# Hyperlinks: a new window whose pane emits an OSC 8 hyperlink over the text
# "LINKED".  Clicking it reports the target URL via mouse_hyperlink (this drives
# the grid hyperlink lookup).  The emitter is written to a small script to keep
# the escape sequence readable.
LINKSH="${TMPDIR:-/tmp}/fmt-mouse-link-$$.sh"
cat >"$LINKSH" <<'EOF'
#!/bin/sh
printf '\033]8;;http://example.com\033\\LINKED\033]8;;\033\\\n'
exec cat
EOF
chmod +x "$LINKSH"
$TMUX neww -t cov: -n link "$LINKSH"
sleep 1
$TMUX select-window -t cov:link
sleep 1
click 3 1
M=$($TMUX show -gv @m 2>/dev/null)
rm -f "$LINKSH"
case "$M" in
*"hl=[http://example.com]"*) ;;
*) fail "Unexpected mouse_hyperlink in: $M" ;;
esac

cleanup
exit 0
