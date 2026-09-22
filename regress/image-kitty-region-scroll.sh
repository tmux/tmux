#!/bin/sh

# Regression test for the image-region-scrolling option (options-table.c),
# Kitty side - see image-sixel-region-scroll.sh for the SIXEL side of the
# same option.
#
# The Kitty backend used to hardcode IMAGE_BACKEND_SCROLLS unconditionally
# (image.c), so tmux always trusted the terminal to have moved a Kitty
# placement along with the rest of a scrolling region, with no way to turn
# that assumption off. At least Windows Terminal does not actually do
# this: it drops the placement entirely on a plain newline- or
# reverse-index-driven scroll (though not on a delete-line/insert-line
# scroll simulation, which goes through a different, always-redraw path).
# image-region-scrolling now gates Kitty exactly like SIXEL: scrolling
# must not retransmit the placement when the option is on (the previously
# unconditional behaviour), and must retransmit it when off (the escape
# hatch this test exists to prove is actually wired up for Kitty too).

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

IMAGEID=424242
HEADER=$(printf '\033_Ga=T,q=2,f=32,s=1,v=1,i=%s;/wAA/w==\033\\' "$IMAGEID")

# A small image near the top of a tall pane, then enough plain output below
# it to force several ordinary (no DECSTBM sub-region) linefeed scrolls,
# each one shifting the image rows along with everything else.
$TMUX new-session -d -s inner -x 40 -y 20 "printf '$HEADER'; exec sh" ||
	exit 1
sleep 0.3

[ "$($TMUX display-message -p '#{image_support}')" = 0 ] && exit 0
$TMUX set -as terminal-features ',*:kitty' || exit 1

$TMUX2 new-session -d -x 40 -y 20 || exit 1
OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
[ -n "$OUTER" ] || fail "No outer pane."
$TMUX2 set -as terminal-features ',*:kitty@' || fail "disable outer kitty failed"
$TMUX2 pipe-pane -t "$OUTER" -O "cat >$TMP" || fail "pipe-pane failed"
$TMUX2 send-keys -t "$OUTER" -l "$TMUX attach -t inner" || fail "send attach failed"
$TMUX2 send-keys -t "$OUTER" Enter || fail "send enter failed"
sleep 2

grep -qa "$(printf '\033_Ga=p')" "$TMP" || fail "sanity: image never reached the client"

# --- Phase 1: image-region-scrolling on (the default) - expect no APC. ---
$TMUX set -s image-region-scrolling on || fail "set option on failed"
sleep 0.5
: >"$TMP"
$TMUX send-keys -t inner -l "yes | head -n 30" || fail "send scroll failed"
$TMUX send-keys -t inner Enter || fail "send enter failed"
sleep 1

n_on=$(grep -ac "$(printf '\033_Ga=p')" "$TMP")
[ "$n_on" -eq 0 ] ||
	fail "image was retransmitted ($n_on times) scrolling with image-region-scrolling on"

# --- Phase 2: image-region-scrolling off - expect the image back, then
# more scrolling to redraw it. ---
$TMUX send-keys -t inner -l "printf '$HEADER'" || fail "resend image failed"
$TMUX send-keys -t inner Enter || fail "send enter failed"
sleep 1.5
grep -qa "$(printf '\033_Ga=p')" "$TMP" || fail "sanity: image did not reappear before phase 2"

$TMUX set -s image-region-scrolling off || fail "set option off failed"
sleep 0.5
: >"$TMP"
$TMUX send-keys -t inner -l "yes | head -n 30" || fail "send scroll failed"
$TMUX send-keys -t inner Enter || fail "send enter failed"
sleep 1

n_off=$(grep -ac "$(printf '\033_Ga=p')" "$TMP")
[ "$n_off" -gt 0 ] ||
	fail "image was not retransmitted scrolling with image-region-scrolling off - expected the old always-redraw behaviour"

exit 0
