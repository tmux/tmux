#!/bin/sh

# A SIXEL image drawn over an earlier one replaces the pixels underneath
# for good, so an earlier image that is completely covered must not be kept
# and drawn again on every repaint - an animation would otherwise pile up a
# layer per frame.
#
# Draw two images over each other, then attach a client: it should be sent
# no more than it would be for one image. An image that is only partly
# covered must survive, so a second case draws a narrower image over a wider
# one and expects both.

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

TMP=$(mktemp)
trap "cleanup; rm -f $TMP" 0 1 15

# Draw the given sixel commands in a new session, attach a sixel client and
# set N to the number of SIXEL images the client was sent (other DCS
# sequences, such as capability queries, do not count).
count_images()
{
	cleanup
	$TMUX new-session -d -s inner -x 40 -y 10 "printf '$1'; exec sh" ||
		exit 1
	sleep 0.3
	[ "$($TMUX display-message -p '#{image_support}')" = 0 ] && exit 0
	$TMUX set -as terminal-features ',*:sixel' || exit 1

	$TMUX2 new-session -d -x 40 -y 10 || exit 1
	OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
	[ -n "$OUTER" ] || fail "No outer pane."
	$TMUX2 set -as terminal-features ',*:sixel@' ||
	    fail "disable outer sixel failed"
	: >"$TMP"
	$TMUX2 pipe-pane -t "$OUTER" -O "cat >$TMP" || fail "pipe-pane failed"
	$TMUX2 send-keys -t "$OUTER" -l "$TMUX attach -t inner" ||
	    fail "send attach failed"
	$TMUX2 send-keys -t "$OUTER" Enter || fail "send enter failed"
	sleep 2
	N=$(grep -aoE "$(printf '\033')P[0-9;]*q" "$TMP" | wc -l)
}

# One image on its own, as a baseline: a client is sent each image more than
# once, as the terminal features are confirmed after attaching.
count_images '\0337\033P9;1q#1!100~\033\\'
SINGLE=$N
[ "$SINGLE" -gt 0 ] || fail "sanity: client was sent no images"

# The same footprint twice, blue then red.
count_images '\0337\033P9;1q#1!100~\033\\\0338\033P9;1q#2!100~\033\\'
[ "$N" -eq "$SINGLE" ] ||
	fail "covered image was kept: client was sent $N images, expected $SINGLE"

# A narrower image over a wider one leaves the wider one partly visible.
count_images '\0337\033P9;1q#1!100~\033\\\0338\033P9;1q#2!40~\033\\'
[ "$N" -gt "$SINGLE" ] ||
	fail "partly covered image was lost: client was sent $N images, expected more than $SINGLE"

exit 0
