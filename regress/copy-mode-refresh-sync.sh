#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
$TMUX kill-server 2>/dev/null

cleanup()
{
	$TMUX kill-server 2>/dev/null
	rm -rf "$TMP"
}
fail()
{
	echo "$1"
	cleanup
	exit 1
}
wait_done()
{
	i=0
	while [ $i -lt 50 ]; do
		[ -f "$DONE" ] && return
		i=$((i + 1))
		sleep 0.1
	done
	fail "pane command did not finish"
}
wait_sync()
{
	i=0
	while [ $i -lt 50 ]; do
		state=$($TMUX display-message -p -t "$PANE" '#{synchronized_output_flag}')
		[ "$state" = "1" ] && return
		i=$((i + 1))
		sleep 0.1
	done
	fail "pane did not enter synchronized output"
}
wait_state()
{
	expected=$1
	i=0
	while [ $i -lt 50 ]; do
		state=$($TMUX display-message -p -t "$PANE" '#{synchronized_output_flag} #{pane_unseen_changes} #{refresh_active}')
		[ "$state" = "$expected" ] && return
		i=$((i + 1))
		sleep 0.1
	done
	fail "unexpected state: $state"
}
trap cleanup 0
trap 'exit 1' 1 2 3 15

TMP=$(mktemp -d) || exit 1
GO="$TMP/go"
DONE="$TMP/done"
SCRIPT="$TMP/pane.sh"

cat >"$SCRIPT" <<EOF
#!/bin/sh
while [ ! -f "$GO" ]; do sleep 0.01; done
printf '\033[?2026h'
printf 'one\n'
sleep 0.5
printf '\033[?2026h'
sleep 0.5
printf '\033[?2026l'
printf 'two\n'
touch "$DONE"
sleep 1
EOF
chmod +x "$SCRIPT" || exit 1

PANE=$($TMUX new -d -x40 -y10 -P -F '#{pane_id}' "$SCRIPT") || exit 1
$TMUX set -g window-size manual || exit 1
$TMUX copy-mode -t "$PANE" || exit 1
$TMUX send-keys -t "$PANE" -X refresh-on || exit 1

touch "$GO"
wait_sync
sleep 0.2
wait_state "1 1 1"

wait_done
wait_state "0 0 1"

exit 0
