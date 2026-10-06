#!/bin/sh

# Auto-hide scrollbars appear on activity or pointer hover, then disappear
# after pane-scrollbars-timeout. Compare captures of the same copy-mode view so
# only the overlay scrollbar changes.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
OUTER="$TEST_TMUX -LtestA$$ -f/dev/null"
INNER="$TEST_TMUX -LtestB$$ -f/dev/null"
DIR=$(mktemp -d) || exit 1
HIDDEN=$DIR/hidden
VISIBLE=$DIR/visible
AFTER=$DIR/after

fail()
{
	echo "$*" >&2
	exit 1
}

cleanup()
{
	$OUTER kill-server 2>/dev/null
	$INNER kill-server 2>/dev/null
	rm -rf "$DIR"
}
trap cleanup 0 1 15

capture()
{
	$OUTER capture-pane -pe -t outer:0.0 >"$1" || exit 1
}

wait_for_client()
{
	i=0
	while [ "$i" -lt 50 ]; do
		$INNER list-clients 2>/dev/null | grep -q . && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "inner client did not attach"
}

$INNER new-session -d -s inner -x40 -y12 \
	"sh -c 'seq 50; exec sleep 100'" || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1
$INNER set-option -g mouse on || exit 1
$INNER set-window-option pane-scrollbars auto-hide || exit 1
$INNER set-window-option pane-scrollbars-timeout 3000 || exit 1
$INNER set-window-option pane-scrollbars-style \
	'bg=colour196,fg=colour231,width=1,pad=0' || exit 1

$OUTER new-session -d -s outer -x40 -y12 "$INNER attach -t inner" || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
wait_for_client

# Enter copy mode and choose a stable view. The initial show timer is allowed
# to expire before taking the hidden reference capture.
$INNER copy-mode -H || exit 1
$INNER send-keys -X history-top || exit 1
sleep 3.5
capture "$HIDDEN"

# A copy-mode page movement shows the overlay and starts its timer. Capture
# that view immediately, then again after the timeout without moving it.
$INNER send-keys -X page-down || exit 1
sleep 0.1
capture "$VISIBLE"
sleep 3.5
capture "$AFTER"
cmp -s "$VISIBLE" "$AFTER" && fail "scrollbar did not hide after timeout"

# Use the post-movement hidden scene as the reference for pointer hover. SGR
# mouse column 40 is the right-hand scrollbar; moving to column 5 restarts the
# timer without changing the copy-mode view.
cp "$AFTER" "$HIDDEN"
hover=$(printf '\033[<35;40;5M')
$OUTER send-keys -l "$hover" || exit 1
sleep 0.1
capture "$VISIBLE"
cmp -s "$HIDDEN" "$VISIBLE" && fail "scrollbar did not appear on hover"
away=$(printf '\033[<35;5;5M')
$OUTER send-keys -l "$away" || exit 1
sleep 3.5
capture "$AFTER"
cmp -s "$HIDDEN" "$AFTER" || fail "scrollbar did not hide after hover"

exit 0
