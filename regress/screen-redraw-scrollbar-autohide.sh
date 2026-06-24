#!/bin/sh

# Exercise auto-hide pane scrollbars using a nested tmux client, matching the
# other screen-redraw tests. Hidden auto-hide scrollbars must not resize panes;
# copy-mode scrolling must reveal them; the timeout must hide them again.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
TMUX2="$TEST_TMUX -Ltest2 -f/dev/null"

TMP=$(mktemp)
trap "rm -f $TMP; $TMUX kill-server 2>/dev/null; $TMUX2 kill-server 2>/dev/null" \
	0 1 15

fail() {
	echo "$*" >&2
	exit 1
}

has_scrollbar_style() {
	$TMUX capturep -pe >$TMP || exit 1
	grep "$(printf '\033\\[41m')" "$TMP" >/dev/null 2>&1
}

$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

$TMUX2 new -d -x20 -y6 "sh -c 'seq 40; exec sleep 100'" || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 set -g window-size manual || exit 1
$TMUX2 setw pane-scrollbars auto-hide || exit 1
$TMUX2 setw pane-scrollbars-timeout 2000 || exit 1
$TMUX2 setw pane-scrollbars-style "bg=red,fg=white,width=1,pad=1" || exit 1

$TMUX new -d -x20 -y6 || exit 1
$TMUX set -g status off || exit 1
$TMUX set -g window-size manual || exit 1
$TMUX set -g default-terminal "tmux-256color" || exit 1
$TMUX send -l "$TMUX2 attach" || exit 1
$TMUX send Enter || exit 1
sleep 2

width=$($TMUX2 display -p '#{pane_width}') || exit 1
[ "$width" = 20 ] || fail "auto-hide scrollbar changed pane width to $width"

if has_scrollbar_style; then
	fail "auto-hide scrollbar visible before reveal"
fi

$TMUX2 copy-mode -H || exit 1
$TMUX2 send -X history-top || exit 1
sleep 1

if ! has_scrollbar_style; then
	fail "auto-hide scrollbar not visible after copy-mode scroll"
fi

sleep 2
if has_scrollbar_style; then
	fail "auto-hide scrollbar still visible after timeout"
fi

$TMUX2 setw pane-scrollbars modal || exit 1
$TMUX2 send -X cancel || exit 1
sleep 1

width=$($TMUX2 display -p '#{pane_width}') || exit 1
[ "$width" = 20 ] || fail "modal scrollbar changed pane width to $width"

if has_scrollbar_style; then
	fail "modal scrollbar visible outside copy mode"
fi

$TMUX2 copy-mode -H || exit 1
$TMUX2 send -X history-top || exit 1
sleep 1

if ! has_scrollbar_style; then
	fail "modal scrollbar not visible after copy-mode scroll"
fi

sleep 2
if has_scrollbar_style; then
	fail "modal scrollbar still visible after timeout"
fi

exit 0
