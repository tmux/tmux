#!/bin/sh

# Hooks run stored command trees.
#
# A hook stores a parsed command tree that is invoked through cmd_invoke_get when
# the hook fires. Covers a single-command hook, a multi-entry hook array (every
# entry fires), and that show-hooks prints the stored commands back.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
$TMUX kill-server 2>/dev/null

$TMUX -f/dev/null start \; new-session -d -sfirst -nbase 2>/dev/null || exit 1

# Single-command hook fires on session creation.
$TMUX set-hook -g session-created 'set -g @hook done' || exit 1
$TMUX new-session -d -ssecond || exit 1
[ "$($TMUX show -gv @hook)" = done ] || {
	echo "single-command hook did not fire" >&2
	exit 1
}

# A hook array fires every entry.
$TMUX set-hook -g session-created[10] 'new-window -dn H1' || exit 1
$TMUX set-hook -g session-created[11] 'set -g @hook2 two' || exit 1
$TMUX new-session -d -sthird || exit 1
[ "$($TMUX show -gv @hook2)" = two ] || {
	echo "hook array entry 11 did not fire" >&2
	exit 1
}
echo "$($TMUX list-windows -t third -F '#{window_name}')" | grep -qx H1 || {
	echo "hook array entry 10 did not create window" >&2
	exit 1
}

# show-hooks prints the stored hook commands.
$TMUX show-hooks -g | grep -q 'session-created.* set -g @hook done' || {
	echo "show-hooks did not print stored hook command" >&2
	exit 1
}

$TMUX kill-server 2>/dev/null
exit 0
