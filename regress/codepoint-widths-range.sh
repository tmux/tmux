#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(cd .. && pwd)/tmux
tmux() { "$TEST_TMUX" -Ltest "$@"; }
tmux kill-server 2>/dev/null

pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; tmux kill-server 2>/dev/null; exit 1; }

# Helper: start server, run set command, kill server
try() {
	desc="$1"; shift
	tmux new-session -d -s test 2>/dev/null || fail "$desc (new-session)"
	tmux "$@" 2>/dev/null
	rc=$?
	tmux kill-server 2>/dev/null
	return $rc
}

# 1. Single codepoint (existing behaviour must still work)
try "single codepoint" set -sa codepoint-widths "U+2665=2" \
	&& pass "single codepoint" || fail "single codepoint"

# 2. Single-point range (start == end)
try "single-point range" set -sa codepoint-widths "U+2665-U+2665=2" \
	&& pass "single-point range" || fail "single-point range"

# 3. Small range
try "small range" set -sa codepoint-widths "U+E000-U+E00F=2" \
	&& pass "small range" || fail "small range"

# 4. Full Nerd Fonts ranges from the issue
tmux new-session -d -s test 2>/dev/null || fail "nerd fonts ranges (new-session)"
tmux set -sa codepoint-widths "U+23FB-U+23FE=2" && \
tmux set -sa codepoint-widths "U+2665-U+2665=2" && \
tmux set -sa codepoint-widths "U+2B58-U+2B58=2" && \
tmux set -sa codepoint-widths "U+E000-U+E09F=2" && \
tmux set -sa codepoint-widths "U+E0C0-U+F8FF=2" && \
tmux set -sa codepoint-widths "U+F0001-U+FFFFF=2" \
	&& pass "nerd fonts ranges" || fail "nerd fonts ranges"
tmux kill-server 2>/dev/null

# 5. Invalid range (end < start) must be rejected without crash
try "invalid range end<start" set -sa codepoint-widths "U+E00F-U+E000=2" \
	&& pass "invalid range end<start rejected" || pass "invalid range end<start rejected"

echo "all tests passed"
exit 0
