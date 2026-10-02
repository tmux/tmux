#!/bin/sh

# Regression test: with imagescroll granted, does tmux actually use a native
# scrolling-region command (DECSTBM, and DECSLRM if the pane isn't full
# width) to perform the scroll, rather than tty_redraw_region()'s fallback
# full manual repaint? This directly checks the concern raised in PR review
# that tmux "isn't using scrolling regions" for an image-bearing pane -
# imagescroll only makes sense to grant a terminal if tmux is actually
# relying on a real native scroll to carry the image along, not silently
# falling back to redrawing every line by hand while still skipping the
# image retransmission (which would just lose the image for nothing).
#
# tty_redraw_region() (tty.c) logs one of two lines whenever it runs at
# all - "%s large region redraw" or "%s small region redraw" - so their
# total absence after the scroll proves the native path
# (tty_cmd_linefeed()/scrollup()/scrolldown()/reverseindex()) was taken
# instead. Combined with image-sixel-region-scroll.sh's own check (the
# image is not retransmitted), this proves imagescroll's trust is actually
# backed by a real native scroll, not a coincidence.
#
# Phase 2 is the deliberate opposite case, and is what PR review's concern
# actually described: pane-scrollbars makes this pane less than full width,
# and without margins granted, tty_cmd_*()'s own
# "(!tty_full_width(tty, ctx) && !tty_use_margin(tty))" check has no native
# scrolling-region option left and must fall back - proving phase 1 wasn't
# trivially passing, and that redraw_image_scroll_result()'s failure path
# correctly also stops imagescroll's trust from applying when that happens
# (the image must be retransmitted here, exactly as
# image-sixel-region-scroll.sh's own phase 2 already separately proves).
#
# A real attached client is needed for any of this redraw code to run at
# all, so this uses the same nested inner/outer tmux pattern as
# image-sixel-region-scroll.sh.

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

HEADER='\033Pq"1;1;26;26#0;2;100;100;100#0!26~-!26~-!26~-!26~-!26B\033\\'

$TMUX new-session -d -s inner -x $WIDTH -y $HEIGHT \
	"printf '$HEADER'; exec sh" || exit 1
sleep 0.3

[ "$($TMUX display-message -p '#{image_support}')" = 0 ] && exit 0
$TMUX set -g pane-scrollbars on || exit 1
$TMUX set -as terminal-features ',*:sixel' || exit 1
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

grep -qa '"1;1;26;26' "$TMP" || fail "sanity: image never reached the client"

LOG=$(ls tmux-server*.log 2>/dev/null | head -1)
[ -n "$LOG" ] || fail "sanity: no server -vv log was produced"

run_scroll()
{
	label=$1
	BEFORE=$(wc -l <"$LOG")
	: >"$TMP"

	$TMUX send-keys -t inner -l "yes | head -n 10" || fail "$label: send scroll failed"
	$TMUX send-keys -t inner Enter || fail "$label: send enter failed"
	sleep 1

	n_dcs=$(grep -ac "$(printf '\033P')" "$TMP")
	n_redraw=$(tail -n +$((BEFORE + 1)) "$LOG" | grep -c "region redraw")
}

# --- Phase 1: margins granted - real native scrolling region expected,
# image never retransmitted. ---
$TMUX set -as terminal-features ',*:margins' || fail "grant margins failed"
sleep 0.3
$TMUX display-message -p '#{client_termfeatures}' |
	grep -q imagescroll || fail "sanity: imagescroll was not granted to the client"

run_scroll "phase 1 (margins granted)"
[ "$n_dcs" -eq 0 ] ||
	fail "phase 1: image was retransmitted ($n_dcs times) - imagescroll's trust did not engage, so this run cannot test what it is meant to"
[ "$n_redraw" -eq 0 ] ||
	fail "phase 1: tty_redraw_region() fired ($n_redraw times) while scrolling an image-bearing pane with margins+imagescroll granted - tmux fell back to a full manual repaint instead of a real native scroll, so trusting the terminal to have moved the image was never justified"

# --- Phase 2: margins revoked - the pane still isn't full width
# (pane-scrollbars is on), so this is the scenario PR review's "not using
# scrolling regions" concern actually described: no native scrolling-region
# option is available, tty_redraw_region() must fire, and
# redraw_image_scroll_result()'s failure path must in turn stop
# imagescroll's trust from applying, so the image is retransmitted instead
# of silently lost. Resend the image first - phase 1's scrolling already
# pushed the original one (26 pixels tall, a couple of rows at most) out of
# the pane, so without this there would be nothing left to retransmit
# either way, and the check below would pass for the wrong reason. ---
$TMUX set -as terminal-features ',*:margins@' || fail "revoke margins failed"
sleep 0.3
: >"$TMP"
$TMUX send-keys -t inner -l "printf '$HEADER'" || fail "resend image failed"
$TMUX send-keys -t inner Enter || fail "send enter failed"
sleep 1.5
grep -qa '"1;1;26;26' "$TMP" || fail "sanity: image did not reappear before phase 2"

run_scroll "phase 2 (margins revoked)"
[ "$n_redraw" -gt 0 ] ||
	fail "sanity: without margins, scrolling a scrollbar-enabled image-bearing pane never fell back to tty_redraw_region() - this scenario no longer exercises the bug phase 1 checks for"
[ "$n_dcs" -gt 0 ] ||
	fail "phase 2: image was not retransmitted after tty_redraw_region() fired - imagescroll's trust incorrectly stayed engaged despite the fallback, which would silently lose the image"

exit 0
