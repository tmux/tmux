#!/bin/sh

# swap-pane refusing a floating pane must leave the zoom alone

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
trap "rm -f $TMP; $TMUX kill-server 2>/dev/null" 0 1 15

$TMUX new -d
$TMUX splitw -d
$TMUX new-pane -d

echo $($TMUX lsp -F'#{pane_id}:#{pane_floating_flag}') >$TMP
(echo "%0:0 %1:0 %2:1"|cmp -s - $TMP) || exit 1

$TMUX resize-pane -Z -t %0
[ "$($TMUX display -p '#{window_zoomed_flag}')" = 1 ] || exit 1

$TMUX swap-pane -Z -D -t %2
[ "$?" = 0 ] && exit 1
[ "$($TMUX display -p '#{window_zoomed_flag}')" = 1 ] || exit 1

$TMUX swap-pane -Z -U -t %2
[ "$?" = 0 ] && exit 1
[ "$($TMUX display -p '#{window_zoomed_flag}')" = 1 ] || exit 1

# A swap that is allowed still happens.
$TMUX swap-pane -D -t %0 || exit 1
echo $($TMUX lsp -F'#{pane_id}') >$TMP
(echo "%1 %0 %2"|cmp -s - $TMP) || exit 1

$TMUX kill-server 2>/dev/null

exit 0
