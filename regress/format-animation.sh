#!/bin/sh

# Tests of the status line #{cycle:<period_ms>:<frames>} animation format
# described in tmux(1) FORMATS (implemented in format.c). These are
# deterministic and depend only on format expansion, not on any status timer.
#
# Inside #() the animation is deliberately static (the time context is dropped),
# so a cycle embedded in #() cannot make the command re-run on every frame. Here
# we only check that such a use does not break expansion.

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

# A cycle embedded in #() does not break expansion. #() output is asynchronous,
# so the first expansion is empty; the point is only that it neither errors nor
# produces a stray value.
test_format "#(true; echo #{cycle:100:a b})" ""

# The server must still be alive after all of the above.
if [ "$($TMUX display-message -p alive)" != "alive" ]; then
	echo "Server died during animation format tests"
	exit 1
fi

$TMUX kill-server 2>/dev/null
exit 0
