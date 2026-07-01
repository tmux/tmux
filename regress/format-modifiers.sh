#!/bin/sh

# Tests of format modifiers as described in tmux(1) FORMATS.
#
# This complements format-strings.sh (which covers escapes, conditionals,
# boolean operators and the l: literal modifier).  Here we exercise the
# remaining modifiers: comparisons/matching (m, C, <, >, ==, ...), numeric
# operations (e|op|), width/padding/truncation (=, p, n, w, a, R), basename
# and dirname (b, d), time conversion (t), loops (S, W, P), colour (c) and
# modifier nesting/limits.

PATH=/bin:/usr/bin
TERM=screen
TZ=UTC
export TZ

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"

ESC=$(printf '\033')

# test_format $format $expected [$target]
#
# Expand $format with display-message and compare with $expected.  If $target
# is given it is passed to display-message with -t.
test_format()
{
	fmt="$1"
	exp="$2"
	target="$3"

	if [ -n "$target" ]; then
		out=$($TMUX display-message -t "$target" -p "$fmt")
	else
		out=$($TMUX display-message -p "$fmt")
	fi

	if [ "$out" != "$exp" ]; then
		echo "Format test failed for '$fmt'."
		echo "Expected: '$exp'"
		echo "But got   '$out'"
		exit 1
	fi
}

# assert_alive
#
# Check that the server is still responding (used after operations that could
# in principle crash it, such as recursion and division by zero).
assert_alive()
{
	if [ "$($TMUX display-message -p alive)" != "alive" ]; then
		echo "Server did not survive: $1"
		exit 1
	fi
}

$TMUX kill-server 2>/dev/null
$TMUX new-session -d -s main -x 80 -y 24 || exit 1

# User options used as inputs.  Modifiers operate on variable names, so plain
# literals must be provided via options (or a nested #{l:...}).
$TMUX set @s 'abcdefghij' || exit 1
$TMUX set @path '/usr/local/bin/foo' || exit 1
$TMUX set @name 'window-name' || exit 1
$TMUX set @greek 'αβγ' || exit 1   # 6 bytes, 3 columns wide
$TMUX set @cjk '中文' || exit 1     # 6 bytes, 4 columns wide
$TMUX set @host 'myhost' || exit 1
$TMUX set @ts '1000000000' || exit 1  # 2001-09-09 01:46:40 UTC


# --- Comparisons and matching --------------------------------------------

# m: glob match, first argument is the pattern.
test_format "#{m:*foo*,barfoobar}" "1"
test_format "#{m:*foo*,barbar}" "0"
test_format "#{m:abc,abc}" "1"
# m/i: ignore case.
test_format "#{m/i:*FOO*,barfoobar}" "1"
test_format "#{m/i:*FOO*,barbar}" "0"
# m/r: regular expression.
test_format "#{m/r:^[0-9]+\$,12345}" "1"
test_format "#{m/r:^[0-9]+\$,12a45}" "0"
# m/ri: regular expression, ignore case.
test_format "#{m/ri:^ab+\$,ABBB}" "1"
test_format "#{m/ri:^ab+\$,ACCC}" "0"

# String comparisons.
test_format "#{==:#{@host},myhost}" "1"
test_format "#{==:#{@host},other}" "0"
test_format "#{!=:abc,xyz}" "1"
test_format "#{!=:abc,abc}" "0"
test_format "#{<:3,5}" "1"
test_format "#{<:5,3}" "0"
test_format "#{>:5,3}" "1"
test_format "#{>:3,5}" "0"
test_format "#{<=:5,5}" "1"
test_format "#{<=:6,5}" "0"
test_format "#{>=:5,5}" "1"
test_format "#{>=:4,5}" "0"

# Negation and canonical boolean.
test_format "#{!:0}" "1"
test_format "#{!:1}" "0"
test_format "#{!!:}" "0"
test_format "#{!!:0}" "0"
test_format "#{!!:non-empty}" "1"


# --- Numeric operations (e) ----------------------------------------------

# Integer operators.
test_format "#{e|+|:2,3}" "5"
test_format "#{e|-|:10,4}" "6"
test_format "#{e|-|:2,5}" "-3"
test_format "#{e|*|:6,7}" "42"
test_format "#{e|/|:20,4}" "5"
# Modulus - both spellings (% must be doubled as it is a strftime specifier).
test_format "#{e|m|:7,3}" "1"
test_format "#{e|%%|:7,3}" "1"

# Numeric comparison operators.
test_format "#{e|==|:5,5}" "1"
test_format "#{e|!=|:5,5}" "0"
test_format "#{e|<|:2,5}" "1"
test_format "#{e|>|:9,2}" "1"
test_format "#{e|<=|:5,5}" "1"
test_format "#{e|>=|:5,5}" "1"

# Floating point with a decimal-place count.
test_format "#{e|*|f|4:5.5,3}" "16.5000"
test_format "#{e|/|f|3:1,3}" "0.333"
test_format "#{e|/|f|2:10,3}" "3.33"
# Default number of decimal places for float is two.
test_format "#{e|*|f:2.5,2}" "5.00"

# Division by zero must not crash the server (result is unspecified).
$TMUX display-message -p "#{e|/|:5,0}" >/dev/null 2>&1
$TMUX display-message -p "#{e|/|f:5,0}" >/dev/null 2>&1
assert_alive "division by zero"


