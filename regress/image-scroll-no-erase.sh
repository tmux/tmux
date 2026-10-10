#!/bin/sh

# Regression test for a tmux bug (not a terminal bug): image_redraw_scroll()
# (image.c) marks the whole scroll region as damage on every scroll. For a
# client whose terminal has imagescroll, the terminal has already moved the
# image (and the text) along with the scroll, but the damage was still
# composed: the image was not retransmitted, but the ordinary text pass
# drew blank grid content directly over the image's cells and erased a
# correctly scrolled image. This is what a user saw as "the image
# disappears when the pane scrolls in Windows Terminal" even with
# imagescroll and margins both granted, and could never be reproduced by
# any terminal-side test because the bug is entirely server side.
#
# redraw_client_damage_rect() (screen-redraw.c) now composes nothing for a
# scroll the client's terminal is trusted to have done itself, logging
# "composing damage" only when it draws. Nothing being drawn is what keeps
# the image, so the absence of that log line for the scroll is checked.
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
# different, unrelated path that the trusted-scroll skip never applies to
# anyway), and the test would not exercise what it is meant to.
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

tail -n +$((BEFORE + 1)) "$LOG" | grep -q "redraw_client_damage_rect: .* composing damage" &&
	fail "damage was composed for a scroll the terminal is trusted to have done - this would draw over the image and erase it"

exit 0
