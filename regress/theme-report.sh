#!/bin/sh

# Test that the theme reported to a pane (mode 2031 / DSR 996) follows the
# terminal's reported theme, not a guess from the background colour.
#
# An inner client is attached inside an outer tmux pane. The outer pane has a
# light background, so the outer server answers the inner client's DSR 996 with
# light. The inner server is given a dark window background, so a guess from the
# background colour would say dark. A pane in the inner server then queries DSR
# 996: the answer must be light (2), following the client's reported theme, not
# dark (1) from the background.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
INNER="$TEST_TMUX -LtestI$$ -f/dev/null"
OUTER="$TEST_TMUX -LtestO$$ -f/dev/null"
$INNER kill-server 2>/dev/null
$OUTER kill-server 2>/dev/null

TMP=$(mktemp)
trap "rm -f $TMP; $INNER kill-server 2>/dev/null; $OUTER kill-server 2>/dev/null" \
	0 1 15

# Inner server, dark background, keep panes alive across the query respawn.
$INNER new-session -d -x80 -y24 || exit 1
$INNER set -g remain-on-exit on
$INNER set -g window-style 'bg=black'

# Outer server with a light background, running the inner client attached so the
# inner client has a real terminal that reports a theme.
$OUTER new-session -d -x80 -y24 || exit 1
$OUTER set -g window-style 'bg=white'
$OUTER new-window "$INNER attach" || exit 1
sleep 2

# Query DSR 996 from an inner pane and capture the CSI ? 997 ; Ps n reply.
$INNER respawnw -k -t:0 -- sh -c "
	exec 2>/dev/null
	stty raw -echo
	printf '\033[?996n'
	dd bs=1 count=9 2>/dev/null | cat -v > $TMP
	sleep 1
"
sleep 2

actual=$(cat "$TMP")
expected='^[[?997;2n'   # 2 = light (from the terminal), not 1 = dark (from bg)

if [ "$actual" = "$expected" ]; then
	[ -n "$VERBOSE" ] && echo "[PASS] reported terminal theme ($actual)"
	exit 0
fi

echo "[FAIL] expected '$expected' (light, from terminal), got '$actual'"
exit 1
