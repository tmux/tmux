#!/bin/sh

# copy-mode -S can be called from a hook without an attached client.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
OUT=$(mktemp -d) || exit 1
TMUX_TMPDIR=$OUT
export TMUX_TMPDIR
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"

fail()
{
	echo "$*" >&2
	exit 1
}

cleanup()
{
	$TMUX kill-server 2>/dev/null || true
	rm -rf "$OUT"
}
trap cleanup EXIT
trap 'exit 1' 1 2 3 15

$TMUX new-session -d -s test 'sleep 300' || fail "new-session failed"
$TMUX set -g remain-on-exit on || fail "setting remain-on-exit failed"
$TMUX set-hook -g pane-died 'copy-mode -S' ||
	fail "setting pane-died hook failed"
$TMUX respawn-pane -k -t test:0 'exit 0' || fail "respawn-pane failed"

i=0
while [ "$i" -lt 50 ]; do
	$TMUX has-session -t test 2>/dev/null || \
		fail "server died in copy-mode -S"
	[ "$($TMUX display-message -p -t test:0 '#{pane_dead}' 2>/dev/null)" = 1 ] && \
		break
	sleep 0.1
	i=$((i + 1))
done
[ "$i" -lt 50 ] || fail "pane-died hook was not triggered"
sleep 0.5
$TMUX has-session -t test || fail "server died after copy-mode -S"

exit 0