# --- ASCII and repeat ----------------------------------------------------

# a: numeric value to its ASCII character.
test_format "#{a:98}" "b"
test_format "#{a:65}" "A"
# R: repeat first argument second-argument times.
test_format "#{R:a,3}" "aaa"
test_format "#{R:ab,2}" "abab"


# --- Width, padding and truncation ---------------------------------------

# =N truncates from the start, =-N from the end.
test_format "#{=5:@s}" "abcde"
test_format "#{=-5:@s}" "fghij"
# A marker is appended/prepended only when trimming actually occurs.
test_format "#{=/5/...:@s}" "abcde..."
test_format "#{=/5/...:@name}" "windo..."
test_format "#{=/20/...:@s}" "abcdefghij"
# Truncation is display-width (UTF-8) aware.
test_format "#{=3:@greek}" "αβγ"
test_format "#{=2:@greek}" "αβ"
test_format "#{=2:@cjk}" "中"
test_format "#{=1:@cjk}" ""

# p pads to a width: a positive width left-aligns (pads on the right), a
# negative width right-aligns (pads on the left).
test_format "#{p12:@name}" "window-name "
test_format "#{p-12:@name}" " window-name"
# No padding once the value already meets the width.
test_format "#{p3:@name}" "window-name"

# n is byte length, w is display width.
test_format "#{n:@s}" "10"
test_format "#{w:@s}" "10"
test_format "#{n:@greek}" "6"
test_format "#{w:@greek}" "3"
test_format "#{n:@cjk}" "6"
test_format "#{w:@cjk}" "4"


# --- basename and dirname ------------------------------------------------

test_format "#{b:@path}" "foo"
test_format "#{d:@path}" "/usr/local/bin"


# --- Time conversion -----------------------------------------------------

# t: converts an integer time to a ctime(3) string.
test_format "#{t:@ts}" "Sun Sep  9 01:46:40 2001"
# t/p: shorter format for times in the past.
test_format "#{t/p:@ts}" "Sep01"
# t/r: relative time depends on the current time, just check it is non-empty.
if [ -z "$($TMUX display-message -p '#{t/r:@ts}')" ]; then
	echo "Format test failed for '#{t/r:@ts}': empty result"
	exit 1
fi

# t/f: custom strftime format applied to the variable's time.  The % specifiers
# are doubled because display-message also expands the format through strftime;
# the colon inside the format is escaped as '#:'.
test_format "#{t/f/%%Y:@ts}" "2001"
test_format "#{t/f/%%Y-%%m-%%d:@ts}" "2001-09-09"
test_format "#{t/f/%%H#:%%M#:%%S:@ts}" "01:46:40"


# --- Loops (S, W, P) -----------------------------------------------------

# Windows in the session, iterated in index order.
$TMUX set -g automatic-rename off
$TMUX rename-window -t main:0 w0
$TMUX new-window -t main: -n w1
$TMUX new-window -t main: -n w2
test_format "#{W:#{window_index}}" "012" "main:"
test_format "#{W:[#{window_name}]}" "[w0][w1][w2]" "main:"

# Panes: iteration order depends on layout, so assert a per-item constant to
# check the count/iteration only.
$TMUX split-window -t main:0 -d
$TMUX split-window -t main:0 -d
test_format "#{P:x}" "xxx" "main:0"

# Sessions: assert a per-session constant (order independent).
$TMUX new-session -d -s alpha
$TMUX new-session -d -s beta
test_format "#{S:s}" "sss"


# --- Content search (C) --------------------------------------------------

# Use a window running cat so the content is deterministic (no shell prompt).
$TMUX new-session -d -s search -x 80 -y 10 'cat'
sleep 1
$TMUX send-keys -t search: 'Zebra_Marker_42' Enter
sleep 1
# C: returns the (1-based) line number of a match or 0 if not found.
test_format "#{C:Zebra_Marker_42}" "1" "search:"
test_format "#{C:Absent_String_999}" "0" "search:"
test_format "#{C/r:Zebra_.*_42}" "1" "search:"
test_format "#{C/i:zebra_marker_42}" "1" "search:"
$TMUX kill-session -t search 2>/dev/null


# --- Colour (c) ----------------------------------------------------------

# c: converts a colour to its six-digit hexadecimal RGB value.
test_format "#{c:red}" "800000"
test_format "#{c:colour4}" "000080"
test_format "#{c:#7f7f7f}" "7f7f7f"
# c/f and c/b produce the SGR escape sequence for fg/bg respectively.
test_format "#{c/f:red}" "${ESC}[31m"
test_format "#{c/b:red}" "${ESC}[41m"
test_format "#{c/b:colour4}" "${ESC}[48;5;4m"


# --- Nesting and limits --------------------------------------------------

# Modifier chaining: inner b: then outer truncation/padding/length.
test_format "#{=5:#{b:@path}}" "foo"
test_format "#{=2:#{b:@path}}" "fo"
test_format "#{p6:#{b:@path}}" "foo   "
test_format "#{n:#{b:@path}}" "3"

# Nested l: literal expanded then truncated.
test_format "#{=5:#{l:abcdefghij}}" "abcde"

# Unbounded self-recursion must hit the loop limit rather than crash.
$TMUX set @rec '#{E:@rec}'
$TMUX display-message -p '#{E:@rec}' >/dev/null 2>&1
assert_alive "recursive expansion"

exit 0
