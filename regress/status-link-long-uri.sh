#!/bin/sh

# A #[link=] URI longer than the 2048-byte buffer style_tostring() used to
# have. The status line is drawn with a 3 KB link: the server must survive the
# draw and the whole URI must reach the terminal. With the old buffer the
# server aborts inside style_tostring() ("xsnprintf: overflow") while drawing;
# with the old 255-byte attribute cap in style_parse() the style is rejected
# and no link is sent at all.
#
# The inner server runs with -v because style_tostring() is only reached from a
# log_debug() call in format-draw.c.

. ./regress-common.inc

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

LOGDIR=$(test_tmpdir) || exit 1

cleanup()
{
	$TMUX2 kill-server >/dev/null 2>&1
	$TMUX kill-server >/dev/null 2>&1
	rm -rf "$LOGDIR"
}
trap cleanup 0 1 15

fail()
{
	echo "$*" >&2
	exit 1
}

URI="https://example.com/?$(head -c 3000 /dev/zero | tr '\0' a)"

# Start the inner server in LOGDIR so its log does not land in the source tree,
# and with logging on so style_tostring() is called at all.
(cd "$LOGDIR" && $TMUX -v new-session -d -s inner -x 40 -y 10 'sleep 100') ||
    fail "failed to start the inner server"
$TMUX2 new-session -d -s outer -x 40 -y 10 "$TMUX attach -t inner" ||
    fail "failed to start the outer server"
wait_for_clients 1 $TMUX || fail "the inner client did not attach"

status_row()
{
	$TMUX2 capture-pane -ep -t outer:0.0 | tail -1
}

status_row_has_uri()
{
	status_row | grep -Fq -- "$URI"
}

$TMUX set -g status-format[0] "#[link=$URI]LINK" >/dev/null 2>&1
$TMUX has-session -t inner >/dev/null 2>&1 || fail "the server died drawing the link"
wait_until status_row_has_uri || fail "the link did not reach the terminal"

exit 0
