#!/bin/sh

# Leaving a modal window must not finish its command before the pane exits.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"
. ./regress-common.inc
TESTDIR=$(mktemp -d) || exit 1
GATE=$TESTDIR/gate
READY=$TESTDIR/ready
OUT=$TESTDIR/status

stop()
{
	stop_servers
}

cleanup()
{
	stop
	rm -f "$GATE" "$READY" "$OUT"
	rmdir "$TESTDIR"
}
trap cleanup 0 1 15

fail()
{
	echo "$*" >&2
	exit 1
}

wait_for()
{
	i=0
	while ! "$@"; do
		[ "$i" -eq 50 ] && fail "timed out: $*"
		i=$((i + 1))
		sleep 0.1
	done
}

option_is()
{
	[ "$($TMUX show -gqv "$1")" = "$2" ]
}

pane_contains()
{
	case "$($TMUX capture-pane -p -t "$1")" in
	*"$2"*) return 0 ;;
	esac
	return 1
}

setup()
{
	stop
	rm -f "$GATE" "$READY" "$OUT"
	$TMUX new-session -d -s s -x 80 -y 24 'cat' || exit 1
	$TMUX set -g default-shell /bin/sh
	$TMUX set -g mouse on
	$TMUX new-window -d -t s:1 'cat'
	$TMUX new-session -d -s other 'cat'
	$TMUX2 new-session -d -x 80 -y 25 "$TMUX attach -t s" || exit 1
	$TMUX2 set -g status off
	wait_for attached
	OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
	OTHER=$($TMUX display-message -p -t other:0 '#{pane_id}')
	PROGRAM="printf POPUP-HERE; touch '$READY'; while [ ! -e '$GATE' ]; do sleep 0.1; done"
}

attached()
{
	CLIENT=$($TMUX list-clients -F '#{client_name}' | head -1)
	[ -n "$CLIENT" ]
}

leave()
{
	case "$1" in
	window) $TMUX select-window -t s:1 ;;
	session) $TMUX switch-client -c "$CLIENT" -t other ;;
	detach) $TMUX detach-client -t "$CLIENT" ;;
	terminal) $TMUX2 kill-pane -t "$OUTER" ;;
	esac
}

# Independent command clients must keep waiting, including their exit status.
for kind in popup modal plain; do
	for action in window session detach terminal; do
		setup
		case "$kind" in
		popup)
			($TMUX display-popup -c "$CLIENT" -E "$PROGRAM; exit 7"; echo $? >"$OUT") &
			;;
		modal)
			($TMUX new-pane -O -W -t s:0 "$PROGRAM; exit 7"; echo $? >"$OUT") &
			;;
		plain)
			($TMUX new-pane -W -t s:0 "$PROGRAM; exit 7"; echo $? >"$OUT") &
			;;
		esac
		waiter=$!
		wait_for test -f "$READY"
		leave "$action"
		sleep 0.2
		[ ! -s "$OUT" ] || fail "$kind/$action returned before its pane exited"
		touch "$GATE"
		wait_for test -s "$OUT"
		wait "$waiter"
		[ "$(cat "$OUT")" = 7 ] || fail "$kind/$action lost exit status 7"
	done
done

# Preserve binding sequences and run-shell waits after changing the view.
for kind in binding script; do
	for action in window session; do
		setup
		if [ "$kind" = binding ]; then
			$TMUX bind P display-popup -E "$PROGRAM" '\;' set -g @after yes
		else
			$TMUX bind P run-shell \
			    "$TMUX display-popup -E \"$PROGRAM; exit 7\"; echo \$? >'$OUT'; exit 0" \
			    '\;' set -g @after yes
		fi
		$TMUX2 send-keys -t "$OUTER" C-b P
		wait_for test -f "$READY"
		leave "$action"
		sleep 0.2
		option_is @after '' || fail "$kind/$action ran its following command early"
		[ ! -s "$OUT" ] || fail "$kind/$action finished its script early"
		touch "$GATE"
		wait_for option_is @after yes
		if [ "$kind" = script ]; then
			[ "$(cat "$OUT")" = 7 ] || fail "$kind/$action lost exit status 7"
		fi
	done
done

# A client showing a different session must never inherit the modal wait.
setup
$TMUX bind P display-popup -E "$PROGRAM"
$TMUX2 send-keys -t "$OUTER" C-b P
wait_for test -f "$READY"
SECOND=$($TMUX2 new-window -PF '#{pane_id}' "$TMUX attach -t other")
sleep 0.2
$TMUX2 send-keys -t "$SECOND" -l 'other-client-text'
wait_for pane_contains "$OTHER" other-client-text
touch "$GATE"

# Modal mouse input must keep bypassing bindings after switching back.
setup
$TMUX bind P display-popup -w 30 -h 6 -E "$PROGRAM"
$TMUX bind -n MouseDown1Pane set -g @mouse-binding yes
$TMUX2 send-keys -t "$OUTER" C-b P
wait_for test -f "$READY"
seq=$(printf '\033[<0;40;12M\033[<0;40;12m')
$TMUX2 send-keys -t "$OUTER" -l "$seq"
sleep 0.2
option_is @mouse-binding '' || fail "popup ran a mouse binding"
leave window
$TMUX select-window -t s:0
$TMUX2 send-keys -t "$OUTER" -l "$seq"
sleep 0.2
option_is @mouse-binding '' || fail "popup ran a mouse binding after switching back"
touch "$GATE"

exit 0
