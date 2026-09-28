# Shared setup for the image-*-noflash.sh tests: each drives some mouse or
# keyboard interaction that must not retransmit an unrelated pane's image,
# checked by counting DCS (\033P) sequences in the client's raw output.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

# A small, distinctive 26x26-pixel SIXEL raster, used across these tests as a
# stand-in image whose retransmission (or lack of it) is being checked for.
SIXEL_HEADER='\033Pq"1;1;26;26#0;2;100;100;100#0!26~-!26~-!26~-!26~-!26B\033\\'

cleanup()
{
	$TMUX kill-server >/dev/null 2>&1
	$TMUX2 kill-server >/dev/null 2>&1
}
fail()
{
	echo "$*" >&2
	cleanup
	exit 1
}

cleanup

TMP=$(mktemp)
trap "cleanup; rm -f $TMP" 0 1 15

# start_capture SX SY - attach an outer client to the inner session and
# start capturing its raw output into $TMP. Starts the outer session with a
# plain shell, then begins capturing before triggering the attach - starting
# the attach as the outer pane's initial command would mean pipe-pane only
# starts after the attach-driven initial redraw (which sends the image) has
# already happened, missing it.
start_capture()
{
	sx=$1
	sy=$2

	$TMUX2 new-session -d -x "$sx" -y "$sy" || exit 1
	OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
	[ -n "$OUTER" ] || fail "No outer pane."
	$TMUX2 pipe-pane -t "$OUTER" -O "cat >$TMP" || fail "pipe-pane failed"
	$TMUX2 send-keys -t "$OUTER" -l "$TMUX attach -t inner" ||
		fail "send attach failed"
	$TMUX2 send-keys -t "$OUTER" Enter || fail "send enter failed"
	sleep 1
}

# Sanity check: the image reached the client at all.
assert_image_reached()
{
	grep -qa '"1;1;26;26' $TMP || fail "sanity: image never reached the client"
}

# assert_no_retransmit CONTEXT - no DCS sequence should have been sent since
# $TMP was last cleared. Expected to fail before the fix each test guards -
# see that test's header comment.
assert_no_retransmit()
{
	n=$(grep -ac "$(printf '\033P')" $TMP)
	[ "$n" -eq 0 ] || fail "image was retransmitted ($n times) $1"
}
