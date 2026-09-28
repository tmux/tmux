#!/bin/sh

# A SIXEL emitted immediately after creating a floating pane must not erase
# pane-border-status text. Exercise the same queued new; newp; send-keys path
# used by the reported reproduction, then repeat it with two tiled panes.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Libss$$ -f/dev/null"
TMUX2="$TEST_TMUX -Libss-outer$$ -f/dev/null"
FIXTURE=$(pwd)/monkey-2.sixel.txt

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
expect_title()
{
	if ! grep -q "$1" "$TMP"; then
		missing_titles="$missing_titles $2"
	fi
}

cleanup
TMP=$(mktemp)
missing_titles=
trap "cleanup; rm -f $TMP" 0 1 15

$TMUX new-session -d -s inner -x 100 -y 30 sh || fail "new session failed"
[ "$($TMUX display-message -p '#{image_support}')" = 0 ] && exit 0
$TMUX set -g status off
$TMUX set -g pane-border-status top
$TMUX set -g pane-border-format '#{pane_title}'
$TMUX set -as terminal-features ',*:sixel'
BASE=$($TMUX list-panes -F '#{pane_id}' | head -1)
$TMUX select-pane -t "$BASE" -T TILED0 || fail "set tiled title failed"

$TMUX2 new-session -d -x 100 -y 30 "$TMUX attach -t inner" ||
	fail "outer session failed"
sleep 1
OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
$TMUX2 capture-pane -p -t "$OUTER" >$TMP || fail "initial capture failed"
grep -q TILED0 $TMP || fail "initial tiled title not visible"

# Keep new-pane and send-keys in one command queue. send-keys targets the new
# active floating pane, exactly as in: new\; newp\; send-keys "cat ..." enter.
$TMUX new-pane -T FLOAT0 sh \; send-keys "cat '$FIXTURE'" Enter ||
	fail "queued floating image command failed"
sleep 1
$TMUX2 capture-pane -p -t "$OUTER" >$TMP || fail "capture failed"
expect_title TILED0 tiled-after-sixel
expect_title FLOAT0 floating-after-sixel

# Add another tiled pane, then create another floating pane and immediately
# output the same SIXEL. Every tiled and floating border-status must remain.
$TMUX select-pane -t "$BASE"
TILED1=$($TMUX split-window -h -d -PF '#{pane_id}' sh) ||
	fail "split tiled pane failed"
$TMUX select-pane -t "$TILED1" -T TILED1 || fail "set second tiled title failed"
$TMUX new-pane -T FLOAT1 sh \; send-keys "cat '$FIXTURE'" Enter ||
	fail "queued second floating image command failed"
sleep 1
$TMUX2 capture-pane -p -t "$OUTER" >$TMP || fail "capture failed"
expect_title TILED0 first-tiled-after-split-sixel
expect_title TILED1 second-tiled-after-split-sixel
expect_title FLOAT1 floating-after-split-sixel
[ -z "$missing_titles" ] || fail "missing pane-border-status titles:$missing_titles"

exit 0
