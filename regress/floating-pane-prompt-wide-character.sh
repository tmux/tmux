#!/bin/sh

# A damage rectangle's clip range is grown to avoid splitting a wide
# character, but redraw_damage_grow_span_clip() (screen-redraw.c) only ever
# checks the span's own pane *content* grid (wp->screen) for that. For a
# REDRAW_SPAN_PANE span, that same range is then also handed to
# redraw_damage_draw_pane_prompt() to recompose the pane's separately
# rendered prompt (wp->prompt, e.g. from "command-prompt -P") over the
# damaged sub-range - but the prompt is drawn into its own, freshly
# allocated one-line screen, unrelated to the pane's content grid, so a
# range grown (or left ungrown) against the content is not necessarily
# grown correctly for the prompt's own wide characters.
#
# This is invisible when the pane's own content is plain ASCII (as here):
# redraw_damage_grow_span_clip() never finds anything to grow against, so
# the raw, ungrown geometric range is passed straight through to the
# prompt - and if that range's edge lands mid-character in the *prompt's*
# grid, tty_draw_line() clears the character it cuts through
# (tty_draw_line_get_empty()'s gc->data.width > nx check, for a trailing
# base cell with no room left for its padding half).
#
# The trigger is a palette change (OSC 4) in a tiled pane that is one half
# of a vertical split running the full height of the window - occluded
# under the floating pane, but still geometrically triggering a redraw of
# its own rectangle. Positioned so the split boundary falls inside the
# floating pane's own CJK prompt, this reproduces exactly Codex's report:
# "a floating pane containing a CJK prompt across a tiled-pane boundary -
# a palette update in the tiled pane blanks a prompt character."

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
cd "$DIR" || exit 1
INNER="$TEST_TMUX -Lpromptwide-inner-$$ -f/dev/null"
OUTER="$TEST_TMUX -Lpromptwide-outer-$$ -f/dev/null"
EMITTER=$DIR/emitter.pl
CAPTURE=$DIR/capture
FLOAT=

fail()
{
	echo "$*" >&2
	[ -s "$CAPTURE" ] && cat "$CAPTURE" >&2
	exit 1
}

cleanup()
{
	$OUTER kill-server 2>/dev/null
	$INNER kill-server 2>/dev/null
	cd /
	rm -rf "$DIR"
}
trap cleanup 0 1 15

wait_for_client()
{
	i=0
	while [ "$i" -lt 50 ]; do
		CLIENT=$($INNER list-clients -F '#{client_name}' 2>/dev/null)
		[ -n "$CLIENT" ] && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "inner client did not attach"
}

wait_outer_has()
{
	marker=$1
	i=0
	while [ "$i" -lt 50 ]; do
		$OUTER capture-pane -p -t outer:0.0 >"$CAPTURE" 2>/dev/null || true
		grep -q "$marker" "$CAPTURE" && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "outer client did not show $marker"
}

wait_prompt_row_intact()
{
	i=0
	while [ "$i" -lt 50 ]; do
		$OUTER capture-pane -p -t outer:0.0 >"$CAPTURE" 2>/dev/null || true
		line=$(sed -n "${PROMPTROW}p" "$CAPTURE")
		case $line in
		*"$PROMPTTEXT"*) return 0 ;;
		esac
		sleep 0.1
		i=$((i + 1))
	done
	fail "the CJK prompt was not intact after the palette-triggered damage - got: $line"
}

cat >"$EMITTER" <<'PERL'
use strict;
use warnings;

$| = 1;
my $line = <STDIN>;
print "\e]4;1;rgb:11/22/33\e\\";
sleep 100;
PERL

$INNER new-session -d -s inner -x 60 -y 12 "perl '$EMITTER'" || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1
LEFT=$($INNER list-panes -t inner -F '#{pane_id}') || exit 1

# Split so the boundary between the two tiled panes falls at column 17 -
# used below to pick a floating-pane column that lands the boundary
# mid-character inside the prompt.
RIGHT=$($INNER split-window -t inner -h -l 43 -PF '#{pane_id}' \
    'sleep 100') || exit 1
RX=$($INNER display-message -p -t "$RIGHT" '#{pane_left}') || exit 1

# Create the floating pane, then check its actual resulting position (the
# border-framing offset added to -X is not something to hand-compute). Try
# adjacent starting columns until the split boundary lands on an odd
# (padding-half) column of the prompt's own numbering, and within the
# prompt's 12-column width.
startx=10
tries=0
while [ "$tries" -lt 4 ]; do
	[ -n "$FLOAT" ] && $INNER kill-pane -t "$FLOAT" 2>/dev/null
	FLOAT=$($INNER new-pane -d -PF '#{pane_id}' -x 24 -y 5 -X "$startx" \
	    -Y 2 "sh -c 'i=0; while [ \$i -lt 10 ]; do \
printf AAAAAAAAAAAAAAAAAAAAAA\\\\n; i=\$((i+1)); done; sleep 100'") ||
	    exit 1
	sleep 0.2
	X1=$($INNER display-message -p -t "$FLOAT" '#{pane_left}')
	Y1=$($INNER display-message -p -t "$FLOAT" '#{pane_top}')
	H1=$($INNER display-message -p -t "$FLOAT" '#{pane_height}')
	local=$((RX - X1 - 1))
	if [ "$local" -ge 1 ] && [ "$local" -le 11 ] &&
	    [ $((local % 2)) -eq 1 ]; then
		break
	fi
	startx=$((startx + 1))
	tries=$((tries + 1))
done
local=$((RX - X1 - 1))
[ "$local" -ge 1 ] && [ "$local" -le 11 ] && [ $((local % 2)) -eq 1 ] ||
	fail "could not find bad-parity starting column"

$OUTER new-session -d -s outer -x 60 -y 12 'sleep 100' || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
$OUTER set-option -g default-terminal screen-256color || exit 1
$OUTER respawn-pane -k -t outer:0.0 \
    "$TEST_TMUX -Lpromptwide-inner-$$ -f/dev/null attach-session -t inner" ||
    exit 1

wait_for_client
wait_outer_has AAAAAAAAAAAAAAAAAAAAAA
CLIENT=$($INNER list-clients -F '#{client_name}') || exit 1

$INNER select-pane -t "$FLOAT" || exit 1
PROMPTTEXT=$(printf '\344\270\255' | perl -CSD -ne 'print $_ x 6')
$INNER command-prompt -b -P -t "$CLIENT" -p "$PROMPTTEXT" \
    'display-message -- %1' || exit 1
wait_outer_has "$PROMPTTEXT"

PROMPTROW=$((Y1 + H1))

# Trigger the damage: unblock the emitter so it fires the palette change in
# the left tiled pane, which is occluded under (but geometrically overlaps)
# the floating pane's prompt row.
$INNER send-keys -t "$LEFT" Enter || exit 1

wait_prompt_row_intact

exit 0
