#!/bin/sh

# Regression test for a tmux bug (not a terminal bug): image_redraw_scroll()
# (image.c) marks the whole scroll region as damage on every scroll, and
# redraw_client_damage_rect()'s skip_images optimization (screen-redraw.c)
# skips retransmitting an image trusted to have moved with the scroll - but
# the ordinary REDRAW_TEXT pass for that same damage rectangle still ran
# unconditionally, drawing blank grid content directly over the image's
# cells. Text is normally drawn first and images composited on top
# immediately after in the same batch, so that is a harmless intermediate
# state - skip_images only ever suppressed that second, correcting pass,
# leaving the blank draw as the final state. This erased a correctly
# scrolled image, independent of anything the terminal itself did - this is
# what a user saw as "the image disappears when the pane scrolls in Windows
# Terminal" even with imagescroll and margins both granted, and could never
# be reproduced by any terminal-side test because the bug is entirely
# server side.
#
# redraw_draw_pane_span() now consults image_grid_next_span() to skip
# drawing text over any x-range with an image span attached, whenever
# skip_images is set, logging "skipping A-B on row N (image)" whenever it
# does - that log line, and the ability to exclude an image's columns from
# a text draw at all, do not exist before this fix, so its mere presence
# is enough to fail outright on any earlier tmux build.
#
# The image is placed comfortably mid-pane (not at the very top) and wide
# enough to span the whole pane, so it is still on screen - not already
# scrolled into history - by the time a real scroll's damage gets composed.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -vv -f/dev/null"
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

DIR=$(mktemp -d) || exit 1
cd "$DIR" || exit 1
trap "cleanup; cd /; rm -rf $DIR" 0 1 15

WIDTH=40
HEIGHT=20
ROW=10

# A sixel image exactly WIDTH cells wide (at a typical ~9px cell, 360px) and
# one cell tall, placed at screen row ROW - comfortably mid-pane, not at
# the very top, so it stays visible (not yet scrolled into history) through
# the first several scrolls.
HEADER='\033Pq"1;1;360;18#0;2;100;100;100#0!360~-!360~-!360B\033\\'

# Pad with blank lines after the image so the shell prompt lands at the
# bottom of the pane - without this, the very first scroll from a
# not-yet-settled cursor position falls back to tty_redraw_region() (a
# different, unrelated path that skip_images never applies to anyway), and
# the fix's own exclusion logic never gets a chance to run at all.
PAD=$((HEIGHT - ROW - 2))
$TMUX new-session -d -s inner -x $WIDTH -y $HEIGHT \
	"printf '\\033[$((ROW + 1));1H$HEADER'; i=0; while [ \$i -lt $PAD ]; do echo; i=\$((i + 1)); done; exec sh" ||
	exit 1
sleep 0.3

[ "$($TMUX display-message -p '#{image_support}')" = 0 ] && exit 0
$TMUX set -g pane-scrollbars on || exit 1
$TMUX set -as terminal-features ',*:sixel' || exit 1
$TMUX set -as terminal-features ',*:margins' || exit 1
$TMUX set -as terminal-features ',*:imagescroll' || exit 1

$TMUX2 new-session -d -x $WIDTH -y $HEIGHT || exit 1
OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
[ -n "$OUTER" ] || fail "No outer pane."
$TMUX2 set -as terminal-features ',*:sixel@' || fail "disable outer sixel failed"
TMP=$(mktemp)
$TMUX2 pipe-pane -t "$OUTER" -O "cat >$TMP" || fail "pipe-pane failed"
$TMUX2 send-keys -t "$OUTER" -l "$TMUX attach -t inner" || fail "send attach failed"
$TMUX2 send-keys -t "$OUTER" Enter || fail "send enter failed"
sleep 1

grep -qa '"1;1;360;18' "$TMP" || fail "sanity: image never reached the client"

LOG=$(ls tmux-server*.log 2>/dev/null | head -1)
[ -n "$LOG" ] || fail "sanity: no server -vv log was produced"
BEFORE=$(wc -l <"$LOG")
: >"$TMP"

# The image (row ROW, 10 of 20) has plenty of room above it - this stays
# well within the visible pane for the whole burst.
$TMUX send-keys -t inner -l "yes | head -n 10" || fail "send scroll failed"
$TMUX send-keys -t inner Enter || fail "send enter failed"
sleep 1

n_dcs=$(grep -ac "$(printf '\033P')" "$TMP")
[ "$n_dcs" -eq 0 ] ||
	fail "sanity: image was retransmitted ($n_dcs times) - imagescroll's trust did not engage, so this run cannot test what it is meant to"

tail -n +$((BEFORE + 1)) "$LOG" | grep -q "redraw_draw_pane_span: skipping .* (image)" ||
	fail "the text-redraw pass never excluded the image's cells while trusting a scroll to have preserved it - this build would draw blank content over the image and erase it"

exit 0
