#!/bin/sh

# Spawning must honour -c after the server's working directory is removed.
# getcwd(3) fails once that directory is gone, and the chdir to the new
# directory must still happen.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"

TMP=$(mktemp -d)
trap '$TMUX kill-server 2>/dev/null; rm -rf "$TMP"' 0 1 15
TMP=$(cd "$TMP" && pwd -P)
mkdir "$TMP/doomed" "$TMP/live" || exit 1

check_directory()
{
	i=0
	while [ ! -s "$TMP/out" ]; do
		if [ "$i" -ge 100 ]; then
			echo "$2: timed out waiting for the pane directory"
			exit 1
		fi
		sleep 0.05
		i=$((i + 1))
	done
	actual=$(cat "$TMP/out")
	if [ "$actual" != "$1" ]; then
		echo "$2: expected directory '$1', got '$actual'"
		exit 1
	fi
	rm -f "$TMP/out"
}

# The server follows the client's working directory, so start it from the
# directory which is about to be removed.
(cd "$TMP/doomed" && $TMUX new-session -d -s test 'sleep 60') || exit 1
rm -rf "$TMP/doomed"

$TMUX split-window -d -t test -c "$TMP/live" \
	"/bin/pwd >'$TMP/out' 2>&1; sleep 60" || exit 1
check_directory "$TMP/live" "split-window"

$TMUX new-window -d -t test -c "$TMP/live" \
	"/bin/pwd >'$TMP/out' 2>&1; sleep 60" || exit 1
check_directory "$TMP/live" "new-window"

$TMUX new-session -d -s second -c "$TMP/live" \
	"/bin/pwd >'$TMP/out' 2>&1; sleep 60" || exit 1
check_directory "$TMP/live" "new-session"

exit 0
