#!/bin/sh

# Verification test, not a known-bug regression test: checks whether a
# structural screen operation (insert-line / delete-line) on a pane that is
# currently obscured by a floating pane correctly retransmits an image
# sitting in the affected rows.
#
# screen_write_insertline()/screen_write_deleteline() (screen-write.c), when
# the target pane is obscured, escalate via screen_write_redraw_pane() ->
# screen_write_redraw_line() -> tty_cmd_redrawline() -> tty_draw_line().
# This is a different, older mechanism from the sub-pane damage-rectangle
# system built elsewhere this session - it was never migrated onto it. But
# both functions also unconditionally call image_redraw_area() up front
# (independent of obscured status), and tty_draw_line() (tty-draw.c) has
# its own built-in ENABLE_IMAGES handling, so reading the code suggests
# this path should already correctly recomposite images, unlike the bugs
# fixed elsewhere this session where nothing signalled a redraw at all.
# This test exists to confirm that empirically rather than relying on that
# reading holding up in practice.
#
# If this ever fails, it means that reading was wrong and the obscured
# structural-op path needs migrating onto the damage system too.

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

cleanup

TMP=$(mktemp)
trap "cleanup; rm -f $TMP" 0 1 15

# A small, distinctive SIXEL raster (26x26 pixels) at the top-left of the
# base pane, matching the fixture already used in image-support.sh. After a
# pause (long enough for the floating pane below to be created), it emits a
# cursor-home, insert-line, delete-line sequence itself - a content no-op,
# but it exercises the obscured-pane structural-op redraw path across the
# image's rows. This is done from the pane's own script rather than typed
# via send-keys, since interactive typing races against the shell's own
# line-editing redraws and does not reliably reach the terminal parser as
# real escape sequences.
SIXEL='\033Pq"1;1;26;26#0;2;100;100;100#0!26~-!26~-!26~-!26~-!26B\033\\'
TRIGGER='\033[H\033[L\033[M'
$TMUX new-session -d -s inner -x 40 -y 15 \
    "printf '$SIXEL'; sleep 2; printf '$TRIGGER'; sleep 100" || exit 1
sleep 0.5
BASE=$($TMUX list-panes -F '#{pane_id}' | head -1)
[ -n "$BASE" ] || fail "No base pane."

[ "$($TMUX display-message -p '#{image_support}')" = 0 ] && exit 0
$TMUX set -as terminal-features ',*:sixel' || exit 1

# Float a small pane so it overlaps the top rows of the base pane, where
# the image sits - this is what makes the base pane obscured there. This
# must happen before the base pane's 2-second internal sleep elapses.
$TMUX new-pane -d -x 12 -y 4 -X 5 -Y 0 || fail "new-pane -X -Y failed"

$TMUX2 new-session -d -x 40 -y 15 "$TMUX attach -t inner" || exit 1
sleep 0.5
OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
[ -n "$OUTER" ] || fail "No outer pane."

$TMUX2 pipe-pane -t "$OUTER" -O "cat >$TMP" || fail "pipe-pane failed"
sleep 2.5

grep -a -q '"1;1;26;26' $TMP ||
	fail "image was not retransmitted after insert/delete-line on an obscured pane"

exit 0
