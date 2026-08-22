#!/bin/sh

# Regression test: dragging a floating pane across another pane's ordinary
# content must not redraw that other pane's scrollbar, unless the drag
# actually crosses the scrollbar's own strip.
#
# cmd_resize_pane_redraw_floating() (cmd-resize-pane.c) used to flag
# PANE_REDRAWSCROLLBAR on any pane whose whole *body* intersected the
# floating pane's old or new rectangle, rather than just its narrow
# scrollbar strip - so dragging a floating pane back and forth over an
# ordinary tiled pane's content (never touching its scrollbar) still
# needlessly redrew that pane's scrollbar on every motion step. See
# tmux-image-redraw-known-bugs.md for the full write-up.
#
# This is checked by giving the non-dragged pane a distinctive scrollbar
# colour and counting how many times its SGR code appears in the client's
# raw output while the floating pane is dragged vertically over that pane's
# body, well clear of its scrollbar column: with the fix, it should never
# reappear after the initial draw.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

cleanup()
{
	$TMUX kill-server >/dev/null 2>&1
	$TMUX2 kill-server >/dev/null 2>&1
}
fail()
{
	echo "$*" >&2
	cleanup
	exit 1
}

# drag STARTCOL STARTROW ENDCOL ENDROW
drag()
{
	scol="$1"
	srow="$2"
	ecol="$3"
	erow="$4"

	seq=$(printf '\033[<0;%s;%sM' "$scol" "$srow")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 0.2
	seq=$(printf '\033[<32;%s;%sM' "$ecol" "$erow")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 0.2
	seq=$(printf '\033[<0;%s;%sm' "$ecol" "$erow")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 0.5
}

cleanup

TMP=$(mktemp)
trap "cleanup; rm -f $TMP" 0 1 15

$TMUX new-session -d -s inner -x 60 -y 20 'sh -c "sleep 100"' || exit 1
$TMUX set -g mouse on || fail "set mouse failed"
$TMUX set -g default-command 'sh -c "sleep 100"' || fail "set default-command failed"
$TMUX set -g pane-scrollbars on || fail "set pane-scrollbars failed"

$TMUX split-window -h -t inner 'sh -c "sleep 100"' || fail "split-window failed"

PANES=$($TMUX list-panes -t inner -F '#{pane_id} #{pane_left}')
LEFT=$(echo "$PANES" | sort -k2 -n | head -1 | cut -d' ' -f1)
[ -n "$LEFT" ] || fail "could not identify left pane"

# A distinctive scrollbar colour for the non-dragged (left) pane only.
$TMUX set -p -t "$LEFT" pane-scrollbars-style 'fg=colour201,bg=colour17' ||
	fail "set pane-scrollbars-style failed"

ALEFT=$($TMUX display-message -p -t "$LEFT" '#{pane_left}')
ATOP=$($TMUX display-message -p -t "$LEFT" '#{pane_top}')
AWIDTH=$($TMUX display-message -p -t "$LEFT" '#{pane_width}')
AHEIGHT=$($TMUX display-message -p -t "$LEFT" '#{pane_height}')
[ "$AWIDTH" -gt 15 ] || fail "left pane too narrow for this test ($AWIDTH)"

# A small floating pane placed well inside the left pane's content area,
# clear of its (right-hand) scrollbar column by several columns.
FLOAT=$($TMUX new-pane -d -PF '#{pane_id}' -x 8 -y 5 \
    -X $((ALEFT + 2)) -Y $((ATOP + 2))) || fail "new-pane -X -Y failed"
FTOP=$($TMUX display-message -p -t "$FLOAT" '#{pane_top}')
FLEFT=$($TMUX display-message -p -t "$FLOAT" '#{pane_left}')
FWIDTH=$($TMUX display-message -p -t "$FLOAT" '#{pane_width}')
[ $((FLEFT + FWIDTH + 3)) -lt $((ALEFT + AWIDTH)) ] ||
	fail "sanity: floating pane too close to the scrollbar column"

# Start the outer session with a plain shell, then start capturing before
# triggering the attach - starting the attach as the outer pane's initial
# command would mean pipe-pane only starts after the attach-driven initial
# redraw (which draws the scrollbars) has already happened, missing it.
$TMUX2 new-session -d -x 60 -y 20 || exit 1
OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
[ -n "$OUTER" ] || fail "No outer pane."
$TMUX2 pipe-pane -t "$OUTER" -O "cat >$TMP" || fail "pipe-pane failed"
$TMUX2 send-keys -t "$OUTER" -l "$TMUX attach -t inner" || fail "send attach failed"
$TMUX2 send-keys -t "$OUTER" Enter || fail "send enter failed"
sleep 1

# Sanity check: the distinctive scrollbar colour reaches the client at all.
grep -qa '48;5;201' $TMP || fail "sanity: scrollbar colour never reached the client"
: >$TMP

# Drag the floating pane straight up and down by its top border, staying at
# a fixed column the whole time - this never crosses the left pane's
# scrollbar strip, only its ordinary content.
GRABCOL=$((FLEFT + FWIDTH / 2))
row=$FTOP
i=0
while [ $i -lt 6 ]; do
	newrow=$((row + 1))
	drag $GRABCOL $row $GRABCOL $newrow
	row=$newrow
	i=$((i + 1))
done
i=0
while [ $i -lt 6 ]; do
	newrow=$((row - 1))
	drag $GRABCOL $row $GRABCOL $newrow
	row=$newrow
	i=$((i + 1))
done

# The scrollbar colour should never reappear - its geometry never changed,
# and the drag never crossed its column. This is expected to fail before
# the fix - see the header comment.
n=$(grep -ac '48;5;201' $TMP)
[ "$n" -eq 0 ] ||
	fail "left pane's scrollbar was redrawn $n times while dragging over its body only"

exit 0
