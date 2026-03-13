#!/bin/sh

# Test for popup getting overwritten by background updates
# We use a nested tmux to capture what the inner tmux actually draws to the terminal.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null
TMUX_OUTER="$TEST_TMUX -Ltest2"
$TMUX_OUTER kill-server 2>/dev/null

trap "$TMUX kill-server 2>/dev/null; $TMUX_OUTER kill-server 2>/dev/null" 0 1 15

# Start outer tmux that will capture the inner tmux's rendering
$TMUX_OUTER -f/dev/null new -d -x80 -y24 "$TMUX -f/dev/null new -x80 -y24" || exit 1
sleep 1

# Start a background process in the inner tmux that writes to the middle of the screen
$TMUX send-keys 'while true; do for i in $(seq 1 20); do printf "\033[H\033[2JBACKGROUND_NOISE\n"; done; sleep 0.1; done' Enter
sleep 1

# Open a popup in the inner tmux
$TMUX popup -w 40 -h 10 -d "$PWD" -E "sleep 5" &
sleep 2

# Capture the output from the outer tmux (which shows what inner tmux drew)
output=$($TMUX_OUTER capturep -p)

# Clean up
$TMUX kill-server 2>/dev/null
$TMUX_OUTER kill-server 2>/dev/null

# If the background overwrites the popup completely, there will be no '│' or '─' characters.
count_border=$(echo "$output" | grep -c "│")
if [ "$count_border" -eq 0 ]; then
    echo "Popup borders missing! It was overwritten by background noise."
    exit 1
fi

echo "Popup correctly resisted being overwritten."
exit 0
