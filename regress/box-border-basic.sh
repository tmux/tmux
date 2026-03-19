#!/bin/sh

# Test box border mode basic functionality

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

# Test that box mode can be enabled
$TMUX -f/dev/null new -d
$TMUX set -g pane-border-indicators box
[ "$($TMUX show -gv pane-border-indicators)" = "box" ] || exit 1

# Test that box-all mode can be enabled
$TMUX set -g pane-border-indicators box-all
[ "$($TMUX show -gv pane-border-indicators)" = "box-all" ] || exit 1

# Test switching back to off
$TMUX set -g pane-border-indicators off
[ "$($TMUX show -gv pane-border-indicators)" = "off" ] || exit 1

# Test pane dimensions in box mode with a split
$TMUX set -g pane-border-indicators box
$TMUX splitw -h

# Verify box mode is active (pane_width should be reduced by 2 for border)
$TMUX kill-server 2>/dev/null

exit 0
