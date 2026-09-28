#!/bin/sh

# link-window -k clears the current window before window-linked is fired.
# Expanding active_window_index and session_stack from that event must not
# kill the server.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
trap '$TMUX kill-server 2>/dev/null' 0
trap 'exit 1' 1 2 15

$TMUX new-session -d -s a -n one
$TMUX new-window -d -t a: -n two

$TMUX wait-for -E -F '#{?#{S:#{active_window_index}#{session_stack}},0,1}' window-linked &
pid=$!
i=0
while [ $i -lt 50 ]; do
	$TMUX wait-for -E -l window-linked | grep -q . && break
	i=$((i + 1))
	sleep 0.1
done
if [ $i -eq 50 ]; then
	kill $pid 2>/dev/null
	echo "waiter did not register" >&2
	exit 1
fi

$TMUX link-window -d -k -s a:1 -t a:0 || exit 1
wait $pid || exit 1

out=$($TMUX list-sessions -F '#{active_window_index}')
[ "$out" = 0 ] || {
	echo "active_window_index was '$out'" >&2
	exit 1
}
