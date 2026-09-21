#!/bin/sh

# run-shell -c should expand formats using the target pane.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"

TMP=$(mktemp -d)
trap '$TMUX kill-server 2>/dev/null; rm -rf "$TMP"' 0 1 15
TMP=$(cd "$TMP" && pwd -P)
mkdir "$TMP/first" "$TMP/second dir" || exit 1

check_directory()
{
	actual=$(cat "$TMP/out")
	if [ "$actual" != "$1" ]; then
		echo "Expected directory '$1', got '$actual'"
		exit 1
	fi
}

# Expand the current pane's directory in the same command queue as creation.
$TMUX new-session -d -s test -c "$TMP/first" 'sleep 60' \; \
	 run-shell -c '#{pane_current_path}' "pwd >'$TMP/out'" || exit 1
check_directory "$TMP/first"

# An explicit target must supply the format context, including with a delay.
pane=$($TMUX new-window -d -P -F '#{pane_id}' -t test \
	-c "$TMP/second dir" 'sleep 60') || exit 1
$TMUX run-shell -t "$pane" -d 0.1 -c '#{pane_current_path}' \
	"pwd >'$TMP/out'" || exit 1
check_directory "$TMP/second dir"

# Literal directories and the default client directory still work.
$TMUX run-shell -c "$TMP/second dir" "pwd >'$TMP/out'" || exit 1
check_directory "$TMP/second dir"
(cd "$TMP/first" && $TMUX run-shell "pwd >'$TMP/out'") || exit 1
check_directory "$TMP/first"

exit 0
