#!/bin/sh

# Exercise modes while their backing objects are changed from outside the
# client displaying the mode. This catches stale selection indexes and pointers
# after a mode list is shrunk or rebuilt.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMP=$(mktemp -d) || exit 1
TMUX_TMPDIR="$TMP"
export TMUX_TMPDIR

TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

cleanup_servers()
{
	$TMUX kill-server 2>/dev/null
	$TMUX2 kill-server 2>/dev/null
	sleep 0.5
}

cleanup()
{
	cleanup_servers
	rm -rf "$TMP"
}
trap cleanup EXIT

fail()
{
	echo "$1" >&2
	cleanup
	exit 1
}

capture()
{
	$TMUX2 capture-pane -p -t out:0 2>/dev/null
}

assert_alive()
{
	$TMUX display-message -p 'alive' >/dev/null 2>&1 || \
		fail "$1: server exited"
}

wait_clients()
{
	i=0
	while [ "$i" -lt 50 ]; do
		c=$($TMUX list-clients -F x 2>/dev/null | grep -c x)
		[ "$c" -eq "$1" ] && return 0
		sleep 0.2
		i=$((i + 1))
	done
	fail "expected $1 clients, have $c"
}

wait_for()
{
	i=0
	while [ "$i" -lt 50 ]; do
		capture | grep -F -q "$1" && return 0
		sleep 0.2
		i=$((i + 1))
	done
	fail "timed out waiting for '$1'"
}

wait_mode()
{
	t=$1
	want=$2

	i=0
	while [ "$i" -lt 50 ]; do
		got=$($TMUX display-message -p -t "$t" '#{pane_in_mode}' \
		    2>/dev/null)
		[ "$got" = "$want" ] && return 0
		sleep 0.2
		i=$((i + 1))
	done
	fail "pane $t mode state is $got, expected $want"
}

repeat_key()
{
	t=$1
	key=$2
	count=$3

	i=0
	while [ "$i" -lt "$count" ]; do
		$TMUX send-keys -t "$t" "$key" || \
			fail "failed to send $key to $t"
		i=$((i + 1))
	done
}

start_client()
{
	s=$1
	cmd=${2:-cat}

	cleanup_servers
	$TMUX new-session -d -s "$s" -n main -x 80 -y 24 "$cmd" || \
		fail "$s: new-session failed"
	$TMUX2 new-session -d -s out -x 80 -y 24 "$TMUX attach -t $s" || \
		fail "$s: outer client failed"
	wait_clients 1
}

new_outer_client()
{
	s=$1

	$TMUX2 new-window -d -t out: "$TMUX attach -t $s" || \
		fail "$s: outer client failed"
}

client_for_session()
{
	$TMUX list-clients -F '#{client_name} #{client_session}' |
	    awk -v s="$1" '$2 == s { print $1; exit }'
}

test_choose_tree()
{
	start_client tree-a
	$TMUX new-session -d -s tree-b -n one 'cat' || fail "tree-b failed"
	$TMUX new-window -d -t tree-b -n two 'cat' || fail "tree-b:1 failed"
	$TMUX new-session -d -s tree-c -n one 'cat' || fail "tree-c failed"
	$TMUX new-session -d -s tree-d -n one 'cat' || fail "tree-d failed"
	$TMUX split-window -d -t tree-a:0 'cat' || fail "tree split failed"

	$TMUX choose-tree -t tree-a:0 -O index -F 'MT #{session_name}:#{window_index}.#{pane_index}' || \
		fail "choose-tree failed"
	wait_for 'MT '
	repeat_key tree-a:0 j 40

	$TMUX kill-session -t tree-d || fail "tree kill-session failed"
	$TMUX kill-session -t tree-c || fail "tree kill-session failed"
	$TMUX rename-session -t tree-b tree-renamed || fail "tree rename failed"
	$TMUX rename-window -t tree-renamed:0 renamed || fail "tree rename-window failed"
	$TMUX kill-window -t tree-renamed:1 || fail "tree kill-window failed"
	side=$($TMUX split-window -d -P -F '#{pane_id}' -t tree-a:0 'cat') || \
		fail "tree side split failed"
	$TMUX break-pane -d -s "$side" || fail "tree break-pane failed"
	$TMUX join-pane -d -s "$side" -t tree-a:0.0 || \
		fail "tree join-pane failed"
	i=0
	while [ "$i" -lt 12 ]; do
		$TMUX new-window -d -t tree-a -n "new$i" 'cat' || \
			fail "tree new-window failed"
		i=$((i + 1))
	done

	assert_alive "choose-tree mutation"
	$TMUX send-keys -t tree-a:0 k j l h Enter || \
		fail "choose-tree keys failed"
	wait_mode tree-a:0 0
	assert_alive "choose-tree exit"
}

