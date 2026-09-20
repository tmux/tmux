#!/bin/sh

# Regression test for the sixel-region-scrolling option (options-table.c):
# scrolling a pane that has a SIXEL image in it must not retransmit the
# image when the option is on - tmux trusts the terminal to have moved the
# image along with the rest of the scrolling region, the same way it
# already trusts Kitty placements to move themselves (IMAGE_BACKEND_SCROLLS
# in image.c). With the option off, today's always-redraw-on-scroll
# behaviour is unchanged.
#
# There is no way to query a terminal for whether it actually moves SIXEL
# pixels along with a scroll, so this only proves tmux's own decision to
# skip or redraw is wired correctly - not that any particular terminal
# renders the result correctly. That needs a human, on real terminals.

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

HEADER='\033Pq"1;1;26;26#0;2;100;100;100#0!26~-!26~-!26~-!26~-!26B\033\\'

# A small image near the top of a tall pane, then enough plain output below
# it to force several ordinary (no DECSTBM sub-region) linefeed scrolls,
# each one shifting the image rows along with everything else.
$TMUX new-session -d -s inner -x 40 -y 20 "printf '$HEADER'; exec sh" ||
	exit 1
sleep 0.3

[ "$($TMUX display-message -p '#{image_support}')" = 0 ] && exit 0
$TMUX set -as terminal-features ',*:sixel' || exit 1

$TMUX2 new-session -d -x 40 -y 20 || exit 1
OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
[ -n "$OUTER" ] || fail "No outer pane."
$TMUX2 set -as terminal-features ',*:sixel@' || fail "disable outer sixel failed"
$TMUX2 pipe-pane -t "$OUTER" -O "cat >$TMP" || fail "pipe-pane failed"
$TMUX2 send-keys -t "$OUTER" -l "$TMUX attach -t inner" || fail "send attach failed"
$TMUX2 send-keys -t "$OUTER" Enter || fail "send enter failed"
sleep 2

grep -qa '"1;1;26;26' "$TMP" || fail "sanity: image never reached the client"

# --- Phase 1: sixel-region-scrolling on (the default) - expect no DCS. ---
$TMUX set -s sixel-region-scrolling on || fail "set option on failed"
sleep 0.5
: >"$TMP"
$TMUX send-keys -t inner -l "yes | head -n 30" || fail "send scroll failed"
$TMUX send-keys -t inner Enter || fail "send enter failed"
sleep 1

n_on=$(grep -ac "$(printf '\033P')" "$TMP")
[ "$n_on" -eq 0 ] ||
	fail "image was retransmitted ($n_on times) scrolling with sixel-region-scrolling on"

# --- Phase 2: sixel-region-scrolling off - expect the image back, then
# more scrolling to redraw it. ---
$TMUX send-keys -t inner -l "printf '$HEADER'" || fail "resend image failed"
$TMUX send-keys -t inner Enter || fail "send enter failed"
sleep 1.5
grep -qa '"1;1;26;26' "$TMP" || fail "sanity: image did not reappear before phase 2"

$TMUX set -s sixel-region-scrolling off || fail "set option off failed"
sleep 0.5
: >"$TMP"
$TMUX send-keys -t inner -l "yes | head -n 30" || fail "send scroll failed"
$TMUX send-keys -t inner Enter || fail "send enter failed"
sleep 1

n_off=$(grep -ac "$(printf '\033P')" "$TMP")
[ "$n_off" -gt 0 ] ||
	fail "image was not retransmitted scrolling with sixel-region-scrolling off - expected the old always-redraw behaviour"

exit 0
