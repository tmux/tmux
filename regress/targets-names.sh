#!/bin/sh

. ./regress-common.inc

# Internally generated targets must identify sessions independently of their
# names. Exercise tree and switch modes and new-session -A with empty names,
# target separators and a name that looks like another session's id.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(test_tmpdir) || exit 1
TMUX_TMPDIR=$DIR
export TMUX_TMPDIR
INNER="$TEST_TMUX -LtestI$$ -f/dev/null"
OUTER="$TEST_TMUX -LtestO$$ -f/dev/null"

cleanup()
{
	stop_server $OUTER || exit 1
	stop_server $INNER || exit 1
	rm -rf "$DIR"
}
trap cleanup 0 1 15

fail()
{
	echo "$*" >&2
	exit 1
}

wait_client_target()
{
	i=0
	while [ "$i" -lt 50 ]; do
		got=$($INNER list-clients -F \
		    '#{session_id}:#{window_index}:#{pane_id}' 2>/dev/null)
		[ "$got" = "$1" ] && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "$mode with name '$name': client target '$got', expected '$1'"
}

wait_rows()
{
	i=0
	while [ "$i" -lt 50 ]; do
		rows=$($OUTER capture-pane -p -t out:0 2>/dev/null | \
		    grep -Fc "$marker")
		[ "$rows" -eq "$1" ] && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "$mode with name '$name': found $rows rows, expected $1"
}

host=$($INNER new-session -dP -F '#{session_id}' -s host -x80 -y24 \
	'exec cat') || exit 1
host_target=$($INNER display-message -p -t "$host:0" \
	'#{session_id}:#{window_index}:#{pane_id}')
$INNER set-option -g status off || exit 1
$OUTER new-session -d -s out -x80 -y24 "$INNER attach -t host" || exit 1
mode=attach
name=host
wait_client_target "$host_target"
client=$($INNER list-clients -F '#{client_name}')

serial=0
for name in '' 'a:b.c' '$0'; do
	sid=$($INNER new-session -dP -F '#{session_id}' -s "$name" \
	    'exec cat') || exit 1
	$INNER split-window -t "$sid:0" 'exec cat' || exit 1
	$INNER new-window -t "$sid:1" 'exec cat' || exit 1

	# A window and its panes can belong to multiple sessions. A bare window
	# or pane id would lose the selected session's context.
	$INNER link-window -d -s "$sid:0" -t "$host:1" || exit 1
	for mode in tree-session tree-window tree-pane \
	    switch-session switch-window attach; do
		$INNER switch-client -c "$client" -t "$host:0" || exit 1
		wait_client_target "$host_target"
		$INNER select-window -t "$sid:1" || exit 1
		serial=$((serial + 1))
		marker=NAMES$serial
		target=$sid:1
		case "$mode" in
		tree-window|switch-window) target=$sid:0 ;;
		tree-pane) target=$sid:0.0 ;;
		esac
		expected=$($INNER display-message -p -t "$target" \
		    '#{session_id}:#{window_index}:#{pane_id}')

		case "$mode" in
		tree-*)
			flags=
			[ "$mode" = tree-session ] && flags=-s
			[ "$mode" = tree-window ] && flags=-w
			$INNER choose-tree $flags -t "$host:0" -O index \
			    -F "$marker" -f "#{==:#{session_id},$sid}" || exit 1
			case "$mode" in
			tree-session) wait_rows 1; keys='g Enter' ;;
			tree-window) wait_rows 3; keys='g Down Enter' ;;
			tree-pane) wait_rows 6; keys='g Down Down Enter' ;;
			esac
			$INNER send-keys -t "$host:0" $keys || exit 1
			;;
		switch-*)
			flags=-s
			[ "$mode" = switch-window ] && flags=-w
			$INNER switch-mode $flags -t "$host:0" -O index \
			    -F "#{?#{==:#{session_id},$sid},$marker,OTHER}" || exit 1
			[ "$mode" = switch-session ] && wait_rows 1
			[ "$mode" = switch-window ] && wait_rows 2
			$INNER send-keys -t "$host:0" -l "$marker" || exit 1
			$INNER send-keys -t "$host:0" Home Enter || exit 1
			;;
		attach)
			$INNER bind-key -n F12 new-session -A -s "$name" || exit 1
			$OUTER send-keys -t out:0 F12 || exit 1
			;;
		esac
		wait_client_target "$expected"
	done
	$INNER switch-client -c "$client" -t "$host:0" || exit 1
	$INNER unlink-window -t "$host:1" || exit 1
	$INNER kill-session -t "$sid" || exit 1
done

exit 0
