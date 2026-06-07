#!/bin/sh

# Test DECRPM response for mode 2026 (synchronized output).
#
# DECRQM (ESC[?2026$p) should elicit DECRPM (ESC[?2026;Ps$y) where
# Ps=1 when MODE_SYNC is active, Ps=2 when reset.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
$TMUX kill-server 2>/dev/null
sleep 1

TMP=$(mktemp)
trap "rm -f $TMP; $TMUX kill-server 2>/dev/null" 0 1 15

$TMUX -f/dev/null new -d -x80 -y24 || exit 1
sleep 1

# Keep the session alive regardless of pane exits.
$TMUX set -g remain-on-exit on

exit_status=0

# query_decrpm <outfile> <mode> [setup_seq]
#   Spawn a pane that optionally sends setup_seq, then sends DECRQM for
#   mode 2026 and captures the response into outfile in cat -v form.
query_decrpm() {
	_outfile=$1
	_mode=$2
	_setup=$3
	_n=$4

	$TMUX respawnw -k -t:0 -- sh -c "
		exec 2>/dev/null
		stty raw -echo
		${_setup:+printf '$_setup'; sleep 0.2}
		printf '\033[%s\$p' "$_mode"
		dd bs=1 count=$_n 2>/dev/null | cat -v > $_outfile
		sleep 0.2
	" || exit 1
	sleep 2
}

# ------------------------------------------------------------------
# Test 1: mode 2026 should be reset by default (Ps=2)
# ------------------------------------------------------------------
query_decrpm "$TMP" "?2026" '' 11

actual=$(cat "$TMP")
expected='^[[?2026;2$y'

if [ "$actual" = "$expected" ]; then
	if [ -n "$VERBOSE" ]; then
		echo "[PASS] DECRQM 2026 (default/reset) -> $actual"
	fi
else
	echo "[FAIL] DECRQM 2026 (default/reset): expected '$expected', got '$actual'"
	exit_status=1
fi

# ------------------------------------------------------------------
# Test 2: set mode 2026 (SM ?2026), then query (expect Ps=1)
# ------------------------------------------------------------------
query_decrpm "$TMP" "?2026" '\033[?2026h' 11

actual=$(cat "$TMP")
expected='^[[?2026;1$y'

if [ "$actual" = "$expected" ]; then
	if [ -n "$VERBOSE" ]; then
		echo "[PASS] DECRQM 2026 (set) -> $actual"
	fi
else
	echo "[FAIL] DECRQM 2026 (set): expected '$expected', got '$actual'"
	exit_status=1
fi

# ------------------------------------------------------------------
# Test 3: mode 25 should return current value
# ------------------------------------------------------------------
query_decrpm "$TMP" "?25" '\033[?25l' 9

actual=$(cat "$TMP")
expected='^[[?25;2$y'

if [ "$actual" = "$expected" ]; then
	if [ -n "$VERBOSE" ]; then
		echo "[PASS] DECTCEM 25 (reset) -> $actual"
	fi
else
	echo "[FAIL] DECTCEM 25 (reset): expected '$expected', got '$actual'"
	exit_status=1
fi

# ------------------------------------------------------------------
# Test 4: mode ?9999 should return not recognized
# ------------------------------------------------------------------
query_decrpm "$TMP" "?9999" '\033[?9999h' 11

actual=$(cat "$TMP")
expected='^[[?9999;0$y'

if [ "$actual" = "$expected" ]; then
	if [ -n "$VERBOSE" ]; then
		echo "[PASS] DECSM 9999 (not recognized) -> $actual"
	fi
else
	echo "[FAIL] DECSM 9999 (not recognized): expected '$expected', got '$actual'"
	exit_status=1
fi

$TMUX kill-server 2>/dev/null

exit $exit_status

# ------------------------------------------------------------------
# Test 5: mode 4 is reset by default
# ------------------------------------------------------------------
query_decrpm "$TMP" "4" '\033[4h' 11

actual=$(cat "$TMP")
expected='^[[4;1$y'

if [ "$actual" = "$expected" ]; then
	if [ -n "$VERBOSE" ]; then
		echo "[PASS] SM 4 (is set) -> $actual"
	fi
else
	echo "[FAIL] SM 4 (is set): expected '$expected', got '$actual'"
	exit_status=1
fi

$TMUX kill-server 2>/dev/null

exit $exit_status
