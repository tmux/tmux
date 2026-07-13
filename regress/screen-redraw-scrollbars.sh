#!/bin/sh

# Exercise screen-redraw.c drawing of pane scrollbars: position (right/left),
# width, and pad (which is handled separately from width). Scrollbars are drawn
# as styled (coloured) cells rather than glyphs, so these scenes are captured
# with escape sequences (-e); without that the scrollbar is invisible.
#
# Each scene is rendered in an inner tmux attached inside an outer tmux pane.
# The outer pane is captured and compared with a golden in screen_redraw_results/.
#
# Run with GENERATE=1 to (re)create the golden files.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"
RESULTS=screen-redraw-results

TMP=$(mktemp)
trap "rm -f $TMP; $TMUX kill-server 2>/dev/null; $TMUX2 kill-server 2>/dev/null" \
	0 1 15

fail() {
	echo "$*" >&2
	exit 1
}

# compare <name>: capture the outer pane with escapes and compare (or generate).
compare() {
	sleep 1
	$TMUX capturep -pe >$TMP || exit 1
	if [ -n "$GENERATE" ]; then
		cp $TMP "$RESULTS/$1.result" || exit 1
		echo "generated $1"
	else
		cmp -s $TMP "$RESULTS/$1.result" || \
			fail "scene $1 differs from $RESULTS/$1.result"
	fi
}

# new_scene: fresh inner window, single full-size pane.
new_scene() {
	$TMUX2 neww -d "sh -c 'i=0; while [ \$i -lt 11 ]; do printf \"SB%02d abcdefghij\n\" \$i; i=\$((i + 1)); done; exec sleep 100'" || exit 1
	$TMUX2 selectw -t:\$ || exit 1
	$TMUX2 resizew -x40 -y12 || exit 1
}

$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

$TMUX2 new -d -x40 -y12 "sh -c 'i=0; while [ \$i -lt 11 ]; do printf \"SB%02d abcdefghij\n\" \$i; i=\$((i + 1)); done; exec sleep 100'" || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 set -g window-size manual || exit 1
$TMUX2 set -g pane-scrollbars on || exit 1

$TMUX new -d -x40 -y12 || exit 1
$TMUX set -g status off || exit 1
$TMUX set -g window-size manual || exit 1
$TMUX set -g default-terminal "tmux-256color" || exit 1
$TMUX send -l "$TMUX2 attach" || exit 1
$TMUX send Enter || exit 1
sleep 1

# Right, width 1, no pad.
new_scene
$TMUX2 setw pane-scrollbars-position right || exit 1
$TMUX2 setw pane-scrollbars-style "bg=black,fg=white,width=1,pad=0" || exit 1
compare scrollbar-right-w1

# Left, width 1, no pad.
new_scene
$TMUX2 setw pane-scrollbars-position left || exit 1
$TMUX2 setw pane-scrollbars-style "bg=black,fg=white,width=1,pad=0" || exit 1
compare scrollbar-left-w1

# Right, width 2, no pad.
new_scene
$TMUX2 setw pane-scrollbars-position right || exit 1
$TMUX2 setw pane-scrollbars-style "bg=black,fg=white,width=2,pad=0" || exit 1
compare scrollbar-right-w2

# Right, width 1, pad 1. The pad is drawn between the pane content and the
# scrollbar; for a right scrollbar that pad cell is at the right edge, so capture
# trims it and this matches scrollbar-right-w1. The golden still pins that
# behaviour (and the pad draw path runs); the left scene below shows pad visibly.
new_scene
$TMUX2 setw pane-scrollbars-position right || exit 1
$TMUX2 setw pane-scrollbars-style "bg=black,fg=white,width=1,pad=1" || exit 1
compare scrollbar-right-pad

# Left, width 1, pad 1: the pad cell sits between the slider and the content, so
# it is visible (one extra column before the pane content).
new_scene
$TMUX2 setw pane-scrollbars-position left || exit 1
$TMUX2 setw pane-scrollbars-style "bg=black,fg=white,width=1,pad=1" || exit 1
compare scrollbar-left-pad

# Floating pane with a scrollbar.
new_scene
$TMUX2 setw pane-scrollbars-position right || exit 1
$TMUX2 setw pane-scrollbars-style "bg=black,fg=white,width=1,pad=0" || exit 1
$TMUX2 new-pane -x20 -y6 -X8 -Y3 "sh -c 'i=0; while [ \$i -lt 5 ]; do printf \"FLOAT%02d abcdef\n\" \$i; i=\$((i + 1)); done; exec sleep 100'" || exit 1
compare scrollbar-floating

# Two tiled panes side by side, each with a right scrollbar. The left pane's
# scrollbar sits between its content and the shared border, so the border is
# extended outward over the scrollbar gap (the right += sb_w path in
# redraw_mark_pane_borders) and must still join cleanly. The right pane's
# scrollbar abuts the window edge.
new_scene
$TMUX2 setw pane-scrollbars-position right || exit 1
$TMUX2 setw pane-scrollbars-style "bg=black,fg=white,width=1,pad=0" || exit 1
$TMUX2 splitw -h "sh -c 'i=0; while [ \$i -lt 11 ]; do printf \"SBR%02d abcdefghij\n\" \$i; i=\$((i + 1)); done; exec sleep 100'" || exit 1
$TMUX2 selectp -t0 || exit 1
compare scrollbar-split-right

# Same split with the scrollbars on the left: the right pane's scrollbar now sits
# between the shared border and its content (the left -= sb_w path).
new_scene
$TMUX2 setw pane-scrollbars-position left || exit 1
$TMUX2 setw pane-scrollbars-style "bg=black,fg=white,width=1,pad=0" || exit 1
$TMUX2 splitw -h "sh -c 'i=0; while [ \$i -lt 11 ]; do printf \"SBL%02d abcdefghij\n\" \$i; i=\$((i + 1)); done; exec sleep 100'" || exit 1
$TMUX2 selectp -t0 || exit 1
compare scrollbar-split-left

# Scrollbar slider in copy mode: with scrollback the slider is shorter than the
# track, so this exercises the slider geometry (which only runs when the pane is
# in a mode). copy-mode -H hides the position indicator, which is not stable.
$TMUX2 neww -d "sh -c 'seq 40; exec sleep 100'" || exit 1
$TMUX2 selectw -t:\$ || exit 1
$TMUX2 resizew -x40 -y12 || exit 1
$TMUX2 setw pane-scrollbars-position right || exit 1
$TMUX2 setw pane-scrollbars-style "bg=black,fg=white,width=1,pad=0" || exit 1
$TMUX2 copy-mode -H || exit 1
$TMUX2 send -X history-top || exit 1
compare scrollbar-copy-mode -e

exit 0
