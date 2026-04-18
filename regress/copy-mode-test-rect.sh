#!/bin/sh
# This test verifies rectangular selection in copy mode.
# It tests both 'copy-mode -r' and 'begin-selection -r'.

PATH=/bin:/usr/bin
TERM=screen

# Ensure we are in the regress directory
cd "$(dirname "$0")" || exit 1

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -f/dev/null -Ltest"

# Test 1: copy-mode -r
$TMUX kill-server 2>/dev/null

# Start a new tmux session with a specific size (40x10).
# The command runs 'cat copy-mode-test.txt' to fill the screen with content.
# Finally, 'cat' runs without arguments to keep the pane open and waiting for input.
$TMUX new -d -x40 -y10 \
      "cat copy-mode-test.txt; cat" || exit 1
$TMUX set -g window-size manual || exit 1

$TMUX set-window-option -g mode-keys vi

# Enter copy mode with rectangular selection enabled
$TMUX copy-mode -r

# Navigate to the start of the 3rd line
$TMUX send-keys -X history-top
$TMUX send-keys -X cursor-down
$TMUX send-keys -X cursor-down
$TMUX send-keys -X start-of-line

# Begin selection (already in rectangular mode due to copy-mode -r)
$TMUX send-keys -X begin-selection

# Move cursor to expand selection to a 3x6 rectangle (down 2, right 5)
$TMUX send-keys -X cursor-down
$TMUX send-keys -X cursor-down
$TMUX send-keys -X cursor-right
$TMUX send-keys -X cursor-right
$TMUX send-keys -X cursor-right
$TMUX send-keys -X cursor-right
$TMUX send-keys -X cursor-right

# Copy the rectangular selection
$TMUX send-keys -X copy-selection

result=$($TMUX show-buffer)
$TMUX kill-server 2>/dev/null

expected=$(printf "Anothe\n... @n\n ?? 50")

if [ "$result" != "$expected" ]; then
    echo "copy-mode -r test failed"
    echo "Expected: '$expected'"
    echo "Result: '$result'"
    exit 1
fi

# Test 2: begin-selection -r
# Start a new tmux session with a specific size (40x10).
# The command runs 'cat copy-mode-test.txt' to fill the screen with content.
# Finally, 'cat' runs without arguments to keep the pane open and waiting for input.
$TMUX new -d -x40 -y10 \
      "cat copy-mode-test.txt; cat" || exit 1
$TMUX set -g window-size manual || exit 1

$TMUX set-window-option -g mode-keys vi

# Enter copy mode (standard mode)
$TMUX copy-mode

# Navigate to the start of the 3rd line
$TMUX send-keys -X history-top
$TMUX send-keys -X cursor-down
$TMUX send-keys -X cursor-down
$TMUX send-keys -X start-of-line

# Begin selection explicitly in rectangular mode (-r flag)
$TMUX send-keys -X begin-selection -r

# Move cursor to expand selection to a 3x6 rectangle (down 2, right 5)
$TMUX send-keys -X cursor-down
$TMUX send-keys -X cursor-down
$TMUX send-keys -X cursor-right
$TMUX send-keys -X cursor-right
$TMUX send-keys -X cursor-right
$TMUX send-keys -X cursor-right
$TMUX send-keys -X cursor-right

# Copy the rectangular selection
$TMUX send-keys -X copy-selection

result=$($TMUX show-buffer)
$TMUX kill-server 2>/dev/null

if [ "$result" != "$expected" ]; then
    echo "begin-selection -r test failed"
    echo "Expected: '$expected'"
    echo "Result: '$result'"
    exit 1
fi

echo "All rectangular selection tests passed"
exit 0
