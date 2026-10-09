#!/bin/sh

# Detached sessions with window-size manual should use the requested size.

PATH=/bin:/usr/bin
TERM=screen
export TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestM$$-1 -f/dev/null"

TMP=$(mktemp) || exit 1
CFG=$(mktemp) || { rm -f "$TMP"; exit 1; }
trap 'rm -f "$TMP" "$CFG"; $TMUX kill-server 2>/dev/null' 0
trap 'exit 1' 1 2 15

# Default fallback.
printf '%s\n' 'set -gw window-size manual' >"$CFG"
$TMUX -f "$CFG" new -d </dev/null || exit 1
sleep 1
$TMUX ls -F '#{window_width} #{window_height}' >"$TMP" || exit 1
printf '80 24\n'|cmp -s "$TMP" - || exit 1
$TMUX kill-server 2>/dev/null

# Custom default-size.
TMUX="$TEST_TMUX -LtestM$$-2 -f/dev/null"
printf '%s\n' 'set -gw window-size manual' \
	'set -g default-size 130x50' >"$CFG"
$TMUX -f "$CFG" new -d </dev/null || exit 1
sleep 1
$TMUX ls -F '#{window_width} #{window_height}' >"$TMP" || exit 1
printf '130 50\n'|cmp -s "$TMP" - || exit 1
$TMUX kill-server 2>/dev/null

# Explicit size overrides default-size.
TMUX="$TEST_TMUX -LtestM$$-3 -f/dev/null"
printf '%s\n' 'set -gw window-size manual' >"$CFG"
$TMUX -f "$CFG" new -d -x 100 -y 40 </dev/null || exit 1
sleep 1
$TMUX ls -F '#{window_width} #{window_height}' >"$TMP" || exit 1
printf '100 40\n'|cmp -s "$TMP" - || exit 1
$TMUX kill-server 2>/dev/null

exit 0
