#!/bin/sh

# Test synchronized output mode (mode 2026)

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null
sleep 1

# Start tmux with a shell
$TMUX -f/dev/null new -d -x40 -y10 || exit 1
# Wait for shell to be ready
sleep 2

exit_status=0

# Test 1: synchronized_output_flag should initially be 0
flag=$($TMUX display -pF '#{synchronized_output_flag}')
if [ "$flag" != "0" ]; then
	echo "[FAIL] Test 1: synchronized_output_flag should initially be 0, got: $flag"
	exit_status=1
else
	echo "[PASS] Test 1: synchronized_output_flag initially 0"
fi

# Test 2: ESC[?2026h should set synchronized_output_flag to 1
$TMUX send-keys "printf '\\033[?2026h'" Enter
sleep 0.5
flag=$($TMUX display -pF '#{synchronized_output_flag}')
if [ "$flag" != "1" ]; then
	echo "[FAIL] Test 2: synchronized_output_flag should be 1 after ESC[?2026h, got: $flag"
	exit_status=1
else
	echo "[PASS] Test 2: synchronized_output_flag set to 1"
fi

# Test 3: ESC[?2026l should clear synchronized_output_flag to 0
# First set it again
$TMUX send-keys "printf '\\033[?2026h'" Enter
sleep 0.3
# Then clear it
$TMUX send-keys "printf '\\033[?2026l'" Enter
sleep 0.5
flag=$($TMUX display -pF '#{synchronized_output_flag}')
if [ "$flag" != "0" ]; then
	echo "[FAIL] Test 3: synchronized_output_flag should be 0 after ESC[?2026l, got: $flag"
	exit_status=1
else
	echo "[PASS] Test 3: synchronized_output_flag cleared to 0"
fi

# Test 4: synchronized_output_flag should auto-clear after timeout (1 second)
$TMUX send-keys "printf '\\033[?2026h'" Enter
sleep 0.5
flag=$($TMUX display -pF '#{synchronized_output_flag}')
if [ "$flag" != "1" ]; then
	echo "[FAIL] Test 4a: synchronized_output_flag should be 1, got: $flag"
	exit_status=1
else
	echo "[PASS] Test 4a: synchronized_output_flag set to 1"
fi
# Wait for timeout (1 second + buffer)
sleep 1.5
flag=$($TMUX display -pF '#{synchronized_output_flag}')
if [ "$flag" != "0" ]; then
	echo "[FAIL] Test 4b: synchronized_output_flag should auto-clear after timeout, got: $flag"
	exit_status=1
else
	echo "[PASS] Test 4b: synchronized_output_flag auto-cleared after timeout"
fi

# Test 5: synchronized_output_flag should clear on resize
# Use resize-window since resize-pane doesn't work with a single pane
$TMUX send-keys "printf '\\033[?2026h'" Enter
sleep 0.5
flag=$($TMUX display -pF '#{synchronized_output_flag}')
if [ "$flag" != "1" ]; then
	echo "[FAIL] Test 5a: synchronized_output_flag should be 1, got: $flag"
	exit_status=1
else
	echo "[PASS] Test 5a: synchronized_output_flag set to 1"
fi
$TMUX resize-window -x 30 -y 8
sleep 0.5
flag=$($TMUX display -pF '#{synchronized_output_flag}')
if [ "$flag" != "0" ]; then
	echo "[FAIL] Test 5b: synchronized_output_flag should clear on resize, got: $flag"
	exit_status=1
else
	echo "[PASS] Test 5b: synchronized_output_flag cleared on resize"
fi

$TMUX kill-server 2>/dev/null
sleep 1

# Test 6: Nested BSU is idempotent - multiple BSU calls don't break things
$TMUX -f/dev/null new -d -x40 -y10 || exit 1
sleep 2

# Send first BSU
$TMUX send-keys "printf '\\033[?2026h'" Enter
sleep 0.3
# Send second BSU (nested) - timer should reset
$TMUX send-keys "printf '\\033[?2026h'" Enter
sleep 0.3
flag=$($TMUX display -pF '#{synchronized_output_flag}')
if [ "$flag" != "1" ]; then
	echo "[FAIL] Test 6a: synchronized_output_flag should be 1 after nested BSU, got: $flag"
	exit_status=1
else
	echo "[PASS] Test 6a: synchronized_output_flag set after nested BSU"
fi

# Single ESU should clear the flag
$TMUX send-keys "printf '\\033[?2026l'" Enter
sleep 0.5
flag=$($TMUX display -pF '#{synchronized_output_flag}')
if [ "$flag" != "0" ]; then
	echo "[FAIL] Test 6b: synchronized_output_flag should be 0 after single ESU, got: $flag"
	exit_status=1
else
	echo "[PASS] Test 6b: Single ESU clears nested BSU"
fi

$TMUX kill-server 2>/dev/null
sleep 1

# Test 7: Content should not appear on screen until sync ends
$TMUX -f/dev/null new -d -x40 -y10 || exit 1
sleep 1

# Create a script that outputs content during sync mode
TMP=$(mktemp)
trap "rm -f $TMP" 0 1 15

# Send sync start, output test pattern, capture (should be empty), then end sync
$TMUX send-keys "printf '\\033[?2026h'; printf 'SYNCTEST'; $TMUX capturep -p > $TMP; printf '\\033[?2026l'" Enter
sleep 1

# Check that SYNCTEST was NOT captured (it was buffered)
if grep -q "SYNCTEST" "$TMP"; then
	echo "[FAIL] Test 7: Content appeared before sync ended"
	exit_status=1
else
	echo "[PASS] Test 7: Content buffered during sync mode"
fi

# Now capture again - content should be visible
$TMUX capturep -p > $TMP
if grep -q "SYNCTEST" "$TMP"; then
	echo "[PASS] Test 8: Content visible after sync ended"
else
	echo "[FAIL] Test 8: Content not visible after sync ended"
	exit_status=1
fi

$TMUX kill-server 2>/dev/null
sleep 1

# Test 9: Content should appear after timeout (timer fires)
$TMUX -f/dev/null new -d -x40 -y10 || exit 1
sleep 1

TMP2=$(mktemp)
trap "rm -f $TMP $TMP2" 0 1 15

# Send sync start, output test pattern, capture immediately (should be empty)
$TMUX send-keys "printf '\\033[?2026h'; printf 'TIMERTEST'; $TMUX capturep -p > $TMP2" Enter
sleep 0.5

# Check that TIMERTEST was NOT captured (it was buffered)
if grep -q "TIMERTEST" "$TMP2"; then
	echo "[FAIL] Test 9a: Content appeared before timeout"
	exit_status=1
else
	echo "[PASS] Test 9a: Content buffered during sync mode"
fi

# Wait for timeout (1 second + buffer) - don't send ESC[?2026l
sleep 1.5

# Now capture again - content should be visible after timer fired
$TMUX capturep -p > $TMP2
if grep -q "TIMERTEST" "$TMP2"; then
	echo "[PASS] Test 9b: Content visible after timeout"
else
	echo "[FAIL] Test 9b: Content not visible after timeout"
	exit_status=1
fi

$TMUX kill-server 2>/dev/null

if [ $exit_status -eq 0 ]; then
	echo "All tests passed"
fi

exit $exit_status
