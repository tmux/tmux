#!/bin/sh

# When the client's terminal has the imagescroll feature, tmux trusts it to
# have moved everything in the scrolled region - text and image alike - so
# a scroll must not cause any existing text to be redrawn. It used to skip
# only the image phases of the damage redraw and still redraw all of the
# text around them.
#
# Without imagescroll the images must be redrawn, and the text around them
# is redrawn too, so the same scroll must resend it.
#
# The feature only takes effect for a client attached after it is set, so
# each case starts a new server and client.

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

HEADER=$(printf '\033_Ga=T,q=2,f=32,s=1,v=1,i=424243;/wAA/w==\033\\')

# Scroll a pane containing an image and marker text on a client with the
# given terminal features, and set N to the number of marker lines sent to
# the terminal.
scroll_markers()
{
	cleanup

	# Marker text around an image, filling the pane so that the next
	# newlines scroll it while the image stays in view.
	$TMUX new-session -d -s inner -x 40 -y 20 \
	    "i=0; while [ \$i -lt 5 ]; do printf 'MARKER%02d\n' \$i; i=\$((i + 1)); done; printf '$HEADER'; while [ \$i -lt 17 ]; do printf 'MARKER%02d\n' \$i; i=\$((i + 1)); done; exec sh" ||
		exit 1
	sleep 0.3
	[ "$($TMUX display-message -p '#{image_support}')" = 0 ] && exit 0
	$TMUX set -as terminal-features "$1" || exit 1

	$TMUX2 new-session -d -x 40 -y 20 || exit 1
	OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
	[ -n "$OUTER" ] || fail "No outer pane."
	$TMUX2 set -as terminal-features ',*:kitty@' ||
	    fail "disable outer kitty failed"
	: >"$TMP"
	$TMUX2 pipe-pane -t "$OUTER" -O "cat >$TMP" || fail "pipe-pane failed"
	$TMUX2 send-keys -t "$OUTER" -l "$TMUX attach -t inner" ||
	    fail "send attach failed"
	$TMUX2 send-keys -t "$OUTER" Enter || fail "send enter failed"
	sleep 2
	grep -qa "$(printf '\033_Ga=p')" "$TMP" ||
	    fail "sanity: image never reached the client"

	: >"$TMP"
	$TMUX send-keys -t inner -l "printf '\n\n\n'" ||
	    fail "send scroll failed"
	$TMUX send-keys -t inner Enter || fail "send enter failed"
	sleep 1
	N=$(grep -ac MARKER "$TMP")
}

scroll_markers ',*:kitty,*:imagescroll'
[ "$N" -eq 0 ] ||
	fail "existing text was redrawn ($N lines) scrolling with imagescroll"

scroll_markers ',*:kitty'
[ "$N" -gt 0 ] ||
	fail "text was not redrawn scrolling without imagescroll"

exit 0
