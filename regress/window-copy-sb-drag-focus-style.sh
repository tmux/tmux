#!/bin/sh

# Dragging an inactive pane's scrollbar slider changes which pane is
# active (window_copy_scroll(), window-copy.c). Every other place in the
# codebase that changes the active pane pairs it with
# window_redraw_active_switch() (or falls back to a full window redraw) so
# the old/new active panes' window-active-style/window-style colours get
# repainted - window_copy_scroll() was the one caller that did neither, so
# a scrollbar-slider drag changed focus but left both panes' body colours
# stale. This used to be masked by window_set_active_pane() itself doing
# an unconditional full redraw on every active-pane change, until that was
# narrowed to borders/status-only for the common case.
#
# This creates two tiled panes with clearly distinguishable
# window-active-style/window-style backgrounds, puts the *inactive* one in
# copy mode, drags its scrollbar slider, and checks - via an attached
# client's own received bytes, since window-style is applied during
# redraw composition rather than stored in the grid, so capture-pane
# alone would not reflect it - that the newly active pane's marker text
# immediately shows the active-style colour, not the stale one.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
INNER="$TEST_TMUX -Lsbfocus-inner-$$ -f/dev/null"
OUTER="$TEST_TMUX -Lsbfocus-outer-$$ -f/dev/null"
CAPTURE=$DIR/capture
ESC=$(printf '\033')

fail()
{
	echo "$*" >&2
	[ -s "$CAPTURE" ] && cat -A "$CAPTURE" >&2
	exit 1
}

cleanup()
{
	$OUTER kill-server 2>/dev/null
	$INNER kill-server 2>/dev/null
	rm -rf "$DIR"
}
trap cleanup 0 1 15

get_mark_color()
{
	$OUTER capture-pane -e -p -t outer:0.0 >"$CAPTURE" 2>/dev/null || true
	LC_ALL=C grep -a "MARK1" "$CAPTURE" |
	    LC_ALL=C grep -aoE "${ESC}\[4[12]m" | tail -1
}

wait_mark_color()
{
	want=$1
	i=0
	while [ "$i" -lt 50 ]; do
		[ "$(get_mark_color)" = "$want" ] && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "MARK1's pane never showed the expected background (wanted '$want', got '$(get_mark_color)')"
}

mouse()
{
	sequence=$(printf '\033[<%s;%s;%s%s' "$1" "$2" "$3" "$4")
	$OUTER send-keys -t outer:0.0 -l "$sequence" || exit 1
	sleep 0.1
}

$INNER new-session -d -s inner -x 40 -y 10 'sleep 100' || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1
$INNER set-option -g mouse on || exit 1
$INNER set-option -g pane-scrollbars on || exit 1
$INNER setw pane-scrollbars-position right || exit 1
$INNER setw pane-scrollbars-style 'width=1,pad=0' || exit 1
$INNER set-option -g window-active-style 'bg=red' || exit 1
$INNER set-option -g window-style 'bg=green' || exit 1
$INNER split-window -h -t inner -d 'printf MARK1; sleep 100' || exit 1

PANE1=$($INNER list-panes -t inner -F '#{pane_id}' | sed -n 2p)
[ "$($INNER display-message -p -t "$PANE1" '#{pane_active}')" = 0 ] ||
	fail "sanity: MARK1's pane is already active before the drag"

# Put the inactive pane in copy mode without switching focus (copy-mode -t
# targets a pane without making it active).
$INNER copy-mode -t "$PANE1" || exit 1

$OUTER new-session -d -s outer -x 40 -y 10 'sleep 100' || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
$OUTER set-option -g default-terminal screen-256color || exit 1
$OUTER respawn-pane -k -t outer:0.0 \
    "$TEST_TMUX -Lsbfocus-inner-$$ -f/dev/null attach-session -t inner" ||
    exit 1

wait_mark_color "${ESC}[42m"

XOFF=$($INNER display-message -p -t "$PANE1" '#{pane_left}')
YOFF=$($INNER display-message -p -t "$PANE1" '#{pane_top}')
SX=$($INNER display-message -p -t "$PANE1" '#{pane_width}')
SBCOL=$((XOFF + SX + 1))
SBROW=$((YOFF + 1))

# With no scrollback beyond the pane's own content, the slider fills the
# whole scrollbar track, so any point on it (here, its very first row) is
# on the slider.
mouse 0 "$SBCOL" "$SBROW" M
mouse 32 "$SBCOL" "$((SBROW + 1))" M
mouse 0 "$SBCOL" "$((SBROW + 1))" m

[ "$($INNER display-message -p -t "$PANE1" '#{pane_active}')" = 1 ] ||
	fail "sanity: dragging the scrollbar slider did not change focus"

wait_mark_color "${ESC}[41m"

exit 0
