#!/bin/sh

# Whole-target aliases must include the attached client's session. In
# particular, swap-window -d uses it to select the swapped windows.

PATH=/bin:/usr/bin
TERM=screen
export TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"

TMP=$(mktemp -d) || exit 1
cleanup()
{
	$TMUX kill-server 2>/dev/null
	rm -rf "$TMP"
}
trap cleanup 0
trap 'exit 1' 1 2 15

$TMUX new-session -d -s test -n first || exit 1
$TMUX new-window -t test:1 -n second || exit 1

for target in '{current}' '{active}' '@'; do
	# Run through an attached client: a standalone command client has no
	# session and rejects these aliases with "no current client".
	cat <<EOF | $TMUX -C attach-session -t test >"$TMP/output" 2>&1
select-window -t test:1
swap-window -d -s '$target' -t -1
display-message -p 'CHECK:detached-source #{session_name}:#{window_index}:#{window_name}'
swap-window -d -s -1 -t '$target'
display-message -p 'CHECK:detached-target #{session_name}:#{window_index}:#{window_name}'
select-window -t test:1
swap-window -s '$target' -t -1
display-message -p 'CHECK:source #{session_name}:#{window_index}:#{window_name}'
swap-window -s -1 -t '$target'
display-message -p 'CHECK:target #{session_name}:#{window_index}:#{window_name}'
display-message -p -t '$target' 'CHECK:alias #{session_name}:#{window_index}:#{window_name}'
detach-client
EOF
	if ! $TMUX has-session -t test; then
		echo "Server died using $target"
		cat "$TMP/output"
		exit 1
	fi
	if grep '^%error' "$TMP/output"; then
		cat "$TMP/output"
		exit 1
	fi
	grep '^CHECK:' "$TMP/output" >"$TMP/actual"
	cat <<EOF >"$TMP/expected"
CHECK:detached-source test:0:second
CHECK:detached-target test:0:first
CHECK:source test:1:first
CHECK:target test:1:second
CHECK:alias test:1:second
EOF
	if ! cmp -s "$TMP/expected" "$TMP/actual"; then
		echo "Wrong windows after swapping with $target"
		cat "$TMP/output"
		exit 1
	fi
done

exit 0
