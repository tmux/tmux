#!/bin/sh

# A floating pane positioned partly off the window's left/top edge (e.g.
# created with -X -5) has a negative wp->xoff/wp->yoff. screen_write_
# redraw_cb() (screen-write.c) used to pass these straight through as u_int
# to redraw_damage_window(), which wraps a negative offset to a huge value
# - redraw_damage_window()'s own bounds check then rejects the whole
# rectangle, so nothing gets redrawn, not even the pane's visible portion.
#
# This fires on returning from the alternate screen (screen_write_
# alternateoff()) among other paths. This test exercises exactly that:
# fills the pane's primary screen, switches it to the alternate screen and
# back, and checks the client actually receives the restored primary
# content in the pane's visible (on-screen) columns - using an attached
# client's own received bytes (via a nested outer client), not
# capture-pane, which reads the grid directly and would pass regardless of
# whether the client was ever actually told to redraw it.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
INNER="$TEST_TMUX -Loffscreen-inner-$$ -f/dev/null"
OUTER="$TEST_TMUX -Loffscreen-outer-$$ -f/dev/null"
EMITTER=$DIR/emitter.pl
CAPTURE=$DIR/capture

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
	rm -rf "$DIR"
}
trap cleanup 0 1 15

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

wait_visible_restored()
{
	i=0
	while [ "$i" -lt 50 ]; do
		$OUTER capture-pane -p -t outer:0.0 >"$CAPTURE" 2>/dev/null || true
		sed -n "${CONTENTROW}p" "$CAPTURE" | grep -q '^AAAAA' && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "primary-screen content was not restored in the pane's visible columns after returning from the alternate screen"
}

cat >"$EMITTER" <<'PERL'
use strict;
use warnings;

$| = 1;
print "\e[1;1H", 'A' x 15;
sleep 2;
print "\e[?1049h";
print "\e[1;1H", 'B' x 15;
sleep 2;
print "\e[?1049l";
sleep 100;
PERL

$INNER new-session -d -s inner -x 40 -y 10 'sleep 100' || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1

# Content pane spans window columns -5..9 (partly off the left edge); only
# columns 0..9 are ever visible.
FLOAT=$($INNER new-pane -d -PF '#{pane_id}' -x 15 -y 5 -X -5 -Y 2 \
    "perl '$EMITTER'") || exit 1
XOFF=$($INNER display-message -p -t "$FLOAT" '#{pane_left}')
YOFF=$($INNER display-message -p -t "$FLOAT" '#{pane_top}')
[ "$XOFF" -lt 0 ] || fail "sanity: floating pane is not off-screen (xoff=$XOFF)"
CONTENTROW=$((YOFF + 1))

$OUTER new-session -d -s outer -x 40 -y 10 'sleep 100' || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
$OUTER set-option -g default-terminal screen-256color || exit 1
$OUTER respawn-pane -k -t outer:0.0 \
    "$TEST_TMUX -Loffscreen-inner-$$ -f/dev/null attach-session -t inner" ||
    exit 1

wait_outer_has AAAAA
wait_outer_has BBBBB
wait_visible_restored

exit 0