test_choose_buffer()
{
	start_client buffer-a

	i=0
	while [ "$i" -lt 30 ]; do
		$TMUX set-buffer -b "mbuf$i" "buffer mutation $i" || \
			fail "set-buffer failed"
		i=$((i + 1))
	done

	$TMUX choose-buffer -t buffer-a:0 -F 'MB #{buffer_name}' || \
		fail "choose-buffer failed"
	wait_for 'MB '
	repeat_key buffer-a:0 j 40

	i=8
	while [ "$i" -lt 30 ]; do
		$TMUX delete-buffer -b "mbuf$i" || fail "delete-buffer failed"
		i=$((i + 1))
	done
	i=30
	while [ "$i" -lt 50 ]; do
		$TMUX set-buffer -b "mbuf$i" "new buffer mutation $i" || \
			fail "new set-buffer failed"
		i=$((i + 1))
	done

	assert_alive "choose-buffer mutation"
	$TMUX send-keys -t buffer-a:0 k j Enter || \
		fail "choose-buffer keys failed"
	wait_mode buffer-a:0 0
	assert_alive "choose-buffer exit"
}

test_choose_client()
{
	start_client client-a
	$TMUX new-session -d -s client-b -n main 'cat' || fail "client-b failed"
	$TMUX new-session -d -s client-c -n main 'cat' || fail "client-c failed"
	new_outer_client client-b
	new_outer_client client-c
	wait_clients 3

	$TMUX choose-client -t client-a:0 -F 'MC #{client_session}' || \
		fail "choose-client failed"
	wait_for 'MC '
	repeat_key client-a:0 j 20

	c=$(client_for_session client-c)
	[ -n "$c" ] || fail "client-c client not found"
	$TMUX detach-client -t "$c" || fail "detach client-c failed"
	c=$(client_for_session client-b)
	[ -n "$c" ] || fail "client-b client not found"
	$TMUX detach-client -t "$c" || fail "detach client-b failed"

	$TMUX new-session -d -s client-d -n main 'cat' || fail "client-d failed"
	new_outer_client client-d
	wait_clients 2

	assert_alive "choose-client mutation"
	$TMUX send-keys -t client-a:0 k j Enter || \
		fail "choose-client keys failed"
	wait_mode client-a:0 0
	assert_alive "choose-client exit"
}

test_customize_mode()
{
	start_client option-a

	i=0
	while [ "$i" -lt 30 ]; do
		$TMUX set-option -g "@mode_mut_$i" "$i" || \
			fail "set option failed"
		i=$((i + 1))
	done

	$TMUX customize-mode -t option-a:0 -F 'MO #{option_name}=#{option_value}' || \
		fail "customize-mode failed"
	wait_mode option-a:0 1
	repeat_key option-a:0 j 80

	i=10
	while [ "$i" -lt 30 ]; do
		$TMUX set-option -gu "@mode_mut_$i" || fail "unset option failed"
		i=$((i + 1))
	done
	i=30
	while [ "$i" -lt 55 ]; do
		$TMUX set-option -g "@mode_mut_$i" "$i" || \
			fail "new option failed"
		i=$((i + 1))
	done
	$TMUX set-option -g status-left 'mutated' || fail "status-left failed"
	$TMUX rename-session -t option-a option-renamed || fail "option rename failed"

	assert_alive "customize-mode mutation"
	$TMUX send-keys -t option-renamed:0 k j C-d C-u q || \
		fail "customize-mode keys failed"
	wait_mode option-renamed:0 0
	assert_alive "customize-mode exit"
}

test_copy_mode()
{
	start_client copy-a 'i=0; while [ $i -lt 200 ]; do echo "copy mutation line $i"; i=$((i + 1)); done; cat'
	$TMUX set-window-option -g mode-keys vi || fail "mode-keys failed"
	$TMUX split-window -d -t copy-a:0 'cat' || fail "copy split failed"

	$TMUX copy-mode -t copy-a:0 || fail "copy-mode failed"
	wait_mode copy-a:0 1
	repeat_key copy-a:0 k 20

	$TMUX rename-window -t copy-a:0 renamed || fail "copy rename-window failed"
	side=$($TMUX split-window -d -P -F '#{pane_id}' -t copy-a:renamed 'cat') || \
		fail "copy side split failed"
	$TMUX break-pane -d -s "$side" || fail "copy break-pane failed"
	$TMUX join-pane -d -s "$side" -t copy-a:renamed.0 || \
		fail "copy join-pane failed"
	$TMUX kill-pane -t copy-a:renamed.1 || fail "copy kill-pane failed"
	$TMUX new-window -d -t copy-a -n extra 'cat' || fail "copy new-window failed"
	$TMUX kill-window -t copy-a:extra || fail "copy kill-window failed"
	$TMUX rename-session -t copy-a copy-renamed || fail "copy rename failed"

	assert_alive "copy-mode mutation"
	$TMUX send-keys -t copy-renamed:renamed.0 j k C-d C-u q || \
		fail "copy-mode keys failed"
	wait_mode copy-renamed:renamed.0 0
	assert_alive "copy-mode exit"
}

cleanup_servers
test_choose_tree
test_choose_buffer
test_choose_client
test_customize_mode
test_copy_mode
cleanup
exit 0
