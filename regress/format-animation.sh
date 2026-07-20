#!/bin/sh

# Tests of the status line animation formats described in tmux(1) FORMATS:
# the #{now_ms} monotonic millisecond clock and the #{cycle:<period_ms>:<frames>}
# frame selector (implemented in format.c). These are deterministic and do not
# depend on the sub-second status-interval timer, only on format expansion.
#
# The guardrail that keeps these tokens from making #() shell commands re-run on
# every frame (they expand to a fixed value inside #()) is anchored in the job
# code (format.c) and is validated separately with a fork-count measurement; see
# the commit message. Here we only check that a token embedded in #() does not
# break expansion.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"

trap "$TMUX kill-server 2>/dev/null" 0 1 15

# test_format $format $expected
#
# Expand $format with display-message -p and require an exact match.
test_format()
{
	fmt="$1"
	exp="$2"

	out=$($TMUX display-message -p "$fmt")
	if [ "$out" != "$exp" ]; then
		echo "Format test failed for '$fmt'."
		echo "Expected: '$exp'"
		echo "But got   '$out'"
		exit 1
	fi
}

# test_numeric $format
#
# Require the expansion to be a non-empty run of decimal digits.
test_numeric()
{
	fmt="$1"

	out=$($TMUX display-message -p "$fmt")
	case "$out" in
	''|*[!0-9]*)
		echo "Format test failed for '$fmt'."
		echo "Expected a number but got '$out'"
		exit 1
		;;
	esac
}

# test_oneof $format $frame...
#
# Require the expansion to equal one of the given frames (used for cycle
# selections whose index depends on the clock and so is not fixed).
test_oneof()
{
	fmt="$1"
	shift

	out=$($TMUX display-message -p "$fmt")
	for exp in "$@"; do
		[ "$out" = "$exp" ] && return 0
	done
	echo "Format test failed for '$fmt'."
	echo "Expected one of '$*' but got '$out'"
	exit 1
}

$TMUX kill-server 2>/dev/null
$TMUX new-session -d || exit 1

# #{now_ms} is a monotonic millisecond clock inside a time-expanded format such
# as a display-message template.
test_numeric "#{now_ms}"

# It must not go backwards between two expansions.
a=$($TMUX display-message -p "#{now_ms}")
b=$($TMUX display-message -p "#{now_ms}")
if [ "$b" -lt "$a" ]; then
	echo "#{now_ms} went backwards: $a then $b"
	exit 1
fi

# A single-frame cycle always yields that frame regardless of the clock.
test_format "#{cycle:1000:x}" "x"
test_format "#{cycle:60000:only}" "only"

# A multi-frame cycle yields exactly one of its space-separated frames.
test_oneof "#{cycle:100:aa bb cc}" "aa" "bb" "cc"

# Frames may be multibyte (UTF-8) and are preserved byte for byte.
test_oneof "#{cycle:1000:⠋ ⠙ ⠹}" "⠋" "⠙" "⠹"

# Invalid input never crashes the expansion and yields the empty string:
# a zero or non-numeric period, an empty frame list, or a missing frame list.
test_format "#{cycle:0:a b}" ""
test_format "#{cycle:-1:a b}" ""
test_format "#{cycle:abc:a b}" ""
test_format "#{cycle:100:}" ""
test_format "#{cycle:100}" ""

# The "cycle:" helper is matched exactly, so keys that merely start with "cycle"
# are left to normal (here: unknown) variable resolution and are not intercepted.
test_format "#{cycle}" ""
test_format "#{cyclez}" ""

# Unrelated variables beginning with "c" are unaffected by the cycle intercept;
# config_files resolves normally (here the single -f argument, /dev/null).
test_format "#{config_files}" "/dev/null"

# A token embedded in #() does not break expansion. #() output is asynchronous,
# so the first expansion is empty; the point is only that it neither errors nor
# produces a stray value.
test_format "#(true; echo #{now_ms})" ""

# The server must still be alive after all of the above.
if [ "$($TMUX display-message -p alive)" != "alive" ]; then
	echo "Server died during animation format tests"
	exit 1
fi

$TMUX kill-server 2>/dev/null
exit 0
