#!/bin/sh

# display-message -c must take the client formats from the given client, even
# when it is attached to a different session from the target pane.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
INNER="$TEST_TMUX -LtestI$$ -f/dev/null"
OUTER="$TEST_TMUX -LtestO$$ -f/dev/null"

fail()
{
	echo "$*" >&2
	exit 1
}

cleanup()
{
	$OUTER kill-server 2>/dev/null
	$INNER kill-server 2>/dev/null
}
trap cleanup 0 1 15

cleanup
$INNER new-session -d -s one -x40 -y10 'exec sleep 100' || exit 1
$INNER new-session -d -s two -x40 -y10 'exec sleep 100' || exit 1
$INNER set-option -g status off || exit 1

# Each client has a distinct TERM so that client_termname identifies it.
attach()
{
	echo "env -i PATH=/bin:/usr/bin TERM=$1 LC_ALL=C.UTF-8 $TEST_TMUX -LtestI$$ -f/dev/null attach-session -t $2"
}
$OUTER new-session -d -s outer -x40 -y10 "$(attach screen one)" || exit 1
$OUTER new-window -d "$(attach xterm two)" || exit 1
$OUTER set-option -g status off || exit 1

i=0
while [ "$i" -lt 50 ]; do
	n=$($INNER list-clients -F '#{client_name}' 2>/dev/null|wc -l)
	[ "$n" -eq 2 ] && break
	sleep 0.1
	i=$((i + 1))
done
[ "$i" -lt 50 ] || fail "clients did not attach"

c1=$($INNER list-clients -F '#{client_name}' -f '#{==:#{client_termname},screen}')
c2=$($INNER list-clients -F '#{client_name}' -f '#{==:#{client_termname},xterm}')
[ -n "$c1" ] && [ -n "$c2" ] || fail "could not find both clients"

# The target is in session two, but the client is the one in session one.
term=$($INNER display-message -c "$c1" -t two: -p '#{client_termname}')
[ "$term" = screen ] || fail "client_termname was '$term', not screen"

# The session formats still come from the target.
session=$($INNER display-message -c "$c1" -t two: -p '#{session_name}')
[ "$session" = two ] || fail "session_name was '$session', not two"

term=$($INNER display-message -c "$c2" -t one: -p '#{client_termname}')
[ "$term" = xterm ] || fail "client_termname was '$term', not xterm"

exit 0
