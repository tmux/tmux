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
LANG=C.UTF-8
LC_ALL=C.UTF-8
export TZ LANG LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"

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

# test_expand $format $expected
#
# Expand $format in a plain format_expand context (list-windows -F on the
# single-window "tf" session) rather than the format_expand_time context of
# display-message.  This matters for t/f: display-message runs the whole format
# through strftime(3), so a strftime specifier there must be doubled (%%H); in a
# format_expand context a single specifier (%H) is applied directly to the
# variable's time.
test_expand()
{
	fmt="$1"
	exp="$2"

	out=$($TMUX list-windows -t tf -F "$fmt")

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
sleep 0.1
$TMUX new-session -d -s main -x 80 -y 24 || exit 1

# Single-window session used by test_expand for format_expand-context tests.
$TMUX new-session -d -s tf || exit 1

# User options used as inputs.  Modifiers operate on variable names, so plain
# literals must be provided via options (or a nested #{l:...}).  They are set
# globally (-g) so they are visible from every session, including the "tf"
# session used by test_expand.
$TMUX set -g @s 'abcdefghij' || exit 1
$TMUX set -g @path '/usr/local/bin/foo' || exit 1
$TMUX set -g @name 'window-name' || exit 1
$TMUX set -g @greek 'αβγ' || exit 1   # 6 bytes, 3 columns wide
$TMUX set -g @cjk '中文' || exit 1     # 6 bytes, 4 columns wide
$TMUX set -g @host 'myhost' || exit 1
$TMUX set -g @ts '1000000000' || exit 1  # 2001-09-09 01:46:40 UTC
$TMUX set -g @sp 'a b$c' || exit 1     # shell-special characters for q:
$TMUX set -g @hash 'a#b' || exit 1     # a "#" for q/e:
$TMUX set -g @sq "a'b" || exit 1       # a single quote for q/s:
$TMUX set -g @nl "$(printf 'a\nb')" || exit 1
q_s_nl=$(printf "'a\nb'")


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
# m/z: fuzzy match, returns a boolean.
test_format "#{m/z:foo,foobar}" "1"
test_format "#{m/z:xyz,foobar}" "0"
# m/p: fuzzy match, returns the matched (0-based) column positions.
test_format "#{m/p:ac,abc}" "0,2"
test_format "#{m/p:xyz,abc}" ""
# Fuzzy match against empty text.
test_format "#{m/p:x,}" ""
test_format "#{m/z:x,}" "0"

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


# --- Quoting (q) ---------------------------------------------------------

# q: escapes shell special characters with a backslash.
test_format "#{q:@sp}" 'a\ b\$c'
# q/s quotes with POSIX shell single quotes.
test_format "#{q/s:@sp}" "'a b\$c'"
test_format "#{q/s:@sq}" "'a'\\''b'"
test_format "#{q/s:@nl}" "$q_s_nl"
# q/e and q/h escape "#" for the format/style parser by doubling it.
test_format "#{q/e:@hash}" 'a##b'
test_format "#{q/h:@hash}" 'a##b'
# q/a quotes the value as a single shell argument.
test_format "#{q/a:@sp}" '"a b\$c"'


# --- Name existence (N) --------------------------------------------------

# N/w is true if a window with the (expanded) name exists in the session, N/s
# if a session with that name exists.  The default (no argument) is /w.
$TMUX rename-window -t main:0 knownwin
test_format "#{N/s:main}" "1"
test_format "#{N/s:nosuchsession}" "0"
test_format "#{N/w:knownwin}" "1" "main:"
test_format "#{N/w:nosuchwindow}" "0" "main:"
test_format "#{N:nosuchwindow}" "0" "main:"


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
# a: out-of-range or non-numeric input yields an empty string.
test_format "#{a:200}" ""
test_format "#{a:notanumber}" ""
# R: repeat first argument second-argument times.
test_format "#{R:a,3}" "aaa"
test_format "#{R:ab,2}" "abab"
# A long repeat exercises output-buffer growth during expansion.
test_format "#{n:#{R:x,300}}" "300"


# --- Width, padding and truncation ---------------------------------------

# =N truncates from the start, =-N from the end.
test_format "#{=5:@s}" "abcde"
test_format "#{=-5:@s}" "fghij"
# = with no width, or a non-numeric width, does not truncate.
test_format "#{=:@s}" "abcdefghij"
test_format "#{=/x:@s}" "abcdefghij"
# A marker is appended/prepended only when trimming actually occurs.
test_format "#{=/5/...:@s}" "abcde..."
test_format "#{=/5/...:@name}" "windo..."
test_format "#{=/20/...:@s}" "abcdefghij"
# Truncation is display-width (UTF-8) aware: a wide (2-column) character is only
# included if it fits entirely within the limit.
test_format "#{=3:@greek}" "αβγ"
test_format "#{=2:@greek}" "αβ"
test_format "#{=2:@cjk}" "中"
test_format "#{=1:@cjk}" ""
# Markers with wide characters: the marker is added when trimming occurs, and a
# limit that splits a wide character drops it entirely.
test_format "#{=/2/x:@cjk}" "中x"
test_format "#{=/1/x:@cjk}" "x"

# p pads to a width: a positive width left-aligns (pads on the right), a
# negative width right-aligns (pads on the left).
test_format "#{p12:@name}" "window-name "
test_format "#{p-12:@name}" " window-name"
# No padding once the value already meets the width.
test_format "#{p3:@name}" "window-name"
# p with no width does nothing.
test_format "#{p:@name}" "window-name"
# Padding is display-width aware: @cjk is 4 columns wide, so p6/p-6 add two
# spaces (not four).
test_format "#{p6:@cjk}" "中文  "
test_format "#{p-6:@cjk}" "  中文"

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

# t/f: custom strftime format applied to the variable's time.  Tested in a
# format_expand context (list-windows -F), where a single strftime specifier is
# applied directly.  (In display-message, which additionally expands the format
# through strftime, these would need to be doubled - %%Y etc.)  The colon in the
# format is escaped as '#:' because it is otherwise the modifier separator.
test_expand "#{t/f/%Y:@ts}" "2001"
test_expand "#{t/f/%Y-%m-%d:@ts}" "2001-09-09"
test_expand "#{t/f/%H#:%M#:%S:@ts}" "01:46:40"
# An escaped comma in the custom format is unescaped before strftime.
test_expand "#{t/f/%Y#,end:@ts}" "2001,end"

# T: expands its argument and then runs the result through strftime with the
# current time.  A value with no strftime specifier is returned unchanged.
test_format "#{T:@ts}" "1000000000"

# t/p (pretty) and t/r (relative) format times by age relative to now, with a
# different branch per age band.  Build options a known number of seconds in the
# past and check each yields a non-empty result (the exact text depends on the
# wall clock, so only non-emptiness is asserted).
now=$(date +%s)
for age in 30 300 4000 90000 200000 3000000 40000000; do
	$TMUX set -g @age "$((now - age))"
	if [ -z "$($TMUX display-message -p '#{t/r:@age}')" ]; then
		echo "Empty #{t/r:@age} for age ${age}s"
		exit 1
	fi
	if [ -z "$($TMUX display-message -p '#{t/p:@age}')" ]; then
		echo "Empty #{t/p:@age} for age ${age}s"
		exit 1
	fi
done
# A time in the future has no relative form.
$TMUX set -g @future "$((now + 100000))"
test_format "#{t/r:@future}" ""


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
# "none" gives a reset; an unknown colour gives an empty string.
test_format "#{c/f:none}" "${ESC}[0m"
test_format "#{c:notacolour}" ""
test_format "#{c/f:notacolour}" ""


# --- Nesting and limits --------------------------------------------------

# Modifier chaining: inner b: then outer truncation/padding/length.
test_format "#{=5:#{b:@path}}" "foo"
test_format "#{=2:#{b:@path}}" "fo"
test_format "#{p6:#{b:@path}}" "foo   "
test_format "#{n:#{b:@path}}" "3"

# Nested l: literal expanded then truncated.
test_format "#{=5:#{l:abcdefghij}}" "abcde"

# Deeper nesting: basename -> pad to 10 -> truncate to 5.
test_format "#{=5:#{p10:#{b:@path}}}" "foo  "
# A substitution applied to a nested basename.
test_format "#{s/o/O/:#{b:@path}}" "fOO"

# Unbounded self-recursion must hit the loop limit rather than crash.
$TMUX set @rec '#{E:@rec}'
$TMUX display-message -p '#{E:@rec}' >/dev/null 2>&1
assert_alive "recursive expansion"


# --- Missing, malformed and limit inputs ---------------------------------

# An undefined variable expands to empty; modifiers on it behave sensibly.
test_format "#{@undefined}" ""
test_format "#{=5:@undefined}" ""
test_format "#{b:@undefined}" ""
test_format "#{n:@undefined}" "0"

# Malformed numeric expressions expand to empty rather than erroring out.
test_format "#{e|+|:notanumber,2}" ""     # invalid left operand
test_format "#{e|+|:2,notanumber}" ""     # invalid right operand
test_format "#{e|badop|:1,2}" ""          # unknown operator
test_format "#{e|+|f|x:1,2}" ""           # invalid precision
test_format "#{e|+|:1}" ""                # too few operands
test_format "#{e|+|f|2|extra:1,2}" ""     # too many arguments (limit is 3)

# Repeat with a non-numeric or zero count yields an empty string.
test_format "#{R:a,notanumber}" ""
test_format "#{R:a,0}" ""

# Comparisons with too few arguments expand to empty.
test_format "#{==:a}" ""
test_format "#{<:a}" ""

# A substitution with fewer than two arguments is a no-op.
test_format "#{s/onlyone:@s}" "abcdefghij"

# A non-numeric width for = or p is treated as no width (no change).
test_format "#{=/x:@s}" "abcdefghij"
test_format "#{p/x:@s}" "abcdefghij"

# The I (client terminal) modifier with no attached client is empty; this also
# exercises its argument parsing (/c termcap, /f feature, default).  The
# non-empty terminal cases are covered with a real client in format-variables.sh.
test_format "#{I/c:RGB}" ""
test_format "#{I/f:overline}" ""
test_format "#{I:x}" ""


# --- Escaping inside modifiers -------------------------------------------

# A "," or "#" inside a modifier argument is escaped with "#".
test_format "#{s/#,/-/:#{l:a,b,c}}" "a-b-c"   # escaped comma in the pattern
test_format "#{=/3/#,:@s}" "abc,"             # escaped comma in the marker
# The truncation marker is itself expanded as a format.
test_format "#{=/3/#{l:>}:@s}" "abc>"

# Substitution flags: a third argument of "i" is case-insensitive; an invalid
# regular expression leaves the text unchanged.
test_format "#{s/A/X/i:@s}" "Xbcdefghij"
test_format "#{s/[/X/:@s}" "abcdefghij"


# --- Unicode in modifier arguments ---------------------------------------

# Wide (CJK) and emoji text: matching, substitution, repeat and markers all
# operate on characters, and n/w report bytes/columns.
$TMUX set -g @emoji '😀😀' || exit 1   # 8 bytes, 4 columns
test_format "#{m:*中*,#{@cjk}}" "1"
test_format "#{s/文/X/:@cjk}" "中X"
test_format "#{R:中,3}" "中中中"
test_format "#{=/1/中:@s}" "a中"
test_format "#{w:@emoji}" "4"
test_format "#{n:@emoji}" "8"
test_format "#{=2:@emoji}" "😀"


# --- Server messages (show-messages) -------------------------------------

# show-messages formats each logged message (this exercises the message-time
# formatting path); just check the server survives producing it.
$TMUX show-messages >/dev/null 2>&1
assert_alive "show-messages"


# --- Verbose expansion (logging) -----------------------------------------

# display-message -v turns on format logging, so re-expanding a representative
# set of formats with -v exercises the logging code paths.  Only survival is
# checked; the log text itself is not asserted.
for f in \
    '#{=3:@s}' \
    '#{e|+|:2,3}' \
    '#{e|*|f|2:2.5,2}' \
    '#{m:*a*,abc}' \
    '#{<:3,5}' \
    '#{s/a/X/:@s}' \
    '#{b:@path}' \
    '#{t:@ts}' \
    '#{p6:@name}' \
    '#{=3:#{b:@path}}'; do
	$TMUX display-message -v -p "$f" >/dev/null 2>&1
done
assert_alive "verbose expansion"


# --- Loops and sorting (S, W, P, L) --------------------------------------
#
# These need a fully controlled server so the set of sessions, windows and
# panes (and their order) is known, so start from a clean server.  This must be
# the last section as it discards the setup above.
$TMUX kill-server 2>/dev/null
sleep 0.1

# Sessions, created in this order, so session ids (and hence creation order)
# are zeta=$0, alpha=$1, mike=$2.
$TMUX new-session -d -s zeta -x 80 -y 24 || exit 1
$TMUX new-session -d -s alpha || exit 1
$TMUX new-session -d -s mike || exit 1
$TMUX set -g automatic-rename off

# S loops over every session.  The default order is by session id (SORT_INDEX),
# /i is the same, /n is by name, and the r suffix reverses.
test_format "#{S:#{session_name} }" "zeta alpha mike "
test_format "#{S/i:#{session_name} }" "zeta alpha mike "
test_format "#{S/n:#{session_name} }" "alpha mike zeta "
test_format "#{S/nr:#{session_name} }" "zeta mike alpha "
test_format "#{S/ir:#{session_name} }" "mike alpha zeta "
# /t sorts by activity time; the exact order is timing-dependent, so just check
# every session is still iterated (this exercises the activity-sort branch).
test_format "#{S/t:x}" "xxx"
# An unrecognised sort letter falls back to the default order; /r on its own
# reverses that default (this covers the fall-through branch).
test_format "#{S/r:#{session_name} }" "mike alpha zeta "

# Windows in session zeta: window 0 renamed charlie, then alpha at 1, bravo at
# 2.  The default order is by index (SORT_ORDER), /n is by name, r reverses.
$TMUX rename-window -t zeta:0 charlie
$TMUX new-window -d -t zeta:1 -n alpha
$TMUX new-window -d -t zeta:2 -n bravo
test_format "#{W:#{window_name} }" "charlie alpha bravo " "zeta:"
test_format "#{W/n:#{window_name} }" "alpha bravo charlie " "zeta:"
test_format "#{W/nr:#{window_name} }" "charlie bravo alpha " "zeta:"
test_format "#{W/ir:#{window_index}}" "210" "zeta:"
# /i (by index) and /t (by activity); /i matches the default order here.
test_format "#{W/i:#{window_name} }" "charlie alpha bravo " "zeta:"
test_format "#{W/t:x}" "xxx" "zeta:"
# An unrecognised sort letter falls back to the default order; /r reverses it.
test_format "#{W/r:#{window_name} }" "bravo alpha charlie " "zeta:"

# Panes in window zeta:charlie.  Splitting the active (newest) pane each time
# makes pane index match creation order (0,1,2 left to right).  The default
# order is by creation (SORT_CREATION), r reverses.
$TMUX split-window -h -t zeta:charlie
$TMUX split-window -h -t zeta:charlie
test_format "#{P:#{pane_index}}" "012" "zeta:charlie"
test_format "#{P/r:#{pane_index}}" "210" "zeta:charlie"
# Pane sort accepts i (pane-list order) and z (z-order). Other sort letters
# fall back to the default creation order; r reverses whichever order is used.
test_format "#{P/i:x}" "xxx" "zeta:charlie"
test_format "#{P/i:#{pane_index}}" "012" "zeta:charlie"
test_format "#{P/z:x}" "xxx" "zeta:charlie"
test_format "#{P/n:x}" "xxx" "zeta:charlie"
test_format "#{P/t:x}" "xxx" "zeta:charlie"
$TMUX new-pane -d -t zeta:charlie -x 20 -y 10 -X 1 -Y 1
test_format "#{P/i:#{pane_index}}" "0123" "zeta:charlie"
test_format "#{P/z:#{pane_index}}" "3012" "zeta:charlie"
test_format "#{P/zr:#{pane_index}}" "0123" "zeta:charlie"

# Verbose expansion of the loops, to exercise their logging paths.
$TMUX display-message -v -p "#{S:#{session_name}}" >/dev/null 2>&1
$TMUX display-message -v -t zeta: -p "#{W:#{window_name}}" >/dev/null 2>&1
$TMUX display-message -v -t zeta:charlie -p "#{P:#{pane_index}}" >/dev/null 2>&1
assert_alive "verbose loop expansion"

# L loops over attached clients.  Attach two control-mode clients, each held
# open by a background process keeping a FIFO's write end open.
FIFO1="${TMPDIR:-/tmp}/fmt-l-$$-1"
FIFO2="${TMPDIR:-/tmp}/fmt-l-$$-2"
rm -f "$FIFO1" "$FIFO2"
mkfifo "$FIFO1" "$FIFO2" || exit 1
# Hold the write ends open so the control clients stay attached.
sleep 30 >"$FIFO1" &
HOLD1=$!
sleep 30 >"$FIFO2" &
HOLD2=$!
$TMUX -C attach -t zeta <"$FIFO1" >/dev/null 2>&1 &
CC1=$!
$TMUX -C attach -t alpha <"$FIFO2" >/dev/null 2>&1 &
CC2=$!
sleep 1
# Two clients attached: L emits one item per client.
test_format "#{L:x}" "xx"
# The client sort orders (default, index, name, activity, reversed) are all
# accepted; assert only the count so the test does not depend on client names or
# timing.
test_format "#{L/i:x}" "xx"
test_format "#{L/n:x}" "xx"
test_format "#{L/t:x}" "xx"
test_format "#{L/nr:x}" "xx"
test_format "#{L/r:x}" "xx"
# Now detach one and confirm the count drops to one.
kill $HOLD2 2>/dev/null
sleep 1
test_format "#{L:x}" "x"
kill $HOLD1 $CC1 $CC2 2>/dev/null
rm -f "$FIFO1" "$FIFO2"

exit 0
